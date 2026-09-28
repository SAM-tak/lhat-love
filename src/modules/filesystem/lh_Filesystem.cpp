/**
 * Copyright (c) 2006-2026 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

// love.filesystem for L^. The reference is wrap_Filesystem.cpp beside this
// file. File and FileData are the first LOVE objects handed to L^ as
// hostdata (05 の 8.8): a fresh wrapper per push, released by dispose.
//
// What can fail on the disk's say-so answers love.filesystem.Error.IO and the caller
// writes catch^ / try^; what can only fail by the caller's mistake panics.

#include "Filesystem.h"
#include "File.h"
#include "FileData.h"
#include "lh/lh.h"

#include <mutex>

#include <string>
#include <vector>

namespace love
{
namespace filesystem
{

#define instance() (Module::getInstance<Filesystem>(Module::M_FILESYSTEM))

struct FilesystemBinding
{
	lh::Errors *errors;
	lh::TypeRegistry *registry;
	LhatProgram *program;
};

static LhatValue dataAsString(LhatMachine *machine, FileData *data)
{
	StrongRef<FileData> owned(data, Acquire::NORETAIN);
	LhatValue out = lhat_nil();
	lh::makeString(machine, std::string((const char *) data->getData(), data->getSize()), &out);
	return out;
}

// ---------------------------------------------------------------------------
// Module functions
// ---------------------------------------------------------------------------

static void lh_read(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	std::string path = lh::optString(arguments, count, 0, "");
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		if (count > 1 && lhat_is_number(arguments[1]))
		{
			answers[0] = dataAsString(machine, instance()->read(path.c_str(), (int64) lh::optNumber(arguments, count, 1, 0)));
			*answerCount = 1;
			return;
		}
		answers[0] = dataAsString(machine, instance()->read(path.c_str()));
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_write(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	std::string path = lh::optString(arguments, count, 0, "");
	size_t length = 0;
	const char *data = count > 1 ? lh::stringOf(arguments[1], &length) : nullptr;
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		instance()->write(path.c_str(), data != nullptr ? data : "", (int64) length);
		answers[0] = lhat_nil();
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_append(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	std::string path = lh::optString(arguments, count, 0, "");
	size_t length = 0;
	const char *data = count > 1 ? lh::stringOf(arguments[1], &length) : nullptr;
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		instance()->append(path.c_str(), data != nullptr ? data : "", (int64) length);
		answers[0] = lhat_nil();
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_exists(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	answers[0] = lhat_bool(instance()->exists(lh::optString(arguments, count, 0, "").c_str()));
	*answerCount = 1;
}

// getInfo(path) -> { type, size, modtime } or nil.
static void lh_getInfo(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	Filesystem::Info info = {};
	if (!instance()->getInfo(lh::optString(arguments, count, 0, "").c_str(), info))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}

	const char *typestr = "other";
	Filesystem::getConstant(info.type, typestr);

	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatTable *t = (LhatTable *) lhat_as_object(table);

	LhatValue key = lhat_nil(), value = lhat_nil();
	bool refused = false;
	lh::makeString(machine, "type", &key);
	value = lh::pushEnum(machine, "love.filesystem", "FileType", typestr);
	lhat_table_set(t, key, value, &refused);
	lh::makeString(machine, "size", &key);
	lhat_table_set(t, key, lhat_integer(info.size), &refused);
	lh::makeString(machine, "modtime", &key);
	lhat_table_set(t, key, lhat_integer(info.modtime), &refused);
	lh::makeString(machine, "readonly", &key);
	lhat_table_set(t, key, lhat_bool(info.readonly), &refused);
	answers[0] = table;
	*answerCount = 1;
}

static void lh_getDirectoryItems(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<std::string> items;
	instance()->getDirectoryItems(lh::optString(arguments, count, 0, "").c_str(), items);

	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < items.size(); i++)
	{
		LhatValue value = lhat_nil();
		bool refused = false;
		lh::makeString(machine, items[i], &value);
		lhat_table_set(t, lhat_integer((int64_t) i), value, &refused);
	}
	answers[0] = table;
	*answerCount = 1;
}

static void lh_createDirectory(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	answers[0] = lhat_bool(instance()->createDirectory(lh::optString(arguments, count, 0, "").c_str()));
	*answerCount = 1;
}

static void lh_remove(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	answers[0] = lhat_bool(instance()->remove(lh::optString(arguments, count, 0, "").c_str()));
	*answerCount = 1;
}

static void lh_getSaveDirectory(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getSaveDirectory(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_getIdentity(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getIdentity(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_setIdentity(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	std::string name = lh::optString(arguments, count, 0, "");
	answers[0] = lhat_bool(instance()->setIdentity(name.c_str(), lh::optBool(arguments, count, 1, false)));
	*answerCount = 1;
}

static void lh_getSource(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getSource(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_getSourceBaseDirectory(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getSourceBaseDirectory(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_isFused(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->isFused());
	*answerCount = 1;
}

static void lh_getWorkingDirectory(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getWorkingDirectory(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_getUserDirectory(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getUserDirectory(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_getRealDirectory(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string path = lh::optString(arguments, count, 0, "");
	try
	{
		LhatValue out = lhat_nil();
		lh::makeString(machine, instance()->getRealDirectory(path.c_str()), &out);
		answers[0] = out;
		*answerCount = 1;
		return;
	}
	catch (const love::Exception &)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
}

// load(path) -> a closure of the unit's top level, or an error: IO when
// the loader has nothing there, Misuse when it does not check. 05 の 5.6:
// the same mechanism as std.load, so the answer is an ordinary closure
// the caller may run more than once.
static void lh_load(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	std::string path = lh::optString(arguments, count, 0, "");
	LhatProto *proto = nullptr;
	// 05 の 5.8: the program takes its own lock over this now.
	LhatLoadStatus status = lhat_program_load_file(b->program, path.c_str(), &proto);
	switch (status)
	{
	case LHAT_LOAD_OK:
		break;
	case LHAT_LOAD_CANNOT_READ:
		answers[0] = lh::fail(machine, b->errors->filesystemIO, "Could not read " + path);
		*answerCount = 1;
		return;
	case LHAT_LOAD_REJECTED:
	{
		const char *why = lhat_program_load_failure(b->program);
		answers[0] = lh::fail(machine, b->errors->filesystemRejected, why != nullptr ? why : "The unit did not check");
		*answerCount = 1;
		return;
	}
	case LHAT_LOAD_OUT_OF_MEMORY:
	default:
		answers[0] = lh::fail(machine, b->errors->filesystemRejected, "Out of memory loading " + path);
		*answerCount = 1;
		return;
	}
	LhatValue closure = lhat_nil();
	if (!lhat_machine_adopt_script(machine, proto, &closure))
	{
		answers[0] = lh::fail(machine, b->errors->filesystemRejected, "Could not adopt " + path);
		*answerCount = 1;
		return;
	}
	answers[0] = closure;
	*answerCount = 1;
}

// ---------------------------------------------------------------------------
// File
// ---------------------------------------------------------------------------

static File *checkFile(LhatMachine *machine, const FilesystemBinding *b, const LhatValue *arguments, size_t count)
{
	File *file = count > 0 ? lh::checkObject<File>(arguments[0], *b->registry) : nullptr;
	if (file == nullptr)
		lh::raise(machine, "Expected a File");
	return file;
}

static void lh_newFile(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	std::string path = lh::optString(arguments, count, 0, "");
	File::Mode mode = File::MODE_CLOSED;
	if (count > 1)
	{
		std::string modestr = lh::optEnum(machine, arguments, count, 1, "love.filesystem", "FileMode", "");
		if (!File::getConstant(modestr.c_str(), mode))
		{
			lh::raise(machine, "Invalid file open mode: " + modestr);
			return;
		}
	}
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		File *file = instance()->openFile(path.c_str(), mode);
		LhatValue out = lh::pushObject(machine, *b->registry, file);
		file->release(); // pushObject retained it
		answers[0] = out;
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_File_open(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	File *file = checkFile(machine, b, arguments, count);
	if (file == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::string modestr = lh::optEnum(machine, arguments, count, 1, "love.filesystem", "FileMode", "r");
	File::Mode mode;
	if (!File::getConstant(modestr.c_str(), mode))
	{
		lh::raise(machine, "Invalid file open mode: " + modestr);
		return;
	}
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		answers[0] = lhat_bool(file->open(mode));
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_File_close(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						  LhatValue *answers, int *answerCount)
{
	File *file = checkFile(machine, (const FilesystemBinding *) context, arguments, count);
	answers[0] = lhat_bool(file != nullptr && file->close());
	*answerCount = 1;
}

static void lh_File_isOpen(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	File *file = checkFile(machine, (const FilesystemBinding *) context, arguments, count);
	answers[0] = lhat_bool(file != nullptr && file->isOpen());
	*answerCount = 1;
}

static void lh_File_read(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	File *file = checkFile(machine, b, arguments, count);
	if (file == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		if (count > 1 && lhat_is_number(arguments[1]))
		{
			answers[0] = dataAsString(machine, file->read((int64) lhat_number_as_real(arguments[1])));
			*answerCount = 1;
			return;
		}
		answers[0] = dataAsString(machine, file->read());
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_File_write(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						  LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	File *file = checkFile(machine, b, arguments, count);
	if (file == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	size_t length = 0;
	const char *data = count > 1 ? lh::stringOf(arguments[1], &length) : nullptr;
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		answers[0] = lhat_bool(file->write(data != nullptr ? data : "", (int64) length));
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_File_getSize(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	File *file = checkFile(machine, (const FilesystemBinding *) context, arguments, count);
	answers[0] = lhat_integer(file != nullptr ? file->getSize() : 0);
	*answerCount = 1;
}

static void lh_File_isEOF(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						  LhatValue *answers, int *answerCount)
{
	File *file = checkFile(machine, (const FilesystemBinding *) context, arguments, count);
	answers[0] = lhat_bool(file == nullptr || file->isEOF());
	*answerCount = 1;
}

static void lh_File_getFilename(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	File *file = checkFile(machine, (const FilesystemBinding *) context, arguments, count);
	LhatValue out = lhat_nil();
	if (file != nullptr)
		lh::makeString(machine, file->getFilename(), &out);
	answers[0] = out;
	*answerCount = 1;
}

// ---------------------------------------------------------------------------
// FileData
// ---------------------------------------------------------------------------

static FileData *checkFileData(LhatMachine *machine, const FilesystemBinding *b, const LhatValue *arguments, size_t count)
{
	FileData *data = count > 0 ? lh::checkObject<FileData>(arguments[0], *b->registry) : nullptr;
	if (data == nullptr)
		lh::raise(machine, "Expected a FileData");
	return data;
}

// newFileData(contents, name) -- from a string; newFileData(path) -- read
// from the mounted filesystem (answers IO).
static void lh_newFileData(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	const FilesystemBinding *b = (const FilesystemBinding *) context;
	if (count >= 2)
	{
		size_t length = 0;
		const char *contents = lh::stringOf(arguments[0], &length);
		std::string name = lh::optString(arguments, count, 1, "");
		FileData *data = instance()->newFileData(contents != nullptr ? contents : "", length, name.c_str());
		LhatValue out = lh::pushObject(machine, *b->registry, data);
		data->release();
		answers[0] = out;
		*answerCount = 1;
		return;
	}
	std::string path = lh::optString(arguments, count, 0, "");
	lh::catchexcept(machine, b->errors->filesystemIO, [&]() {
		FileData *data = instance()->read(path.c_str());
		LhatValue out = lh::pushObject(machine, *b->registry, data);
		data->release();
		answers[0] = out;
		*answerCount = 1;
	}, answers, answerCount);
}

static void lh_FileData_getString(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	FileData *data = checkFileData(machine, (const FilesystemBinding *) context, arguments, count);
	LhatValue out = lhat_nil();
	if (data != nullptr)
		lh::makeString(machine, std::string((const char *) data->getData(), data->getSize()), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_FileData_getSize(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	FileData *data = checkFileData(machine, (const FilesystemBinding *) context, arguments, count);
	answers[0] = lhat_integer(data != nullptr ? (int64_t) data->getSize() : 0);
	*answerCount = 1;
}

static void lh_FileData_getFilename(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									LhatValue *answers, int *answerCount)
{
	FileData *data = checkFileData(machine, (const FilesystemBinding *) context, arguments, count);
	LhatValue out = lhat_nil();
	if (data != nullptr)
		lh::makeString(machine, data->getFilename(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_FileData_getExtension(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									 LhatValue *answers, int *answerCount)
{
	FileData *data = checkFileData(machine, (const FilesystemBinding *) context, arguments, count);
	LhatValue out = lhat_nil();
	if (data != nullptr)
		lh::makeString(machine, data->getExtension(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static FilesystemBinding binding;

} // filesystem

namespace lh
{

bool lhopen_love_filesystem(Context &ctx)
{
	using namespace love::filesystem;
	if (ctx.types())
	{
		if (!ctx.enumType("love.filesystem", "FileType", Filesystem::getConstants(Filesystem::FileType{})))
			return false;
		if (!ctx.enumType("love.filesystem", "FileMode", File::getConstants(File::Mode{})))
			return false;
	}

	const char *m = "love.filesystem";

	// 04 の 2.4: what love.filesystem can fail at, declared where it fails.
	if (ctx.types())
	{
		static const char *const variants[] = {"IO", "Rejected"};
		const LhatErrorKind *kinds[2] = {nullptr, nullptr};
		if (!ctx.errorKind(m, variants, 2, kinds))
			return false;
		ctx.errors->filesystemIO = kinds[0];
		ctx.errors->filesystemRejected = kinds[1];
	}
	if (!ctx.objectType(m, "File", File::type) || !ctx.objectType(m, "FileData", FileData::type))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;
	binding.program = ctx.program;
	void *b = &binding;

	return ctx.func(m, "read", "p^string^ -> string^|love.filesystem.Error.IO;", lh_read, b)
		&& ctx.func(m, "read", "p^string^, number^ -> string^|love.filesystem.Error.IO;", lh_read, b)
		&& ctx.func(m, "write", "p^string^, string^ -> nil^|love.filesystem.Error.IO;", lh_write, b)
		&& ctx.func(m, "append", "p^string^, string^ -> nil^|love.filesystem.Error.IO;", lh_append, b)
		&& ctx.func(m, "exists", "f^string^ -> bool^;", lh_exists, b)
		&& ctx.func(m, "getInfo", "f^string^ -> t^{ type : love.filesystem.FileType, size : number^, modtime : number^, readonly : bool^ }|nil^;", lh_getInfo, b)
		&& ctx.func(m, "getDirectoryItems", "f^string^ -> t^{string^[]};", lh_getDirectoryItems, b)
		&& ctx.func(m, "createDirectory", "p^string^ -> bool^;", lh_createDirectory, b)
		&& ctx.func(m, "remove", "p^string^ -> bool^;", lh_remove, b)
		&& ctx.func(m, "getSaveDirectory", "f^ -> string^;", lh_getSaveDirectory, b)
		&& ctx.func(m, "getIdentity", "f^ -> string^;", lh_getIdentity, b)
		&& ctx.func(m, "setIdentity", "p^string^ -> bool^;", lh_setIdentity, b)
		&& ctx.func(m, "setIdentity", "p^string^, bool^ -> bool^;", lh_setIdentity, b)
		&& ctx.func(m, "getSource", "f^ -> string^;", lh_getSource, b)
		&& ctx.func(m, "getSourceBaseDirectory", "f^ -> string^;", lh_getSourceBaseDirectory, b)
		&& ctx.func(m, "isFused", "f^ -> bool^;", lh_isFused, b)
		&& ctx.func(m, "getWorkingDirectory", "f^ -> string^;", lh_getWorkingDirectory, b)
		&& ctx.func(m, "getUserDirectory", "f^ -> string^;", lh_getUserDirectory, b)
		&& ctx.func(m, "getRealDirectory", "f^string^ -> string^|nil^;", lh_getRealDirectory, b)
		&& ctx.func(m, "load", "p^string^ -> p^... -> any^; | love.filesystem.Error;", lh_load, b)
		&& ctx.func(m, "newFile", "p^string^ -> love.filesystem.File|love.filesystem.Error.IO;", lh_newFile, b)
		&& ctx.func(m, "newFile", "p^string^, love.filesystem.FileMode -> love.filesystem.File|love.filesystem.Error.IO;", lh_newFile, b)
		&& ctx.member(m, "File", "open", "p^self^, love.filesystem.FileMode -> bool^|love.filesystem.Error.IO;", lh_File_open, b)
		&& ctx.member(m, "File", "close", "p^self^ -> bool^;", lh_File_close, b)
		&& ctx.member(m, "File", "isOpen", "f^self^ -> bool^;", lh_File_isOpen, b)
		&& ctx.member(m, "File", "read", "p^self^ -> string^|love.filesystem.Error.IO;", lh_File_read, b)
		&& ctx.member(m, "File", "read", "p^self^, number^ -> string^|love.filesystem.Error.IO;", lh_File_read, b)
		&& ctx.member(m, "File", "write", "p^self^, string^ -> bool^|love.filesystem.Error.IO;", lh_File_write, b)
		&& ctx.member(m, "File", "getSize", "f^self^ -> number^;", lh_File_getSize, b)
		&& ctx.member(m, "File", "isEOF", "f^self^ -> bool^;", lh_File_isEOF, b)
		&& ctx.member(m, "File", "getFilename", "f^self^ -> string^;", lh_File_getFilename, b)
		&& ctx.func(m, "newFileData", "p^string^ -> love.filesystem.FileData|love.filesystem.Error.IO;", lh_newFileData, b)
		&& ctx.func(m, "newFileData", "f^string^, string^ -> love.filesystem.FileData;", lh_newFileData, b)
		&& ctx.member(m, "FileData", "getString", "f^self^ -> string^;", lh_FileData_getString, b)
		&& ctx.member(m, "FileData", "getSize", "f^self^ -> number^;", lh_FileData_getSize, b)
		&& ctx.member(m, "FileData", "getFilename", "f^self^ -> string^;", lh_FileData_getFilename, b)
		&& ctx.member(m, "FileData", "getExtension", "f^self^ -> string^;", lh_FileData_getExtension, b);
}

} // lh
} // love

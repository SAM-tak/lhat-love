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

// love.data for L^: encoding, hashing and compression over strings. The
// reference is wrap_DataModule.cpp beside this file. ByteData, DataView
// and the Data containers come with the modules that need them.

#include "DataModule.h"
#include "CompressedData.h"
#include "lh/lh.h"

#include <string>

namespace love
{
namespace data
{

struct DataBinding
{
	lh::Errors *errors;
};

static DataBinding binding;

static bool bytesOf(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index, const char *&bytes, size_t &size)
{
	bytes = index < count ? lh::stringOf(arguments[index], &size) : nullptr;
	if (bytes == nullptr)
	{
		lh::raise(machine, "Expected a string");
		return false;
	}
	return true;
}

// encode(format, text) -> string; format is "base64" or "hex".
static void lh_encode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string formatstr = lh::optEnum(machine, arguments, count, 0, "love.data", "EncodeFormat", "");
	EncodeFormat format;
	if (!getConstant(formatstr.c_str(), format))
	{
		lh::raise(machine, "Invalid encode format: " + formatstr);
		return;
	}
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	size_t linelen = (size_t) lh::optNumber(arguments, count, 2, 0);
	lh::guard(machine, [&]() {
		size_t dstlen = 0;
		char *dst = encode(format, bytes, size, dstlen, linelen);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string(dst != nullptr ? dst : "", dstlen), &out);
		delete[] dst;
		answers[0] = out;
		*answerCount = 1;
	});
}

static void lh_decode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string formatstr = lh::optEnum(machine, arguments, count, 0, "love.data", "EncodeFormat", "");
	EncodeFormat format;
	if (!getConstant(formatstr.c_str(), format))
	{
		lh::raise(machine, "Invalid encode format: " + formatstr);
		return;
	}
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		size_t dstlen = 0;
		char *dst = decode(format, bytes, size, dstlen);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string(dst != nullptr ? dst : "", dstlen), &out);
		delete[] dst;
		answers[0] = out;
		*answerCount = 1;
	});
}

// hash(function, text) -> the raw digest bytes, as a string.
static void lh_hash(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string funcstr = lh::optEnum(machine, arguments, count, 0, "love.data", "HashFunction", "");
	HashFunction::Function function;
	if (!HashFunction::getConstant(funcstr.c_str(), function))
	{
		lh::raise(machine, "Invalid hash function: " + funcstr);
		return;
	}
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		LhatValue out = lhat_nil();
		lh::makeString(machine, hash(function, bytes, size), &out);
		answers[0] = out;
		*answerCount = 1;
	});
}

// compress(format, text[, level]) -> the compressed bytes, as a string.
static void lh_compress(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string formatstr = lh::optEnum(machine, arguments, count, 0, "love.data", "CompressedDataFormat", "");
	Compressor::Format format;
	if (!Compressor::getConstant(formatstr.c_str(), format))
	{
		lh::raise(machine, "Invalid compressed data format: " + formatstr);
		return;
	}
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	int level = (int) lh::optNumber(arguments, count, 2, -1);
	lh::guard(machine, [&]() {
		StrongRef<CompressedData> data(compress(format, bytes, size, level), Acquire::NORETAIN);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string((const char *) data->getData(), data->getSize()), &out);
		answers[0] = out;
		*answerCount = 1;
	});
}

static void lh_decompress(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string formatstr = lh::optEnum(machine, arguments, count, 0, "love.data", "CompressedDataFormat", "");
	Compressor::Format format;
	if (!Compressor::getConstant(formatstr.c_str(), format))
	{
		lh::raise(machine, "Invalid compressed data format: " + formatstr);
		return;
	}
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		size_t rawsize = 0;
		char *raw = decompress(format, bytes, size, rawsize);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string(raw != nullptr ? raw : "", rawsize), &out);
		delete[] raw;
		answers[0] = out;
		*answerCount = 1;
	});
}

} // data

namespace lh
{

bool lhopen_love_data(Context &ctx)
{
	using namespace love::data;
	if (ctx.types())
	{
		if (!ctx.enumType("love.data", "EncodeFormat", getConstants(EncodeFormat{})))
			return false;
		if (!ctx.enumType("love.data", "HashFunction", HashFunction::getConstants(HashFunction::Function{})))
			return false;
		if (!ctx.enumType("love.data", "CompressedDataFormat", Compressor::getConstants(Compressor::Format{})))
			return false;
	}

	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	const char *m = "love.data";
	return ctx.func(m, "encode", "f^love.data.EncodeFormat, string^ -> string^;", lh_encode, nullptr)
		&& ctx.func(m, "encode", "f^love.data.EncodeFormat, string^, number^ -> string^;", lh_encode, nullptr)
		&& ctx.func(m, "decode", "f^love.data.EncodeFormat, string^ -> string^;", lh_decode, nullptr)
		&& ctx.func(m, "hash", "f^love.data.HashFunction, string^ -> string^;", lh_hash, nullptr)
		&& ctx.func(m, "compress", "f^love.data.CompressedDataFormat, string^ -> string^;", lh_compress, nullptr)
		&& ctx.func(m, "compress", "f^love.data.CompressedDataFormat, string^, number^ -> string^;", lh_compress, nullptr)
		&& ctx.func(m, "decompress", "f^love.data.CompressedDataFormat, string^ -> string^;", lh_decompress, nullptr);
}

} // lh
} // love

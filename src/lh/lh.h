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

#ifndef LOVE_LH_LH_H
#define LOVE_LH_LH_H

// The glue between LOVE and the L^ runtime. This takes over the role that
// src/common/runtime.h (the luax_* helpers) played for Lua: registering
// what the engine provides, converting values at the boundary, and owning
// the program/machine pair. Nothing here is part of the L^ language.

#include "common/config.h"
#include "common/Exception.h"
#include "common/Object.h"
#include "common/Variant.h"
#include "common/int.h"

#include <lhat.h>
#include "stdlib/carry.h"

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace love
{
namespace lh
{

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

// 05 の 8.7: every registration belongs before the check, and a signature
// may only name types registered before it. Each module registrar is
// therefore called twice: once to declare its types, once to fill in
// members and functions against the now-known types.
enum class Phase
{
	TYPES,
	MEMBERS,
};

// 04 の 2.4: an error kind is its declaration, and a declaration belongs
// where the failures it names come from. So each module that can fail
// declares its own -- love.audio.Error, love.graphics.Error -- rather than
// every one of them answering with variants of a single love.Error.
//
// What a variant is named for is what happened, not which layer noticed:
// love.audio.Error.CouldNotLoad rather than an IO shared by everyone. A
// module with one variant needs no leaf in a signature at all, since
// `-> love.audio.Source|love.audio.Error` takes the one it has.
//
// Registered by the module's own registrar, in the TYPES phase, before any
// signature names it.
struct Errors
{
	// love.audio.Error{CouldNotLoad} -- the file is not a sound.
	const LhatErrorKind *audioCouldNotLoad = nullptr;
	// love.sound.Error{CouldNotLoad} -- nothing here decodes it.
	const LhatErrorKind *soundCouldNotLoad = nullptr;
	// love.image.Error{CouldNotLoad} -- the bytes are not an image.
	const LhatErrorKind *imageCouldNotLoad = nullptr;
	// love.graphics.Error{CouldNotLoad, ShaderFailed} -- a texture, font or
	// video that would not load, and a shader the driver refused.
	const LhatErrorKind *graphicsCouldNotLoad = nullptr;
	const LhatErrorKind *graphicsShaderFailed = nullptr;
	// love.filesystem.Error{IO, Rejected} -- the device said no, and a unit
	// that would not check or compile.
	const LhatErrorKind *filesystemIO = nullptr;
	const LhatErrorKind *filesystemRejected = nullptr;
};

// 05 の 8.8: one hostdata tag per LOVE object type. The tag is what a later
// call checks before reading the pointer as the type it expects; the
// love::Type beside it is what lets a union parameter (draw(Drawable)) be
// served by one check, through love::Type::isa.
class TypeRegistry
{
public:

	// The tag registered for exactly `type`, else for the nearest registered
	// type it derives from, else nullptr.
	const LhatHostDataTag *tagFor(love::Type &type) const;
	love::Type *typeFor(const LhatHostDataTag *tag) const;

	void add(love::Type &type, const LhatHostDataTag *tag);

private:

	std::map<love::Type *, const LhatHostDataTag *> tags;
	std::map<const LhatHostDataTag *, love::Type *> types;
};

class ParkingLot;

struct Context
{
	LhatProgram *program = nullptr;
	Phase phase = Phase::TYPES;
	Errors *errors = nullptr;
	TypeRegistry *registry = nullptr;
	ParkingLot *lot = nullptr;     // where a binding parks values it must keep
	std::string *failed = nullptr; // the registration that refused, for the report

	bool types() const { return phase == Phase::TYPES; }
	bool members() const { return phase == Phase::MEMBERS; }

	// Thin wrappers that keep the registration calls short at the call site.
	bool func(const char *module, const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool member(const char *module, const char *type, const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool global(const char *name, const char *signature, LhatHostFn fn, void *ctx) const;
	bool bind(const char *name, const char *member) const;
	bool enumType(const char *module, const char *name, const std::vector<std::string> &members) const;
	// The same with each member's .value given, for an enum whose numbers mean
	// something outside L^ (a mouse button is 1, 2, 3 to SDL and to LOVE).
	bool enumType(const char *module, const char *name, const std::vector<std::string> &members,
	              const std::vector<int64_t> &values) const;
	template <typename T>
	bool enumType(const char *module, const char *name, T end, bool (*constant)(T, const char *&)) const
	{
		if (!types())
			return true;
		std::vector<std::string> members;
		for (int i = 0; i < (int) end; ++i)
		{
			const char *spelling = nullptr;
			if (constant((T) i, spelling))
				members.emplace_back(spelling);
		}
		return enumType(module, name, members);
	}

	// 04 の 2.4: the module's own error declaration. Call in the TYPES phase,
	// before any signature names it. `variants` is the list in order and
	// `out` takes one kind per name. False if the registration refused.
	bool errorKind(const char *module, const char *const *variants, size_t count,
	               const LhatErrorKind **out) const;

	// A LOVE object type as a hostdata type. Call in both phases: the TYPES
	// phase declares the tag, the MEMBERS phase adds what every object
	// answers (dispose, type, typeOf). Returns false if either refused.
	bool objectType(const char *module, const char *name, love::Type &type) const;

	// 05 の 8.8改: the same, declared under a type registered earlier -- what
	// LOVE's own class tree says, rather than a union spelt out by hand at
	// every place the base would do. The MEMBERS phase adds nothing: dispose,
	// type and typeOf come from the base with everything else it registered.
	//
	// Declaring this is a promise that a pointer of `type` may be read as one
	// of the base's. It holds here for a reason of its own: a hostdata value
	// always carries a love::Object * (pushObject takes one), so there is only
	// ever the one pointer and no offset to get wrong.
	bool objectType(const char *module, const char *name, love::Type &type,
	                const char *baseModule, const char *baseName) const;
};

typedef bool (*Registrar)(Context &ctx);

// ---------------------------------------------------------------------------
// Program + machine
// ---------------------------------------------------------------------------

// Owns one LhatProgram and, once compiled, one LhatMachine.
class Runtime
{
public:

	Runtime(LhatProgramLoader loader, void *loaderContext);
	~Runtime();

	Runtime(const Runtime &) = delete;
	Runtime &operator=(const Runtime &) = delete;

	LhatProgram *program() const { return program_; }
	LhatMachine *machine() const { return machine_; }
	// Both live as long as the process, not as long as this Runtime: 05 の 8.7
	// makes a registration a declaration, and lhat interns what one declares,
	// so a restart's new program comes away with the very tags and error
	// kinds the last one got. See the note beside them in lh.cpp.
	static Errors &errors();
	static TypeRegistry &registry();
	ParkingLot *lot() const { return lot_.get(); }

	// Registers love.Error, then runs every registrar through both phases.
	// False if any refused.
	bool registerAll(const Registrar *registrars, size_t count);

	// Which registrar refused, for the report.
	const std::string &failedRegistrar() const { return failedRegistrar_; }

	// lhat_program_check; the unit is returned even when it failed so the
	// caller can decide what to do with the diagnostics.
	const LhatUnit *check(const char *path);

	// True when the program and every unit checked without a diagnostic.
	bool ok() const;

	// Every diagnostic the program and its units recorded, one per line.
	std::string diagnostics() const;
	// Compiler failures include the unit, source position and reason.
	// VM creation / installation failures have their own phase-specific text.
	std::string compileDiagnostics() const;

	// lhat_program_compile + machine creation + install. False on failure;
	// compile may be called again after later checks (incremental).
	bool compile();

	// A run result rendered the way a driver reports it: status, panic value,
	// line, and the traceback when frames are standing.
	std::string describe(const LhatRunResult &ran) const;

	// Makes the machine `program` was compiled for run on another OS thread:
	// a fresh machine with the program installed and a ParkingLot of its
	// own, or nullptr. The caller disposes it with disposeMachine.
	static LhatMachine *spawnMachine(LhatProgram *program);
	static void disposeMachine(LhatMachine *machine);

private:

	LhatProgram *program_;
	LhatMachine *machine_;
	StrongRef<ParkingLot> lot_;
	std::string failedRegistrar_;
	std::string compileError_;
};

// ---------------------------------------------------------------------------
// Parking
// ---------------------------------------------------------------------------

// The host holds no GC roots (05 の 8.8), so an L^ value the engine keeps
// across calls -- a Body's user data, a World's contact callback -- is parked
// in a table at L^.modules.love.registry under an integer slot. The lot owns
// the slots; a Parked object is one slot, given back when it dies. This is
// what common/Reference.h was for Lua.

// 05 の 8.12: one wrapper per object per machine, kept by the collector.
//
// A getter called every frame (world.getBodies, shape.getBody) handed back a
// fresh hostdata each time. Correct -- 8.8 compares a hostdata by tag and
// pointer, so two wrappers of one object are equal and hash alike -- but
// wasteful, so this engine kept a map of its own from love::Object to its
// wrapper.
//
// THAT MAP WAS UNSOUND, and the reason is worth keeping written down. The
// argument for it was: Lua needed __mode = "v" because a strong cache would
// have held every Proxy alive; here the map is C++'s own and is not a root
// (8.8's collector walks L^ and the frames), so a wrapper nothing else names
// is collected as before and the type's dispose^ takes the entry out. Every
// sentence of that is true and the conclusion is false, because lhat's
// collector is incremental: the program runs between the marking and the
// sweep, so a wrapper decided white at the end of the marking could still be
// handed out of the map and put somewhere live before the sweep freed it.
// It cost about half of testing/lh/physics -- a Body whose members arrived
// without their self, an instruction given the wrong type.
//
// What a weak table gives is not "do not root it" but that the COLLECTOR
// takes the entry out itself, at the end of the marking, and marks again
// whatever a get answers while a marking is running. lhat has that table
// now (lhat_machine_weak_cache_get / _put / _forget, vm.h), and pushObject
// is written on it: ask, make one only where the answer was nothing.
//
// The key is the love::Object's address, compared and never read through, so
// dispose^ takes the entry out by hand as well -- a wrapper disposed of by
// name leaves its object free to be freed, and the next allocation may be
// given the same address.

class ParkingLot : public love::Object
{
public:

	static love::Type type;

	ParkingLot();
	virtual ~ParkingLot();

	// Makes the registry table on `machine` and parks it at love.registry.
	// The lot is then what lotOf(machine) answers.
	bool attach(LhatMachine *machine);

	// Forgets the machine: slots given back after this are not written.
	void detach();

	// The lot attached to `machine`, making one if the machine has none.
	// A binding that parks values asks this rather than keeping a lot of
	// its own, since every worker has a machine and a lot apiece.
	//
	// Made on demand because std.thread starts a machine without lhatove
	// looking on: only lhat_machine_new and lhat_program_install run there,
	// and the first binding that parks something is where the engine gets
	// its turn. Call it from inside a host function, so the machine is the
	// calling thread's own and the table can be made on it. nullptr only
	// when the machine refused the table.
	static ParkingLot *lotOf(LhatMachine *machine);

	LhatMachine *machine() const { return machine_; }

	// A slot holding `value`, or 0 when the table refused. Runs the sweep
	// first; call only from a safe point (inside a host function).
	uint32 park(LhatValue value);

	// The value in `slot`; nil for 0 or when detached.
	LhatValue get(uint32 slot) const;

	// Gives the slot back at the next safe point. A wrapper's dispose may run
	// inside the collector, where the host must not write a table, so the
	// write waits for the next park() or sweep().
	void releaseLater(uint32 slot);

	// Writes nil into every slot given back since the last sweep.
	void sweep();

private:

	LhatMachine *machine_;
	LhatTable *table_;
	uint32 next_;
	std::vector<uint32> free_;
	std::vector<uint32> pending_;
};

// One parked value, as a love::Object so the engine can hold it the way it
// holds any other (StrongRef, userdata slots, listener fields).
class Parked : public love::Object
{
public:

	static love::Type type;

	Parked(ParkingLot *lot, LhatValue value);
	virtual ~Parked();

	// False when the lot refused the value.
	bool ok() const { return slot_ != 0; }

	LhatValue get() const;
	ParkingLot *lot() const { return lot_.get(); }

private:

	StrongRef<ParkingLot> lot_;
	uint32 slot_;
};

// The same rendering for any machine (a love.thread worker reports this way).
std::string describeRun(LhatMachine *machine, const LhatRunResult &ran);

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------

// Measure-then-fill helpers.
std::string valueText(LhatValue value);
bool makeString(LhatMachine *machine, const std::string &text, LhatValue *out);

// The string a value holds, or nullptr when it is not a string.
const char *stringOf(LhatValue value, size_t *length = nullptr);

// Enum values retain their declaration identity at the host boundary. The
// spelling is only used internally to consult LOVE's existing constant maps.
const char *enumName(LhatMachine *machine, LhatValue value, const char *module, const char *type);
std::string optEnum(LhatMachine *machine, const LhatValue *args, size_t count, size_t index,
                    const char *module, const char *type, const std::string &fallback);
LhatValue pushEnum(LhatMachine *machine, const char *module, const char *type, const char *name);

// Argument readers. A missing or mistyped argument answers the fallback; the
// checker has already refused a call whose declared arguments are wrong, so
// these only guard the variadic tail and the host's own mistakes.
double optNumber(const LhatValue *args, size_t count, size_t index, double fallback);
bool optBool(const LhatValue *args, size_t count, size_t index, bool fallback);
std::string optString(const LhatValue *args, size_t count, size_t index, const std::string &fallback);

// Table fields by name (a missing field answers nil / the fallback). The key
// string is made on `machine`; nothing here runs an instruction.
LhatValue field(LhatMachine *machine, LhatValue table, const char *name);
double fieldNumber(LhatMachine *machine, LhatValue table, const char *name, double fallback);
bool fieldBool(LhatMachine *machine, LhatValue table, const char *name, bool fallback);
std::string fieldString(LhatMachine *machine, LhatValue table, const char *name, const std::string &fallback);
bool fieldIs(LhatMachine *machine, LhatValue table, const char *name, LhatValueTag tag);

// A love::Variant (what events and channels carry) as a value on `machine`.
// Objects come back as fresh hostdata; tables are copied; light userdata is
// refused (nil); a Carried object is put back together here.
LhatValue pushVariant(LhatMachine *machine, const TypeRegistry &registry, const Variant &v);

// The reverse, for what a Channel takes: nil, bool, number and string as
// themselves, a wrapper as its LOVE object, and any other value (a table, a
// closure) as a Carried object -- lhat_carry's copy, which no machine owns.
// False with `why` when the value cannot cross (hostdata inside a table, a
// coroutine, an error value).
bool variantOf(LhatMachine *machine, const TypeRegistry &registry, LhatValue value, Variant &out, std::string &why);

// A carried L^ value as a love::Object, so a Variant can hold it across
// threads. Uncarry as many times as wanted; the copy is freed with this.
class Carried : public love::Object
{
public:

	static love::Type type;

	// Takes the copy over.
	explicit Carried(LhatCarried *carried);
	virtual ~Carried();

	LhatValue get(LhatMachine *machine) const;

private:

	LhatCarried *carried;
};

// ---------------------------------------------------------------------------
// Objects
// ---------------------------------------------------------------------------

// A fresh hostdata wrapper around `object` (retained; the wrapper's dispose
// releases it). 05 の 8.8: two wrappers of one object compare equal, so a
// fresh one per push is what the engine hands out. `type` is the most
// derived type the caller knows, as luax_pushtype took it; nil when neither
// it nor a type it derives from was registered.
LhatValue pushObject(LhatMachine *machine, const TypeRegistry &registry, love::Type &type, love::Object *object);

template <typename T>
LhatValue pushObject(LhatMachine *machine, const TypeRegistry &registry, T *object)
{
	return pushObject(machine, registry, T::type, object);
}

// The object a hostdata value wraps, as T, or nullptr when the value is not
// a wrapper of a T (derived types pass: love::Type::isa).
love::Object *checkObject(LhatValue value, const TypeRegistry &registry, love::Type &type);

template <typename T>
T *checkObject(LhatValue value, const TypeRegistry &registry)
{
	return (T *) checkObject(value, registry, T::type);
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

// An error value of `kind` carrying `message`; nil when the machine could
// not make one (out of memory), which the VM reports as a type error.
LhatValue fail(LhatMachine *machine, const LhatErrorKind *kind, const std::string &message);

// A programmer error the API does not declare (a bad enum, a module that is
// not loaded, a GL failure). Lua raised these with lua_error; here it is
// 05 の 8.7改2's lhat_machine_panic: the run ends as if the call site had
// written panic^, traceback standing. The answer is nil for the caller to
// return -- it is dropped.
LhatValue raise(LhatMachine *machine, const std::string &message);


// Runs `body`; a love::Exception thrown inside is raised as above.
//
// 05 の 8.7: a binding is handed the machine's room and writes its answers
// into it, so a body that has written them has nothing left to return.
void guard(LhatMachine *machine, const std::function<void()> &body);

// The same, and a love::Exception replaces whatever was written with one
// error value of `kind` -- which is why this one needs the room.
void catchexcept(LhatMachine *machine, const LhatErrorKind *kind,
                 const std::function<void()> &body, LhatValue *answers, int *answerCount);

// ---------------------------------------------------------------------------
// Calling back
// ---------------------------------------------------------------------------

// Calls table[name] if it holds a subroutine. Answers false when there was
// no such member; the run result is otherwise the callee's. A fault inside
// ends the run the host function was called from (vm.c's nested-fault rule).
bool callMember(LhatMachine *machine, LhatValue table, const char *name, const LhatValue *args, size_t count, LhatRunResult *out);

// A value kept alive in the machine's host root (lhat_machine_host_root):
// the table the machine roots and L^ has no name for -- what Lua calls its
// registry. The host holds no GC roots of its own, so anything it needs
// across calls is parked there, under `key`.
//
// It used to be somewhere in L^.modules, and twice that was wrong. First
// love.* (love.registry, love.boot.handlers): lhat d7dc455 made a registered
// module's table the program's, shared by every machine, and a host write
// went through into it -- a table of one machine's hung under a black table
// every machine shares, one the collector never looks at again, so what was
// parked there was collected while still named (physics lost every contact
// callback, or crashed). Then L^.modules.lhatove, a path no registration
// made: sound, but a place a program can reach by name. The host root is
// neither -- lhat b10f3fe made it for exactly this, and refuses a host write
// into a shared table outright now.
bool park(LhatMachine *machine, const char *key, LhatValue value);

} // lh
} // love

#endif // LOVE_LH_LH_H

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

#include "lh.h"

#include "stdlib/channel.h"
#include "stdlib/thread.h"

// 05 の 10.7: the signature table, for a build without the front end.
#ifdef LHATOVE_VM_ONLY
#include "Signatures.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace love
{
namespace lh
{

// ---------------------------------------------------------------------------
// TypeRegistry
// ---------------------------------------------------------------------------

const LhatHostDataTag *TypeRegistry::tagFor(love::Type &type) const
{
	auto it = tags.find(&type);
	if (it != tags.end())
		return it->second;

	// Not registered itself: the nearest registered ancestor. love::Type has
	// no parent walk, but isa answers the same question for each candidate;
	// "nearest" is the candidate that every other matching candidate isa.
	const LhatHostDataTag *best = nullptr;
	love::Type *bestType = nullptr;
	for (const auto &entry : tags)
	{
		if (!type.isa(*entry.first))
			continue;
		if (bestType == nullptr || entry.first->isa(*bestType))
		{
			best = entry.second;
			bestType = entry.first;
		}
	}
	return best;
}

love::Type *TypeRegistry::typeFor(const LhatHostDataTag *tag) const
{
	auto it = types.find(tag);
	return it != types.end() ? it->second : nullptr;
}

void TypeRegistry::add(love::Type &type, const LhatHostDataTag *tag)
{
	tags[&type] = tag;
	types[tag] = &type;
}

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------

// LHATOVE_SKIP_REGISTRATIONS="love.x.a,love.y.B.b,love.z.*": registrations left
// out, a development aid for bisecting a registration the runtime chokes
// on. An entry ending in "*" leaves out everything with that prefix.
static bool skipped(const std::string &what)
{
	static const char *list = getenv("LHATOVE_SKIP_REGISTRATIONS");
	if (list == nullptr)
		return false;
	std::string all = std::string(",") + list + ",";
	if (all.find("," + what + ",") != std::string::npos)
		return true;
	size_t from = 0;
	while (true)
	{
		size_t star = all.find("*,", from);
		if (star == std::string::npos)
			return false;
		size_t start = all.rfind(',', star) + 1;
		if (what.compare(0, star - start, all, start, star - start) == 0)
			return true;
		from = star + 1;
	}
}

static bool noteFailure(std::string *failed, const char *what, const char *signature)
{
	if (failed != nullptr)
		*failed = std::string(what) + " : " + signature;
	return false;
}

bool Context::func(const char *module, const char *name, const char *signature, LhatHostFn fn, void *ctx) const
{
	std::string what = std::string(module) + "." + name;
	if (skipped(what))
		return true;
	return lhat_register_func(program, module, name, signature, fn, ctx) || noteFailure(failed, what.c_str(), signature);
}

bool Context::member(const char *module, const char *type, const char *name, const char *signature, LhatHostFn fn, void *ctx) const
{
	std::string what = std::string(module) + "." + type + "." + name;
	if (skipped(what))
		return true;
	return lhat_register_member(program, module, type, name, signature, fn, ctx) || noteFailure(failed, what.c_str(), signature);
}

bool Context::global(const char *name, const char *signature, LhatHostFn fn, void *ctx) const
{
	return lhat_register_global(program, name, signature, fn, ctx);
}

bool Context::bind(const char *name, const char *member) const
{
	return lhat_bind_initial(program, name, member);
}

bool Context::errorKind(const char *module, const char *const *variants, size_t count,
                        const LhatErrorKind **out) const
{
	// One declaration named "Error" per module, so a signature may write the
	// module's name and take whichever variant it has.
	return lhat_register_error_kind(program, module, "Error", variants, count, nullptr, out);
}

// What every object answers. The context handed to these is the TypeRegistry,
// since the wrapper's own tag is what says which love::Type it is.

// 05 の 8.8: registering `dispose` hands the wrapper's lifetime to L^ -- the
// collector calls it once, and so may the program. The release must not
// reach back into the lhat API (the sweep may be the caller); Object::release
// does not.
// 05 の 8.8改2: the holds a shared type takes and gives. love::Object counts
// them atomically, so the pair is what carry needs to move a pointer between
// machines -- one hold for the carried tree, one for every wrapper rebuilt
// from it, each given back through the dispose^ above.
//
// Called with no machine at hand and on whichever thread is carrying, so
// nothing here reaches into lhat.
static void lh_object_retain(void *pointer, void *context)
{
	(void) context;
	((love::Object *) pointer)->retain();
}

static void lh_object_let_go(void *pointer, void *context)
{
	(void) context;
	((love::Object *) pointer)->release();
}

static void lh_object_dispose(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count < 1 || !lhat_is_object_kind(arguments[0], LHAT_OBJECT_HOSTDATA))
		return;
	LhatHostData *data = (LhatHostData *) lhat_as_object(arguments[0]);
	if (data->pointer != nullptr)
	{
		// 05 の 8.12: the key is an address, and this object is about to be
		// free to be freed -- so the entry goes before the next allocation
		// can be handed the same one. The collector takes out what it found
		// unreachable; this is the other way a wrapper ends, by name.
		lhat_machine_weak_cache_forget(machine, data->pointer);
		((love::Object *) data->pointer)->release();
		data->pointer = nullptr;
	}
}

// obj.type() -> the registered name of the wrapper's own type.
static void lh_object_type(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count < 1 || !lhat_is_object_kind(arguments[0], LHAT_OBJECT_HOSTDATA))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	const LhatHostData *data = (const LhatHostData *) lhat_as_object(arguments[0]);
	LhatValue out = lhat_nil();
	makeString(machine, data->tag->name, &out);
	answers[0] = out;
	*answerCount = 1;
}

// obj.typeOf(name) -> whether the object is (or derives from) the named type.
static void lh_object_typeOf(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) machine;
	const TypeRegistry *registry = (const TypeRegistry *) context;
	if (count < 2 || !lhat_is_object_kind(arguments[0], LHAT_OBJECT_HOSTDATA))
	{
		answers[0] = lhat_bool(false);
		*answerCount = 1;
		return;
	}
	const char *name = stringOf(arguments[1]);
	if (name == nullptr)
	{
		answers[0] = lhat_bool(false);
		*answerCount = 1;
		return;
	}
	const LhatHostData *data = (const LhatHostData *) lhat_as_object(arguments[0]);
	love::Type *own = registry->typeFor(data->tag);
	love::Type *asked = love::Type::byName(name);
	if (own == nullptr || asked == nullptr)
	{
		answers[0] = lhat_bool(false);
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_bool(own->isa(*asked));
	*answerCount = 1;
}

bool Context::objectType(const char *module, const char *name, love::Type &type) const
{
	if (types())
	{
		type.init();
		const LhatHostDataTag *tag = lhat_register_hostdata_type(program, module, name);
		if (tag == nullptr)
			return false;
		registry->add(type, tag);
		// 05 の 8.8改2: every LOVE object may cross to another machine, as
		// every one could cross a Lua Channel. What is not safe is USING
		// some of them off the thread that made them (a GPU object), and
		// that was the caller's to know there too.
		return lhat_register_hostdata_shared(program, module, name,
		                                     lh_object_retain, lh_object_let_go, nullptr);
	}
	return member(module, name, "dispose", "p^self^;", lh_object_dispose, registry)
		&& member(module, name, "type", "f^self^ -> string^;", lh_object_type, registry)
		&& member(module, name, "typeOf", "f^self^, string^ -> bool^;", lh_object_typeOf, registry);
}

bool Context::objectType(const char *module, const char *name, love::Type &type,
                         const char *baseModule, const char *baseName) const
{
	if (types())
	{
		type.init();
		const LhatHostDataTag *tag =
			lhat_register_hostdata_subtype(program, module, name, baseModule, baseName);
		if (tag == nullptr)
			return false;
		registry->add(type, tag);
		// 05 の 8.8改2: every LOVE object may cross to another machine, as
		// every one could cross a Lua Channel. What is not safe is USING
		// some of them off the thread that made them (a GPU object), and
		// that was the caller's to know there too.
		return lhat_register_hostdata_shared(program, module, name,
		                                     lh_object_retain, lh_object_let_go, nullptr);
	}
	// 8.8改: what the base registered is taken when registration closes, so
	// dispose, type and typeOf are already here. Registering dispose again
	// would only replace what already works -- and 02 の 12.5 reads a
	// lifetime off the member, so the two halves are better left together.
	return true;
}

// ---------------------------------------------------------------------------
// Runtime
// ---------------------------------------------------------------------------

// 05 の 5.8: the one lock over the program's writes. It used to be taken by
// hand at each call; the program takes it itself now (lhat_program_set_lock
// in the constructor), which reaches the writes inside lhat as well -- the
// install a std.thread worker makes for itself, above all. It must not be
// taken here as well: lhat says the pair never nests, and this one does not.
static std::mutex &programMutex()
{
	static std::mutex mutex;
	return mutex;
}

static void lockProgram(void *context)
{
	(void) context;
	programMutex().lock();
}

static void unlockProgram(void *context)
{
	(void) context;
	programMutex().unlock();
}

Runtime::Runtime(LhatProgramLoader loader, void *loaderContext)
	: program_(nullptr)
	, machine_(nullptr)
{
	// 03 の 3.1: a file defaults to strict.
	program_ = lhat_program_new(true, loader, loaderContext);
	lhat_program_set_lock(program_, lockProgram, unlockProgram, nullptr);
	lot_.set(new ParkingLot(), Acquire::NORETAIN);
}

// 05 の 8.7: a registration IS a declaration, and lhat keeps one identity per
// declaration for the whole process (its src/registry.c) -- register the same
// hostdata type or error kind into a second program and the same tag comes
// back. So what pairs those tags with love::Type, and what holds love.Error's
// three kinds, has no reason to be built per program either: a restart makes
// a new Runtime, and these two survive it unchanged. love_lh_shutdown gives
// the lhat side back once the last program is gone.
static Errors &sharedErrors()
{
	static Errors errors;
	return errors;
}

static TypeRegistry &sharedRegistry()
{
	static TypeRegistry registry;
	return registry;
}

Errors &Runtime::errors()
{
	return sharedErrors();
}

TypeRegistry &Runtime::registry()
{
	return sharedRegistry();
}

Runtime::~Runtime()
{
	// Workers first: a body still running is reading a proto this program
	// owns, and a named channel may be holding a closure of one. Both have
	// to be given up before anything below is torn down.
	if (program_ != nullptr)
		lhatstdlib_thread_join_all(program_);
	lhatstdlib_channel_forget_named();
	// The lot next, so a Parked value released by a wrapper's dispose below
	// does not write into the heap being torn down.
	lot_->detach();
	// The machine next: its heap holds values whose release callbacks reach
	// into the engine, and the program's registrations must still exist then.
	if (machine_ != nullptr)
	{
		lhat_machine_dispose(machine_);
	}
	if (program_ != nullptr)
		lhat_program_free(program_);
}

bool Runtime::registerAll(const Registrar *registrars, size_t count)
{
	if (program_ == nullptr)
		return false;

	// 05 の 10.7: without the front end there is nothing to read a
	// registration's signature text with, so each is looked up in a table a
	// full build of this same engine wrote (scripts/bin2header.ps1 turns
	// --dump-signatures' output into Signatures.h). Read before the first
	// registration; a build with the front end reads the text and consults
	// no table.
#ifdef LHATOVE_VM_ONLY
	if (!lhat_program_read_signatures(program_, lh_signatures, lh_signatures_length))
	{
		failedRegistrar_ = "the signature table this build carries does not fit its registrations";
		return false;
	}
#endif

	// 04 の 2.4: each module declares the failures of its own, in its own
	// registrar. Nothing central is left to declare here.
	Errors &shared = errors();

	Context ctx;
	ctx.program = program_;
	ctx.errors = &shared;
	ctx.registry = &registry();
	ctx.lot = lot_.get();
	std::string failure;
	ctx.failed = &failure;

	for (Phase phase : {Phase::TYPES, Phase::MEMBERS})
	{
		ctx.phase = phase;
		for (size_t i = 0; i < count; i++)
		{
			if (!registrars[i](ctx))
			{
				failedRegistrar_ = "registrar #" + std::to_string(i) + " in the " + (phase == Phase::TYPES ? "TYPES" : "MEMBERS") + " phase" + (failure.empty() ? "" : " (" + failure + ")");
				return false;
			}
		}
	}
	return true;
}

const LhatUnit *Runtime::check(const char *path)
{
	if (program_ == nullptr)
		return nullptr;
	return lhat_program_check(program_, path);
}

bool Runtime::ok() const
{
	if (program_ == nullptr || lhat_program_has_errors(program_))
		return false;
	for (const LhatUnit *unit = lhat_program_units(program_); unit != nullptr; unit = lhat_unit_next(unit))
	{
		if (!lhat_unit_ok(unit))
			return false;
	}
	return true;
}

std::string Runtime::diagnostics() const
{
	std::string out;
	if (program_ == nullptr)
		return "lhat: could not create a program\n";

	// 05 の 6.2: the program's own diagnostics are about a unit rather than
	// a place inside one (unreadable, a cycle), so they have no position.
	size_t own = lhat_program_diagnostic_count(program_);
	for (size_t i = 0; i < own; i++)
	{
		const LhatProgramDiagnostic *d = lhat_program_diagnostic(program_, i);
		out += d->path;
		out += ": error: ";
		out += lhat_program_error_message(d->code);
		out += "\n";
	}

	std::vector<char> room(1024);
	for (const LhatUnit *unit = lhat_program_units(program_); unit != nullptr; unit = lhat_unit_next(unit))
	{
		size_t count = lhat_unit_diagnostic_count(unit);
		for (size_t i = 0; i < count; i++)
		{
			size_t needed = lhat_unit_diagnostic_write(unit, i, true, room.data(), room.size());
			if (needed >= room.size())
			{
				room.resize(needed + 1);
				lhat_unit_diagnostic_write(unit, i, true, room.data(), room.size());
			}
			out += room.data();
			out += "\n";
		}
	}
	return out;
}

bool Runtime::compile()
{
	if (program_ == nullptr || !lhat_program_compile(program_))
		return false;

	if (machine_ == nullptr)
	{
		machine_ = lhat_machine_new();
		if (machine_ == nullptr)
			return false;
	}

	// Puts what was registered into L^.modules so an import^ finds it.
	if (!lhat_program_install(program_, machine_))
		return false;
	return lot_->attach(machine_);
}

// ---------------------------------------------------------------------------
// Parking
// ---------------------------------------------------------------------------

love::Type ParkingLot::type("lh.ParkingLot", &Object::type);
love::Type Parked::type("lh.Parked", &Object::type);

// Which lot is attached to which machine. A lot is retained here from
// attach to detach, so a machine spawned for a love.thread worker keeps its
// lot alive for as long as it runs.
// ---------------------------------------------------------------------------
// One wrapper per object per machine
// ---------------------------------------------------------------------------

// Held the way the lots are, and for the same reason: a machine apiece.
static std::mutex &lotsMutex()
{
	static std::mutex mutex;
	return mutex;
}

static std::map<LhatMachine *, StrongRef<ParkingLot>> &lots()
{
	static std::map<LhatMachine *, StrongRef<ParkingLot>> held;
	return held;
}

ParkingLot::ParkingLot()
	: machine_(nullptr)
	, table_(nullptr)
	, next_(1)
{
}

ParkingLot::~ParkingLot()
{
}

bool ParkingLot::attach(LhatMachine *machine)
{
	LhatValue table;
	if (!lhat_machine_make_table(machine, &table))
		return false;
	// Rooted in the host root, where L^ cannot name it: see park in lh.h.
	// Qualified: the lot's own park(LhatValue) would hide it here.
	if (!lh::park(machine, "registry", table))
		return false;
	machine_ = machine;
	table_ = (LhatTable *) lhat_as_object(table);
	{
		std::lock_guard<std::mutex> hold(lotsMutex());
		lots()[machine].set(this);
	}
	return true;
}

void ParkingLot::detach()
{
	if (machine_ != nullptr)
	{
		std::lock_guard<std::mutex> hold(lotsMutex());
		lots().erase(machine_);
	}
	machine_ = nullptr;
	table_ = nullptr;
	pending_.clear();
}

ParkingLot *ParkingLot::lotOf(LhatMachine *machine)
{
	{
		std::lock_guard<std::mutex> hold(lotsMutex());
		auto it = lots().find(machine);
		if (it != lots().end())
			return it->second.get();
	}
	// A machine nobody here attached one to -- std.thread's worker. Attach
	// takes the lock itself, and only this thread can be asking about its
	// own machine, so there is no second maker to race with.
	StrongRef<ParkingLot> lot(new ParkingLot(), Acquire::NORETAIN);
	if (!lot->attach(machine))
		return nullptr;
	return lot.get();
}

uint32 ParkingLot::park(LhatValue value)
{
	if (machine_ == nullptr || lhat_is_nil(value))
		return 0;
	sweep();
	uint32 slot;
	if (!free_.empty())
	{
		slot = free_.back();
		free_.pop_back();
	}
	else
		slot = next_++;
	bool refused = false;
	if (!lhat_machine_table_set(machine_, table_, lhat_integer((int64_t) slot), value, &refused) || refused)
	{
		free_.push_back(slot);
		return 0;
	}
	return slot;
}

LhatValue ParkingLot::get(uint32 slot) const
{
	if (machine_ == nullptr || slot == 0)
		return lhat_nil();
	return lhat_table_get(table_, lhat_integer((int64_t) slot));
}

void ParkingLot::releaseLater(uint32 slot)
{
	if (machine_ == nullptr || slot == 0)
		return;
	pending_.push_back(slot);
}

void ParkingLot::sweep()
{
	if (machine_ == nullptr)
	{
		pending_.clear();
		return;
	}
	for (uint32 slot : pending_)
	{
		bool refused = false;
		lhat_machine_table_set(machine_, table_, lhat_integer((int64_t) slot), lhat_nil(), &refused);
		free_.push_back(slot);
	}
	pending_.clear();
}

Parked::Parked(ParkingLot *lot, LhatValue value)
	: lot_(lot)
	, slot_(0)
{
	if (lot != nullptr)
		slot_ = lot->park(value);
}

Parked::~Parked()
{
	if (lot_.get() != nullptr)
		lot_->releaseLater(slot_);
}

LhatValue Parked::get() const
{
	if (lot_.get() == nullptr)
		return lhat_nil();
	return lot_->get(slot_);
}

std::string Runtime::describe(const LhatRunResult &ran) const
{
	return describeRun(machine_, ran);
}

std::string describeRun(LhatMachine *machine, const LhatRunResult &ran)
{
	std::string text = lhat_run_status_message(ran.status);
	if (ran.status == LHAT_RUN_PANIC)
		text += ": " + valueText(ran.value);
	if (ran.line > 0)
		text = "line " + std::to_string(ran.line) + ": " + text;

	// 04 の 11.6改: the frames still standing, readable until the machine is
	// run again or disposed.
	if (machine != nullptr && lhat_machine_fault_depth(machine) >= 2)
	{
		size_t needed = lhat_machine_traceback(machine, nullptr, 0);
		std::vector<char> spelt(needed + 1);
		lhat_machine_traceback(machine, spelt.data(), spelt.size());
		text += "\n";
		text += spelt.data();
	}
	return text;
}

LhatMachine *Runtime::spawnMachine(LhatProgram *program)
{
	if (program == nullptr)
		return nullptr;
	LhatMachine *machine = lhat_machine_new();
	if (machine == nullptr)
		return nullptr;
	// 05 の 8.7: a registration becomes an object on the heap of the machine
	// it is installed on, so every machine installs for itself. The lot is
	// retained by the lots table until detach.
	StrongRef<ParkingLot> lot(new ParkingLot(), Acquire::NORETAIN);
	if (!lhat_program_install(program, machine) || !lot->attach(machine))
	{
		lhat_machine_dispose(machine);
		return nullptr;
	}
	return machine;
}

void Runtime::disposeMachine(LhatMachine *machine)
{
	if (machine == nullptr)
		return;
	ParkingLot *lot = ParkingLot::lotOf(machine);
	if (lot != nullptr)
		lot->detach();
	lhat_machine_dispose(machine);
}

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------

std::string valueText(LhatValue value)
{
	size_t needed = lhat_value_text(value, nullptr, 0);
	std::vector<char> text(needed + 1);
	lhat_value_text(value, text.data(), text.size());
	return std::string(text.data(), needed);
}

bool makeString(LhatMachine *machine, const std::string &text, LhatValue *out)
{
	return lhat_machine_make_string(machine, text.data(), text.size(), out);
}

const char *stringOf(LhatValue value, size_t *length)
{
	if (!lhat_is_object_kind(value, LHAT_OBJECT_STRING))
		return nullptr;
	const LhatString *s = (const LhatString *) lhat_as_object(value);
	if (length != nullptr)
		*length = s->length;
	return s->text;
}

double optNumber(const LhatValue *args, size_t count, size_t index, double fallback)
{
	if (index >= count || !lhat_is_number(args[index]))
		return fallback;
	return lhat_number_as_real(args[index]);
}

bool optBool(const LhatValue *args, size_t count, size_t index, bool fallback)
{
	if (index >= count || !lhat_is_bool(args[index]))
		return fallback;
	return lhat_as_bool(args[index]);
}

std::string optString(const LhatValue *args, size_t count, size_t index, const std::string &fallback)
{
	if (index >= count)
		return fallback;
	size_t length = 0;
	const char *text = stringOf(args[index], &length);
	return text != nullptr ? std::string(text, length) : fallback;
}

LhatValue field(LhatMachine *machine, LhatValue table, const char *name)
{
	if (!lhat_is_object_kind(table, LHAT_OBJECT_TABLE))
		return lhat_nil();
	LhatValue key = lhat_nil();
	if (!makeString(machine, name, &key))
		return lhat_nil();
	return lhat_table_get((const LhatTable *) lhat_as_object(table), key);
}

double fieldNumber(LhatMachine *machine, LhatValue table, const char *name, double fallback)
{
	LhatValue v = field(machine, table, name);
	return lhat_is_number(v) ? lhat_number_as_real(v) : fallback;
}

bool fieldBool(LhatMachine *machine, LhatValue table, const char *name, bool fallback)
{
	LhatValue v = field(machine, table, name);
	return lhat_is_bool(v) ? lhat_as_bool(v) : fallback;
}

std::string fieldString(LhatMachine *machine, LhatValue table, const char *name, const std::string &fallback)
{
	size_t length = 0;
	const char *text = stringOf(field(machine, table, name), &length);
	return text != nullptr ? std::string(text, length) : fallback;
}

bool fieldIs(LhatMachine *machine, LhatValue table, const char *name, LhatValueTag tag)
{
	return field(machine, table, name).tag == tag;
}

LhatValue pushVariant(LhatMachine *machine, const TypeRegistry &registry, const Variant &v)
{
	const Variant::Data &data = v.getData();
	LhatValue out = lhat_nil();

	switch (v.getType())
	{
	case Variant::BOOLEAN:
		return lhat_bool(data.boolean);
	case Variant::NUMBER:
		return lhat_real(data.number);
	case Variant::STRING:
		makeString(machine, std::string(data.string->str, data.string->len), &out);
		return out;
	case Variant::SMALLSTRING:
		makeString(machine, std::string(data.smallstring.str, data.smallstring.len), &out);
		return out;
	case Variant::LOVEOBJECT:
		if (data.objectproxy.type == nullptr || data.objectproxy.object == nullptr)
			return lhat_nil();
		if (data.objectproxy.type->isa(Carried::type))
			return ((const Carried *) data.objectproxy.object)->get(machine);
		return pushObject(machine, registry, *data.objectproxy.type, data.objectproxy.object);
	case Variant::TABLE:
	{
		if (!lhat_machine_make_table(machine, &out))
			return lhat_nil();
		LhatTable *table = (LhatTable *) lhat_as_object(out);
		for (const auto &pair : data.table->pairs)
		{
			LhatValue key = pushVariant(machine, registry, pair.first);
			LhatValue value = pushVariant(machine, registry, pair.second);
			bool refused = false;
			if (lhat_is_nil(key))
				continue;
			lhat_table_set(table, key, value, &refused);
		}
		return out;
	}
	case Variant::LUSERDATA:
		// Touch ids travel as light userdata (Event.cpp: a double would lose
		// bits); here they are the integer they were made from.
		return lhat_integer((int64_t) (intptr_t) data.userdata);
	case Variant::NIL:
	case Variant::UNKNOWN:
	default:
		return lhat_nil();
	}
}

// ---------------------------------------------------------------------------
// Objects
// ---------------------------------------------------------------------------

LhatValue pushObject(LhatMachine *machine, const TypeRegistry &registry, love::Type &type, love::Object *object)
{
	if (object == nullptr)
		return lhat_nil();
	// 05 の 8.12: the one this machine already has, if the marking has not
	// decided against it. A get during a marking marks what it answers, so
	// what comes back here is alive.
	LhatValue known = lhat_machine_weak_cache_get(machine, object);
	if (!lhat_is_nil(known))
		return known;
	const LhatHostDataTag *tag = registry.tagFor(type);
	if (tag == nullptr)
		return lhat_nil();
	LhatValue out = lhat_nil();
	if (!lhat_machine_make_hostdata(machine, tag, object, &out))
		return lhat_nil();
	object->retain();
	// A table that could not grow is a miss next time, and nothing worse.
	lhat_machine_weak_cache_put(machine, object, out);
	return out;
}

love::Object *checkObject(LhatValue value, const TypeRegistry &registry, love::Type &type)
{
	if (!lhat_is_object_kind(value, LHAT_OBJECT_HOSTDATA))
		return nullptr;
	const LhatHostData *data = (const LhatHostData *) lhat_as_object(value);
	if (data->released || data->pointer == nullptr)
		return nullptr;
	love::Type *own = registry.typeFor(data->tag);
	if (own == nullptr || !own->isa(type))
		return nullptr;
	return (love::Object *) data->pointer;
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

LhatValue fail(LhatMachine *machine, const LhatErrorKind *kind, const std::string &message)
{
	LhatValue error = lhat_nil();
	if (!lhat_machine_make_error(machine, kind, message.c_str(), lhat_nil(), &error))
		return lhat_nil();
	return error;
}

LhatValue raise(LhatMachine *machine, const std::string &message)
{
	if (!lhat_machine_panic_text(machine, message.c_str()))
	{
		// No room for the message: say it here, then panic with nil.
		fprintf(stderr, "lhatove: %s\n", message.c_str());
		fflush(stderr);
		lhat_machine_panic(machine, lhat_nil());
	}
	return lhat_nil();
}

void guard(LhatMachine *machine, const std::function<void()> &body)
{
	try
	{
		body();
	}
	catch (const love::Exception &e)
	{
		raise(machine, e.what());
	}
}

void catchexcept(LhatMachine *machine, const LhatErrorKind *kind,
                 const std::function<void()> &body, LhatValue *answers, int *answerCount)
{
	try
	{
		body();
	}
	catch (const love::Exception &e)
	{
		// Whatever the body had written is replaced: an error is one answer.
		answers[0] = fail(machine, kind, e.what());
		*answerCount = 1;
	}
}

// ---------------------------------------------------------------------------
// Carrying
// ---------------------------------------------------------------------------

love::Type Carried::type("lh.Carried", &Object::type);

Carried::Carried(LhatCarried *carried)
	: carried(carried)
{
}

Carried::~Carried()
{
	if (carried != nullptr)
		lhat_carried_free(carried);
}

LhatValue Carried::get(LhatMachine *machine) const
{
	LhatValue out = lhat_nil();
	if (carried == nullptr || !lhat_uncarry(machine, carried, &out))
		return lhat_nil();
	return out;
}

bool variantOf(LhatMachine *machine, const TypeRegistry &registry, LhatValue value, Variant &out, std::string &why)
{
	(void) machine;
	if (lhat_is_nil(value))
	{
		out = Variant();
		return true;
	}
	if (lhat_is_bool(value))
	{
		out = Variant(lhat_as_bool(value));
		return true;
	}
	if (lhat_is_number(value))
	{
		out = Variant(lhat_is_integer(value) ? (double) lhat_as_integer(value) : lhat_as_real(value));
		return true;
	}
	size_t length = 0;
	const char *text = stringOf(value, &length);
	if (text != nullptr)
	{
		out = Variant(text, length);
		return true;
	}
	if (lhat_is_object_kind(value, LHAT_OBJECT_HOSTDATA))
	{
		const LhatHostData *data = (const LhatHostData *) lhat_as_object(value);
		love::Type *type = registry.typeFor(data->tag);
		if (type == nullptr || data->pointer == nullptr || data->released)
		{
			why = "the object is not one LOVE can carry";
			return false;
		}
		out = Variant(type, (love::Object *) data->pointer);
		return true;
	}
	LhatCarried *carried = nullptr;
	const char *refused = nullptr;
	if (!lhat_carry(value, &carried, &refused))
	{
		why = refused != nullptr ? refused : "out of memory";
		return false;
	}
	StrongRef<Carried> holder(new Carried(carried), Acquire::NORETAIN);
	out = Variant(&Carried::type, holder.get());
	return true;
}

// ---------------------------------------------------------------------------
// Calling back
// ---------------------------------------------------------------------------

bool callMember(LhatMachine *machine, LhatValue table, const char *name, const LhatValue *args, size_t count, LhatRunResult *out)
{
	if (!lhat_is_object_kind(table, LHAT_OBJECT_TABLE))
		return false;
	LhatValue key = lhat_nil();
	if (!makeString(machine, name, &key))
		return false;
	LhatValue member = lhat_table_get((const LhatTable *) lhat_as_object(table), key);
	if (!lhat_is_object_kind(member, LHAT_OBJECT_SUBROUTINE) && !lhat_is_object_kind(member, LHAT_OBJECT_HOST))
		return false;
	*out = lhat_machine_call(machine, member, args, count);
	return true;
}

bool park(LhatMachine *machine, const char *key, LhatValue value)
{
	LhatValue name = lhat_nil();
	if (!makeString(machine, key, &name))
		return false;
	bool refused = false;
	return lhat_machine_table_set(machine, lhat_machine_host_root(machine), name, value, &refused)
		&& !refused;
}

} // lh
} // love

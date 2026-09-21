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

// love.physics.World for L^. The reference is wrap_World.cpp beside this
// file. Contact callbacks and the contact filter are L^ procedures parked
// for the world's lifetime (lh::Parked); queries and ray casts take theirs
// as arguments and call them before returning.

#include "lh_Physics.h"

namespace love
{
namespace physics
{
namespace box2d
{

// ---------------------------------------------------------------------------
// Listeners over parked procedures
// ---------------------------------------------------------------------------

// begin/end/presolve: fn(shapeA, shapeB, contact); postsolve adds the
// normal and tangent impulses of the (at most two) manifold points.
class ContactCallback : public World::ContactListener
{
public:

	// The world is used by the machine that set the callback, which is the
	// one the listener calls back into.
	ContactCallback(LhatMachine *machine, LhatValue fn, bool postsolve)
		: parked(new lh::Parked(lh::ParkingLot::lotOf(machine), fn), Acquire::NORETAIN)
		, machine(machine)
		, postsolve(postsolve)
	{
	}

	void onContact(Shape *a, Shape *b, Contact *contact, const std::vector<float> &impulses) override
	{
		LhatValue args[7];
		args[0] = pushShape(machine, a);
		args[1] = pushShape(machine, b);
		args[2] = pushContact(machine, contact);
		size_t count = 3;
		if (postsolve)
		{
			for (size_t i = 0; i < 4; i++)
				args[3 + i] = lhat_real(i < impulses.size() ? impulses[i] : 0.0f);
			count = 7;
		}
		callParked(machine, parked.get(), args, count, nullptr);
	}

	LhatValue value() const { return parked->get(); }

private:

	StrongRef<lh::Parked> parked;
	LhatMachine *machine;
	bool postsolve;
};

// fn(shapeA, shapeB) -> bool^; anything but an explicit false lets the pair collide.
class ContactFilter : public World::ContactFilterListener
{
public:

	ContactFilter(LhatMachine *machine, LhatValue fn)
		: parked(new lh::Parked(lh::ParkingLot::lotOf(machine), fn), Acquire::NORETAIN)
		, machine(machine)
	{
	}

	bool shouldCollide(Shape *a, Shape *b) override
	{
		LhatValue args[2] = {pushShape(machine, a), pushShape(machine, b)};
		LhatValue answer = lhat_nil();
		if (!callParked(machine, parked.get(), args, 2, &answer))
			return true;
		return !(lhat_is_bool(answer) && !lhat_as_bool(answer));
	}

	LhatValue value() const { return parked->get(); }

private:

	StrongRef<lh::Parked> parked;
	LhatMachine *machine;
};

// Query and ray cast procedures are arguments of the call in progress, so
// they are rooted by the frame and need no parking.
class QueryVisitor : public World::ShapeVisitor
{
public:

	QueryVisitor(LhatMachine *machine, LhatValue fn)
		: machine(machine)
		, fn(fn)
		, faulted(false)
	{
	}

	bool onShape(Shape *shape) override
	{
		LhatValue arg = pushShape(machine, shape);
		LhatRunResult ran = lhat_machine_call(machine, fn, &arg, 1);
		if (ran.status != LHAT_RUN_OK)
		{
			faulted = true;
			return false;
		}
		return !(lhat_is_bool(ran.value) && !lhat_as_bool(ran.value));
	}

	bool faulted;

private:

	LhatMachine *machine;
	LhatValue fn;
};

class RayVisitor : public World::RayCastVisitor
{
public:

	RayVisitor(LhatMachine *machine, LhatValue fn)
		: machine(machine)
		, fn(fn)
		, faulted(false)
	{
	}

	float onHit(Shape *shape, float x, float y, float nx, float ny, float fraction) override
	{
		LhatValue args[6] = {pushShape(machine, shape), lhat_real(x), lhat_real(y), lhat_real(nx), lhat_real(ny), lhat_real(fraction)};
		LhatRunResult ran = lhat_machine_call(machine, fn, args, 6);
		if (ran.status != LHAT_RUN_OK)
		{
			faulted = true;
			return 0.0f; // stop the cast
		}
		return (float) lh::optNumber(&ran.value, 1, 0, 0.0);
	}

	bool faulted;

private:

	LhatMachine *machine;
	LhatValue fn;
};

// ---------------------------------------------------------------------------
// Members
// ---------------------------------------------------------------------------

static World *self(LhatMachine *machine, const LhatValue *args, size_t count)
{
	World *world = checkWorld(machine, args, count, 0);
	if (world != nullptr && !world->isValid())
	{
		lh::raise(machine, "Attempt to use destroyed world.");
		return nullptr;
	}
	return world;
}

#define WORLD_SELF() World *w = self(machine, args, count); if (w == nullptr) return

// update(dt) / update(dt, velocityIterations, positionIterations)
static void lh_World_update(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	float dt = numberAt(args, count, 1);
	lh::guard(machine, [&]() {
		if (count >= 4)
			w->update(dt, (int) numberAt(args, count, 2, 8), (int) numberAt(args, count, 3, 3));
		else
			w->update(dt);
	});
}

// setCallbacks([begin[, end[, presolve[, postsolve]]]]): what is not given
// is cleared, so setCallbacks() alone removes them all.
static void lh_World_setCallbacks(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	const World::CallbackKind kinds[] = {World::CALLBACK_BEGIN, World::CALLBACK_END, World::CALLBACK_PRESOLVE, World::CALLBACK_POSTSOLVE};
	for (size_t i = 0; i < 4; i++)
	{
		size_t index = i + 1;
		if (index < count && lhat_is_object_kind(args[index], LHAT_OBJECT_SUBROUTINE))
		{
			StrongRef<ContactCallback> listener(new ContactCallback(machine, args[index], kinds[i] == World::CALLBACK_POSTSOLVE), Acquire::NORETAIN);
			w->setCallback(kinds[i], listener.get());
		}
		else
			w->setCallback(kinds[i], nullptr);
	}
}

static LhatValue callbackValue(World *w, World::CallbackKind kind)
{
	World::ContactListener *listener = w->getCallback(kind);
	ContactCallback *own = dynamic_cast<ContactCallback *>(listener);
	return own != nullptr ? own->value() : lhat_nil();
}

// getCallbacks() -> (begin, end, presolve, postsolve), nil^ where none is set.
static void lh_World_getCallbacks(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	answers[0] = callbackValue(w, World::CALLBACK_BEGIN);
	answers[1] = callbackValue(w, World::CALLBACK_END);
	answers[2] = callbackValue(w, World::CALLBACK_PRESOLVE);
	answers[3] = callbackValue(w, World::CALLBACK_POSTSOLVE);
	*answerCount = 4;
}

// setContactFilter(fn) / setContactFilter() to clear.
static void lh_World_setContactFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	if (count >= 2 && lhat_is_object_kind(args[1], LHAT_OBJECT_SUBROUTINE))
	{
		StrongRef<ContactFilter> filter(new ContactFilter(machine, args[1]), Acquire::NORETAIN);
		w->setContactFilter(filter.get());
	}
	else
		w->setContactFilter(nullptr);
}

static void lh_World_getContactFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	ContactFilter *own = dynamic_cast<ContactFilter *>(w->getContactFilter());
	answers[0] = own != nullptr ? own->value() : lhat_nil();
	*answerCount = 1;
}

static void lh_World_setGravity(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	w->setGravity(numberAt(args, count, 1), numberAt(args, count, 2));
}

static void lh_World_getGravity(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	b2Vec2 g = w->getGravity();
	float out[2] = {g.x, g.y};
	numbers(out, 2, answers, answerCount);
}

static void lh_World_translateOrigin(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() {
		w->translateOrigin(numberAt(args, count, 1), numberAt(args, count, 2));
	});
}

static void lh_World_setSleepingAllowed(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	w->setSleepingAllowed(boolAt(args, count, 1, true));
}

static void lh_World_isSleepingAllowed(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	answers[0] = lhat_bool(w->isSleepingAllowed());
	*answerCount = 1;
}

static void lh_World_isLocked(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	answers[0] = lhat_bool(w->isLocked());
	*answerCount = 1;
}

static void lh_World_getBodyCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	answers[0] = lhat_integer(w->getBodyCount());
	*answerCount = 1;
}

static void lh_World_getJointCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	answers[0] = lhat_integer(w->getJointCount());
	*answerCount = 1;
}

static void lh_World_getContactCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	answers[0] = lhat_integer(w->getContactCount());
	*answerCount = 1;
}

static void lh_World_getBodies(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() { answers[0] = objectList(machine, w->getBodies()); *answerCount = 1; });
}

static void lh_World_getJoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() {
		std::vector<Joint *> joints = w->getJoints();
		LhatValue table = lhat_nil();
		if (!lhat_machine_make_table(machine, &table))
		{
			answers[0] = lhat_nil();
			*answerCount = 1;
			return;
		}
		LhatTable *t = (LhatTable *) lhat_as_object(table);
		for (size_t i = 0; i < joints.size(); i++)
		{
			bool refused = false;
			lhat_table_set(t, lhat_integer((int64_t) i), pushJoint(machine, joints[i]), &refused);
		}
		answers[0] = table;
		*answerCount = 1;
	});
}

static void lh_World_getContacts(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() { answers[0] = objectList(machine, w->getContacts(), true); *answerCount = 1; });
}

// queryShapesInArea(lx, ly, ux, uy, fn) where fn(shape) -> bool^ (false stops).
static void lh_World_queryShapesInArea(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	if (count < 6 || !lhat_is_object_kind(args[5], LHAT_OBJECT_SUBROUTINE))
	{
		lh::raise(machine, "queryShapesInArea needs a procedure to call.");
		return;
	}
	QueryVisitor visitor(machine, args[5]);
	lh::guard(machine, [&]() {
		w->queryShapesInArea(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4), visitor);
		answers[0] = lhat_nil();
		*answerCount = 1;
	});
}

// getShapesInArea(lx, ly, ux, uy) -> t^{Shape[]}
static void lh_World_getShapesInArea(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() {
		std::vector<Shape *> shapes = w->getShapesInArea(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4));
		LhatValue table = lhat_nil();
		if (!lhat_machine_make_table(machine, &table))
		{
			answers[0] = lhat_nil();
			*answerCount = 1;
			return;
		}
		LhatTable *t = (LhatTable *) lhat_as_object(table);
		for (size_t i = 0; i < shapes.size(); i++)
		{
			bool refused = false;
			lhat_table_set(t, lhat_integer((int64_t) i), pushShape(machine, shapes[i]), &refused);
		}
		answers[0] = table;
		*answerCount = 1;
	});
}

// rayCast(x1, y1, x2, y2, fn) where fn(shape, x, y, nx, ny, fraction) -> number^
// (the fraction to clip the ray to: 0 stops, 1 continues, -1 ignores).
static void lh_World_rayCast(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	if (count < 6 || !lhat_is_object_kind(args[5], LHAT_OBJECT_SUBROUTINE))
	{
		lh::raise(machine, "rayCast needs a function to call.");
		return;
	}
	RayVisitor visitor(machine, args[5]);
	lh::guard(machine, [&]() {
		w->rayCast(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4), visitor);
		answers[0] = lhat_nil();
		*answerCount = 1;
	});
}

// A hit as { shape, x, y, nx, ny, fraction }; nil^ when nothing was hit.
static LhatValue hitTable(LhatMachine *machine, const World::RayHit &hit)
{
	if (hit.shape == nullptr)
		return lhat_nil();
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return lhat_nil();
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	struct { const char *name; LhatValue value; } fields[] = {
		{"shape", pushShape(machine, hit.shape)},
		{"x", lhat_real(hit.x)},
		{"y", lhat_real(hit.y)},
		{"nx", lhat_real(hit.nx)},
		{"ny", lhat_real(hit.ny)},
		{"fraction", lhat_real(hit.fraction)},
	};
	for (const auto &f : fields)
	{
		LhatValue key = lhat_nil();
		bool refused = false;
		if (lh::makeString(machine, f.name, &key))
			lhat_table_set(t, key, f.value, &refused);
	}
	return table;
}

static void lh_World_rayCastAny(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() {
		answers[0] = hitTable(machine, w->rayCastAny(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4)));
		*answerCount = 1;
	});
}

static void lh_World_rayCastClosest(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() {
		answers[0] = hitTable(machine, w->rayCastClosest(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4)));
		*answerCount = 1;
	});
}

static void lh_World_destroy(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	WORLD_SELF();
	lh::guard(machine, [&]() {
		w->destroy();
	});
}

static void lh_World_isDestroyed(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	World *w = checkWorld(machine, args, count, 0);
	if (w == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_bool(!w->isValid());
	*answerCount = 1;
}

bool lhPhysicsWorld(lh::Context &ctx)
{
	const char *m = LH_PHYSICS;
	if (!ctx.objectType(m, "World", World::type))
		return false;
	if (ctx.types())
		return true;

	const char *W = "World";
	// A contact callback; postsolve also receives the impulses.
	const char *cb = "p^love.physics.Shape, love.physics.Shape, love.physics.Contact;";
	const char *post = "p^love.physics.Shape, love.physics.Shape, love.physics.Contact, number^, number^, number^, number^;";
	std::string set1 = std::string("p^self^, ") + cb;
	std::string set2 = set1 + ", " + cb;
	std::string set3 = set2 + ", " + cb;
	std::string set4 = set3 + ", " + post;
	std::string get = std::string("f^self^ -> (") + cb + "|nil^, " + cb + "|nil^, " + cb + "|nil^, " + post + "|nil^);";
	const char *hit = "t^{ shape : love.physics.Shape, x : number^, y : number^, nx : number^, ny : number^, fraction : number^ }|nil^";

	return ctx.member(m, W, "update", "p^self^, number^;", lh_World_update, nullptr)
		&& ctx.member(m, W, "update", "p^self^, number^, number^, number^;", lh_World_update, nullptr)
		&& ctx.member(m, W, "setCallbacks", "p^self^;", lh_World_setCallbacks, nullptr)
		&& ctx.member(m, W, "setCallbacks", (set1 + ";").c_str(), lh_World_setCallbacks, nullptr)
		&& ctx.member(m, W, "setCallbacks", (set2 + ";").c_str(), lh_World_setCallbacks, nullptr)
		&& ctx.member(m, W, "setCallbacks", (set3 + ";").c_str(), lh_World_setCallbacks, nullptr)
		&& ctx.member(m, W, "setCallbacks", (set4 + ";").c_str(), lh_World_setCallbacks, nullptr)
		&& ctx.member(m, W, "getCallbacks", get.c_str(), lh_World_getCallbacks, nullptr)
		&& ctx.member(m, W, "setContactFilter", "p^self^;", lh_World_setContactFilter, nullptr)
		&& ctx.member(m, W, "setContactFilter", "p^self^, p^love.physics.Shape, love.physics.Shape -> bool^;;", lh_World_setContactFilter, nullptr)
		&& ctx.member(m, W, "getContactFilter", "f^self^ -> p^love.physics.Shape, love.physics.Shape -> bool^;|nil^;", lh_World_getContactFilter, nullptr)
		&& ctx.member(m, W, "setGravity", "p^self^, number^, number^;", lh_World_setGravity, nullptr)
		&& ctx.member(m, W, "getGravity", "f^self^ -> (number^, number^);", lh_World_getGravity, nullptr)
		&& ctx.member(m, W, "translateOrigin", "p^self^, number^, number^;", lh_World_translateOrigin, nullptr)
		&& ctx.member(m, W, "setSleepingAllowed", "p^self^, bool^;", lh_World_setSleepingAllowed, nullptr)
		&& ctx.member(m, W, "isSleepingAllowed", "f^self^ -> bool^;", lh_World_isSleepingAllowed, nullptr)
		&& ctx.member(m, W, "isLocked", "f^self^ -> bool^;", lh_World_isLocked, nullptr)
		&& ctx.member(m, W, "getBodyCount", "f^self^ -> number^;", lh_World_getBodyCount, nullptr)
		&& ctx.member(m, W, "getJointCount", "f^self^ -> number^;", lh_World_getJointCount, nullptr)
		&& ctx.member(m, W, "getContactCount", "f^self^ -> number^;", lh_World_getContactCount, nullptr)
		&& ctx.member(m, W, "getBodies", "p^self^ -> t^{love.physics.Body[]};", lh_World_getBodies, nullptr)
		&& ctx.member(m, W, "getJoints", "p^self^ -> t^{love.physics.Joint[]};", lh_World_getJoints, nullptr)
		&& ctx.member(m, W, "getContacts", "p^self^ -> t^{love.physics.Contact[]};", lh_World_getContacts, nullptr)
		&& ctx.member(m, W, "queryShapesInArea", "p^self^, number^, number^, number^, number^, p^love.physics.Shape -> bool^;;", lh_World_queryShapesInArea, nullptr)
		&& ctx.member(m, W, "getShapesInArea", "p^self^, number^, number^, number^, number^ -> t^{love.physics.Shape[]};", lh_World_getShapesInArea, nullptr)
		&& ctx.member(m, W, "rayCast", "p^self^, number^, number^, number^, number^, p^love.physics.Shape, number^, number^, number^, number^, number^ -> number^;;", lh_World_rayCast, nullptr)
		&& ctx.member(m, W, "rayCastAny", (std::string("p^self^, number^, number^, number^, number^ -> ") + hit + ";").c_str(), lh_World_rayCastAny, nullptr)
		&& ctx.member(m, W, "rayCastClosest", (std::string("p^self^, number^, number^, number^, number^ -> ") + hit + ";").c_str(), lh_World_rayCastClosest, nullptr)
		&& ctx.member(m, W, "destroy", "p^self^;", lh_World_destroy, nullptr)
		&& ctx.member(m, W, "isDestroyed", "f^self^ -> bool^;", lh_World_isDestroyed, nullptr);
}

} // box2d
} // physics
} // love

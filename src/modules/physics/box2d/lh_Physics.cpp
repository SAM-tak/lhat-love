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

// love.physics for L^: the module functions (worlds, bodies, shapes, joints,
// the meter) and the helpers the other lh_*.cpp files in this directory use.
// The reference is wrap_Physics.cpp beside this file. The deprecated
// body-less shape constructors and newFixture are not carried over.

#include "lh_Physics.h"

#include "CircleShape.h"
#include "PolygonShape.h"
#include "EdgeShape.h"
#include "ChainShape.h"
#include "DistanceJoint.h"
#include "MouseJoint.h"
#include "RevoluteJoint.h"
#include "PrismaticJoint.h"
#include "PulleyJoint.h"
#include "GearJoint.h"
#include "FrictionJoint.h"
#include "WeldJoint.h"
#include "WheelJoint.h"
#include "RopeJoint.h"
#include "MotorJoint.h"

namespace love
{
namespace physics
{
namespace box2d
{

#define instance() (Module::getInstance<Physics>(Module::M_PHYSICS))

PhysicsBinding physicsBinding;

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

template <typename T>
static T *checkAt(LhatMachine *machine, const LhatValue *args, size_t count, size_t index, const char *what)
{
	T *object = index < count ? lh::checkObject<T>(args[index], *physicsBinding.registry) : nullptr;
	if (object == nullptr)
		lh::raise(machine, std::string("Expected a ") + what);
	return object;
}

World *checkWorld(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<World>(machine, args, count, index, "World");
}

Body *checkBody(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Body>(machine, args, count, index, "Body");
}

Shape *checkShape(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Shape>(machine, args, count, index, "Shape");
}

Joint *checkJoint(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Joint>(machine, args, count, index, "Joint");
}

Contact *checkContact(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	return checkAt<Contact>(machine, args, count, index, "Contact");
}

float numberAt(const LhatValue *args, size_t count, size_t index, float fallback)
{
	return (float) lh::optNumber(args, count, index, fallback);
}

bool boolAt(const LhatValue *args, size_t count, size_t index, bool fallback)
{
	return lh::optBool(args, count, index, fallback);
}

bool coordsOf(LhatMachine *machine, const LhatValue *args, size_t count, size_t from, std::vector<Vector2> &out)
{
	if (from > count || (count - from) % 2 != 0)
	{
		lh::raise(machine, "Number of vertex components must be a multiple of two.");
		return false;
	}
	for (size_t i = from; i + 1 < count; i += 2)
	{
		if (!lhat_is_number(args[i]) || !lhat_is_number(args[i + 1]))
		{
			lh::raise(machine, "Vertex components must be numbers.");
			return false;
		}
		out.emplace_back((float) lh::optNumber(args, count, i, 0), (float) lh::optNumber(args, count, i + 1, 0));
	}
	return true;
}

LhatValue pushWorld(LhatMachine *machine, World *world)
{
	return lh::pushObject(machine, *physicsBinding.registry, world);
}

LhatValue pushBody(LhatMachine *machine, Body *body)
{
	return lh::pushObject(machine, *physicsBinding.registry, body);
}

// 05 の 8.8改: the four kinds are types of their own, so a shape reaches L^
// as the one it actually is -- which is what keeps a circle from being asked
// for a chain's vertices.
LhatValue pushShape(LhatMachine *machine, Shape *shape)
{
	if (shape == nullptr)
		return lhat_nil();
	love::Type *type = &Shape::type;
	switch (shape->getType())
	{
	case Shape::SHAPE_CIRCLE:  type = &CircleShape::type;  break;
	case Shape::SHAPE_POLYGON: type = &PolygonShape::type; break;
	case Shape::SHAPE_EDGE:    type = &EdgeShape::type;    break;
	case Shape::SHAPE_CHAIN:   type = &ChainShape::type;   break;
	default: break;
	}
	return lh::pushObject(machine, *physicsBinding.registry, *type, shape);
}

// 05 の 8.8改: the same for the eleven kinds of joint -- what reaches L^ is
// the kind it is, so getTarget is there on a mouse joint and nowhere else.
LhatValue pushJoint(LhatMachine *machine, Joint *joint)
{
	if (joint == nullptr)
		return lhat_nil();
	love::Type *type = &Joint::type;
	switch (joint->getType())
	{
	case Joint::JOINT_DISTANCE:  type = &DistanceJoint::type;  break;
	case Joint::JOINT_REVOLUTE:  type = &RevoluteJoint::type;  break;
	case Joint::JOINT_PRISMATIC: type = &PrismaticJoint::type; break;
	case Joint::JOINT_MOUSE:     type = &MouseJoint::type;     break;
	case Joint::JOINT_PULLEY:    type = &PulleyJoint::type;    break;
	case Joint::JOINT_GEAR:      type = &GearJoint::type;      break;
	case Joint::JOINT_FRICTION:  type = &FrictionJoint::type;  break;
	case Joint::JOINT_WELD:      type = &WeldJoint::type;      break;
	case Joint::JOINT_WHEEL:     type = &WheelJoint::type;     break;
	case Joint::JOINT_ROPE:      type = &RopeJoint::type;      break;
	case Joint::JOINT_MOTOR:     type = &MotorJoint::type;     break;
	default: break;
	}
	return lh::pushObject(machine, *physicsBinding.registry, *type, joint);
}

LhatValue pushContact(LhatMachine *machine, Contact *contact)
{
	return lh::pushObject(machine, *physicsBinding.registry, contact);
}

// 05 の 8.7: several answers go into the room the machine handed over, in
// order -- what used to be one tuple value is just this now.
void numbers(const float *values, size_t count, LhatValue *answers, int *answerCount)
{
	for (size_t i = 0; i < count; i++)
		answers[i] = lhat_real(values[i]);
	*answerCount = (int) count;
}

LhatValue numberList(LhatMachine *machine, const std::vector<float> &values)
{
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return lhat_nil();
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < values.size(); i++)
	{
		bool refused = false;
		lhat_table_set(t, lhat_integer((int64_t) i), lhat_real(values[i]), &refused);
	}
	return table;
}

bool callParked(LhatMachine *machine, lh::Parked *callee, const LhatValue *args, size_t count, LhatValue *result)
{
	if (callee == nullptr)
		return false;
	LhatValue fn = callee->get();
	if (!lhat_is_object_kind(fn, LHAT_OBJECT_SUBROUTINE) && !lhat_is_object_kind(fn, LHAT_OBJECT_HOST))
		return false;
	LhatRunResult ran = lhat_machine_call(machine, fn, args, count);
	if (ran.status != LHAT_RUN_OK)
		return false;
	if (result != nullptr)
		*result = ran.value;
	return true;
}

// ---------------------------------------------------------------------------
// Module functions
// ---------------------------------------------------------------------------

static bool bodyTypeAt(LhatMachine *machine, const LhatValue *args, size_t count, size_t index, Body::Type &out)
{
	std::string name = lh::optEnum(machine, args, count, index, "love.physics", "BodyType", "static");
	if (!Body::getConstant(name.c_str(), out))
	{
		lh::raise(machine, "Invalid Body type: " + name);
		return false;
	}
	return true;
}

// Pushes (body, shape) and drops the references the factory handed over.
static void bodyAndShape(LhatMachine *machine, Body *body, Shape *shape,
						 LhatValue *answers, int *answerCount)
{
	answers[0] = pushBody(machine, body);
	answers[1] = pushShape(machine, shape);
	*answerCount = 2;
	body->release();
	shape->release();
}

// newWorld([gx, gy[, sleep]])
static void lh_newWorld(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	float gx = numberAt(args, count, 0);
	float gy = numberAt(args, count, 1);
	bool sleep = boolAt(args, count, 2, true);
	lh::guard(machine, [&]() {
		StrongRef<World> world(instance()->newWorld(gx, gy, sleep), Acquire::NORETAIN);
		answers[0] = pushWorld(machine, world.get());
		*answerCount = 1;
	});
}

// newBody(world[, x, y[, type]])
static void lh_newBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	if (world == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = numberAt(args, count, 1);
	float y = numberAt(args, count, 2);
	Body::Type btype = Body::BODY_STATIC;
	if (!bodyTypeAt(machine, args, count, 3, btype))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		StrongRef<Body> body(instance()->newBody(world, x, y, btype), Acquire::NORETAIN);
		answers[0] = pushBody(machine, body.get());
		*answerCount = 1;
	});
}

// newCircleBody(world, type, x, y, radius) -> (Body, Shape)
static void lh_newCircleBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = numberAt(args, count, 2);
	float y = numberAt(args, count, 3);
	float radius = numberAt(args, count, 4);
	lh::guard(machine, [&]() {
		CircleShape *shape = nullptr;
		Body *body = instance()->newCircleBody(world, btype, x, y, radius, shape);
		bodyAndShape(machine, body, shape, answers, answerCount);
	});
}

// newRectangleBody(world, type, x, y, w, h[, angle]) -> (Body, Shape)
static void lh_newRectangleBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = numberAt(args, count, 2);
	float y = numberAt(args, count, 3);
	float w = numberAt(args, count, 4);
	float h = numberAt(args, count, 5);
	float angle = numberAt(args, count, 6);
	lh::guard(machine, [&]() {
		PolygonShape *shape = nullptr;
		Body *body = instance()->newRectangleBody(world, btype, x, y, w, h, angle, shape);
		bodyAndShape(machine, body, shape, answers, answerCount);
	});
}

// newPolygonBody(world, type, x1, y1, x2, y2, x3, y3, ...) -> (Body, Shape)
static void lh_newPolygonBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 2, coords))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		PolygonShape *shape = nullptr;
		Body *body = instance()->newPolygonBody(world, btype, coords.data(), (int) coords.size(), shape);
		bodyAndShape(machine, body, shape, answers, answerCount);
	});
}

// newEdgeBody(world, type, x1, y1, x2, y2[, prevx, prevy, nextx, nexty]) -> (Body, Shape)
static void lh_newEdgeBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x1 = numberAt(args, count, 2);
	float y1 = numberAt(args, count, 3);
	float x2 = numberAt(args, count, 4);
	float y2 = numberAt(args, count, 5);
	lh::guard(machine, [&]() {
		EdgeShape *shape = nullptr;
		Body *body = nullptr;
		if (count >= 10)
		{
			float prevx = numberAt(args, count, 6);
			float prevy = numberAt(args, count, 7);
			float nextx = numberAt(args, count, 8);
			float nexty = numberAt(args, count, 9);
			body = instance()->newEdgeBody(world, btype, x1, y1, x2, y2, prevx, prevy, nextx, nexty, shape);
		}
		else
			body = instance()->newEdgeBody(world, btype, x1, y1, x2, y2, shape);
		bodyAndShape(machine, body, shape, answers, answerCount);
	});
}

// newChainBody(world, type, loop, x1, y1, x2, y2, ...) -> (Body, Shape)
static void lh_newChainBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	World *world = checkWorld(machine, args, count, 0);
	Body::Type btype = Body::BODY_STATIC;
	if (world == nullptr || !bodyTypeAt(machine, args, count, 1, btype))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	bool loop = boolAt(args, count, 2);
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 3, coords) || coords.empty())
	{
		answers[0] = coords.empty() ? lh::raise(machine, "A chain needs at least one vertex.") : lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		ChainShape *shape = nullptr;
		Body *body = instance()->newChainBody(world, btype, loop, coords.data(), (int) coords.size(), shape);
		bodyAndShape(machine, body, shape, answers, answerCount);
	});
}

// newCircleShape(body, radius) / newCircleShape(body, x, y, radius)
static void lh_newCircleShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = 0, y = 0, radius = 0;
	if (count >= 4)
	{
		x = numberAt(args, count, 1);
		y = numberAt(args, count, 2);
		radius = numberAt(args, count, 3);
	}
	else
		radius = numberAt(args, count, 1);
	lh::guard(machine, [&]() {
		StrongRef<CircleShape> shape(instance()->newCircleShape(body, x, y, radius), Acquire::NORETAIN);
		answers[0] = pushShape(machine, shape.get());
		*answerCount = 1;
	});
}

// newRectangleShape(body, w, h) / newRectangleShape(body, x, y, w, h[, angle])
static void lh_newRectangleShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = 0, y = 0, w = 0, h = 0, angle = 0;
	if (count >= 5)
	{
		x = numberAt(args, count, 1);
		y = numberAt(args, count, 2);
		w = numberAt(args, count, 3);
		h = numberAt(args, count, 4);
		angle = numberAt(args, count, 5);
	}
	else
	{
		w = numberAt(args, count, 1);
		h = numberAt(args, count, 2);
	}
	lh::guard(machine, [&]() {
		StrongRef<PolygonShape> shape(instance()->newRectangleShape(body, x, y, w, h, angle), Acquire::NORETAIN);
		answers[0] = pushShape(machine, shape.get());
		*answerCount = 1;
	});
}

// newEdgeShape(body, x1, y1, x2, y2[, prevx, prevy, nextx, nexty])
static void lh_newEdgeShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x1 = numberAt(args, count, 1);
	float y1 = numberAt(args, count, 2);
	float x2 = numberAt(args, count, 3);
	float y2 = numberAt(args, count, 4);
	lh::guard(machine, [&]() {
		EdgeShape *made = nullptr;
		if (count >= 9)
		{
			float prevx = numberAt(args, count, 5);
			float prevy = numberAt(args, count, 6);
			float nextx = numberAt(args, count, 7);
			float nexty = numberAt(args, count, 8);
			made = instance()->newEdgeShape(body, x1, y1, x2, y2, prevx, prevy, nextx, nexty);
		}
		else
			made = instance()->newEdgeShape(body, x1, y1, x2, y2);
		StrongRef<EdgeShape> shape(made, Acquire::NORETAIN);
		answers[0] = pushShape(machine, shape.get());
		*answerCount = 1;
	});
}

// newPolygonShape(body, x1, y1, x2, y2, x3, y3, ...)
static void lh_newPolygonShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 1, coords))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		StrongRef<PolygonShape> shape(instance()->newPolygonShape(body, coords.data(), (int) coords.size()), Acquire::NORETAIN);
		answers[0] = pushShape(machine, shape.get());
		*answerCount = 1;
	});
}

// newChainShape(body, loop, x1, y1, x2, y2, ...)
static void lh_newChainShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	bool loop = boolAt(args, count, 1);
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 2, coords))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	if (coords.empty())
	{
		lh::raise(machine, "A chain needs at least one vertex.");
		return;
	}
	lh::guard(machine, [&]() {
		StrongRef<ChainShape> shape(instance()->newChainShape(body, loop, coords.data(), (int) coords.size()), Acquire::NORETAIN);
		answers[0] = pushShape(machine, shape.get());
		*answerCount = 1;
	});
}

// Two bodies at args[0], args[1]; false (raised) when either is missing.
static bool twoBodies(LhatMachine *machine, const LhatValue *args, size_t count, Body *&a, Body *&b)
{
	a = checkBody(machine, args, count, 0);
	b = a != nullptr ? checkBody(machine, args, count, 1) : nullptr;
	return b != nullptr;
}

template <typename T>
static void pushNewJoint(LhatMachine *machine, const std::function<T *()> &make,
						 LhatValue *answers, int *answerCount)
{
	lh::guard(machine, [&]() {
		StrongRef<T> joint(make(), Acquire::NORETAIN);
		answers[0] = pushJoint(machine, joint.get());
		*answerCount = 1;
	});
}

// newDistanceJoint(b1, b2, x1, y1, x2, y2[, collideConnected])
static void lh_newDistanceJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	float x1 = numberAt(args, count, 2), y1 = numberAt(args, count, 3);
	float x2 = numberAt(args, count, 4), y2 = numberAt(args, count, 5);
	bool collide = boolAt(args, count, 6);
	pushNewJoint<DistanceJoint>(machine, [&]() { return instance()->newDistanceJoint(b1, b2, x1, y1, x2, y2, collide); }, answers, answerCount);
}

// newMouseJoint(body, x, y)
static void lh_newMouseJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *body = checkBody(machine, args, count, 0);
	if (body == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = numberAt(args, count, 1), y = numberAt(args, count, 2);
	pushNewJoint<MouseJoint>(machine, [&]() { return instance()->newMouseJoint(body, x, y); }, answers, answerCount);
}

// The anchor arguments the revolute/weld/friction joints share:
// (b1, b2, x, y[, collide]) or (b1, b2, x1, y1, x2, y2[, collide[, referenceAngle]]).
struct Anchors
{
	float xA = 0, yA = 0, xB = 0, yB = 0;
	bool collide = false;
	bool hasReferenceAngle = false;
	float referenceAngle = 0;
};

static Anchors anchorsOf(const LhatValue *args, size_t count)
{
	Anchors a;
	a.xA = numberAt(args, count, 2);
	a.yA = numberAt(args, count, 3);
	if (count >= 6)
	{
		a.xB = numberAt(args, count, 4);
		a.yB = numberAt(args, count, 5);
		a.collide = boolAt(args, count, 6);
		a.hasReferenceAngle = count >= 8;
		a.referenceAngle = numberAt(args, count, 7);
	}
	else
	{
		a.xB = a.xA;
		a.yB = a.yA;
		a.collide = boolAt(args, count, 4);
	}
	return a;
}

static void lh_newRevoluteJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	Anchors a = anchorsOf(args, count);
	pushNewJoint<RevoluteJoint>(machine, [&]() {
		if (a.hasReferenceAngle)
			return instance()->newRevoluteJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide, a.referenceAngle);
		return instance()->newRevoluteJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide);
	}, answers, answerCount);
}

static void lh_newWeldJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	Anchors a = anchorsOf(args, count);
	pushNewJoint<WeldJoint>(machine, [&]() {
		if (a.hasReferenceAngle)
			return instance()->newWeldJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide, a.referenceAngle);
		return instance()->newWeldJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide);
	}, answers, answerCount);
}

static void lh_newFrictionJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	Anchors a = anchorsOf(args, count);
	pushNewJoint<FrictionJoint>(machine, [&]() { return instance()->newFrictionJoint(b1, b2, a.xA, a.yA, a.xB, a.yB, a.collide); }, answers, answerCount);
}

// (b1, b2, x, y, ax, ay[, collide]) or (b1, b2, x1, y1, x2, y2, ax, ay[, collide[, referenceAngle]])
struct AxisAnchors
{
	Anchors anchors;
	float ax = 0, ay = 0;
};

static AxisAnchors axisAnchorsOf(const LhatValue *args, size_t count)
{
	AxisAnchors a;
	a.anchors.xA = numberAt(args, count, 2);
	a.anchors.yA = numberAt(args, count, 3);
	if (count >= 8)
	{
		a.anchors.xB = numberAt(args, count, 4);
		a.anchors.yB = numberAt(args, count, 5);
		a.ax = numberAt(args, count, 6);
		a.ay = numberAt(args, count, 7);
		a.anchors.collide = boolAt(args, count, 8);
		a.anchors.hasReferenceAngle = count >= 10;
		a.anchors.referenceAngle = numberAt(args, count, 9);
	}
	else
	{
		a.anchors.xB = a.anchors.xA;
		a.anchors.yB = a.anchors.yA;
		a.ax = numberAt(args, count, 4);
		a.ay = numberAt(args, count, 5);
		a.anchors.collide = boolAt(args, count, 6);
	}
	return a;
}

static void lh_newPrismaticJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	AxisAnchors a = axisAnchorsOf(args, count);
	const Anchors &n = a.anchors;
	pushNewJoint<PrismaticJoint>(machine, [&]() {
		if (n.hasReferenceAngle)
			return instance()->newPrismaticJoint(b1, b2, n.xA, n.yA, n.xB, n.yB, a.ax, a.ay, n.collide, n.referenceAngle);
		return instance()->newPrismaticJoint(b1, b2, n.xA, n.yA, n.xB, n.yB, a.ax, a.ay, n.collide);
	}, answers, answerCount);
}

static void lh_newWheelJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	AxisAnchors a = axisAnchorsOf(args, count);
	const Anchors &n = a.anchors;
	pushNewJoint<WheelJoint>(machine, [&]() { return instance()->newWheelJoint(b1, b2, n.xA, n.yA, n.xB, n.yB, a.ax, a.ay, n.collide); }, answers, answerCount);
}

// newPulleyJoint(b1, b2, gx1, gy1, gx2, gy2, x1, y1, x2, y2, ratio[, collide])
static void lh_newPulleyJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	b2Vec2 g1(numberAt(args, count, 2), numberAt(args, count, 3));
	b2Vec2 g2(numberAt(args, count, 4), numberAt(args, count, 5));
	b2Vec2 a1(numberAt(args, count, 6), numberAt(args, count, 7));
	b2Vec2 a2(numberAt(args, count, 8), numberAt(args, count, 9));
	float ratio = numberAt(args, count, 10, 1.0f);
	bool collide = boolAt(args, count, 11, true);
	pushNewJoint<PulleyJoint>(machine, [&]() { return instance()->newPulleyJoint(b1, b2, g1, g2, a1, a2, ratio, collide); }, answers, answerCount);
}

// newGearJoint(j1, j2[, ratio[, collide]])
static void lh_newGearJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	Joint *j1 = checkJoint(machine, args, count, 0);
	Joint *j2 = j1 != nullptr ? checkJoint(machine, args, count, 1) : nullptr;
	if (j2 == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float ratio = numberAt(args, count, 2, 1.0f);
	bool collide = boolAt(args, count, 3);
	pushNewJoint<GearJoint>(machine, [&]() { return instance()->newGearJoint(j1, j2, ratio, collide); }, answers, answerCount);
}

// newRopeJoint(b1, b2, x1, y1, x2, y2, maxLength[, collide])
static void lh_newRopeJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	float x1 = numberAt(args, count, 2), y1 = numberAt(args, count, 3);
	float x2 = numberAt(args, count, 4), y2 = numberAt(args, count, 5);
	float maxLength = numberAt(args, count, 6);
	bool collide = boolAt(args, count, 7);
	pushNewJoint<RopeJoint>(machine, [&]() { return instance()->newRopeJoint(b1, b2, x1, y1, x2, y2, maxLength, collide); }, answers, answerCount);
}

// newMotorJoint(b1, b2[, correctionFactor[, collide]])
static void lh_newMotorJoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b1, *b2;
	if (!twoBodies(machine, args, count, b1, b2))
		return;
	pushNewJoint<MotorJoint>(machine, [&]() {
		if (count >= 3)
			return instance()->newMotorJoint(b1, b2, numberAt(args, count, 2), boolAt(args, count, 3));
		return instance()->newMotorJoint(b1, b2);
	}, answers, answerCount);
}

// getDistance(shapeA, shapeB) -> (distance, x1, y1, x2, y2)
static void lh_getDistance(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	Shape *a = checkShape(machine, args, count, 0);
	Shape *b = a != nullptr ? checkShape(machine, args, count, 1) : nullptr;
	if (b == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		float out[5];
		out[0] = Physics::getDistance(a, b, out[1], out[2], out[3], out[4]);
		numbers(out, 5, answers, answerCount);
	});
}

static void lh_getMeter(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) args;
	(void) count;
	answers[0] = lhat_real(Physics::getMeter());
	*answerCount = 1;
}

static void lh_setMeter(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	float meter = numberAt(args, count, 0);
	lh::guard(machine, [&]() {
		Physics::setMeter(meter);
	});
}

// The four stiffness/frequency conversions: (a, b, body1[, body2]) -> (x, y),
// the ground body standing in when body2 is not given.
typedef void (*Conversion)(float &, float &, float, float, b2Body *, b2Body *);

static void convert(LhatMachine *machine, const LhatValue *args, size_t count, Conversion conversion,
					LhatValue *answers, int *answerCount)
{
	float a = numberAt(args, count, 0);
	float b = numberAt(args, count, 1);
	Body *body1 = checkBody(machine, args, count, 2);
	if (body1 == nullptr)
		return;
	b2Body *other = nullptr;
	if (count >= 4)
	{
		Body *body2 = checkBody(machine, args, count, 3);
		if (body2 == nullptr)
			return;
		other = body2->body;
	}
	else
		other = body1->getWorld()->getGroundBody();
	float out[2];
	conversion(out[0], out[1], a, b, body1->body, other);
	numbers(out, 2, answers, answerCount);
}

static void lh_computeLinearStiffness(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	convert(machine, args, count, Physics::computeLinearStiffness, answers, answerCount);
}

static void lh_computeLinearFrequency(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	convert(machine, args, count, Physics::computeLinearFrequency, answers, answerCount);
}

static void lh_computeAngularStiffness(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	convert(machine, args, count, Physics::computeAngularStiffness, answers, answerCount);
}

static void lh_computeAngularFrequency(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	convert(machine, args, count, Physics::computeAngularFrequency, answers, answerCount);
}

} // box2d
} // physics
} // love

namespace love
{
namespace lh
{

bool lhopen_love_physics(Context &ctx)
{
	using namespace love::physics::box2d;
	if (ctx.types())
	{
		if (!ctx.enumType("love.physics", "BodyType", Body::getConstants(Body::Type{})))
			return false;
		if (!ctx.enumType("love.physics", "ShapeType", Shape::SHAPE_MAX_ENUM, Shape::getConstant))
			return false;
		if (!ctx.enumType("love.physics", "JointType", Joint::JOINT_MAX_ENUM, Joint::getConstant))
			return false;
	}

	const char *m = LH_PHYSICS;

	// The types first (every signature below names them), then each type's
	// members, then the module functions.
	if (!lhPhysicsWorld(ctx) || !lhPhysicsBody(ctx) || !lhPhysicsShape(ctx) || !lhPhysicsJoint(ctx) || !lhPhysicsContact(ctx))
		return false;
	if (ctx.types())
		return true;

	physicsBinding.errors = ctx.errors;
	physicsBinding.registry = ctx.registry;

	const char *W = "love.physics.World";
	const char *B = "love.physics.Body";
	const char *S = "love.physics.Shape";
	const char *J = "love.physics.Joint";
	// 8.8改: what these answer is the kind they made, not "some Shape".
	auto bodyAnd = [&](const char *kind) { return std::string(" -> (") + B + ", love.physics." + kind + ");"; };

	return ctx.func(m, "newWorld", (std::string("p^ -> ") + W + ";").c_str(), lh_newWorld, nullptr)
		&& ctx.func(m, "newWorld", (std::string("p^number^, number^ -> ") + W + ";").c_str(), lh_newWorld, nullptr)
		&& ctx.func(m, "newWorld", (std::string("p^number^, number^, bool^ -> ") + W + ";").c_str(), lh_newWorld, nullptr)
		&& ctx.func(m, "newBody", (std::string("p^") + W + " -> " + B + ";").c_str(), lh_newBody, nullptr)
		&& ctx.func(m, "newBody", (std::string("p^") + W + ", number^, number^ -> " + B + ";").c_str(), lh_newBody, nullptr)
		&& ctx.func(m, "newBody", (std::string("p^") + W + ", number^, number^, love.physics.BodyType -> " + B + ";").c_str(), lh_newBody, nullptr)
		&& ctx.func(m, "newCircleBody", (std::string("p^") + W + ", love.physics.BodyType, number^, number^, number^" + bodyAnd("CircleShape")).c_str(), lh_newCircleBody, nullptr)
		&& ctx.func(m, "newRectangleBody", (std::string("p^") + W + ", love.physics.BodyType, number^, number^, number^, number^" + bodyAnd("PolygonShape")).c_str(), lh_newRectangleBody, nullptr)
		&& ctx.func(m, "newRectangleBody", (std::string("p^") + W + ", love.physics.BodyType, number^, number^, number^, number^, number^" + bodyAnd("PolygonShape")).c_str(), lh_newRectangleBody, nullptr)
		&& ctx.func(m, "newPolygonBody", (std::string("p^") + W + ", love.physics.BodyType, number^, number^, number^, number^, number^, number^, ..." + bodyAnd("PolygonShape")).c_str(), lh_newPolygonBody, nullptr)
		&& ctx.func(m, "newEdgeBody", (std::string("p^") + W + ", love.physics.BodyType, number^, number^, number^, number^" + bodyAnd("EdgeShape")).c_str(), lh_newEdgeBody, nullptr)
		&& ctx.func(m, "newEdgeBody", (std::string("p^") + W + ", love.physics.BodyType, number^, number^, number^, number^, number^, number^, number^, number^" + bodyAnd("EdgeShape")).c_str(), lh_newEdgeBody, nullptr)
		&& ctx.func(m, "newChainBody", (std::string("p^") + W + ", love.physics.BodyType, bool^, number^, number^, ..." + bodyAnd("ChainShape")).c_str(), lh_newChainBody, nullptr)
		&& ctx.func(m, "newCircleShape", (std::string("p^") + B + ", number^ -> love.physics.CircleShape;").c_str(), lh_newCircleShape, nullptr)
		&& ctx.func(m, "newCircleShape", (std::string("p^") + B + ", number^, number^, number^ -> love.physics.CircleShape;").c_str(), lh_newCircleShape, nullptr)
		&& ctx.func(m, "newRectangleShape", (std::string("p^") + B + ", number^, number^ -> love.physics.PolygonShape;").c_str(), lh_newRectangleShape, nullptr)
		&& ctx.func(m, "newRectangleShape", (std::string("p^") + B + ", number^, number^, number^, number^ -> love.physics.PolygonShape;").c_str(), lh_newRectangleShape, nullptr)
		&& ctx.func(m, "newRectangleShape", (std::string("p^") + B + ", number^, number^, number^, number^, number^ -> love.physics.PolygonShape;").c_str(), lh_newRectangleShape, nullptr)
		&& ctx.func(m, "newEdgeShape", (std::string("p^") + B + ", number^, number^, number^, number^ -> love.physics.EdgeShape;").c_str(), lh_newEdgeShape, nullptr)
		&& ctx.func(m, "newEdgeShape", (std::string("p^") + B + ", number^, number^, number^, number^, number^, number^, number^, number^ -> love.physics.EdgeShape;").c_str(), lh_newEdgeShape, nullptr)
		&& ctx.func(m, "newPolygonShape", (std::string("p^") + B + ", number^, number^, number^, number^, number^, number^, ... -> love.physics.PolygonShape;").c_str(), lh_newPolygonShape, nullptr)
		&& ctx.func(m, "newChainShape", (std::string("p^") + B + ", bool^, number^, number^, ... -> love.physics.ChainShape;").c_str(), lh_newChainShape, nullptr)
		&& ctx.func(m, "newDistanceJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> love.physics.DistanceJoint;").c_str(), lh_newDistanceJoint, nullptr)
		&& ctx.func(m, "newDistanceJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> love.physics.DistanceJoint;").c_str(), lh_newDistanceJoint, nullptr)
		&& ctx.func(m, "newMouseJoint", (std::string("p^") + B + ", number^, number^ -> love.physics.MouseJoint;").c_str(), lh_newMouseJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^ -> love.physics.RevoluteJoint;").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, bool^ -> love.physics.RevoluteJoint;").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> love.physics.RevoluteJoint;").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> love.physics.RevoluteJoint;").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newRevoluteJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^, number^ -> love.physics.RevoluteJoint;").c_str(), lh_newRevoluteJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> love.physics.PrismaticJoint;").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> love.physics.PrismaticJoint;").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^ -> love.physics.PrismaticJoint;").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, bool^ -> love.physics.PrismaticJoint;").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPrismaticJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, bool^, number^ -> love.physics.PrismaticJoint;").c_str(), lh_newPrismaticJoint, nullptr)
		&& ctx.func(m, "newPulleyJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, number^, number^, number^ -> love.physics.PulleyJoint;").c_str(), lh_newPulleyJoint, nullptr)
		&& ctx.func(m, "newPulleyJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, number^, number^, number^, bool^ -> love.physics.PulleyJoint;").c_str(), lh_newPulleyJoint, nullptr)
		&& ctx.func(m, "newGearJoint", (std::string("p^") + J + ", " + J + " -> love.physics.GearJoint;").c_str(), lh_newGearJoint, nullptr)
		&& ctx.func(m, "newGearJoint", (std::string("p^") + J + ", " + J + ", number^ -> love.physics.GearJoint;").c_str(), lh_newGearJoint, nullptr)
		&& ctx.func(m, "newGearJoint", (std::string("p^") + J + ", " + J + ", number^, bool^ -> love.physics.GearJoint;").c_str(), lh_newGearJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^ -> love.physics.FrictionJoint;").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^, bool^ -> love.physics.FrictionJoint;").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> love.physics.FrictionJoint;").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newFrictionJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> love.physics.FrictionJoint;").c_str(), lh_newFrictionJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^ -> love.physics.WeldJoint;").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, bool^ -> love.physics.WeldJoint;").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> love.physics.WeldJoint;").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> love.physics.WeldJoint;").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWeldJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^, number^ -> love.physics.WeldJoint;").c_str(), lh_newWeldJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^ -> love.physics.WheelJoint;").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, bool^ -> love.physics.WheelJoint;").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^ -> love.physics.WheelJoint;").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newWheelJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, number^, bool^ -> love.physics.WheelJoint;").c_str(), lh_newWheelJoint, nullptr)
		&& ctx.func(m, "newRopeJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^ -> love.physics.RopeJoint;").c_str(), lh_newRopeJoint, nullptr)
		&& ctx.func(m, "newRopeJoint", (std::string("p^") + B + ", " + B + ", number^, number^, number^, number^, number^, bool^ -> love.physics.RopeJoint;").c_str(), lh_newRopeJoint, nullptr)
		&& ctx.func(m, "newMotorJoint", (std::string("p^") + B + ", " + B + " -> love.physics.MotorJoint;").c_str(), lh_newMotorJoint, nullptr)
		&& ctx.func(m, "newMotorJoint", (std::string("p^") + B + ", " + B + ", number^ -> love.physics.MotorJoint;").c_str(), lh_newMotorJoint, nullptr)
		&& ctx.func(m, "newMotorJoint", (std::string("p^") + B + ", " + B + ", number^, bool^ -> love.physics.MotorJoint;").c_str(), lh_newMotorJoint, nullptr)
		&& ctx.func(m, "getDistance", (std::string("f^") + S + ", " + S + " -> (number^, number^, number^, number^, number^);").c_str(), lh_getDistance, nullptr)
		&& ctx.func(m, "getMeter", "f^ -> number^;", lh_getMeter, nullptr)
		&& ctx.func(m, "setMeter", "p^number^;", lh_setMeter, nullptr)
		&& ctx.func(m, "computeLinearStiffness", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeLinearStiffness, nullptr)
		&& ctx.func(m, "computeLinearStiffness", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeLinearStiffness, nullptr)
		&& ctx.func(m, "computeLinearFrequency", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeLinearFrequency, nullptr)
		&& ctx.func(m, "computeLinearFrequency", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeLinearFrequency, nullptr)
		&& ctx.func(m, "computeAngularStiffness", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeAngularStiffness, nullptr)
		&& ctx.func(m, "computeAngularStiffness", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeAngularStiffness, nullptr)
		&& ctx.func(m, "computeAngularFrequency", (std::string("f^number^, number^, ") + B + " -> (number^, number^);").c_str(), lh_computeAngularFrequency, nullptr)
		&& ctx.func(m, "computeAngularFrequency", (std::string("f^number^, number^, ") + B + ", " + B + " -> (number^, number^);").c_str(), lh_computeAngularFrequency, nullptr);
}

} // lh
} // love

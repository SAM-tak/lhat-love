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

// love.physics.Body for L^. The reference is wrap_Body.cpp beside this file.
// User data is any L^ value, parked for as long as the body holds it.

#include "lh_Physics.h"

namespace love
{
namespace physics
{
namespace box2d
{

static Body *self(LhatMachine *machine, const LhatValue *args, size_t count)
{
	Body *body = checkBody(machine, args, count, 0);
	if (body != nullptr && body->body == nullptr)
	{
		lh::raise(machine, "Attempt to use destroyed body.");
		return nullptr;
	}
	return body;
}

#define BODY_SELF() Body *b = self(machine, args, count); if (b == nullptr) return

#define BODY_NUMBER(name, expr) \
	static void lh_Body_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                           LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		BODY_SELF(); \
		answers[0] = lhat_real(expr); \
		*answerCount = 1; \
	}

#define BODY_BOOL(name, expr) \
	static void lh_Body_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                           LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		BODY_SELF(); \
		answers[0] = lhat_bool(expr); \
		*answerCount = 1; \
	}

#define BODY_PAIR(name, call) \
	static void lh_Body_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                           LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		BODY_SELF(); \
		float out[2] = {0, 0}; \
		b->call; \
		answers[0] = lhat_real(out[0]); \
		answers[1] = lhat_real(out[1]); \
		*answerCount = 2; \
	}

#define BODY_SET_NUMBER(name, call) \
	static void lh_Body_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                           LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		(void) answers; \
		(void) answerCount; \
		BODY_SELF(); \
		float v = numberAt(args, count, 1); \
		lh::guard(machine, [&]() { b->call; }); \
	}

#define BODY_SET_BOOL(name, call) \
	static void lh_Body_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                           LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		(void) answers; \
		(void) answerCount; \
		BODY_SELF(); \
		bool v = boolAt(args, count, 1); \
		b->call; \
	}

BODY_NUMBER(getX, b->getX())
BODY_NUMBER(getY, b->getY())
BODY_NUMBER(getAngle, b->getAngle())
BODY_PAIR(getPosition, getPosition(out[0], out[1]))
BODY_PAIR(getLinearVelocity, getLinearVelocity(out[0], out[1]))
BODY_PAIR(getWorldCenter, getWorldCenter(out[0], out[1]))
BODY_PAIR(getLocalCenter, getLocalCenter(out[0], out[1]))
BODY_NUMBER(getAngularVelocity, b->getAngularVelocity())
BODY_NUMBER(getMass, b->getMass())
BODY_NUMBER(getInertia, b->getInertia())
BODY_BOOL(hasCustomMassData, b->hasCustomMassData())
BODY_NUMBER(getAngularDamping, b->getAngularDamping())
BODY_NUMBER(getLinearDamping, b->getLinearDamping())
BODY_NUMBER(getGravityScale, b->getGravityScale())
BODY_SET_NUMBER(setX, setX(v))
BODY_SET_NUMBER(setY, setY(v))
BODY_SET_NUMBER(setAngle, setAngle(v))
BODY_SET_NUMBER(setAngularVelocity, setAngularVelocity(v))
BODY_SET_NUMBER(setMass, setMass(v))
BODY_SET_NUMBER(setInertia, setInertia(v))
BODY_SET_NUMBER(setAngularDamping, setAngularDamping(v))
BODY_SET_NUMBER(setLinearDamping, setLinearDamping(v))
BODY_SET_NUMBER(setGravityScale, setGravityScale(v))
BODY_BOOL(isBullet, b->isBullet())
BODY_SET_BOOL(setBullet, setBullet(v))
BODY_BOOL(isActive, b->isEnabled())
BODY_SET_BOOL(setActive, setEnabled(v))
BODY_BOOL(isAwake, b->isAwake())
BODY_SET_BOOL(setAwake, setAwake(v))
BODY_BOOL(isSleepingAllowed, b->isSleepingAllowed())
BODY_SET_BOOL(setSleepingAllowed, setSleepingAllowed(v))
BODY_BOOL(isFixedRotation, b->isFixedRotation())
BODY_SET_BOOL(setFixedRotation, setFixedRotation(v))

static void lh_Body_getTransform(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	float out[3];
	b->getPosition(out[0], out[1]);
	out[2] = b->getAngle();
	numbers(out, 3, answers, answerCount);
}

static void lh_Body_setTransform(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		b->setPosition(numberAt(args, count, 1), numberAt(args, count, 2));
		b->setAngle(numberAt(args, count, 3));
	});
}

static void lh_Body_getKinematicState(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	b2Vec2 pos, vel;
	float a, da;
	b->getKinematicState(pos, a, vel, da);
	float out[6] = {pos.x, pos.y, a, vel.x, vel.y, da};
	numbers(out, 6, answers, answerCount);
}

static void lh_Body_setKinematicState(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		b->setKinematicState(b2Vec2(numberAt(args, count, 1), numberAt(args, count, 2)), numberAt(args, count, 3),
		                     b2Vec2(numberAt(args, count, 4), numberAt(args, count, 5)), numberAt(args, count, 6));
	});
}

static void lh_Body_getMassData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	float out[4];
	b->getMassData(out[0], out[1], out[2], out[3]);
	numbers(out, 4, answers, answerCount);
}

static void lh_Body_setMassData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		b->setMassData(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4));
	});
}

static void lh_Body_resetMassData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		b->resetMassData();
	});
}

static void lh_Body_getType(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	const char *type = nullptr;
	Body::getConstant(b->getType(), type);
	LhatValue out = lhat_nil();
	lh::makeString(machine, type != nullptr ? type : "", &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_Body_setType(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	std::string name = lh::optString(args, count, 1, "");
	Body::Type type;
	if (!Body::getConstant(name.c_str(), type))
	{
		lh::raise(machine, "Invalid Body type: " + name);
		return;
	}
	lh::guard(machine, [&]() {
		b->setType(type);
	});
}

// applyLinearImpulse(jx, jy[, wake]) / applyLinearImpulse(jx, jy, rx, ry[, wake])
static void lh_Body_applyLinearImpulse(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	float jx = numberAt(args, count, 1), jy = numberAt(args, count, 2);
	if (count >= 5)
		b->applyLinearImpulse(jx, jy, numberAt(args, count, 3), numberAt(args, count, 4), boolAt(args, count, 5, true));
	else
		b->applyLinearImpulse(jx, jy, boolAt(args, count, 3, true));
}

static void lh_Body_applyAngularImpulse(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	b->applyAngularImpulse(numberAt(args, count, 1), boolAt(args, count, 2, true));
}

static void lh_Body_applyTorque(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	b->applyTorque(numberAt(args, count, 1), boolAt(args, count, 2, true));
}

// applyForce(fx, fy[, wake]) / applyForce(fx, fy, rx, ry[, wake])
static void lh_Body_applyForce(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	float fx = numberAt(args, count, 1), fy = numberAt(args, count, 2);
	if (count >= 5)
		b->applyForce(fx, fy, numberAt(args, count, 3), numberAt(args, count, 4), boolAt(args, count, 5, true));
	else
		b->applyForce(fx, fy, boolAt(args, count, 3, true));
}

static void lh_Body_setLinearVelocity(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	b->setLinearVelocity(numberAt(args, count, 1), numberAt(args, count, 2));
}

static void lh_Body_setPosition(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		b->setPosition(numberAt(args, count, 1), numberAt(args, count, 2));
	});
}

// The (x, y) -> (x, y) transforms.
#define BODY_XY(name, call) \
	static void lh_Body_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                           LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		BODY_SELF(); \
		float x = numberAt(args, count, 1), y = numberAt(args, count, 2); \
		float out[2] = {0, 0}; \
		b->call; \
		answers[0] = lhat_real(out[0]); \
		answers[1] = lhat_real(out[1]); \
		*answerCount = 2; \
	}

BODY_XY(getWorldPoint, getWorldPoint(x, y, out[0], out[1]))
BODY_XY(getWorldVector, getWorldVector(x, y, out[0], out[1]))
BODY_XY(getLocalPoint, getLocalPoint(x, y, out[0], out[1]))
BODY_XY(getLocalVector, getLocalVector(x, y, out[0], out[1]))
BODY_XY(getLinearVelocityFromWorldPoint, getLinearVelocityFromWorldPoint(x, y, out[0], out[1]))
BODY_XY(getLinearVelocityFromLocalPoint, getLinearVelocityFromLocalPoint(x, y, out[0], out[1]))

// getWorldPoints(x1, y1, x2, y2, ...) -> t^{number^[]}: the same pairs transformed.
static void lh_Body_getWorldPoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 1, coords))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::vector<float> points;
	for (const Vector2 &v : coords)
	{
		points.push_back(v.x);
		points.push_back(v.y);
	}
	b->getWorldPoints(points);
	answers[0] = numberList(machine, points);
	*answerCount = 1;
}

static void lh_Body_getLocalPoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	std::vector<Vector2> coords;
	if (!coordsOf(machine, args, count, 1, coords))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::vector<float> points;
	for (const Vector2 &v : coords)
	{
		points.push_back(v.x);
		points.push_back(v.y);
	}
	b->getLocalPoints(points);
	answers[0] = numberList(machine, points);
	*answerCount = 1;
}

static void lh_Body_isTouching(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	Body *other = checkBody(machine, args, count, 1);
	if (other == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_bool(b->isTouching(other));
	*answerCount = 1;
}

static void lh_Body_getWorld(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	answers[0] = pushWorld(machine, b->getWorld());
	*answerCount = 1;
}

static void lh_Body_getShape(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	Shape *shape = b->getShape();
	answers[0] = shape != nullptr ? pushShape(machine, shape) : lhat_nil();
	*answerCount = 1;
}

static void lh_Body_getShapes(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		std::vector<Shape *> shapes = b->getShapes();
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

static void lh_Body_getJoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		std::vector<Joint *> joints = b->getJoints();
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

static void lh_Body_getContacts(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() { answers[0] = objectList(machine, b->getContacts(), true); *answerCount = 1; });
}

static void lh_Body_destroy(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::guard(machine, [&]() {
		b->destroy();
	});
}

static void lh_Body_isDestroyed(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Body *b = checkBody(machine, args, count, 0);
	if (b == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_bool(b->body == nullptr);
	*answerCount = 1;
}

static void lh_Body_setUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	if (count < 2 || lhat_is_nil(args[1]))
		b->setUserData(nullptr);
	else
	{
		StrongRef<lh::Parked> parked(new lh::Parked(lh::ParkingLot::lotOf(machine), args[1]), Acquire::NORETAIN);
		b->setUserData(parked.get());
	}
}

static void lh_Body_getUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	BODY_SELF();
	lh::Parked *parked = dynamic_cast<lh::Parked *>(b->getUserData());
	answers[0] = parked != nullptr ? parked->get() : lhat_nil();
	*answerCount = 1;
}

bool lhPhysicsBody(lh::Context &ctx)
{
	const char *m = LH_PHYSICS;
	if (!ctx.objectType(m, "Body", Body::type))
		return false;
	if (ctx.types())
		return true;

	const char *B = "Body";
	return ctx.member(m, B, "getX", "f^self^ -> number^;", lh_Body_getX, nullptr)
		&& ctx.member(m, B, "getY", "f^self^ -> number^;", lh_Body_getY, nullptr)
		&& ctx.member(m, B, "getAngle", "f^self^ -> number^;", lh_Body_getAngle, nullptr)
		&& ctx.member(m, B, "getPosition", "f^self^ -> (number^, number^);", lh_Body_getPosition, nullptr)
		&& ctx.member(m, B, "getTransform", "f^self^ -> (number^, number^, number^);", lh_Body_getTransform, nullptr)
		&& ctx.member(m, B, "setTransform", "p^self^, number^, number^, number^;", lh_Body_setTransform, nullptr)
		&& ctx.member(m, B, "getLinearVelocity", "f^self^ -> (number^, number^);", lh_Body_getLinearVelocity, nullptr)
		&& ctx.member(m, B, "getWorldCenter", "f^self^ -> (number^, number^);", lh_Body_getWorldCenter, nullptr)
		&& ctx.member(m, B, "getLocalCenter", "f^self^ -> (number^, number^);", lh_Body_getLocalCenter, nullptr)
		&& ctx.member(m, B, "getAngularVelocity", "f^self^ -> number^;", lh_Body_getAngularVelocity, nullptr)
		&& ctx.member(m, B, "getKinematicState", "f^self^ -> (number^, number^, number^, number^, number^, number^);", lh_Body_getKinematicState, nullptr)
		&& ctx.member(m, B, "getMass", "f^self^ -> number^;", lh_Body_getMass, nullptr)
		&& ctx.member(m, B, "getInertia", "f^self^ -> number^;", lh_Body_getInertia, nullptr)
		&& ctx.member(m, B, "getMassData", "f^self^ -> (number^, number^, number^, number^);", lh_Body_getMassData, nullptr)
		&& ctx.member(m, B, "hasCustomMassData", "f^self^ -> bool^;", lh_Body_hasCustomMassData, nullptr)
		&& ctx.member(m, B, "getAngularDamping", "f^self^ -> number^;", lh_Body_getAngularDamping, nullptr)
		&& ctx.member(m, B, "getLinearDamping", "f^self^ -> number^;", lh_Body_getLinearDamping, nullptr)
		&& ctx.member(m, B, "getGravityScale", "f^self^ -> number^;", lh_Body_getGravityScale, nullptr)
		&& ctx.member(m, B, "getType", "f^self^ -> string^;", lh_Body_getType, nullptr)
		&& ctx.member(m, B, "applyLinearImpulse", "p^self^, number^, number^;", lh_Body_applyLinearImpulse, nullptr)
		&& ctx.member(m, B, "applyLinearImpulse", "p^self^, number^, number^, bool^;", lh_Body_applyLinearImpulse, nullptr)
		&& ctx.member(m, B, "applyLinearImpulse", "p^self^, number^, number^, number^, number^;", lh_Body_applyLinearImpulse, nullptr)
		&& ctx.member(m, B, "applyLinearImpulse", "p^self^, number^, number^, number^, number^, bool^;", lh_Body_applyLinearImpulse, nullptr)
		&& ctx.member(m, B, "applyAngularImpulse", "p^self^, number^;", lh_Body_applyAngularImpulse, nullptr)
		&& ctx.member(m, B, "applyAngularImpulse", "p^self^, number^, bool^;", lh_Body_applyAngularImpulse, nullptr)
		&& ctx.member(m, B, "applyTorque", "p^self^, number^;", lh_Body_applyTorque, nullptr)
		&& ctx.member(m, B, "applyTorque", "p^self^, number^, bool^;", lh_Body_applyTorque, nullptr)
		&& ctx.member(m, B, "applyForce", "p^self^, number^, number^;", lh_Body_applyForce, nullptr)
		&& ctx.member(m, B, "applyForce", "p^self^, number^, number^, bool^;", lh_Body_applyForce, nullptr)
		&& ctx.member(m, B, "applyForce", "p^self^, number^, number^, number^, number^;", lh_Body_applyForce, nullptr)
		&& ctx.member(m, B, "applyForce", "p^self^, number^, number^, number^, number^, bool^;", lh_Body_applyForce, nullptr)
		&& ctx.member(m, B, "setX", "p^self^, number^;", lh_Body_setX, nullptr)
		&& ctx.member(m, B, "setY", "p^self^, number^;", lh_Body_setY, nullptr)
		&& ctx.member(m, B, "setLinearVelocity", "p^self^, number^, number^;", lh_Body_setLinearVelocity, nullptr)
		&& ctx.member(m, B, "setAngle", "p^self^, number^;", lh_Body_setAngle, nullptr)
		&& ctx.member(m, B, "setAngularVelocity", "p^self^, number^;", lh_Body_setAngularVelocity, nullptr)
		&& ctx.member(m, B, "setPosition", "p^self^, number^, number^;", lh_Body_setPosition, nullptr)
		&& ctx.member(m, B, "setKinematicState", "p^self^, number^, number^, number^, number^, number^, number^;", lh_Body_setKinematicState, nullptr)
		&& ctx.member(m, B, "resetMassData", "p^self^;", lh_Body_resetMassData, nullptr)
		&& ctx.member(m, B, "setMassData", "p^self^, number^, number^, number^, number^;", lh_Body_setMassData, nullptr)
		&& ctx.member(m, B, "setMass", "p^self^, number^;", lh_Body_setMass, nullptr)
		&& ctx.member(m, B, "setInertia", "p^self^, number^;", lh_Body_setInertia, nullptr)
		&& ctx.member(m, B, "setAngularDamping", "p^self^, number^;", lh_Body_setAngularDamping, nullptr)
		&& ctx.member(m, B, "setLinearDamping", "p^self^, number^;", lh_Body_setLinearDamping, nullptr)
		&& ctx.member(m, B, "setGravityScale", "p^self^, number^;", lh_Body_setGravityScale, nullptr)
		&& ctx.member(m, B, "setType", "p^self^, string^;", lh_Body_setType, nullptr)
		&& ctx.member(m, B, "getWorldPoint", "f^self^, number^, number^ -> (number^, number^);", lh_Body_getWorldPoint, nullptr)
		&& ctx.member(m, B, "getWorldVector", "f^self^, number^, number^ -> (number^, number^);", lh_Body_getWorldVector, nullptr)
		&& ctx.member(m, B, "getWorldPoints", "f^self^, ... -> t^{number^[]};", lh_Body_getWorldPoints, nullptr)
		&& ctx.member(m, B, "getLocalPoint", "f^self^, number^, number^ -> (number^, number^);", lh_Body_getLocalPoint, nullptr)
		&& ctx.member(m, B, "getLocalVector", "f^self^, number^, number^ -> (number^, number^);", lh_Body_getLocalVector, nullptr)
		&& ctx.member(m, B, "getLocalPoints", "f^self^, ... -> t^{number^[]};", lh_Body_getLocalPoints, nullptr)
		&& ctx.member(m, B, "getLinearVelocityFromWorldPoint", "f^self^, number^, number^ -> (number^, number^);", lh_Body_getLinearVelocityFromWorldPoint, nullptr)
		&& ctx.member(m, B, "getLinearVelocityFromLocalPoint", "f^self^, number^, number^ -> (number^, number^);", lh_Body_getLinearVelocityFromLocalPoint, nullptr)
		&& ctx.member(m, B, "isBullet", "f^self^ -> bool^;", lh_Body_isBullet, nullptr)
		&& ctx.member(m, B, "setBullet", "p^self^, bool^;", lh_Body_setBullet, nullptr)
		&& ctx.member(m, B, "isActive", "f^self^ -> bool^;", lh_Body_isActive, nullptr)
		&& ctx.member(m, B, "setActive", "p^self^, bool^;", lh_Body_setActive, nullptr)
		&& ctx.member(m, B, "isAwake", "f^self^ -> bool^;", lh_Body_isAwake, nullptr)
		&& ctx.member(m, B, "setAwake", "p^self^, bool^;", lh_Body_setAwake, nullptr)
		&& ctx.member(m, B, "isSleepingAllowed", "f^self^ -> bool^;", lh_Body_isSleepingAllowed, nullptr)
		&& ctx.member(m, B, "setSleepingAllowed", "p^self^, bool^;", lh_Body_setSleepingAllowed, nullptr)
		&& ctx.member(m, B, "isFixedRotation", "f^self^ -> bool^;", lh_Body_isFixedRotation, nullptr)
		&& ctx.member(m, B, "setFixedRotation", "p^self^, bool^;", lh_Body_setFixedRotation, nullptr)
		&& ctx.member(m, B, "isTouching", "f^self^, love.physics.Body -> bool^;", lh_Body_isTouching, nullptr)
		&& ctx.member(m, B, "getWorld", "p^self^ -> love.physics.World;", lh_Body_getWorld, nullptr)
		&& ctx.member(m, B, "getShape", "p^self^ -> love.physics.Shape|nil^;", lh_Body_getShape, nullptr)
		&& ctx.member(m, B, "getShapes", "p^self^ -> t^{love.physics.Shape[]};", lh_Body_getShapes, nullptr)
		&& ctx.member(m, B, "getJoints", "p^self^ -> t^{love.physics.Joint[]};", lh_Body_getJoints, nullptr)
		&& ctx.member(m, B, "getContacts", "p^self^ -> t^{love.physics.Contact[]};", lh_Body_getContacts, nullptr)
		&& ctx.member(m, B, "destroy", "p^self^;", lh_Body_destroy, nullptr)
		&& ctx.member(m, B, "isDestroyed", "f^self^ -> bool^;", lh_Body_isDestroyed, nullptr)
		&& ctx.member(m, B, "setUserData", "p^self^, any^;", lh_Body_setUserData, nullptr)
		&& ctx.member(m, B, "getUserData", "f^self^ -> any^;", lh_Body_getUserData, nullptr);
}

} // box2d
} // physics
} // love

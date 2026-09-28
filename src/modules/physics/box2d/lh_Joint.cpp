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

// love.physics.Joint for L^: one hostdata type for every joint kind. The
// references are wrap_Joint.cpp and the wrap_*Joint.cpp files beside this
// file. A member a kind does not have raises when called on it; getType
// tells the kinds apart. Members several kinds share (setMotorSpeed,
// setStiffness, getLimits ...) dispatch on the kind.

#include "lh_Physics.h"

#include "DistanceJoint.h"
#include "FrictionJoint.h"
#include "GearJoint.h"
#include "MotorJoint.h"
#include "MouseJoint.h"
#include "PrismaticJoint.h"
#include "PulleyJoint.h"
#include "RevoluteJoint.h"
#include "RopeJoint.h"
#include "WeldJoint.h"
#include "WheelJoint.h"

#include <initializer_list>

namespace love
{
namespace physics
{
namespace box2d
{

static Joint *self(LhatMachine *machine, const LhatValue *args, size_t count)
{
	Joint *joint = checkJoint(machine, args, count, 0);
	if (joint != nullptr && !joint->isValid())
	{
		lh::raise(machine, "Attempt to use destroyed joint.");
		return nullptr;
	}
	return joint;
}

#define JOINT_SELF() Joint *j = self(machine, args, count); if (j == nullptr) return

static void notAMember(LhatMachine *machine, Joint *j, const char *name)
{
	const char *have = "";
	Joint::getConstant(j->getType(), have);
	lh::raise(machine, std::string(name) + " is not a member of a " + have + " joint.");
}

// The kind-dispatched accessors. Each case names the joint kind and the
// expression on `t`, the joint cast to that kind. 8.8改 put every kind
// behind a type of its own, so the default arm is the last net under the
// checker rather than the thing that catches the mistake.
#define KIND(Kind, Type, expr) case Joint::Type: { Kind *t = (Kind *) j; (void) t; answers[0] = expr; *answerCount = 1; return; }

#define JOINT_GET_NUMBER(name, cases) \
	static void lh_Joint_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                            LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		JOINT_SELF(); \
		float v = numberAt(args, count, 1); \
		(void) v; \
		lh::guard(machine, [&]() { \
			switch (j->getType()) \
			{ \
			cases \
			default: notAMember(machine, j, #name); return; \
			} \
		}); \
	}

#define JOINT_SET(name, cases) \
	static void lh_Joint_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                            LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		(void) answers; \
		(void) answerCount; \
		JOINT_SELF(); \
		float v = numberAt(args, count, 1); \
		float v2 = numberAt(args, count, 2); \
		bool flag = boolAt(args, count, 1); \
		(void) v; (void) v2; (void) flag; \
		lh::guard(machine, [&]() { \
			switch (j->getType()) \
			{ \
			cases \
			default: notAMember(machine, j, #name); return; \
			} \
		}); \
	}

#define JOINT_PAIR(name, cases) \
	static void lh_Joint_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                            LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		JOINT_SELF(); \
		lh::guard(machine, [&]() { \
			float out[2] = {0, 0}; \
			switch (j->getType()) \
			{ \
			cases \
			default: notAMember(machine, j, #name); return; \
			} \
			answers[0] = lhat_real(out[0]); \
			answers[1] = lhat_real(out[1]); \
			*answerCount = 2; \
		}); \
	}

#define PAIR(Kind, Type, call) case Joint::Type: { Kind *t = (Kind *) j; t->call; break; }
#define SET(Kind, Type, call) case Joint::Type: { Kind *t = (Kind *) j; t->call; return; }

// --- the base members ---

static void lh_Joint_getType(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	const char *type = "";
	Joint::getConstant(j->getType(), type);
	LhatValue out = lhat_nil();
	out = lh::pushEnum(machine, "love.physics", "JointType", type);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_Joint_getBodies(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	lh::guard(machine, [&]() {
		answers[0] = pushBody(machine, j->getBodyA());
		answers[1] = pushBody(machine, j->getBodyB());
		*answerCount = 2;
	});
}

static void lh_Joint_getAnchors(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	float out[4];
	j->getAnchors(out[0], out[1], out[2], out[3]);
	numbers(out, 4, answers, answerCount);
}

static void lh_Joint_getReactionForce(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	float out[2];
	j->getReactionForce(numberAt(args, count, 1), out[0], out[1]);
	numbers(out, 2, answers, answerCount);
}

static void lh_Joint_getReactionTorque(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	answers[0] = lhat_real(j->getReactionTorque(numberAt(args, count, 1)));
	*answerCount = 1;
}

static void lh_Joint_getCollideConnected(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										 LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	answers[0] = lhat_bool(j->getCollideConnected());
	*answerCount = 1;
}

static void lh_Joint_setUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	if (count < 2 || lhat_is_nil(args[1]))
		j->setUserData(nullptr);
	else
	{
		StrongRef<lh::Parked> parked(new lh::Parked(lh::ParkingLot::lotOf(machine), args[1]), Acquire::NORETAIN);
		j->setUserData(parked.get());
	}
}

static void lh_Joint_getUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	lh::Parked *parked = dynamic_cast<lh::Parked *>(j->getUserData());
	answers[0] = parked != nullptr ? parked->get() : lhat_nil();
	*answerCount = 1;
}

static void lh_Joint_destroy(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	lh::guard(machine, [&]() {
		j->destroyJoint();
	});
}

static void lh_Joint_isDestroyed(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Joint *j = checkJoint(machine, args, count, 0);
	if (j == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_bool(!j->isValid());
	*answerCount = 1;
}

// --- members several kinds share, dispatched on the kind ---

JOINT_GET_NUMBER(getLength,
	KIND(DistanceJoint, JOINT_DISTANCE, lhat_real(t->getLength())))
JOINT_SET(setLength,
	SET(DistanceJoint, JOINT_DISTANCE, setLength(v)))

JOINT_GET_NUMBER(getStiffness,
	KIND(DistanceJoint, JOINT_DISTANCE, lhat_real(t->getStiffness()))
	KIND(MouseJoint, JOINT_MOUSE, lhat_real(t->getStiffness()))
	KIND(WeldJoint, JOINT_WELD, lhat_real(t->getStiffness()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getStiffness())))
JOINT_SET(setStiffness,
	SET(DistanceJoint, JOINT_DISTANCE, setStiffness(v))
	SET(MouseJoint, JOINT_MOUSE, setStiffness(v))
	SET(WeldJoint, JOINT_WELD, setStiffness(v))
	SET(WheelJoint, JOINT_WHEEL, setStiffness(v)))

JOINT_GET_NUMBER(getDamping,
	KIND(DistanceJoint, JOINT_DISTANCE, lhat_real(t->getDamping()))
	KIND(MouseJoint, JOINT_MOUSE, lhat_real(t->getDamping()))
	KIND(WeldJoint, JOINT_WELD, lhat_real(t->getDamping()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getDamping())))
JOINT_SET(setDamping,
	SET(DistanceJoint, JOINT_DISTANCE, setDamping(v))
	SET(MouseJoint, JOINT_MOUSE, setDamping(v))
	SET(WeldJoint, JOINT_WELD, setDamping(v))
	SET(WheelJoint, JOINT_WHEEL, setDamping(v)))

JOINT_GET_NUMBER(getMaxForce,
	KIND(FrictionJoint, JOINT_FRICTION, lhat_real(t->getMaxForce()))
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getMaxForce()))
	KIND(MouseJoint, JOINT_MOUSE, lhat_real(t->getMaxForce())))
JOINT_SET(setMaxForce,
	SET(FrictionJoint, JOINT_FRICTION, setMaxForce(v))
	SET(MotorJoint, JOINT_MOTOR, setMaxForce(v))
	SET(MouseJoint, JOINT_MOUSE, setMaxForce(v)))

JOINT_GET_NUMBER(getMaxTorque,
	KIND(FrictionJoint, JOINT_FRICTION, lhat_real(t->getMaxTorque()))
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getMaxTorque())))
JOINT_SET(setMaxTorque,
	SET(FrictionJoint, JOINT_FRICTION, setMaxTorque(v))
	SET(MotorJoint, JOINT_MOTOR, setMaxTorque(v)))

JOINT_GET_NUMBER(getRatio,
	KIND(GearJoint, JOINT_GEAR, lhat_real(t->getRatio()))
	KIND(PulleyJoint, JOINT_PULLEY, lhat_real(t->getRatio())))
JOINT_SET(setRatio,
	SET(GearJoint, JOINT_GEAR, setRatio(v)))

static void lh_Joint_getJoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	if (j->getType() != Joint::JOINT_GEAR)
	{
		notAMember(machine, j, "getJoints");
		return;
	}
	GearJoint *t = (GearJoint *) j;
	lh::guard(machine, [&]() {
		answers[0] = pushJoint(machine, t->getJointA());
		answers[1] = pushJoint(machine, t->getJointB());
		*answerCount = 2;
	});
}

JOINT_PAIR(getLinearOffset,
	PAIR(MotorJoint, JOINT_MOTOR, getLinearOffset(out[0], out[1])))
JOINT_SET(setLinearOffset,
	SET(MotorJoint, JOINT_MOTOR, setLinearOffset(v, v2)))
JOINT_GET_NUMBER(getAngularOffset,
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getAngularOffset())))
JOINT_SET(setAngularOffset,
	SET(MotorJoint, JOINT_MOTOR, setAngularOffset(v)))
JOINT_GET_NUMBER(getCorrectionFactor,
	KIND(MotorJoint, JOINT_MOTOR, lhat_real(t->getCorrectionFactor())))
JOINT_SET(setCorrectionFactor,
	SET(MotorJoint, JOINT_MOTOR, setCorrectionFactor(v)))

JOINT_PAIR(getTarget,
	PAIR(MouseJoint, JOINT_MOUSE, getTarget(out[0], out[1])))
JOINT_SET(setTarget,
	SET(MouseJoint, JOINT_MOUSE, setTarget(v, v2)))

JOINT_GET_NUMBER(getJointTranslation,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getJointTranslation()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getJointTranslation())))
JOINT_GET_NUMBER(getJointSpeed,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getJointSpeed()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getJointSpeed()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getJointSpeed())))
JOINT_GET_NUMBER(getJointAngle,
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getJointAngle())))

JOINT_SET(setMotorEnabled,
	SET(PrismaticJoint, JOINT_PRISMATIC, setMotorEnabled(flag))
	SET(RevoluteJoint, JOINT_REVOLUTE, setMotorEnabled(flag))
	SET(WheelJoint, JOINT_WHEEL, setMotorEnabled(flag)))
JOINT_GET_NUMBER(isMotorEnabled,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_bool(t->isMotorEnabled()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_bool(t->isMotorEnabled()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_bool(t->isMotorEnabled())))
JOINT_SET(setMotorSpeed,
	SET(PrismaticJoint, JOINT_PRISMATIC, setMotorSpeed(v))
	SET(RevoluteJoint, JOINT_REVOLUTE, setMotorSpeed(v))
	SET(WheelJoint, JOINT_WHEEL, setMotorSpeed(v)))
JOINT_GET_NUMBER(getMotorSpeed,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getMotorSpeed()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getMotorSpeed()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getMotorSpeed())))

JOINT_SET(setMaxMotorForce,
	SET(PrismaticJoint, JOINT_PRISMATIC, setMaxMotorForce(v)))
JOINT_GET_NUMBER(getMaxMotorForce,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getMaxMotorForce())))
JOINT_GET_NUMBER(getMotorForce,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getMotorForce(v))))

JOINT_SET(setMaxMotorTorque,
	SET(RevoluteJoint, JOINT_REVOLUTE, setMaxMotorTorque(v))
	SET(WheelJoint, JOINT_WHEEL, setMaxMotorTorque(v)))
JOINT_GET_NUMBER(getMaxMotorTorque,
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getMaxMotorTorque()))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getMaxMotorTorque())))
JOINT_GET_NUMBER(getMotorTorque,
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getMotorTorque(v)))
	KIND(WheelJoint, JOINT_WHEEL, lhat_real(t->getMotorTorque(v))))

JOINT_SET(setLimitsEnabled,
	SET(PrismaticJoint, JOINT_PRISMATIC, setLimitsEnabled(flag))
	SET(RevoluteJoint, JOINT_REVOLUTE, setLimitsEnabled(flag)))
JOINT_GET_NUMBER(areLimitsEnabled,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_bool(t->areLimitsEnabled()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_bool(t->areLimitsEnabled())))
JOINT_SET(setUpperLimit,
	SET(PrismaticJoint, JOINT_PRISMATIC, setUpperLimit(v))
	SET(RevoluteJoint, JOINT_REVOLUTE, setUpperLimit(v)))
JOINT_SET(setLowerLimit,
	SET(PrismaticJoint, JOINT_PRISMATIC, setLowerLimit(v))
	SET(RevoluteJoint, JOINT_REVOLUTE, setLowerLimit(v)))
JOINT_SET(setLimits,
	SET(PrismaticJoint, JOINT_PRISMATIC, setLimits(v, v2))
	SET(RevoluteJoint, JOINT_REVOLUTE, setLimits(v, v2)))
JOINT_GET_NUMBER(getLowerLimit,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getLowerLimit()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getLowerLimit())))
JOINT_GET_NUMBER(getUpperLimit,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getUpperLimit()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getUpperLimit())))
JOINT_PAIR(getLimits,
	PAIR(PrismaticJoint, JOINT_PRISMATIC, getLimits(out[0], out[1]))
	PAIR(RevoluteJoint, JOINT_REVOLUTE, getLimits(out[0], out[1])))

JOINT_PAIR(getAxis,
	PAIR(PrismaticJoint, JOINT_PRISMATIC, getAxis(out[0], out[1]))
	PAIR(WheelJoint, JOINT_WHEEL, getAxis(out[0], out[1])))

JOINT_GET_NUMBER(getReferenceAngle,
	KIND(PrismaticJoint, JOINT_PRISMATIC, lhat_real(t->getReferenceAngle()))
	KIND(RevoluteJoint, JOINT_REVOLUTE, lhat_real(t->getReferenceAngle()))
	KIND(WeldJoint, JOINT_WELD, lhat_real(t->getReferenceAngle())))

static void lh_Joint_getGroundAnchors(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	JOINT_SELF();
	if (j->getType() != Joint::JOINT_PULLEY)
	{
		notAMember(machine, j, "getGroundAnchors");
		return;
	}
	float out[4];
	((PulleyJoint *) j)->getGroundAnchors(out[0], out[1], out[2], out[3]);
	numbers(out, 4, answers, answerCount);
}

JOINT_GET_NUMBER(getLengthA,
	KIND(PulleyJoint, JOINT_PULLEY, lhat_real(t->getLengthA())))
JOINT_GET_NUMBER(getLengthB,
	KIND(PulleyJoint, JOINT_PULLEY, lhat_real(t->getLengthB())))

JOINT_GET_NUMBER(getMaxLength,
	KIND(RopeJoint, JOINT_ROPE, lhat_real(t->getMaxLength())))
JOINT_SET(setMaxLength,
	SET(RopeJoint, JOINT_ROPE, setMaxLength(v)))

// One member registered on each kind that has it. box2d shares members
// between kinds without sharing a class -- stiffness belongs to a distance,
// a mouse, a weld and a wheel joint, which have only Joint above them -- so
// what the C++ tree cannot say once is said here once per kind.
static bool jointMember(lh::Context &ctx, std::initializer_list<const char *> kinds,
                        const char *name, const char *signature, LhatHostFn fn)
{
	for (const char *kind : kinds)
	{
		if (!ctx.member(LH_PHYSICS, kind, name, signature, fn, nullptr))
			return false;
	}
	return true;
}

bool lhPhysicsJoint(lh::Context &ctx)
{
	const char *m = LH_PHYSICS;
	// 05 の 8.8改: box2d's eleven kinds are LOVE's eleven classes, so each is
	// a type here. A member of one kind is registered on that kind, and
	// reaching for it on another is a diagnostic rather than a panic.
	static const char *const kindNames[] = {
		"DistanceJoint", "FrictionJoint", "GearJoint", "MotorJoint", "MouseJoint",
		"PrismaticJoint", "PulleyJoint", "RevoluteJoint", "RopeJoint", "WeldJoint",
		"WheelJoint",
	};
	love::Type *const kindTypes[] = {
		&DistanceJoint::type, &FrictionJoint::type, &GearJoint::type, &MotorJoint::type,
		&MouseJoint::type, &PrismaticJoint::type, &PulleyJoint::type, &RevoluteJoint::type,
		&RopeJoint::type, &WeldJoint::type, &WheelJoint::type,
	};
	if (!ctx.objectType(m, "Joint", Joint::type))
		return false;
	for (size_t i = 0; i < sizeof(kindNames) / sizeof(*kindNames); i++)
	{
		if (!ctx.objectType(m, kindNames[i], *kindTypes[i], m, "Joint"))
			return false;
	}
	if (ctx.types())
		return true;

	const char *J = "Joint";
	const char *num = "f^self^ -> number^;";
	const char *flag = "f^self^ -> bool^;";
	const char *setNum = "p^self^, number^;";
	const char *setFlag = "p^self^, bool^;";
	const char *setPair = "p^self^, number^, number^;";
	const char *pair = "f^self^ -> (number^, number^);";
	const char *numOfNum = "f^self^, number^ -> number^;";
	const char *twoJoints = "p^self^ -> (love.physics.Joint, love.physics.Joint);";
	const char *four = "f^self^ -> (number^, number^, number^, number^);";

	// What every joint answers.
	bool ok = ctx.member(m, J, "getType", "f^self^ -> love.physics.JointType;", lh_Joint_getType, nullptr)
		&& ctx.member(m, J, "getBodies", "p^self^ -> (love.physics.Body, love.physics.Body);", lh_Joint_getBodies, nullptr)
		&& ctx.member(m, J, "getAnchors", four, lh_Joint_getAnchors, nullptr)
		&& ctx.member(m, J, "getReactionForce", "f^self^, number^ -> (number^, number^);", lh_Joint_getReactionForce, nullptr)
		&& ctx.member(m, J, "getReactionTorque", numOfNum, lh_Joint_getReactionTorque, nullptr)
		&& ctx.member(m, J, "getCollideConnected", flag, lh_Joint_getCollideConnected, nullptr)
		&& ctx.member(m, J, "setUserData", "p^self^, any^;", lh_Joint_setUserData, nullptr)
		&& ctx.member(m, J, "getUserData", "f^self^ -> any^;", lh_Joint_getUserData, nullptr)
		&& ctx.member(m, J, "destroy", "p^self^;", lh_Joint_destroy, nullptr)
		&& ctx.member(m, J, "isDestroyed", flag, lh_Joint_isDestroyed, nullptr);
	if (!ok)
		return false;

	// The groups box2d puts a member in, named for what they have in common.
	const std::initializer_list<const char *> springy = {"DistanceJoint", "MouseJoint", "WeldJoint", "WheelJoint"};
	const std::initializer_list<const char *> forceful = {"FrictionJoint", "MotorJoint", "MouseJoint"};
	const std::initializer_list<const char *> torquey = {"FrictionJoint", "MotorJoint"};
	const std::initializer_list<const char *> motored = {"PrismaticJoint", "RevoluteJoint", "WheelJoint"};
	const std::initializer_list<const char *> limited = {"PrismaticJoint", "RevoluteJoint"};
	const std::initializer_list<const char *> axial = {"PrismaticJoint", "WheelJoint"};
	const std::initializer_list<const char *> angled = {"PrismaticJoint", "RevoluteJoint", "WeldJoint"};
	const std::initializer_list<const char *> torqued = {"RevoluteJoint", "WheelJoint"};

	return jointMember(ctx, {"DistanceJoint"}, "getLength", num, lh_Joint_getLength)
		&& jointMember(ctx, {"DistanceJoint"}, "setLength", setNum, lh_Joint_setLength)
		&& jointMember(ctx, springy, "getStiffness", num, lh_Joint_getStiffness)
		&& jointMember(ctx, springy, "setStiffness", setNum, lh_Joint_setStiffness)
		&& jointMember(ctx, springy, "getDamping", num, lh_Joint_getDamping)
		&& jointMember(ctx, springy, "setDamping", setNum, lh_Joint_setDamping)
		&& jointMember(ctx, forceful, "getMaxForce", num, lh_Joint_getMaxForce)
		&& jointMember(ctx, forceful, "setMaxForce", setNum, lh_Joint_setMaxForce)
		&& jointMember(ctx, torquey, "getMaxTorque", num, lh_Joint_getMaxTorque)
		&& jointMember(ctx, torquey, "setMaxTorque", setNum, lh_Joint_setMaxTorque)
		&& jointMember(ctx, {"GearJoint", "PulleyJoint"}, "getRatio", num, lh_Joint_getRatio)
		&& jointMember(ctx, {"GearJoint"}, "setRatio", setNum, lh_Joint_setRatio)
		&& jointMember(ctx, {"GearJoint"}, "getJoints", twoJoints, lh_Joint_getJoints)
		&& jointMember(ctx, {"MotorJoint"}, "getLinearOffset", pair, lh_Joint_getLinearOffset)
		&& jointMember(ctx, {"MotorJoint"}, "setLinearOffset", setPair, lh_Joint_setLinearOffset)
		&& jointMember(ctx, {"MotorJoint"}, "getAngularOffset", num, lh_Joint_getAngularOffset)
		&& jointMember(ctx, {"MotorJoint"}, "setAngularOffset", setNum, lh_Joint_setAngularOffset)
		&& jointMember(ctx, {"MotorJoint"}, "getCorrectionFactor", num, lh_Joint_getCorrectionFactor)
		&& jointMember(ctx, {"MotorJoint"}, "setCorrectionFactor", setNum, lh_Joint_setCorrectionFactor)
		&& jointMember(ctx, {"MouseJoint"}, "getTarget", pair, lh_Joint_getTarget)
		&& jointMember(ctx, {"MouseJoint"}, "setTarget", setPair, lh_Joint_setTarget)
		&& jointMember(ctx, axial, "getJointTranslation", num, lh_Joint_getJointTranslation)
		&& jointMember(ctx, motored, "getJointSpeed", num, lh_Joint_getJointSpeed)
		&& jointMember(ctx, {"RevoluteJoint"}, "getJointAngle", num, lh_Joint_getJointAngle)
		&& jointMember(ctx, motored, "setMotorEnabled", setFlag, lh_Joint_setMotorEnabled)
		&& jointMember(ctx, motored, "isMotorEnabled", flag, lh_Joint_isMotorEnabled)
		&& jointMember(ctx, motored, "setMotorSpeed", setNum, lh_Joint_setMotorSpeed)
		&& jointMember(ctx, motored, "getMotorSpeed", num, lh_Joint_getMotorSpeed)
		&& jointMember(ctx, {"PrismaticJoint"}, "setMaxMotorForce", setNum, lh_Joint_setMaxMotorForce)
		&& jointMember(ctx, {"PrismaticJoint"}, "getMaxMotorForce", num, lh_Joint_getMaxMotorForce)
		&& jointMember(ctx, {"PrismaticJoint"}, "getMotorForce", numOfNum, lh_Joint_getMotorForce)
		&& jointMember(ctx, torqued, "setMaxMotorTorque", setNum, lh_Joint_setMaxMotorTorque)
		&& jointMember(ctx, torqued, "getMaxMotorTorque", num, lh_Joint_getMaxMotorTorque)
		&& jointMember(ctx, torqued, "getMotorTorque", numOfNum, lh_Joint_getMotorTorque)
		&& jointMember(ctx, limited, "setLimitsEnabled", setFlag, lh_Joint_setLimitsEnabled)
		&& jointMember(ctx, limited, "areLimitsEnabled", flag, lh_Joint_areLimitsEnabled)
		&& jointMember(ctx, limited, "setUpperLimit", setNum, lh_Joint_setUpperLimit)
		&& jointMember(ctx, limited, "setLowerLimit", setNum, lh_Joint_setLowerLimit)
		&& jointMember(ctx, limited, "setLimits", setPair, lh_Joint_setLimits)
		&& jointMember(ctx, limited, "getLowerLimit", num, lh_Joint_getLowerLimit)
		&& jointMember(ctx, limited, "getUpperLimit", num, lh_Joint_getUpperLimit)
		&& jointMember(ctx, limited, "getLimits", pair, lh_Joint_getLimits)
		&& jointMember(ctx, axial, "getAxis", pair, lh_Joint_getAxis)
		&& jointMember(ctx, angled, "getReferenceAngle", num, lh_Joint_getReferenceAngle)
		&& jointMember(ctx, {"PulleyJoint"}, "getGroundAnchors", four, lh_Joint_getGroundAnchors)
		&& jointMember(ctx, {"PulleyJoint"}, "getLengthA", num, lh_Joint_getLengthA)
		&& jointMember(ctx, {"PulleyJoint"}, "getLengthB", num, lh_Joint_getLengthB)
		&& jointMember(ctx, {"RopeJoint"}, "getMaxLength", num, lh_Joint_getMaxLength)
		&& jointMember(ctx, {"RopeJoint"}, "setMaxLength", setNum, lh_Joint_setMaxLength);
}

} // box2d
} // physics
} // love

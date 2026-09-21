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

// love.physics.Shape for L^: one hostdata type for every shape kind. The
// references are wrap_Shape.cpp, wrap_CircleShape.cpp, wrap_PolygonShape.cpp,
// wrap_EdgeShape.cpp and wrap_ChainShape.cpp beside this file. Members a
// kind does not have raise when called on it; getType tells the kinds apart.

#include "lh_Physics.h"

#include "CircleShape.h"
#include "PolygonShape.h"
#include "EdgeShape.h"
#include "ChainShape.h"

#include <bitset>

namespace love
{
namespace physics
{
namespace box2d
{

static Shape *self(LhatMachine *machine, const LhatValue *args, size_t count)
{
	return checkShape(machine, args, count, 0);
}

#define SHAPE_SELF() Shape *s = self(machine, args, count); if (s == nullptr) return

// 8.8改 put each kind behind a type of its own, so a member registered on
// CircleShape is reached only through one -- the checker turns the old
// mistake into a diagnostic. This stays as the last net under that: nothing
// verifies the promise a subtype declaration makes.
template <typename T>
static T *kindOf(LhatMachine *machine, Shape *s, Shape::Type type, const char *name)
{
	if (s->getType() != type)
	{
		const char *have = "";
		Shape::getConstant(s->getType(), have);
		lh::raise(machine, std::string(name) + " is not a member of a " + have + " shape.");
		return nullptr;
	}
	return (T *) s;
}

#define SHAPE_NUMBER(name, expr) \
	static void lh_Shape_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                            LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		SHAPE_SELF(); \
		lh::guard(machine, [&]() { answers[0] = lhat_real(expr); *answerCount = 1; }); \
	}

#define SHAPE_BOOL(name, expr) \
	static void lh_Shape_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                            LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		SHAPE_SELF(); \
		lh::guard(machine, [&]() { answers[0] = lhat_bool(expr); *answerCount = 1; }); \
	}

#define SHAPE_SET_NUMBER(name, call) \
	static void lh_Shape_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                            LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		(void) answers; \
		(void) answerCount; \
		SHAPE_SELF(); \
		float v = numberAt(args, count, 1); \
		lh::guard(machine, [&]() { s->call; }); \
	}

SHAPE_NUMBER(getRadius, s->getRadius())
SHAPE_NUMBER(getChildCount, (float) s->getChildCount())
SHAPE_SET_NUMBER(setFriction, setFriction(v))
SHAPE_SET_NUMBER(setRestitution, setRestitution(v))
SHAPE_SET_NUMBER(setDensity, setDensity(v))
SHAPE_NUMBER(getFriction, s->getFriction())
SHAPE_NUMBER(getRestitution, s->getRestitution())
SHAPE_NUMBER(getDensity, s->getDensity())
SHAPE_BOOL(isSensor, s->isSensor())
SHAPE_NUMBER(getGroupIndex, (float) s->getGroupIndex())

static void lh_Shape_getType(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	const char *type = "";
	Shape::getConstant(s->getType(), type);
	LhatValue out = lhat_nil();
	lh::makeString(machine, type, &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_Shape_setSensor(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	bool sensor = boolAt(args, count, 1);
	lh::guard(machine, [&]() {
		s->setSensor(sensor);
	});
}

static void lh_Shape_setGroupIndex(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	int index = (int) numberAt(args, count, 1);
	lh::guard(machine, [&]() {
		s->setGroupIndex(index);
	});
}

static void lh_Shape_getBody(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	Body *body = s->getBody();
	answers[0] = body != nullptr ? pushBody(machine, body) : lhat_nil();
	*answerCount = 1;
}

// testPoint(x, y) against the fixture; testPoint(x, y, r, px, py) against
// the bare shape placed at (x, y, r).
static void lh_Shape_testPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		if (count >= 6)
		{
			answers[0] = lhat_bool(s->testPoint(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), numberAt(args, count, 4), numberAt(args, count, 5)));
			*answerCount = 1;
			return;
		}
		answers[0] = lhat_bool(s->testPoint(numberAt(args, count, 1), numberAt(args, count, 2)));
		*answerCount = 1;
	});
}

// rayCast(x1, y1, x2, y2, maxFraction[, childIndex]) -> (hit, nx, ny, fraction)
// rayCast(x1, y1, x2, y2, maxFraction, x, y, r[, childIndex]) -> the same, against the bare shape
static void lh_Shape_rayCast(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	float x1 = numberAt(args, count, 1), y1 = numberAt(args, count, 2);
	float x2 = numberAt(args, count, 3), y2 = numberAt(args, count, 4);
	float maxFraction = numberAt(args, count, 5);
	lh::guard(machine, [&]() {
		float nx = 0, ny = 0, fraction = 0;
		bool hit = false;
		if (count >= 9)
		{
			int childIndex = (int) numberAt(args, count, 9, 0);
			hit = s->rayCast(x1, y1, x2, y2, maxFraction, numberAt(args, count, 6), numberAt(args, count, 7), numberAt(args, count, 8), childIndex, nx, ny, fraction);
		}
		else
		{
			int childIndex = (int) numberAt(args, count, 6, 0);
			hit = s->rayCast(x1, y1, x2, y2, maxFraction, childIndex, nx, ny, fraction);
		}
		answers[0] = lhat_bool(hit);
		answers[1] = lhat_real(nx);
		answers[2] = lhat_real(ny);
		answers[3] = lhat_real(fraction);
		*answerCount = 4;
	});
}

// computeAABB(x, y, r[, childIndex]) -> (lx, ly, ux, uy)
static void lh_Shape_computeAABB(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		float out[4];
		s->computeAABB(numberAt(args, count, 1), numberAt(args, count, 2), numberAt(args, count, 3), (int) numberAt(args, count, 4, 1) - 1, out[0], out[1], out[2], out[3]);
		numbers(out, 4, answers, answerCount);
	});
}

// computeMass(density) -> (x, y, mass, inertia)
static void lh_Shape_computeMass(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		float out[4];
		s->computeMass(numberAt(args, count, 1), out[0], out[1], out[2], out[3]);
		numbers(out, 4, answers, answerCount);
	});
}

static void lh_Shape_setFilterData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	int v[3] = {(int) numberAt(args, count, 1), (int) numberAt(args, count, 2), (int) numberAt(args, count, 3)};
	lh::guard(machine, [&]() {
		s->setFilterData(v);
	});
}

static void lh_Shape_getFilterData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		int v[3];
		s->getFilterData(v);
		answers[0] = lhat_integer(v[0]);
		answers[1] = lhat_integer(v[1]);
		answers[2] = lhat_integer(v[2]);
		*answerCount = 3;
	});
}

// Category and mask bits are spelled as LOVE's category numbers, 1 to 16.
// Those are names, as a mouse button's number is, not positions in anything
// L^ indexes -- so they did not move when L^ began counting from 0.
static bool bitsOf(LhatMachine *machine, const LhatValue *args, size_t count, uint16 &out)
{
	std::bitset<16> b;
	for (size_t i = 1; i < count; i++)
	{
		double index = lh::optNumber(args, count, i, 0);
		if (index < 1 || index > 16)
		{
			lh::raise(machine, "Values must be in range 1-16.");
			return false;
		}
		b.set((size_t) index - 1, true);
	}
	out = (uint16) b.to_ulong();
	return true;
}

static LhatValue bitList(LhatMachine *machine, uint16 bits)
{
	std::bitset<16> b((int) bits);
	std::vector<float> indices;
	for (int i = 0; i < 16; i++)
		if (b.test(i))
			indices.push_back((float) (i + 1));
	return numberList(machine, indices);
}

// setCategory() with no bits puts the shape back in category 1, and
// setMask() with none lets it collide with everything (wrap_Shape.cpp).
static void lh_Shape_setCategory(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	uint16 bits = count > 1 ? 0 : 1;
	if (!bitsOf(machine, args, count, bits))
		return;
	lh::guard(machine, [&]() {
		s->setCategory(bits);
	});
}

static void lh_Shape_setMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	uint16 bits = 0;
	if (!bitsOf(machine, args, count, bits))
		return;
	lh::guard(machine, [&]() {
		s->setMask(bits);
	});
}

static void lh_Shape_getCategory(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() { answers[0] = bitList(machine, s->getCategory()); *answerCount = 1; });
}

static void lh_Shape_getMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() { answers[0] = bitList(machine, s->getMask()); *answerCount = 1; });
}

static void lh_Shape_setUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	if (count < 2 || lhat_is_nil(args[1]))
		s->setUserData(nullptr);
	else
	{
		StrongRef<lh::Parked> parked(new lh::Parked(lh::ParkingLot::lotOf(machine), args[1]), Acquire::NORETAIN);
		s->setUserData(parked.get());
	}
}

static void lh_Shape_getUserData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::Parked *parked = dynamic_cast<lh::Parked *>(s->getUserData());
	answers[0] = parked != nullptr ? parked->get() : lhat_nil();
	*answerCount = 1;
}

// getBoundingBox([childIndex]) -> (lx, ly, ux, uy)
static void lh_Shape_getBoundingBox(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		float out[4];
		s->getBoundingBox((int) numberAt(args, count, 1, 1) - 1, out[0], out[1], out[2], out[3]);
		numbers(out, 4, answers, answerCount);
	});
}

static void lh_Shape_getMassData(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		float out[4];
		s->getMassData(out[0], out[1], out[2], out[3]);
		numbers(out, 4, answers, answerCount);
	});
}

static void lh_Shape_destroy(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		s->destroy();
	});
}

static void lh_Shape_isDestroyed(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	answers[0] = lhat_bool(!s->isValid());
	*answerCount = 1;
}

// --- CircleShape ---

static void lh_Shape_setRadius(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	CircleShape *c = kindOf<CircleShape>(machine, s, Shape::SHAPE_CIRCLE, "setRadius");
	if (c == nullptr)
		return;
	float r = numberAt(args, count, 1);
	lh::guard(machine, [&]() {
		c->setRadius(r);
	});
}

// getPoint() -> (x, y) of a circle; getPoint(index) -> (x, y) of a chain's vertex.
static void lh_Shape_getPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		float out[2] = {0, 0};
		if (count >= 2)
		{
			ChainShape *chain = kindOf<ChainShape>(machine, s, Shape::SHAPE_CHAIN, "getPoint(index)");
			if (chain == nullptr)
			{
				answers[0] = lhat_nil();
				*answerCount = 1;
				return;
			}
			b2Vec2 v = chain->getPoint((int) numberAt(args, count, 1, 1) - 1);
			out[0] = v.x;
			out[1] = v.y;
		}
		else
		{
			CircleShape *c = kindOf<CircleShape>(machine, s, Shape::SHAPE_CIRCLE, "getPoint()");
			if (c == nullptr)
			{
				answers[0] = lhat_nil();
				*answerCount = 1;
				return;
			}
			c->getPoint(out[0], out[1]);
		}
		numbers(out, 2, answers, answerCount);
	});
}

static void lh_Shape_setPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	CircleShape *c = kindOf<CircleShape>(machine, s, Shape::SHAPE_CIRCLE, "setPoint");
	if (c == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = numberAt(args, count, 1), y = numberAt(args, count, 2);
	lh::guard(machine, [&]() {
		c->setPoint(x, y);
		answers[0] = lhat_nil();
		*answerCount = 1;
	});
}

// --- Polygon / Edge / Chain ---

// getPoints() -> t^{number^[]}: the vertices as x, y pairs, for polygon,
// edge and chain shapes alike.
static void lh_Shape_getPoints(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	lh::guard(machine, [&]() {
		std::vector<float> points;
		switch (s->getType())
		{
		case Shape::SHAPE_POLYGON:
			points = ((PolygonShape *) s)->getPoints();
			break;
		case Shape::SHAPE_EDGE:
		{
			points.resize(4);
			((EdgeShape *) s)->getPoints(points[0], points[1], points[2], points[3]);
			break;
		}
		case Shape::SHAPE_CHAIN:
		{
			ChainShape *chain = (ChainShape *) s;
			const b2Vec2 *verts = chain->getPoints();
			int n = chain->getVertexCount();
			for (int i = 0; i < n; i++)
			{
				b2Vec2 v = Physics::scaleUp(verts[i]);
				points.push_back(v.x);
				points.push_back(v.y);
			}
			break;
		}
		default:
			{
				lh::raise(machine, "getPoints is not a member of a circle shape.");
				return;
			}
		}
		answers[0] = numberList(machine, points);
		*answerCount = 1;
	});
}

// The ghost vertices of an edge or a chain.
typedef void (EdgeShape::*EdgeSetter)(float, float);
typedef b2Vec2 (EdgeShape::*EdgeGetter)() const;
typedef void (ChainShape::*ChainSetter)(float, float);
typedef b2Vec2 (ChainShape::*ChainGetter)() const;

static void setVertex(LhatMachine *machine, Shape *s, const LhatValue *args, size_t count, EdgeSetter edge, ChainSetter chain, const char *name)
{
	float x = numberAt(args, count, 1), y = numberAt(args, count, 2);
	lh::guard(machine, [&]() {
		if (s->getType() == Shape::SHAPE_EDGE)
			(((EdgeShape *) s)->*edge)(x, y);
		else if (s->getType() == Shape::SHAPE_CHAIN)
			(((ChainShape *) s)->*chain)(x, y);
		else
			lh::raise(machine, std::string(name) + " is only a member of edge and chain shapes.");
	});
}

static void getVertex(LhatMachine *machine, Shape *s, EdgeGetter edge, ChainGetter chain, const char *name,
					  LhatValue *answers, int *answerCount)
{
	lh::guard(machine, [&]() {
		b2Vec2 v;
		if (s->getType() == Shape::SHAPE_EDGE)
			v = (((EdgeShape *) s)->*edge)();
		else if (s->getType() == Shape::SHAPE_CHAIN)
			v = (((ChainShape *) s)->*chain)();
		else
		{
			lh::raise(machine, std::string(name) + " is only a member of edge and chain shapes.");
			return;
		}
		float out[2] = {v.x, v.y};
		numbers(out, 2, answers, answerCount);
	});
}

static void lh_Shape_setNextVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	setVertex(machine, s, args, count, &EdgeShape::setNextVertex, &ChainShape::setNextVertex, "setNextVertex");
}

static void lh_Shape_setPreviousVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	setVertex(machine, s, args, count, &EdgeShape::setPreviousVertex, &ChainShape::setPreviousVertex, "setPreviousVertex");
}

static void lh_Shape_getNextVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	getVertex(machine, s, &EdgeShape::getNextVertex, &ChainShape::getNextVertex, "getNextVertex", answers, answerCount);
}

static void lh_Shape_getPreviousVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	getVertex(machine, s, &EdgeShape::getPreviousVertex, &ChainShape::getPreviousVertex, "getPreviousVertex", answers, answerCount);
}

static void lh_Shape_getVertexCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	SHAPE_SELF();
	ChainShape *chain = kindOf<ChainShape>(machine, s, Shape::SHAPE_CHAIN, "getVertexCount");
	if (chain == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() { answers[0] = lhat_integer(chain->getVertexCount()); *answerCount = 1; });
}

bool lhPhysicsShape(lh::Context &ctx)
{
	const char *m = LH_PHYSICS;
	// 05 の 8.8改: box2d's four kinds are LOVE's four classes, so they are
	// four types here too. What belongs to one kind is registered on that
	// kind, and asking for it on another is a diagnostic before the game
	// runs rather than a panic once it does.
	if (!ctx.objectType(m, "Shape", Shape::type)
		|| !ctx.objectType(m, "CircleShape", CircleShape::type, m, "Shape")
		|| !ctx.objectType(m, "PolygonShape", PolygonShape::type, m, "Shape")
		|| !ctx.objectType(m, "EdgeShape", EdgeShape::type, m, "Shape")
		|| !ctx.objectType(m, "ChainShape", ChainShape::type, m, "Shape"))
		return false;
	if (ctx.types())
		return true;

	const char *S = "Shape";
	const char *pair = "f^self^ -> (number^, number^);";
	const char *setPair = "p^self^, number^, number^;";
	const char *points = "f^self^ -> t^{number^[]};";

	// What every shape answers.
	bool ok = ctx.member(m, S, "getType", "f^self^ -> string^;", lh_Shape_getType, nullptr)
		&& ctx.member(m, S, "getRadius", "f^self^ -> number^;", lh_Shape_getRadius, nullptr)
		&& ctx.member(m, S, "getChildCount", "f^self^ -> number^;", lh_Shape_getChildCount, nullptr)
		&& ctx.member(m, S, "setFriction", "p^self^, number^;", lh_Shape_setFriction, nullptr)
		&& ctx.member(m, S, "setRestitution", "p^self^, number^;", lh_Shape_setRestitution, nullptr)
		&& ctx.member(m, S, "setDensity", "p^self^, number^;", lh_Shape_setDensity, nullptr)
		&& ctx.member(m, S, "setSensor", "p^self^, bool^;", lh_Shape_setSensor, nullptr)
		&& ctx.member(m, S, "getFriction", "f^self^ -> number^;", lh_Shape_getFriction, nullptr)
		&& ctx.member(m, S, "getRestitution", "f^self^ -> number^;", lh_Shape_getRestitution, nullptr)
		&& ctx.member(m, S, "getDensity", "f^self^ -> number^;", lh_Shape_getDensity, nullptr)
		&& ctx.member(m, S, "getBody", "p^self^ -> love.physics.Body|nil^;", lh_Shape_getBody, nullptr)
		&& ctx.member(m, S, "isSensor", "f^self^ -> bool^;", lh_Shape_isSensor, nullptr)
		&& ctx.member(m, S, "testPoint", "f^self^, number^, number^ -> bool^;", lh_Shape_testPoint, nullptr)
		&& ctx.member(m, S, "testPoint", "f^self^, number^, number^, number^, number^, number^ -> bool^;", lh_Shape_testPoint, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "rayCast", "f^self^, number^, number^, number^, number^, number^, number^, number^, number^, number^ -> (bool^, number^, number^, number^);", lh_Shape_rayCast, nullptr)
		&& ctx.member(m, S, "computeAABB", "f^self^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_Shape_computeAABB, nullptr)
		&& ctx.member(m, S, "computeAABB", "f^self^, number^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_Shape_computeAABB, nullptr)
		&& ctx.member(m, S, "computeMass", "f^self^, number^ -> (number^, number^, number^, number^);", lh_Shape_computeMass, nullptr)
		&& ctx.member(m, S, "setFilterData", "p^self^, number^, number^, number^;", lh_Shape_setFilterData, nullptr)
		&& ctx.member(m, S, "getFilterData", "f^self^ -> (number^, number^, number^);", lh_Shape_getFilterData, nullptr)
		&& ctx.member(m, S, "setCategory", "p^self^;", lh_Shape_setCategory, nullptr)
		&& ctx.member(m, S, "setCategory", "p^self^, number^, ...;", lh_Shape_setCategory, nullptr)
		&& ctx.member(m, S, "getCategory", points, lh_Shape_getCategory, nullptr)
		&& ctx.member(m, S, "setMask", "p^self^;", lh_Shape_setMask, nullptr)
		&& ctx.member(m, S, "setMask", "p^self^, number^, ...;", lh_Shape_setMask, nullptr)
		&& ctx.member(m, S, "getMask", points, lh_Shape_getMask, nullptr)
		&& ctx.member(m, S, "setUserData", "p^self^, any^;", lh_Shape_setUserData, nullptr)
		&& ctx.member(m, S, "getUserData", "f^self^ -> any^;", lh_Shape_getUserData, nullptr)
		&& ctx.member(m, S, "getBoundingBox", "f^self^ -> (number^, number^, number^, number^);", lh_Shape_getBoundingBox, nullptr)
		&& ctx.member(m, S, "getBoundingBox", "f^self^, number^ -> (number^, number^, number^, number^);", lh_Shape_getBoundingBox, nullptr)
		&& ctx.member(m, S, "getMassData", "f^self^ -> (number^, number^, number^, number^);", lh_Shape_getMassData, nullptr)
		&& ctx.member(m, S, "getGroupIndex", "f^self^ -> number^;", lh_Shape_getGroupIndex, nullptr)
		&& ctx.member(m, S, "setGroupIndex", "p^self^, number^;", lh_Shape_setGroupIndex, nullptr)
		&& ctx.member(m, S, "destroy", "p^self^;", lh_Shape_destroy, nullptr)
		&& ctx.member(m, S, "isDestroyed", "f^self^ -> bool^;", lh_Shape_isDestroyed, nullptr);
	if (!ok)
		return false;

	// A circle is the one with a centre it can be asked for and moved.
	ok = ctx.member(m, "CircleShape", "setRadius", "p^self^, number^;", lh_Shape_setRadius, nullptr)
		&& ctx.member(m, "CircleShape", "getPoint", pair, lh_Shape_getPoint, nullptr)
		&& ctx.member(m, "CircleShape", "setPoint", setPair, lh_Shape_setPoint, nullptr);
	if (!ok)
		return false;

	// The three with vertices answer getPoints. They share no class of their
	// own under Shape, so each says it -- three registrations rather than a
	// kind check inside one.
	for (const char *kind : {"PolygonShape", "EdgeShape", "ChainShape"})
	{
		if (!ctx.member(m, kind, "getPoints", points, lh_Shape_getPoints, nullptr))
			return false;
	}

	// 04 の ghost vertices: an edge and a chain carry the neighbours box2d
	// uses to smooth collisions across a seam.
	for (const char *kind : {"EdgeShape", "ChainShape"})
	{
		ok = ctx.member(m, kind, "setNextVertex", setPair, lh_Shape_setNextVertex, nullptr)
			&& ctx.member(m, kind, "setPreviousVertex", setPair, lh_Shape_setPreviousVertex, nullptr)
			&& ctx.member(m, kind, "getNextVertex", pair, lh_Shape_getNextVertex, nullptr)
			&& ctx.member(m, kind, "getPreviousVertex", pair, lh_Shape_getPreviousVertex, nullptr);
		if (!ok)
			return false;
	}

	// A chain is the one whose vertices are counted and read one at a time.
	return ctx.member(m, "ChainShape", "getPoint", "f^self^, number^ -> (number^, number^);", lh_Shape_getPoint, nullptr)
		&& ctx.member(m, "ChainShape", "getVertexCount", "f^self^ -> number^;", lh_Shape_getVertexCount, nullptr);
}

} // box2d
} // physics
} // love

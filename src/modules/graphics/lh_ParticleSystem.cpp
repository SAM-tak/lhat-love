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

// love.graphics.ParticleSystem and love.graphics.TextBatch for L^. The
// references are wrap_ParticleSystem.cpp and wrap_TextBatch.cpp beside this
// file. Angles are radians, as love.graphics takes them everywhere.

#include "lh_Graphics.h"

#include "ParticleSystem.h"
#include "TextBatch.h"
#include "Font.h"

namespace love
{
namespace graphics
{

#define instance() graphicsInstance()
#define binding graphicsBinding

// ---------------------------------------------------------------------------
// ParticleSystem
// ---------------------------------------------------------------------------

static ParticleSystem *checkParticles(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	ParticleSystem *p = index < count ? lh::checkObject<ParticleSystem>(args[index], *binding.registry) : nullptr;
	if (p == nullptr)
		lh::raise(machine, "Expected a ParticleSystem");
	return p;
}

#define PS_SELF() ParticleSystem *ps = checkParticles(machine, args, count, 0); if (ps == nullptr) return

#define PS_NUMBER(name, expr) \
	static void lh_PS_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                         LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		PS_SELF(); \
		answers[0] = lhat_real(expr); \
		*answerCount = 1; \
	}

#define PS_BOOL(name, expr) \
	static void lh_PS_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                         LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		PS_SELF(); \
		answers[0] = lhat_bool(expr); \
		*answerCount = 1; \
	}

#define PS_SET_NUMBER(name, call) \
	static void lh_PS_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                         LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		(void) answers; \
		(void) answerCount; \
		PS_SELF(); \
		float v = (float) lh::optNumber(args, count, 1, 0.0); \
		(void) v; \
		lh::guard(machine, [&]() { ps->call; }); \
	}

// set<X>(min[, max]) pairs: the second defaults to the first.
#define PS_SET_RANGE(name, call) \
	static void lh_PS_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                         LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		(void) answers; \
		(void) answerCount; \
		PS_SELF(); \
		float lo = (float) lh::optNumber(args, count, 1, 0.0); \
		float hi = (float) lh::optNumber(args, count, 2, lo); \
		lh::guard(machine, [&]() { ps->call; }); \
	}

#define PS_GET_RANGE(name, call) \
	static void lh_PS_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                         LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		PS_SELF(); \
		float values[2] = {0, 0}; \
		ps->call; \
		answers[0] = lhat_real(values[0]); \
		answers[1] = lhat_real(values[1]); \
		*answerCount = 2; \
	}

#define PS_DO(name, call) \
	static void lh_PS_##name(LhatMachine *machine, void *context, const LhatValue *args, size_t count, \
	                         LhatValue *answers, int *answerCount) \
	{ \
		(void) context; \
		(void) answers; \
		(void) answerCount; \
		PS_SELF(); \
		lh::guard(machine, [&]() { ps->call; }); \
	}

// newParticleSystem(texture[, buffer])
static void lh_newParticleSystem(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture *texture = checkTexture(machine, args, count, 0);
	if (texture == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	double size = lh::optNumber(args, count, 1, 1000);
	if (size < 1.0 || size > ParticleSystem::MAX_PARTICLES)
	{
		lh::raise(machine, "Invalid ParticleSystem size");
		return;
	}
	lh::guard(machine, [&]() {
		StrongRef<ParticleSystem> ps(instance()->newParticleSystem(texture, (int) size), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, ps.get());
		*answerCount = 1;
	});
}

static void lh_PS_clone(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	lh::guard(machine, [&]() {
		StrongRef<ParticleSystem> made(ps->clone(), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, made.get());
		*answerCount = 1;
	});
}

static void lh_PS_setTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	Texture *texture = checkTexture(machine, args, count, 1);
	if (texture == nullptr)
		return;
	lh::guard(machine, [&]() {
		ps->setTexture(texture);
	});
}

static void lh_PS_getTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	answers[0] = lh::pushObject(machine, *binding.registry, ps->getTexture());
	*answerCount = 1;
}

PS_SET_NUMBER(setBufferSize, setBufferSize((uint32) v))
PS_NUMBER(getBufferSize, (float) ps->getBufferSize())
PS_SET_NUMBER(setEmissionRate, setEmissionRate(v))
PS_NUMBER(getEmissionRate, ps->getEmissionRate())
PS_SET_NUMBER(setEmitterLifetime, setEmitterLifetime(v))
PS_NUMBER(getEmitterLifetime, ps->getEmitterLifetime())
PS_SET_RANGE(setParticleLifetime, setParticleLifetime(lo, hi))
PS_GET_RANGE(getParticleLifetime, getParticleLifetime(values[0], values[1]))
PS_SET_NUMBER(setDirection, setDirection(v))
PS_NUMBER(getDirection, ps->getDirection())
PS_SET_NUMBER(setSpread, setSpread(v))
PS_NUMBER(getSpread, ps->getSpread())
PS_SET_RANGE(setSpeed, setSpeed(lo, hi))
PS_GET_RANGE(getSpeed, getSpeed(values[0], values[1]))
PS_SET_RANGE(setRadialAcceleration, setRadialAcceleration(lo, hi))
PS_GET_RANGE(getRadialAcceleration, getRadialAcceleration(values[0], values[1]))
PS_SET_RANGE(setTangentialAcceleration, setTangentialAcceleration(lo, hi))
PS_GET_RANGE(getTangentialAcceleration, getTangentialAcceleration(values[0], values[1]))
PS_SET_RANGE(setLinearDamping, setLinearDamping(lo, hi))
PS_GET_RANGE(getLinearDamping, getLinearDamping(values[0], values[1]))
PS_SET_NUMBER(setSizeVariation, setSizeVariation(v))
PS_NUMBER(getSizeVariation, ps->getSizeVariation())
PS_SET_RANGE(setRotation, setRotation(lo, hi))
PS_GET_RANGE(getRotation, getRotation(values[0], values[1]))
PS_SET_RANGE(setSpin, setSpin(lo, hi))
PS_GET_RANGE(getSpin, getSpin(values[0], values[1]))
PS_SET_NUMBER(setSpinVariation, setSpinVariation(v))
PS_NUMBER(getSpinVariation, ps->getSpinVariation())
PS_NUMBER(getCount, (float) ps->getCount())
PS_DO(start, start())
PS_DO(stop, stop())
PS_DO(pause, pause())
PS_DO(reset, reset())
PS_SET_NUMBER(emit, emit((uint32) v))
PS_SET_NUMBER(update, update(v))
PS_BOOL(isActive, ps->isActive())
PS_BOOL(isPaused, ps->isPaused())
PS_BOOL(isStopped, ps->isStopped())
PS_BOOL(hasRelativeRotation, ps->hasRelativeRotation())

static void lh_PS_setInsertMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	std::string name = lh::optString(args, count, 1, "top");
	ParticleSystem::InsertMode mode;
	if (!ParticleSystem::getConstant(name.c_str(), mode))
	{
		lh::raise(machine, "Invalid insert mode: " + name);
		return;
	}
	ps->setInsertMode(mode);
}

static void lh_PS_getInsertMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	const char *name = "top";
	ParticleSystem::getConstant(ps->getInsertMode(), name);
	LhatValue out = lhat_nil();
	lh::makeString(machine, name, &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_PS_setPosition(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	ps->setPosition((float) lh::optNumber(args, count, 1, 0), (float) lh::optNumber(args, count, 2, 0));
}

static void lh_PS_getPosition(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	Vector2 p = ps->getPosition();
	float values[2] = {p.x, p.y};
	numberTuple(values, 2, answers, answerCount);
}

static void lh_PS_moveTo(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	ps->moveTo((float) lh::optNumber(args, count, 1, 0), (float) lh::optNumber(args, count, 2, 0));
}

// setEmissionArea(distribution[, dx, dy[, angle[, directionRelativeToCenter]]])
static void lh_PS_setEmissionArea(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	std::string name = lh::optString(args, count, 1, "none");
	ParticleSystem::AreaSpreadDistribution distribution;
	if (!ParticleSystem::getConstant(name.c_str(), distribution))
	{
		lh::raise(machine, "Invalid distribution: " + name);
		return;
	}
	float x = (float) lh::optNumber(args, count, 2, 0), y = (float) lh::optNumber(args, count, 3, 0);
	float angle = (float) lh::optNumber(args, count, 4, 0);
	bool relative = lh::optBool(args, count, 5, false);
	lh::guard(machine, [&]() {
		ps->setEmissionArea(distribution, x, y, angle, relative);
	});
}

// setLinearAcceleration(xmin, ymin[, xmax, ymax])
static void lh_PS_setLinearAcceleration(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	float xmin = (float) lh::optNumber(args, count, 1, 0), ymin = (float) lh::optNumber(args, count, 2, 0);
	float xmax = (float) lh::optNumber(args, count, 3, xmin), ymax = (float) lh::optNumber(args, count, 4, ymin);
	lh::guard(machine, [&]() {
		ps->setLinearAcceleration(xmin, ymin, xmax, ymax);
	});
}

static void lh_PS_getLinearAcceleration(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	Vector2 lo, hi;
	ps->getLinearAcceleration(lo, hi);
	float values[4] = {lo.x, lo.y, hi.x, hi.y};
	numberTuple(values, 4, answers, answerCount);
}

// setSizes(size1, size2, ...)
static void lh_PS_setSizes(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	std::vector<float> sizes;
	for (size_t i = 1; i < count; i++)
		sizes.push_back((float) lh::optNumber(args, count, i, 1.0));
	if (sizes.empty() || sizes.size() > 8)
	{
		lh::raise(machine, "setSizes takes between 1 and 8 sizes");
		return;
	}
	lh::guard(machine, [&]() {
		ps->setSizes(sizes);
	});
}

static void lh_PS_getSizes(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	const std::vector<float> &sizes = ps->getSizes();
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < sizes.size(); i++)
	{
		bool refused = false;
		lhat_table_set(t, lhat_integer((int64_t) i), lhat_real(sizes[i]), &refused);
	}
	answers[0] = table;
	*answerCount = 1;
}

// setColors(r1, g1, b1, a1, r2, g2, b2, a2, ...): a color per four numbers.
static void lh_PS_setColors(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	size_t given = count - 1;
	if (given == 0 || given % 4 != 0 || given / 4 > 8)
	{
		lh::raise(machine, "setColors takes between 1 and 8 colors of four numbers each");
		return;
	}
	std::vector<Colorf> colors;
	for (size_t i = 1; i < count; i += 4)
		colors.push_back(colorOf(args, count, i));
	lh::guard(machine, [&]() {
		ps->setColor(colors);
	});
}

static void lh_PS_getColors(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	std::vector<Colorf> colors = ps->getColor();
	std::vector<float> flat;
	for (const Colorf &c : colors)
	{
		flat.push_back(c.r);
		flat.push_back(c.g);
		flat.push_back(c.b);
		flat.push_back(c.a);
	}
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < flat.size(); i++)
	{
		bool refused = false;
		lhat_table_set(t, lhat_integer((int64_t) i), lhat_real(flat[i]), &refused);
	}
	answers[0] = table;
	*answerCount = 1;
}

// setQuads(quad1, quad2, ...) / setQuads() to clear.
static void lh_PS_setQuads(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	std::vector<Quad *> quads;
	for (size_t i = 1; i < count; i++)
	{
		Quad *quad = checkQuad(machine, args, count, i);
		if (quad == nullptr)
			return;
		quads.push_back(quad);
	}
	lh::guard(machine, [&]() {
		if (quads.empty())
			ps->setQuads();
		else
			ps->setQuads(quads);
	});
}

static void lh_PS_setOffset(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	ps->setOffset((float) lh::optNumber(args, count, 1, 0), (float) lh::optNumber(args, count, 2, 0));
}

static void lh_PS_getOffset(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	Vector2 o = ps->getOffset();
	float values[2] = {o.x, o.y};
	numberTuple(values, 2, answers, answerCount);
}

static void lh_PS_setRelativeRotation(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	PS_SELF();
	ps->setRelativeRotation(lh::optBool(args, count, 1, false));
}

bool lhGraphicsParticleSystem(lh::Context &ctx)
{
	const char *m = LH_GRAPHICS;
	if (!ctx.objectType(m, "ParticleSystem", ParticleSystem::type, m, "Drawable"))
		return false;
	if (ctx.types())
		return true;
	const char *P = "ParticleSystem";
	const char *num = "f^self^ -> number^;";
	const char *flag = "f^self^ -> bool^;";
	const char *setNum = "p^self^, number^;";
	const char *setRange1 = "p^self^, number^;";
	const char *setRange2 = "p^self^, number^, number^;";
	const char *pair = "f^self^ -> (number^, number^);";
	const char *act = "p^self^;";
	return ctx.func(m, "newParticleSystem", "p^love.graphics.Texture -> love.graphics.ParticleSystem;", lh_newParticleSystem, nullptr)
		&& ctx.func(m, "newParticleSystem", "p^love.graphics.Texture, number^ -> love.graphics.ParticleSystem;", lh_newParticleSystem, nullptr)
		&& ctx.member(m, P, "clone", "p^self^ -> love.graphics.ParticleSystem;", lh_PS_clone, nullptr)
		&& ctx.member(m, P, "setTexture", "p^self^, love.graphics.Texture;", lh_PS_setTexture, nullptr)
		&& ctx.member(m, P, "getTexture", "p^self^ -> love.graphics.Texture;", lh_PS_getTexture, nullptr)
		&& ctx.member(m, P, "setBufferSize", setNum, lh_PS_setBufferSize, nullptr)
		&& ctx.member(m, P, "getBufferSize", num, lh_PS_getBufferSize, nullptr)
		&& ctx.member(m, P, "setInsertMode", "p^self^, string^;", lh_PS_setInsertMode, nullptr)
		&& ctx.member(m, P, "getInsertMode", "f^self^ -> string^;", lh_PS_getInsertMode, nullptr)
		&& ctx.member(m, P, "setEmissionRate", setNum, lh_PS_setEmissionRate, nullptr)
		&& ctx.member(m, P, "getEmissionRate", num, lh_PS_getEmissionRate, nullptr)
		&& ctx.member(m, P, "setEmitterLifetime", setNum, lh_PS_setEmitterLifetime, nullptr)
		&& ctx.member(m, P, "getEmitterLifetime", num, lh_PS_getEmitterLifetime, nullptr)
		&& ctx.member(m, P, "setParticleLifetime", setRange1, lh_PS_setParticleLifetime, nullptr)
		&& ctx.member(m, P, "setParticleLifetime", setRange2, lh_PS_setParticleLifetime, nullptr)
		&& ctx.member(m, P, "getParticleLifetime", pair, lh_PS_getParticleLifetime, nullptr)
		&& ctx.member(m, P, "setPosition", "p^self^, number^, number^;", lh_PS_setPosition, nullptr)
		&& ctx.member(m, P, "getPosition", pair, lh_PS_getPosition, nullptr)
		&& ctx.member(m, P, "moveTo", "p^self^, number^, number^;", lh_PS_moveTo, nullptr)
		&& ctx.member(m, P, "setEmissionArea", "p^self^, string^;", lh_PS_setEmissionArea, nullptr)
		&& ctx.member(m, P, "setEmissionArea", "p^self^, string^, number^, number^;", lh_PS_setEmissionArea, nullptr)
		&& ctx.member(m, P, "setEmissionArea", "p^self^, string^, number^, number^, number^;", lh_PS_setEmissionArea, nullptr)
		&& ctx.member(m, P, "setEmissionArea", "p^self^, string^, number^, number^, number^, bool^;", lh_PS_setEmissionArea, nullptr)
		&& ctx.member(m, P, "setDirection", setNum, lh_PS_setDirection, nullptr)
		&& ctx.member(m, P, "getDirection", num, lh_PS_getDirection, nullptr)
		&& ctx.member(m, P, "setSpread", setNum, lh_PS_setSpread, nullptr)
		&& ctx.member(m, P, "getSpread", num, lh_PS_getSpread, nullptr)
		&& ctx.member(m, P, "setSpeed", setRange1, lh_PS_setSpeed, nullptr)
		&& ctx.member(m, P, "setSpeed", setRange2, lh_PS_setSpeed, nullptr)
		&& ctx.member(m, P, "getSpeed", pair, lh_PS_getSpeed, nullptr)
		&& ctx.member(m, P, "setLinearAcceleration", "p^self^, number^, number^;", lh_PS_setLinearAcceleration, nullptr)
		&& ctx.member(m, P, "setLinearAcceleration", "p^self^, number^, number^, number^, number^;", lh_PS_setLinearAcceleration, nullptr)
		&& ctx.member(m, P, "getLinearAcceleration", "f^self^ -> (number^, number^, number^, number^);", lh_PS_getLinearAcceleration, nullptr)
		&& ctx.member(m, P, "setRadialAcceleration", setRange1, lh_PS_setRadialAcceleration, nullptr)
		&& ctx.member(m, P, "setRadialAcceleration", setRange2, lh_PS_setRadialAcceleration, nullptr)
		&& ctx.member(m, P, "getRadialAcceleration", pair, lh_PS_getRadialAcceleration, nullptr)
		&& ctx.member(m, P, "setTangentialAcceleration", setRange1, lh_PS_setTangentialAcceleration, nullptr)
		&& ctx.member(m, P, "setTangentialAcceleration", setRange2, lh_PS_setTangentialAcceleration, nullptr)
		&& ctx.member(m, P, "getTangentialAcceleration", pair, lh_PS_getTangentialAcceleration, nullptr)
		&& ctx.member(m, P, "setLinearDamping", setRange1, lh_PS_setLinearDamping, nullptr)
		&& ctx.member(m, P, "setLinearDamping", setRange2, lh_PS_setLinearDamping, nullptr)
		&& ctx.member(m, P, "getLinearDamping", pair, lh_PS_getLinearDamping, nullptr)
		&& ctx.member(m, P, "setSizes", "p^self^, number^, ...;", lh_PS_setSizes, nullptr)
		&& ctx.member(m, P, "getSizes", "f^self^ -> t^{number^[]};", lh_PS_getSizes, nullptr)
		&& ctx.member(m, P, "setSizeVariation", setNum, lh_PS_setSizeVariation, nullptr)
		&& ctx.member(m, P, "getSizeVariation", num, lh_PS_getSizeVariation, nullptr)
		&& ctx.member(m, P, "setRotation", setRange1, lh_PS_setRotation, nullptr)
		&& ctx.member(m, P, "setRotation", setRange2, lh_PS_setRotation, nullptr)
		&& ctx.member(m, P, "getRotation", pair, lh_PS_getRotation, nullptr)
		&& ctx.member(m, P, "setSpin", setRange1, lh_PS_setSpin, nullptr)
		&& ctx.member(m, P, "setSpin", setRange2, lh_PS_setSpin, nullptr)
		&& ctx.member(m, P, "getSpin", pair, lh_PS_getSpin, nullptr)
		&& ctx.member(m, P, "setSpinVariation", setNum, lh_PS_setSpinVariation, nullptr)
		&& ctx.member(m, P, "getSpinVariation", num, lh_PS_getSpinVariation, nullptr)
		&& ctx.member(m, P, "setOffset", "p^self^, number^, number^;", lh_PS_setOffset, nullptr)
		&& ctx.member(m, P, "getOffset", pair, lh_PS_getOffset, nullptr)
		&& ctx.member(m, P, "setColors", "p^self^, number^, number^, number^, number^, ...;", lh_PS_setColors, nullptr)
		&& ctx.member(m, P, "getColors", "f^self^ -> t^{number^[]};", lh_PS_getColors, nullptr)
		&& ctx.member(m, P, "setQuads", "p^self^, ...;", lh_PS_setQuads, nullptr)
		&& ctx.member(m, P, "setRelativeRotation", "p^self^, bool^;", lh_PS_setRelativeRotation, nullptr)
		&& ctx.member(m, P, "hasRelativeRotation", flag, lh_PS_hasRelativeRotation, nullptr)
		&& ctx.member(m, P, "getCount", num, lh_PS_getCount, nullptr)
		&& ctx.member(m, P, "start", act, lh_PS_start, nullptr)
		&& ctx.member(m, P, "stop", act, lh_PS_stop, nullptr)
		&& ctx.member(m, P, "pause", act, lh_PS_pause, nullptr)
		&& ctx.member(m, P, "reset", act, lh_PS_reset, nullptr)
		&& ctx.member(m, P, "emit", setNum, lh_PS_emit, nullptr)
		&& ctx.member(m, P, "update", setNum, lh_PS_update, nullptr)
		&& ctx.member(m, P, "isActive", flag, lh_PS_isActive, nullptr)
		&& ctx.member(m, P, "isPaused", flag, lh_PS_isPaused, nullptr)
		&& ctx.member(m, P, "isStopped", flag, lh_PS_isStopped, nullptr);
}

// ---------------------------------------------------------------------------
// TextBatch
// ---------------------------------------------------------------------------

static TextBatch *checkText(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	TextBatch *text = index < count ? lh::checkObject<TextBatch>(args[index], *binding.registry) : nullptr;
	if (text == nullptr)
		lh::raise(machine, "Expected a TextBatch");
	return text;
}

#define TEXT_SELF() TextBatch *text = checkText(machine, args, count, 0); if (text == nullptr) return

static std::vector<love::font::ColoredString> coloredOf(const std::string &s)
{
	std::vector<love::font::ColoredString> out;
	love::font::ColoredString cs;
	cs.str = s;
	cs.color = Colorf(1, 1, 1, 1);
	out.push_back(cs);
	return out;
}

// newTextBatch(font[, text])
static void lh_newTextBatch(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	Font *font = count > 0 ? lh::checkObject<Font>(args[0], *binding.registry) : nullptr;
	if (font == nullptr)
	{
		lh::raise(machine, "Expected a Font");
		return;
	}
	std::string s = lh::optString(args, count, 1, "");
	lh::guard(machine, [&]() {
		StrongRef<TextBatch> text(instance()->newTextBatch(font, coloredOf(s)), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, text.get());
		*answerCount = 1;
	});
}

static void lh_TextBatch_set(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	std::string s = lh::optString(args, count, 1, "");
	lh::guard(machine, [&]() {
		text->set(coloredOf(s));
	});
}

// setf(text, wraplimit, align)
static void lh_TextBatch_setf(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	std::string s = lh::optString(args, count, 1, "");
	float wrap = (float) lh::optNumber(args, count, 2, 0);
	std::string alignstr = lh::optString(args, count, 3, "left");
	Font::AlignMode align;
	if (!Font::getConstant(alignstr.c_str(), align))
	{
		lh::raise(machine, "Invalid align mode: " + alignstr);
		return;
	}
	lh::guard(machine, [&]() {
		text->set(coloredOf(s), wrap, align);
	});
}

// add(text, x, y, ...) -> index
static void lh_TextBatch_add(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	std::string s = lh::optString(args, count, 1, "");
	Matrix4 m = transformOf(args, count, 2);
	lh::guard(machine, [&]() { answers[0] = lhat_integer(text->add(coloredOf(s), m)); *answerCount = 1; });
}

// addf(text, wraplimit, align, x, y, ...) -> index
static void lh_TextBatch_addf(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	std::string s = lh::optString(args, count, 1, "");
	float wrap = (float) lh::optNumber(args, count, 2, 0);
	std::string alignstr = lh::optString(args, count, 3, "left");
	Font::AlignMode align;
	if (!Font::getConstant(alignstr.c_str(), align))
	{
		lh::raise(machine, "Invalid align mode: " + alignstr);
		return;
	}
	Matrix4 m = transformOf(args, count, 4);
	lh::guard(machine, [&]() { answers[0] = lhat_integer(text->addf(coloredOf(s), wrap, align, m)); *answerCount = 1; });
}

static void lh_TextBatch_clear(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	lh::guard(machine, [&]() {
		text->clear();
	});
}

static void lh_TextBatch_setFont(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	Font *font = count > 1 ? lh::checkObject<Font>(args[1], *binding.registry) : nullptr;
	if (font == nullptr)
	{
		lh::raise(machine, "Expected a Font");
		return;
	}
	lh::guard(machine, [&]() {
		text->setFont(font);
	});
}

static void lh_TextBatch_getFont(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	answers[0] = lh::pushObject(machine, *binding.registry, text->getFont());
	*answerCount = 1;
}

// getWidth([index]) / getHeight([index]) / getDimensions([index])
static void lh_TextBatch_getWidth(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	int index = (int) lh::optNumber(args, count, 1, -1);
	lh::guard(machine, [&]() { answers[0] = lhat_real(text->getWidth(index)); *answerCount = 1; });
}

static void lh_TextBatch_getHeight(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	int index = (int) lh::optNumber(args, count, 1, -1);
	lh::guard(machine, [&]() { answers[0] = lhat_real(text->getHeight(index)); *answerCount = 1; });
}

static void lh_TextBatch_getDimensions(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXT_SELF();
	int index = (int) lh::optNumber(args, count, 1, -1);
	lh::guard(machine, [&]() {
		float values[2] = {(float) text->getWidth(index), (float) text->getHeight(index)};
		numberTuple(values, 2, answers, answerCount);
	});
}

bool lhGraphicsTextBatch(lh::Context &ctx)
{
	const char *m = LH_GRAPHICS;
	if (!ctx.objectType(m, "TextBatch", TextBatch::type, m, "Drawable"))
		return false;
	if (ctx.types())
		return true;
	const char *T = "TextBatch";
	return ctx.func(m, "newTextBatch", "p^love.graphics.Font -> love.graphics.TextBatch;", lh_newTextBatch, nullptr)
		&& ctx.func(m, "newTextBatch", "p^love.graphics.Font, string^ -> love.graphics.TextBatch;", lh_newTextBatch, nullptr)
		&& ctx.member(m, T, "set", "p^self^, string^;", lh_TextBatch_set, nullptr)
		&& ctx.member(m, T, "setf", "p^self^, string^, number^, string^;", lh_TextBatch_setf, nullptr)
		&& ctx.member(m, T, "add", "p^self^, string^, number^, number^, ... -> number^;", lh_TextBatch_add, nullptr)
		&& ctx.member(m, T, "addf", "p^self^, string^, number^, string^, number^, number^, ... -> number^;", lh_TextBatch_addf, nullptr)
		&& ctx.member(m, T, "clear", "p^self^;", lh_TextBatch_clear, nullptr)
		&& ctx.member(m, T, "setFont", "p^self^, love.graphics.Font;", lh_TextBatch_setFont, nullptr)
		&& ctx.member(m, T, "getFont", "p^self^ -> love.graphics.Font;", lh_TextBatch_getFont, nullptr)
		&& ctx.member(m, T, "getWidth", "f^self^ -> number^;", lh_TextBatch_getWidth, nullptr)
		&& ctx.member(m, T, "getWidth", "f^self^, number^ -> number^;", lh_TextBatch_getWidth, nullptr)
		&& ctx.member(m, T, "getHeight", "f^self^ -> number^;", lh_TextBatch_getHeight, nullptr)
		&& ctx.member(m, T, "getHeight", "f^self^, number^ -> number^;", lh_TextBatch_getHeight, nullptr)
		&& ctx.member(m, T, "getDimensions", "f^self^ -> (number^, number^);", lh_TextBatch_getDimensions, nullptr)
		&& ctx.member(m, T, "getDimensions", "f^self^, number^ -> (number^, number^);", lh_TextBatch_getDimensions, nullptr);
}

} // graphics
} // love

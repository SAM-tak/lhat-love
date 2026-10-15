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

// love.math for L^: random numbers, noise, transforms, color and polygon
// helpers. The reference is wrap_Math.cpp, wrap_RandomGenerator.cpp and
// wrap_Transform.cpp beside this file. Scalar maths (sin, sqrt, ...) is
// std.math's; angles here are radians, as LOVE's API has them.

#include "MathModule.h"
#include "RandomGenerator.h"
#include "Transform.h"
#include "lh/lh.h"

#include "common/Vector.h"

#include <cmath>
#include <vector>

namespace love
{
namespace math
{

#define instance() (Module::getInstance<Math>(Module::M_MATH))

struct MathBinding
{
	lh::Errors *errors;
	lh::TypeRegistry *registry;
};

static MathBinding binding;

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------

// wrap_RandomGenerator.lua's getrandom: random() in [0, 1); random(max) an
// integer in [1, max]; random(min, max) an integer in [min, max].
static LhatValue randomWith(RandomGenerator *rng, const LhatValue *arguments, size_t count, size_t first)
{
	double r = rng->random();
	if (count > first + 1)
	{
		double l = lh::optNumber(arguments, count, first, 0.0);
		double u = lh::optNumber(arguments, count, first + 1, 0.0);
		return lhat_integer((int64_t) (std::floor(r * (u - l + 1)) + l));
	}
	if (count > first)
		return lhat_integer((int64_t) (std::floor(r * lh::optNumber(arguments, count, first, 1.0)) + 1));
	return lhat_real(r);
}

static RandomGenerator::Seed seedOf(const LhatValue *arguments, size_t count, size_t first)
{
	RandomGenerator::Seed s;
	if (count > first + 1)
	{
		s.b32.low = (uint32) lh::optNumber(arguments, count, first, 0.0);
		s.b32.high = (uint32) lh::optNumber(arguments, count, first + 1, 0.0);
	}
	else
		s.b64 = (uint64) lh::optNumber(arguments, count, first, 0.0);
	return s;
}

static void seedTuple(RandomGenerator::Seed s, LhatValue *answers, int *answerCount)
{
	answers[0] = lhat_integer(s.b32.low);
	answers[1] = lhat_integer(s.b32.high);
	*answerCount = 2;
}

static void lh_random(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	answers[0] = randomWith(instance()->getRandomGenerator(), arguments, count, 0);
	*answerCount = 1;
}

static void lh_randomNormal(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	double stddev = lh::optNumber(arguments, count, 0, 1.0);
	double mean = lh::optNumber(arguments, count, 1, 0.0);
	answers[0] = lhat_real(instance()->getRandomGenerator()->randomNormal(stddev) + mean);
	*answerCount = 1;
}

static void lh_setRandomSeed(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	RandomGenerator::Seed s = seedOf(arguments, count, 0);
	lh::guard(machine, [&]() {
		instance()->getRandomGenerator()->setSeed(s);
	});
}

static void lh_getRandomSeed(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	seedTuple(instance()->getRandomGenerator()->getSeed(), answers, answerCount);
}

static void lh_getRandomState(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	lh::guard(machine, [&]() {
		lh::makeString(machine, instance()->getRandomGenerator()->getState(), &answers[0]);
		*answerCount = 1;
	});
}

static void lh_setRandomState(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	lh::guard(machine, [&]() {
		instance()->getRandomGenerator()->setState(lh::optString(arguments, count, 0, ""));
	});
}

static void lh_newRandomGenerator(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	lh::guard(machine, [&]() {
		StrongRef<RandomGenerator> rng(instance()->newRandomGenerator(), Acquire::NORETAIN);
		if (count > 0)
			rng->setSeed(seedOf(arguments, count, 0));
		answers[0] = lh::pushObject(machine, *binding.registry, rng.get());
		*answerCount = 1;
	});
}

static RandomGenerator *checkRng(LhatMachine *machine, const LhatValue *arguments, size_t count)
{
	RandomGenerator *rng = count > 0 ? lh::checkObject<RandomGenerator>(arguments[0], *binding.registry) : nullptr;
	if (rng == nullptr)
		lh::raise(machine, "Expected a RandomGenerator");
	return rng;
}

static void lh_RandomGenerator_random(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	RandomGenerator *rng = checkRng(machine, arguments, count);
	answers[0] = rng != nullptr ? randomWith(rng, arguments, count, 1) : lhat_nil();
	*answerCount = 1;
}

static void lh_RandomGenerator_randomNormal(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
											LhatValue *answers, int *answerCount)
{
	(void) context;
	RandomGenerator *rng = checkRng(machine, arguments, count);
	if (rng == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_real(rng->randomNormal(lh::optNumber(arguments, count, 1, 1.0)) + lh::optNumber(arguments, count, 2, 0.0));
	*answerCount = 1;
}

static void lh_RandomGenerator_setSeed(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	RandomGenerator *rng = checkRng(machine, arguments, count);
	if (rng == nullptr)
		return;
	RandomGenerator::Seed s = seedOf(arguments, count, 1);
	lh::guard(machine, [&]() {
		rng->setSeed(s);
	});
}

static void lh_RandomGenerator_getSeed(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	RandomGenerator *rng = checkRng(machine, arguments, count);
	if (rng == nullptr)
		return;
	seedTuple(rng->getSeed(), answers, answerCount);
}

static void lh_RandomGenerator_getState(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	RandomGenerator *rng = checkRng(machine, arguments, count);
	if (rng == nullptr)
		return;
	lh::guard(machine, [&]() {
		lh::makeString(machine, rng->getState(), &answers[0]);
		*answerCount = 1;
	});
}

static void lh_RandomGenerator_setState(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	RandomGenerator *rng = checkRng(machine, arguments, count);
	if (rng == nullptr)
		return;
	lh::guard(machine, [&]() {
		rng->setState(lh::optString(arguments, count, 1, ""));
	});
}

// ---------------------------------------------------------------------------
// Noise, color, polygons
// ---------------------------------------------------------------------------

// noise(x[, y[, z[, w]]]) -> [0, 1]
static void lh_noise(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	double args[4] = {0, 0, 0, 0};
	size_t n = count < 4 ? count : 4;
	for (size_t i = 0; i < n; i++)
		args[i] = lh::optNumber(arguments, count, i, 0.0);
	double value = 0.0;
	switch (n)
	{
	case 1: value = simplexNoise1(args[0]); break;
	case 2: value = simplexNoise2(args[0], args[1]); break;
	case 3: value = perlinNoise3(args[0], args[1], args[2]); break;
	case 4: value = perlinNoise4(args[0], args[1], args[2], args[3]); break;
	default: break;
	}
	answers[0] = lhat_real(value);
	*answerCount = 1;
}

static void lh_gammaToLinear(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count < 3)
	{
		answers[0] = lhat_real(gammaToLinear((float) lh::optNumber(arguments, count, 0, 0.0)));
		*answerCount = 1;
		return;
	}
	LhatValue parts[4];
	size_t n = count < 4 ? count : 4;
	for (size_t i = 0; i < n; i++)
		parts[i] = i < 3 ? lhat_real(gammaToLinear((float) lh::optNumber(arguments, count, i, 0.0))) : lhat_real(lh::optNumber(arguments, count, i, 1.0));
	for (size_t i = 0; i < n; i++)
		answers[i] = parts[i];
	*answerCount = (int) n;
}

static void lh_linearToGamma(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count < 3)
	{
		answers[0] = lhat_real(linearToGamma((float) lh::optNumber(arguments, count, 0, 0.0)));
		*answerCount = 1;
		return;
	}
	LhatValue parts[4];
	size_t n = count < 4 ? count : 4;
	for (size_t i = 0; i < n; i++)
		parts[i] = i < 3 ? lhat_real(linearToGamma((float) lh::optNumber(arguments, count, i, 0.0))) : lhat_real(lh::optNumber(arguments, count, i, 1.0));
	for (size_t i = 0; i < n; i++)
		answers[i] = parts[i];
	*answerCount = (int) n;
}

// colorToBytes(r, g, b[, a]) -> 0-255 integers; colorFromBytes the reverse.
static void lh_colorToBytes(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	size_t n = count < 4 ? count : 4;
	LhatValue parts[4];
	for (size_t i = 0; i < n; i++)
	{
		double c = lh::optNumber(arguments, count, i, 1.0);
		c = c < 0.0 ? 0.0 : (c > 1.0 ? 1.0 : c);
		parts[i] = lhat_integer((int64_t) (c * 255.0 + 0.5));
	}
	for (size_t i = 0; i < n; i++)
		answers[i] = parts[i];
	*answerCount = (int) n;
}

static void lh_colorFromBytes(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	size_t n = count < 4 ? count : 4;
	LhatValue parts[4];
	for (size_t i = 0; i < n; i++)
		parts[i] = lhat_real(lh::optNumber(arguments, count, i, 255.0) / 255.0);
	for (size_t i = 0; i < n; i++)
		answers[i] = parts[i];
	*answerCount = (int) n;
}

static std::vector<Vector2> verticesOf(const LhatValue *args, size_t count)
{
	std::vector<Vector2> vertices;
	for (size_t i = 0; i + 1 < count; i += 2)
		vertices.emplace_back((float) lh::optNumber(args, count, i, 0.0), (float) lh::optNumber(args, count, i + 1, 0.0));
	return vertices;
}

static void lh_isConvex(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	answers[0] = lhat_bool(isConvex(verticesOf(arguments, count)));
	*answerCount = 1;
}

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------

static Transform *checkTransform(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index = 0)
{
	Transform *t = index < count ? lh::checkObject<Transform>(arguments[index], *binding.registry) : nullptr;
	if (t == nullptr)
		lh::raise(machine, "Expected a Transform");
	return t;
}

static void lh_newTransform(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	StrongRef<Transform> t(new Transform(), Acquire::NORETAIN);
	if (count > 0)
	{
		float x = (float) lh::optNumber(arguments, count, 0, 0.0);
		float y = (float) lh::optNumber(arguments, count, 1, 0.0);
		float a = (float) lh::optNumber(arguments, count, 2, 0.0);
		float sx = (float) lh::optNumber(arguments, count, 3, 1.0);
		float sy = (float) lh::optNumber(arguments, count, 4, sx);
		float ox = (float) lh::optNumber(arguments, count, 5, 0.0);
		float oy = (float) lh::optNumber(arguments, count, 6, 0.0);
		float kx = (float) lh::optNumber(arguments, count, 7, 0.0);
		float ky = (float) lh::optNumber(arguments, count, 8, 0.0);
		t->setTransformation(x, y, a, sx, sy, ox, oy, kx, ky);
	}
	answers[0] = lh::pushObject(machine, *binding.registry, t.get());
	*answerCount = 1;
}

// Each mutator answers the transform itself, so calls chain.
static void lh_Transform_translate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	t->translate((float) lh::optNumber(arguments, count, 1, 0.0), (float) lh::optNumber(arguments, count, 2, 0.0));
	answers[0] = arguments[0];
	*answerCount = 1;
}

static void lh_Transform_rotate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	t->rotate((float) lh::optNumber(arguments, count, 1, 0.0));
	answers[0] = arguments[0];
	*answerCount = 1;
}

static void lh_Transform_scale(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float sx = (float) lh::optNumber(arguments, count, 1, 1.0);
	t->scale(sx, (float) lh::optNumber(arguments, count, 2, sx));
	answers[0] = arguments[0];
	*answerCount = 1;
}

static void lh_Transform_shear(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	t->shear((float) lh::optNumber(arguments, count, 1, 0.0), (float) lh::optNumber(arguments, count, 2, 0.0));
	answers[0] = arguments[0];
	*answerCount = 1;
}

static void lh_Transform_reset(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	t->reset();
	answers[0] = arguments[0];
	*answerCount = 1;
}

static void lh_Transform_setTransformation(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
										   LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	float x = (float) lh::optNumber(arguments, count, 1, 0.0);
	float y = (float) lh::optNumber(arguments, count, 2, 0.0);
	float a = (float) lh::optNumber(arguments, count, 3, 0.0);
	float sx = (float) lh::optNumber(arguments, count, 4, 1.0);
	float sy = (float) lh::optNumber(arguments, count, 5, sx);
	float ox = (float) lh::optNumber(arguments, count, 6, 0.0);
	float oy = (float) lh::optNumber(arguments, count, 7, 0.0);
	float kx = (float) lh::optNumber(arguments, count, 8, 0.0);
	float ky = (float) lh::optNumber(arguments, count, 9, 0.0);
	t->setTransformation(x, y, a, sx, sy, ox, oy, kx, ky);
	answers[0] = arguments[0];
	*answerCount = 1;
}

static void lh_Transform_apply(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	Transform *other = t != nullptr ? checkTransform(machine, arguments, count, 1) : nullptr;
	if (t == nullptr || other == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	t->apply(other);
	answers[0] = arguments[0];
	*answerCount = 1;
}

static void lh_Transform_inverse(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	StrongRef<Transform> inverse(t->inverse(), Acquire::NORETAIN);
	answers[0] = lh::pushObject(machine, *binding.registry, inverse.get());
	*answerCount = 1;
}

static void lh_Transform_transformPoint(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	Vector2 p = t->transformPoint(Vector2((float) lh::optNumber(arguments, count, 1, 0.0), (float) lh::optNumber(arguments, count, 2, 0.0)));
	answers[0] = lhat_real(p.x);
	answers[1] = lhat_real(p.y);
	*answerCount = 2;
}

static void lh_Transform_inverseTransformPoint(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
											   LhatValue *answers, int *answerCount)
{
	(void) context;
	Transform *t = checkTransform(machine, arguments, count);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	Vector2 p = t->inverseTransformPoint(Vector2((float) lh::optNumber(arguments, count, 1, 0.0), (float) lh::optNumber(arguments, count, 2, 0.0)));
	answers[0] = lhat_real(p.x);
	answers[1] = lhat_real(p.y);
	*answerCount = 2;
}

} // math

namespace lh
{

bool lhopen_love_math(Context &ctx)
{
	using namespace love::math;
	const char *m = "love.math";

	if (!ctx.objectType(m, "RandomGenerator", RandomGenerator::type) || !ctx.objectType(m, "Transform", Transform::type))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;

	return ctx.func(m, "random", "p^ -> number^;", lh_random, nullptr)
		&& ctx.func(m, "random", "p^number^ -> number^;", lh_random, nullptr)
		&& ctx.func(m, "random", "p^number^, number^ -> number^;", lh_random, nullptr)
		&& ctx.func(m, "randomNormal", "p^ -> number^;", lh_randomNormal, nullptr)
		&& ctx.func(m, "randomNormal", "p^number^ -> number^;", lh_randomNormal, nullptr)
		&& ctx.func(m, "randomNormal", "p^number^, number^ -> number^;", lh_randomNormal, nullptr)
		&& ctx.func(m, "setRandomSeed", "p^number^;", lh_setRandomSeed, nullptr)
		&& ctx.func(m, "setRandomSeed", "p^number^, number^;", lh_setRandomSeed, nullptr)
		&& ctx.func(m, "getRandomSeed", "f^ -> (number^, number^);", lh_getRandomSeed, nullptr)
		&& ctx.func(m, "getRandomState", "f^ -> string^;", lh_getRandomState, nullptr)
		&& ctx.func(m, "setRandomState", "p^string^;", lh_setRandomState, nullptr)
		&& ctx.func(m, "newRandomGenerator", "p^ -> love.math.RandomGenerator;", lh_newRandomGenerator, nullptr)
		&& ctx.func(m, "newRandomGenerator", "p^number^ -> love.math.RandomGenerator;", lh_newRandomGenerator, nullptr)
		&& ctx.func(m, "newRandomGenerator", "p^number^, number^ -> love.math.RandomGenerator;", lh_newRandomGenerator, nullptr)
		&& ctx.member(m, "RandomGenerator", "random", "p^self^ -> number^;", lh_RandomGenerator_random, nullptr)
		&& ctx.member(m, "RandomGenerator", "random", "p^self^, number^ -> number^;", lh_RandomGenerator_random, nullptr)
		&& ctx.member(m, "RandomGenerator", "random", "p^self^, number^, number^ -> number^;", lh_RandomGenerator_random, nullptr)
		&& ctx.member(m, "RandomGenerator", "randomNormal", "p^self^, number^, number^ -> number^;", lh_RandomGenerator_randomNormal, nullptr)
		&& ctx.member(m, "RandomGenerator", "setSeed", "p^self^, number^;", lh_RandomGenerator_setSeed, nullptr)
		&& ctx.member(m, "RandomGenerator", "setSeed", "p^self^, number^, number^;", lh_RandomGenerator_setSeed, nullptr)
		&& ctx.member(m, "RandomGenerator", "getSeed", "f^self^ -> (number^, number^);", lh_RandomGenerator_getSeed, nullptr)
		&& ctx.member(m, "RandomGenerator", "getState", "f^self^ -> string^;", lh_RandomGenerator_getState, nullptr)
		&& ctx.member(m, "RandomGenerator", "setState", "p^self^, string^;", lh_RandomGenerator_setState, nullptr)
		&& ctx.func(m, "noise", "f^number^, ... -> number^;", lh_noise, nullptr)
		&& ctx.func(m, "gammaToLinear", "f^number^ -> number^;", lh_gammaToLinear, nullptr)
		&& ctx.func(m, "gammaToLinear", "f^number^, number^, number^ -> (number^, number^, number^);", lh_gammaToLinear, nullptr)
		&& ctx.func(m, "gammaToLinear", "f^number^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_gammaToLinear, nullptr)
		&& ctx.func(m, "linearToGamma", "f^number^ -> number^;", lh_linearToGamma, nullptr)
		&& ctx.func(m, "linearToGamma", "f^number^, number^, number^ -> (number^, number^, number^);", lh_linearToGamma, nullptr)
		&& ctx.func(m, "linearToGamma", "f^number^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_linearToGamma, nullptr)
		&& ctx.func(m, "colorToBytes", "f^number^, number^, number^ -> (number^, number^, number^);", lh_colorToBytes, nullptr)
		&& ctx.func(m, "colorToBytes", "f^number^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_colorToBytes, nullptr)
		&& ctx.func(m, "colorFromBytes", "f^number^, number^, number^ -> (number^, number^, number^);", lh_colorFromBytes, nullptr)
		&& ctx.func(m, "colorFromBytes", "f^number^, number^, number^, number^ -> (number^, number^, number^, number^);", lh_colorFromBytes, nullptr)
		&& ctx.func(m, "isConvex", "f^number^, number^, number^, number^, number^, number^, ... -> bool^;", lh_isConvex, nullptr)
		&& ctx.func(m, "newTransform", "p^... -> love.math.Transform;", lh_newTransform, nullptr)
		&& ctx.member(m, "Transform", "translate", "p^self^, number^, number^ -> Self^;", lh_Transform_translate, nullptr)
		&& ctx.member(m, "Transform", "rotate", "p^self^, number^ -> Self^;", lh_Transform_rotate, nullptr)
		&& ctx.member(m, "Transform", "scale", "p^self^, number^ -> Self^;", lh_Transform_scale, nullptr)
		&& ctx.member(m, "Transform", "scale", "p^self^, number^, number^ -> Self^;", lh_Transform_scale, nullptr)
		&& ctx.member(m, "Transform", "shear", "p^self^, number^, number^ -> Self^;", lh_Transform_shear, nullptr)
		&& ctx.member(m, "Transform", "reset", "p^self^ -> Self^;", lh_Transform_reset, nullptr)
		&& ctx.member(m, "Transform", "setTransformation", "p^self^, number^, number^, ... -> Self^;", lh_Transform_setTransformation, nullptr)
		&& ctx.member(m, "Transform", "apply", "p^self^, Self^ -> Self^;", lh_Transform_apply, nullptr)
		&& ctx.member(m, "Transform", "inverse", "p^self^ -> Self^;", lh_Transform_inverse, nullptr)
		&& ctx.member(m, "Transform", "transformPoint", "f^self^, number^, number^ -> (number^, number^);", lh_Transform_transformPoint, nullptr)
		&& ctx.member(m, "Transform", "inverseTransformPoint", "p^self^, number^, number^ -> (number^, number^);", lh_Transform_inverseTransformPoint, nullptr);
}

} // lh
} // love

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

// love.graphics.Shader and love.graphics.Quad for L^. The references are
// wrap_Shader.cpp, wrap_Quad.cpp and wrap_Graphics.cpp's newShader beside
// this file.
//
// send(name, ...) takes what the uniform is: numbers (one per component,
// or one table of numbers per array element), booleans, a Texture, or --
// for a matrix -- a flat table of columns*rows numbers in column-major
// order, a table of rows, or a love.math.Transform for a mat4.

#include "lh_Graphics.h"

#include "Shader.h"
#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/FileData.h"
#include "modules/math/Transform.h"
#include "modules/math/MathModule.h"

#include <cstring>

namespace love
{
namespace graphics
{

#define instance() graphicsInstance()
#define binding graphicsBinding

// ---------------------------------------------------------------------------
// Quad
// ---------------------------------------------------------------------------

// newQuad(x, y, w, h, sw, sh) / newQuad(x, y, w, h, texture)
static void lh_newQuad(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	Quad::Viewport v;
	v.x = lh::optNumber(args, count, 0, 0);
	v.y = lh::optNumber(args, count, 1, 0);
	v.w = lh::optNumber(args, count, 2, 0);
	v.h = lh::optNumber(args, count, 3, 0);
	double sw = 0, sh = 0;
	if (count >= 5 && lhat_is_object_kind(args[4], LHAT_OBJECT_HOSTDATA))
	{
		Texture *t = checkTexture(machine, args, count, 4);
		if (t == nullptr)
		{
			answers[0] = lhat_nil();
			*answerCount = 1;
			return;
		}
		sw = t->getWidth();
		sh = t->getHeight();
	}
	else
	{
		sw = lh::optNumber(args, count, 4, 0);
		sh = lh::optNumber(args, count, 5, 0);
	}
	lh::guard(machine, [&]() {
		StrongRef<Quad> quad(instance()->newQuad(v, sw, sh), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, quad.get());
		*answerCount = 1;
	});
}

#define QUAD_SELF() Quad *q = checkQuad(machine, args, count, 0); if (q == nullptr) return

static void lh_Quad_setViewport(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	QUAD_SELF();
	Quad::Viewport v;
	v.x = lh::optNumber(args, count, 1, 0);
	v.y = lh::optNumber(args, count, 2, 0);
	v.w = lh::optNumber(args, count, 3, 0);
	v.h = lh::optNumber(args, count, 4, 0);
	if (count >= 7)
		q->refresh(v, lh::optNumber(args, count, 5, 0), lh::optNumber(args, count, 6, 0));
	else
		q->setViewport(v);
}

static void lh_Quad_getViewport(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	QUAD_SELF();
	Quad::Viewport v = q->getViewport();
	float values[4] = {(float) v.x, (float) v.y, (float) v.w, (float) v.h};
	numberTuple(values, 4, answers, answerCount);
}

static void lh_Quad_getTextureDimensions(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										 LhatValue *answers, int *answerCount)
{
	(void) context;
	QUAD_SELF();
	float values[2] = {(float) q->getTextureWidth(), (float) q->getTextureHeight()};
	numberTuple(values, 2, answers, answerCount);
}

static void lh_Quad_setLayer(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	QUAD_SELF();
	q->setLayer((int) lh::optNumber(args, count, 1, 0));
}

static void lh_Quad_getLayer(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	QUAD_SELF();
	answers[0] = lhat_integer(q->getLayer());
	*answerCount = 1;
}

bool lhGraphicsQuad(lh::Context &ctx)
{
	const char *m = LH_GRAPHICS;
	if (!ctx.objectType(m, "Quad", Quad::type))
		return false;
	if (ctx.types())
		return true;
	const char *Q = "Quad";
	return ctx.func(m, "newQuad", "p^number^, number^, number^, number^, number^, number^ -> love.graphics.Quad;", lh_newQuad, nullptr)
		&& ctx.func(m, "newQuad", "p^number^, number^, number^, number^, love.graphics.Texture -> love.graphics.Quad;", lh_newQuad, nullptr)
		&& ctx.member(m, Q, "setViewport", "p^self^, number^, number^, number^, number^;", lh_Quad_setViewport, nullptr)
		&& ctx.member(m, Q, "setViewport", "p^self^, number^, number^, number^, number^, number^, number^;", lh_Quad_setViewport, nullptr)
		&& ctx.member(m, Q, "getViewport", "f^self^ -> (number^, number^, number^, number^);", lh_Quad_getViewport, nullptr)
		&& ctx.member(m, Q, "getTextureDimensions", "f^self^ -> (number^, number^);", lh_Quad_getTextureDimensions, nullptr)
		&& ctx.member(m, Q, "setLayer", "p^self^, number^;", lh_Quad_setLayer, nullptr)
		&& ctx.member(m, Q, "getLayer", "f^self^ -> number^;", lh_Quad_getLayer, nullptr);
}

// ---------------------------------------------------------------------------
// Shader
// ---------------------------------------------------------------------------

// A stage argument: a file the filesystem can read, else the code itself.
static bool shaderSource(LhatMachine *machine, LhatValue value, std::string &out)
{
	size_t length = 0;
	const char *text = lh::stringOf(value, &length);
	if (text != nullptr)
	{
		std::string s(text, length);
		auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
		love::filesystem::Filesystem::Info info = {};
		if (fs != nullptr && s.find('\n') == std::string::npos && fs->getInfo(s.c_str(), info))
		{
			try
			{
				StrongRef<love::filesystem::FileData> data(fs->read(s.c_str()), Acquire::NORETAIN);
				out.assign((const char *) data->getData(), data->getSize());
				return true;
			}
			catch (const love::Exception &e)
			{
				lh::raise(machine, e.what());
				return false;
			}
		}
		out = s;
		return true;
	}
	auto *data = lh::checkObject<love::filesystem::FileData>(value, *binding.registry);
	if (data != nullptr)
	{
		out.assign((const char *) data->getData(), data->getSize());
		return true;
	}
	lh::raise(machine, "A shader stage is a string or a FileData");
	return false;
}

// newShader(code | path | FileData[, code | path | FileData])
static void lh_newShader(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<std::string> stages;
	for (size_t i = 0; i < count && i < 2; i++)
	{
		std::string source;
		if (!shaderSource(machine, args[i], source))
		{
			answers[0] = lhat_nil();
			*answerCount = 1;
			return;
		}
		stages.push_back(source);
	}
	if (stages.empty())
	{
		lh::raise(machine, "newShader needs at least one stage");
		return;
	}
	Shader::CompileOptions options;
	try
	{
		StrongRef<Shader> shader(instance()->newShader(stages, options), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, shader.get());
		*answerCount = 1;
		return;
	}
	catch (const love::Exception &e)
	{
		answers[0] = lh::fail(machine, binding.errors->graphicsShaderFailed, e.what());
		*answerCount = 1;
		return;
	}
}

// validateShader(gles, code[, code]) -> (ok, message)
static void lh_validateShader(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	bool gles = lh::optBool(args, count, 0, false);
	std::vector<std::string> stages;
	for (size_t i = 1; i < count && i < 3; i++)
	{
		std::string source;
		if (!shaderSource(machine, args[i], source))
		{
			answers[0] = lhat_nil();
			*answerCount = 1;
			return;
		}
		stages.push_back(source);
	}
	Shader::CompileOptions options;
	std::string err;
	bool ok = false;
	// The compiler throws on code it cannot even parse, which is exactly
	// what this function is asked about -- so that is an answer, not a fault.
	try
	{
		ok = instance()->validateShader(gles, stages, options, err);
	}
	catch (const love::Exception &e)
	{
		ok = false;
		err = e.what();
	}
	LhatValue message = lhat_nil();
	lh::makeString(machine, err, &message);
	answers[0] = lhat_bool(ok);
	answers[1] = message;
	*answerCount = 2;
}

#define SHADER_SELF() Shader *s = count > 0 ? lh::checkObject<Shader>(args[0], *binding.registry) : nullptr; if (s == nullptr) { lh::raise(machine, "Expected a Shader"); return; }

// The array element count a send carries: the values given, capped to
// what the uniform holds.
static int sendCount(size_t given, const Shader::UniformInfo *info)
{
	return (int) std::min(given, (size_t) info->count);
}

static void sendFloats(LhatMachine *machine, Shader *s, const Shader::UniformInfo *info, const LhatValue *args, size_t count, bool colors)
{
	int components = info->components;
	float *values = info->floats;
	int n = 0;
	if (components == 1)
	{
		n = sendCount(count, info);
		for (int i = 0; i < n; i++)
			values[i] = (float) lh::optNumber(args, count, i, 0.0);
	}
	else
	{
		// One table of numbers per element, or the components written out
		// for a single element.
		if (count > 0 && lhat_is_number(args[0]))
		{
			n = 1;
			for (int c = 0; c < components; c++)
				values[c] = (float) lh::optNumber(args, count, c, 0.0);
		}
		else
		{
			n = sendCount(count, info);
			for (int i = 0; i < n; i++)
			{
				std::vector<float> parts;
				if (!numbersOf(args[i], parts))
				{
					lh::raise(machine, "Shader uniform '" + info->name + "' needs a table of " + std::to_string(components) + " numbers per element");
					return;
				}
				for (int c = 0; c < components; c++)
					values[i * components + c] = c < (int) parts.size() ? parts[c] : 0.0f;
			}
		}
	}
	if (colors && graphics::isGammaCorrect())
	{
		int gammacomponents = std::min(components, 3);
		for (int i = 0; i < n; i++)
			for (int j = 0; j < gammacomponents; j++)
				values[i * components + j] = math::gammaToLinear(values[i * components + j]);
	}
	lh::guard(machine, [&]() {
		s->updateUniform(info, n);
	});
}

static void sendInts(LhatMachine *machine, Shader *s, const Shader::UniformInfo *info, const LhatValue *args, size_t count)
{
	int components = info->components;
	int n = 0;
	if (components == 1)
	{
		n = sendCount(count, info);
		for (int i = 0; i < n; i++)
		{
			if (info->baseType == Shader::UNIFORM_UINT)
				info->uints[i] = (unsigned int) lh::optNumber(args, count, i, 0.0);
			else
				info->ints[i] = (int) lh::optNumber(args, count, i, 0.0);
		}
	}
	else if (count > 0 && lhat_is_number(args[0]))
	{
		n = 1;
		for (int c = 0; c < components; c++)
		{
			if (info->baseType == Shader::UNIFORM_UINT)
				info->uints[c] = (unsigned int) lh::optNumber(args, count, c, 0.0);
			else
				info->ints[c] = (int) lh::optNumber(args, count, c, 0.0);
		}
	}
	else
	{
		n = sendCount(count, info);
		for (int i = 0; i < n; i++)
		{
			std::vector<float> parts;
			if (!numbersOf(args[i], parts))
			{
				lh::raise(machine, "Shader uniform '" + info->name + "' needs a table of " + std::to_string(components) + " numbers per element");
				return;
			}
			for (int c = 0; c < components; c++)
			{
				float v = c < (int) parts.size() ? parts[c] : 0.0f;
				if (info->baseType == Shader::UNIFORM_UINT)
					info->uints[i * components + c] = (unsigned int) v;
				else
					info->ints[i * components + c] = (int) v;
			}
		}
	}
	lh::guard(machine, [&]() {
		s->updateUniform(info, n);
	});
}

static void sendBooleans(LhatMachine *machine, Shader *s, const Shader::UniformInfo *info, const LhatValue *args, size_t count)
{
	int components = info->components;
	int n = sendCount(count, info);
	if (components == 1)
	{
		for (int i = 0; i < n; i++)
			info->ints[i] = lh::optBool(args, count, i, false) ? 1 : 0;
	}
	else
	{
		for (int i = 0; i < n; i++)
		{
			if (!lhat_is_object_kind(args[i], LHAT_OBJECT_TABLE))
			{
				lh::raise(machine, "Shader uniform '" + info->name + "' needs a table of booleans per element");
				return;
			}
			const LhatTable *t = (const LhatTable *) lhat_as_object(args[i]);
			for (int c = 0; c < components; c++)
			{
				LhatValue v = lhat_table_get(t, lhat_integer(c));
				info->ints[i * components + c] = (lhat_is_bool(v) && lhat_as_bool(v)) ? 1 : 0;
			}
		}
	}
	lh::guard(machine, [&]() {
		s->updateUniform(info, n);
	});
}

static void sendMatrices(LhatMachine *machine, Shader *s, const Shader::UniformInfo *info, const LhatValue *args, size_t count)
{
	int columns = info->matrix.columns;
	int rows = info->matrix.rows;
	int elements = columns * rows;
	float *values = info->floats;
	int n = sendCount(count, info);
	for (int i = 0; i < n; i++)
	{
		auto *transform = lh::checkObject<love::math::Transform>(args[i], *binding.registry);
		if (transform != nullptr && columns == 4 && rows == 4)
		{
			memcpy(&values[i * 16], transform->getMatrix().getElements(), sizeof(float) * 16);
			continue;
		}
		if (!lhat_is_object_kind(args[i], LHAT_OBJECT_TABLE))
		{
			lh::raise(machine, "Shader uniform '" + info->name + "' needs a table per matrix");
			return;
		}
		const LhatTable *t = (const LhatTable *) lhat_as_object(args[i]);
		LhatValue first = lhat_table_get(t, lhat_integer(0));
		int base = i * elements;
		if (lhat_is_object_kind(first, LHAT_OBJECT_TABLE))
		{
			// A table of rows, as written on paper; stored column-major.
			for (int row = 0; row < rows; row++)
			{
				std::vector<float> line;
				numbersOf(lhat_table_get(t, lhat_integer(row)), line);
				for (int column = 0; column < columns; column++)
					values[base + column * rows + row] = column < (int) line.size() ? line[column] : 0.0f;
			}
		}
		else
		{
			// A flat table, already column-major.
			std::vector<float> flat;
			numbersOf(args[i], flat);
			for (int k = 0; k < elements; k++)
				values[base + k] = k < (int) flat.size() ? flat[k] : 0.0f;
		}
	}
	lh::guard(machine, [&]() {
		s->updateUniform(info, n);
	});
}

static void sendTextures(LhatMachine *machine, Shader *s, const Shader::UniformInfo *info, const LhatValue *args, size_t count)
{
	int n = sendCount(count, info);
	std::vector<Texture *> textures;
	for (int i = 0; i < n; i++)
	{
		Texture *t = checkTexture(machine, args, count, i);
		if (t == nullptr)
			return;
		textures.push_back(t);
	}
	lh::guard(machine, [&]() {
		s->sendTextures(info, textures.data(), n);
	});
}

// send(name, ...) / sendColor(name, ...)
static void sendTo(LhatMachine *machine, const LhatValue *args, size_t count, bool colors)
{
	SHADER_SELF();
	std::string name = lh::optString(args, count, 1, "");
	const Shader::UniformInfo *info = s->getUniformInfo(name);
	if (info == nullptr)
	{
		lh::raise(machine, "Shader uniform '" + name + "' does not exist.\nA common error is to define but not use the variable.");
		return;
	}
	const LhatValue *values = args + 2;
	size_t given = count > 2 ? count - 2 : 0;
	if (given == 0)
	{
		lh::raise(machine, "Shader uniform '" + name + "' needs a value");
		return;
	}
	switch (info->baseType)
	{
	case Shader::UNIFORM_FLOAT:
		sendFloats(machine, s, info, values, given, colors);
		return;
	case Shader::UNIFORM_MATRIX:
		sendMatrices(machine, s, info, values, given);
		return;
	case Shader::UNIFORM_INT:
	case Shader::UNIFORM_UINT:
		sendInts(machine, s, info, values, given);
		return;
	case Shader::UNIFORM_BOOL:
		sendBooleans(machine, s, info, values, given);
		return;
	case Shader::UNIFORM_SAMPLER:
	case Shader::UNIFORM_STORAGETEXTURE:
		sendTextures(machine, s, info, values, given);
		return;
	default:
		{
			lh::raise(machine, "Shader uniform '" + name + "' has a type send cannot fill");
			return;
		}
	}
}

static void lh_Shader_send(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	sendTo(machine, args, count, false);
}

static void lh_Shader_sendColor(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	sendTo(machine, args, count, true);
}

static void lh_Shader_hasUniform(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	SHADER_SELF();
	answers[0] = lhat_bool(s->hasUniform(lh::optString(args, count, 1, "")));
	*answerCount = 1;
}

static void lh_Shader_getWarnings(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	SHADER_SELF();
	LhatValue out = lhat_nil();
	lh::makeString(machine, s->getWarnings(), &out);
	answers[0] = out;
	*answerCount = 1;
}

bool lhGraphicsShader(lh::Context &ctx)
{
	const char *m = LH_GRAPHICS;
	if (!ctx.objectType(m, "Shader", Shader::type))
		return false;
	if (ctx.types())
		return true;
	const char *S = "Shader";
	const char *src = "string^|love.filesystem.FileData";
	return ctx.func(m, "newShader", (std::string("p^") + src + " -> love.graphics.Shader|love.graphics.Error.ShaderFailed;").c_str(), lh_newShader, nullptr)
		&& ctx.func(m, "newShader", (std::string("p^") + src + ", " + src + " -> love.graphics.Shader|love.graphics.Error.ShaderFailed;").c_str(), lh_newShader, nullptr)
		&& ctx.func(m, "validateShader", (std::string("p^bool^, ") + src + " -> (bool^, string^);").c_str(), lh_validateShader, nullptr)
		&& ctx.func(m, "validateShader", (std::string("p^bool^, ") + src + ", " + src + " -> (bool^, string^);").c_str(), lh_validateShader, nullptr)
		&& ctx.member(m, S, "send", "p^self^, string^, ...;", lh_Shader_send, nullptr)
		&& ctx.member(m, S, "sendColor", "p^self^, string^, ...;", lh_Shader_sendColor, nullptr)
		&& ctx.member(m, S, "hasUniform", "f^self^, string^ -> bool^;", lh_Shader_hasUniform, nullptr)
		&& ctx.member(m, S, "getWarnings", "f^self^ -> string^;", lh_Shader_getWarnings, nullptr);
}

} // graphics
} // love

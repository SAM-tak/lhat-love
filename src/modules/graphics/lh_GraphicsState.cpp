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

// love.graphics for L^, the render state half: canvases (render targets),
// the active shader, blending, scissor, stencil, color mask, default
// filters, line style, the transform stack's extras, ellipses and arcs,
// renderer info and stats, and reading a texture back. The reference is
// wrap_Graphics.cpp beside this file.

#include "lh_Graphics.h"

#include "Shader.h"
#include "modules/image/Image.h"
#include "modules/image/ImageData.h"
#include "modules/math/Transform.h"

namespace love
{
namespace graphics
{

#define instance() graphicsInstance()
#define binding graphicsBinding

static LhatValue stringValue(LhatMachine *machine, const char *text)
{
	LhatValue out = lhat_nil();
	lh::makeString(machine, text != nullptr ? text : "", &out);
	return out;
}

// ---------------------------------------------------------------------------
// Canvases and shaders
// ---------------------------------------------------------------------------

// newCanvas([w, h[, settings]]): a 2D render-target texture at the screen's
// pixel density. `settings` may carry msaa, format, mipmaps ("none" /
// "auto" / "manual"), readable and dpiscale.
static void lh_newCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture::Settings s;
	s.renderTarget = true;
	s.width = (int) lh::optNumber(args, count, 0, instance()->getWidth());
	s.height = (int) lh::optNumber(args, count, 1, instance()->getHeight());
	s.dpiScale = (float) instance()->getScreenDPIScale();

	if (count >= 3 && lhat_is_object_kind(args[2], LHAT_OBJECT_TABLE))
	{
		LhatValue settings = args[2];
		s.msaa = (int) lh::fieldNumber(machine, settings, "msaa", 1);
		LhatValue formatValue = lh::field(machine, settings, "format");
		std::string format = lhat_is_nil(formatValue) ? "" : lh::enumName(machine, formatValue, "love.graphics", "PixelFormat");
		if (!lhat_is_nil(formatValue) && format.empty())
			return;
		if (!format.empty() && !getConstant(format.c_str(), s.format))
		{
			lh::raise(machine, "Invalid pixel format: " + format);
			return;
		}
		LhatValue mipmapsValue = lh::field(machine, settings, "mipmaps");
		std::string mipmaps = lhat_is_nil(mipmapsValue) ? "" : lh::enumName(machine, mipmapsValue, "love.graphics", "MipmapMode");
		if (!lhat_is_nil(mipmapsValue) && mipmaps.empty())
			return;
		if (!mipmaps.empty() && !Texture::getConstant(mipmaps.c_str(), s.mipmaps))
		{
			lh::raise(machine, "Invalid mipmap mode: " + mipmaps);
			return;
		}
		if (lh::fieldIs(machine, settings, "readable", LHAT_VALUE_BOOL))
			s.readable.set(lh::fieldBool(machine, settings, "readable", true));
		s.dpiScale = (float) lh::fieldNumber(machine, settings, "dpiscale", s.dpiScale);
	}

	lh::guard(machine, [&]() {
		StrongRef<Texture> texture(instance()->newTexture(s), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, texture.get());
		*answerCount = 1;
	});
}

// setCanvas(texture, ...[, depth[, stencil]]) / setCanvas(): the color
// targets, then two optional booleans asking for temporary depth and
// stencil buffers. One variadic arm, since a list-of-Texture arm is not
// told apart from the Texture one at registration.
static void lh_setCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count == 0)
	{
		lh::guard(machine, [&]() {
			instance()->setRenderTarget();
		});
		return;
	}

	Graphics::RenderTargets targets;
	size_t at = 0;
	for (; at < count && lhat_is_object_kind(args[at], LHAT_OBJECT_HOSTDATA); at++)
	{
		Texture *texture = checkTexture(machine, args, count, at);
		if (texture == nullptr)
			return;
		targets.colors.emplace_back(texture, 0);
	}
	if (targets.colors.empty())
	{
		lh::raise(machine, "setCanvas needs at least one Texture");
		return;
	}
	if (lh::optBool(args, count, at, false))
		targets.temporaryRTFlags |= Graphics::TEMPORARY_RT_DEPTH;
	if (lh::optBool(args, count, at + 1, false))
		targets.temporaryRTFlags |= Graphics::TEMPORARY_RT_STENCIL;
	lh::guard(machine, [&]() {
		instance()->setRenderTargets(targets);
	});
}

// getCanvas() -> the first color target, nil^ when drawing to the screen.
static void lh_getCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	Graphics::RenderTargets targets = instance()->getRenderTargets();
	if (targets.colors.empty() || targets.colors[0].texture == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lh::pushObject(machine, *binding.registry, targets.colors[0].texture);
	*answerCount = 1;
}

static void lh_setShader(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count == 0)
	{
		lh::guard(machine, [&]() {
			instance()->setShader();
		});
		return;
	}
	Shader *shader = lh::checkObject<Shader>(args[0], *binding.registry);
	if (shader == nullptr)
	{
		lh::raise(machine, "Expected a Shader");
		return;
	}
	lh::guard(machine, [&]() {
		instance()->setShader(shader);
	});
}

static void lh_getShader(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	Shader *shader = instance()->getShader();
	answers[0] = shader != nullptr ? lh::pushObject(machine, *binding.registry, shader) : lhat_nil();
	*answerCount = 1;
}

// ---------------------------------------------------------------------------
// Blending, scissor, stencil, masks
// ---------------------------------------------------------------------------

// setBlendMode(mode[, alphamode])
static void lh_setBlendMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string modestr = lh::optEnum(machine, args, count, 0, "love.graphics", "BlendMode", "alpha");
	BlendMode mode;
	if (!getConstant(modestr.c_str(), mode))
	{
		lh::raise(machine, "Invalid blend mode: " + modestr);
		return;
	}
	BlendAlpha alpha = BLENDALPHA_MULTIPLY;
	if (count >= 2)
	{
		std::string alphastr = lh::optEnum(machine, args, count, 1, "love.graphics", "BlendAlphaMode", "alphamultiply");
		if (!getConstant(alphastr.c_str(), alpha))
		{
			lh::raise(machine, "Invalid blend alpha mode: " + alphastr);
			return;
		}
	}
	lh::guard(machine, [&]() {
		instance()->setBlendMode(mode, alpha);
	});
}

static void lh_getBlendMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	BlendAlpha alpha = BLENDALPHA_MULTIPLY;
	BlendMode mode = instance()->getBlendMode(alpha);
	const char *modestr = "alpha";
	const char *alphastr = "alphamultiply";
	getConstant(mode, modestr);
	getConstant(alpha, alphastr);
	answers[0] = lh::pushEnum(machine, "love.graphics", "BlendMode", modestr);
	answers[1] = lh::pushEnum(machine, "love.graphics", "BlendAlphaMode", alphastr);
	*answerCount = 2;
}

// setScissor(x, y, w, h) / setScissor() to disable.
static void lh_setScissor(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						  LhatValue *answers, int *answerCount)
{
	(void) context;
	lh::guard(machine, [&]() {
		if (count == 0)
			instance()->setScissor();
		else
		{
			FRect rect = {(float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0), (float) lh::optNumber(args, count, 2, 0), (float) lh::optNumber(args, count, 3, 0)};
			if (rect.w < 0 || rect.h < 0)
			{
				lh::raise(machine, "Can't set scissor with negative width and/or height.");
				return;
			}
			instance()->setScissor(rect);
		}
	});
}

static void lh_intersectScissor(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	FRect rect = {(float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0), (float) lh::optNumber(args, count, 2, 0), (float) lh::optNumber(args, count, 3, 0)};
	if (rect.w < 0 || rect.h < 0)
	{
		lh::raise(machine, "Can't set scissor with negative width and/or height.");
		return;
	}
	lh::guard(machine, [&]() {
		instance()->intersectScissor(rect);
	});
}

// getScissor() -> (x, y, w, h), all zero when none is set.
static void lh_getScissor(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	// The rectangle Graphics keeps is the last one set, whether or not the
	// scissor is on; a scissor that is off answers zeros here.
	FRect rect = {0, 0, 0, 0};
	if (!instance()->getScissor(rect))
		rect = {0, 0, 0, 0};
	float values[4] = {rect.x, rect.y, rect.w, rect.h};
	numberTuple(values, 4, answers, answerCount);
}

// setStencilMode(mode[, value]) / setStencilMode() to disable.
static void lh_setStencilMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	lh::guard(machine, [&]() {
		if (count == 0)
		{
			instance()->setStencilMode();
			return;
		}
		std::string modestr = lh::optEnum(machine, args, count, 0, "love.graphics", "StencilMode", "off");
		StencilMode mode;
		if (!getConstant(modestr.c_str(), mode))
		{
			lh::raise(machine, "Invalid stencil mode: " + modestr);
			return;
		}
		instance()->setStencilMode(mode, (int) lh::optNumber(args, count, 1, 1));
	});
}

static void lh_getStencilMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	int value = 0;
	StencilMode mode = instance()->getStencilMode(value);
	const char *modestr = "off";
	getConstant(mode, modestr);
	answers[0] = lh::pushEnum(machine, "love.graphics", "StencilMode", modestr);
	answers[1] = lhat_integer(value);
	*answerCount = 2;
}

// setColorMask(r, g, b, a) / setColorMask() for all.
static void lh_setColorMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	ColorChannelMask mask;
	if (count >= 4)
	{
		mask.r = lh::optBool(args, count, 0, true);
		mask.g = lh::optBool(args, count, 1, true);
		mask.b = lh::optBool(args, count, 2, true);
		mask.a = lh::optBool(args, count, 3, true);
	}
	lh::guard(machine, [&]() {
		instance()->setColorMask(mask);
	});
}

static void lh_getColorMask(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	ColorChannelMask mask = instance()->getColorMask();
	answers[0] = lhat_bool(mask.r);
	answers[1] = lhat_bool(mask.g);
	answers[2] = lhat_bool(mask.b);
	answers[3] = lhat_bool(mask.a);
	*answerCount = 4;
}

// ---------------------------------------------------------------------------
// Defaults and line style
// ---------------------------------------------------------------------------

// setDefaultFilter(min[, mag[, anisotropy]])
static void lh_setDefaultFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	SamplerState s = instance()->getDefaultSamplerState();
	std::string minstr = lh::optEnum(machine, args, count, 0, "love.graphics", "FilterMode", "linear");
	std::string magstr = lh::optEnum(machine, args, count, 1, "love.graphics", "FilterMode", minstr);
	if (!SamplerState::getConstant(minstr.c_str(), s.minFilter))
	{
		lh::raise(machine, "Invalid filter mode: " + minstr);
		return;
	}
	if (!SamplerState::getConstant(magstr.c_str(), s.magFilter))
	{
		lh::raise(machine, "Invalid filter mode: " + magstr);
		return;
	}
	s.maxAnisotropy = (uint8) std::min(std::max(1.0, lh::optNumber(args, count, 2, 1.0)), (double) LOVE_UINT8_MAX);
	instance()->setDefaultSamplerState(s);
}

static void lh_getDefaultFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	const SamplerState &s = instance()->getDefaultSamplerState();
	const char *minstr = "linear";
	const char *magstr = "linear";
	SamplerState::getConstant(s.minFilter, minstr);
	SamplerState::getConstant(s.magFilter, magstr);
	answers[0] = lh::pushEnum(machine, "love.graphics", "FilterMode", minstr);
	answers[1] = lh::pushEnum(machine, "love.graphics", "FilterMode", magstr);
	answers[2] = lhat_integer(s.maxAnisotropy);
	*answerCount = 3;
}

static void lh_setLineStyle(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string name = lh::optEnum(machine, args, count, 0, "love.graphics", "LineStyle", "smooth");
	Graphics::LineStyle style;
	if (!Graphics::getConstant(name.c_str(), style))
	{
		lh::raise(machine, "Invalid line style: " + name);
		return;
	}
	instance()->setLineStyle(style);
}

static void lh_getLineStyle(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	const char *name = "smooth";
	Graphics::getConstant(instance()->getLineStyle(), name);
	answers[0] = lh::pushEnum(machine, "love.graphics", "LineStyle", name);
	*answerCount = 1;
}

static void lh_setLineJoin(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	std::string name = lh::optEnum(machine, args, count, 0, "love.graphics", "LineJoin", "miter");
	Graphics::LineJoin join;
	if (!Graphics::getConstant(name.c_str(), join))
	{
		lh::raise(machine, "Invalid line join: " + name);
		return;
	}
	instance()->setLineJoin(join);
}

static void lh_getLineJoin(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	const char *name = "miter";
	Graphics::getConstant(instance()->getLineJoin(), name);
	answers[0] = lh::pushEnum(machine, "love.graphics", "LineJoin", name);
	*answerCount = 1;
}

static void lh_setWireframe(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	lh::guard(machine, [&]() {
		instance()->setWireframe(lh::optBool(args, count, 0, false));
	});
}

static void lh_isWireframe(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	answers[0] = lhat_bool(instance()->isWireframe());
	*answerCount = 1;
}

static void lh_getPointSize(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	answers[0] = lhat_real(instance()->getPointSize());
	*answerCount = 1;
}

static void lh_reset(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	lh::guard(machine, [&]() {
		instance()->reset();
	});
}

// ---------------------------------------------------------------------------
// Transforms
// ---------------------------------------------------------------------------

static void lh_shear(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) machine;
	instance()->shear((float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0));
}

static love::math::Transform *checkTransform(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	auto *transform = index < count ? lh::checkObject<love::math::Transform>(args[index], *binding.registry) : nullptr;
	if (transform == nullptr)
		lh::raise(machine, "Expected a Transform");
	return transform;
}

static void lh_applyTransform(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	auto *transform = checkTransform(machine, args, count, 0);
	if (transform == nullptr)
		return;
	lh::guard(machine, [&]() {
		instance()->applyTransform(transform->getMatrix());
	});
}

static void lh_replaceTransform(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	auto *transform = checkTransform(machine, args, count, 0);
	if (transform == nullptr)
		return;
	lh::guard(machine, [&]() {
		instance()->replaceTransform(transform->getMatrix());
	});
}

static void lh_transformPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	Vector2 p((float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0));
	p = instance()->transformPoint(p);
	float values[2] = {p.x, p.y};
	numberTuple(values, 2, answers, answerCount);
}

static void lh_inverseTransformPoint(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	Vector2 p((float) lh::optNumber(args, count, 0, 0), (float) lh::optNumber(args, count, 1, 0));
	p = instance()->inverseTransformPoint(p);
	float values[2] = {p.x, p.y};
	numberTuple(values, 2, answers, answerCount);
}

static void lh_getStackDepth(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	answers[0] = lhat_integer((int64_t) instance()->getStackDepth());
	*answerCount = 1;
}

// ---------------------------------------------------------------------------
// More shapes
// ---------------------------------------------------------------------------

// ellipse(mode, x, y, a, b[, segments])
static void lh_ellipse(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, args[0], mode))
		return;
	float x = (float) lh::optNumber(args, count, 1, 0), y = (float) lh::optNumber(args, count, 2, 0);
	float a = (float) lh::optNumber(args, count, 3, 0), b = (float) lh::optNumber(args, count, 4, a);
	lh::guard(machine, [&]() {
		if (count >= 6)
			instance()->ellipse(mode, x, y, a, b, (int) lh::optNumber(args, count, 5, 10));
		else
			instance()->ellipse(mode, x, y, a, b);
	});
}

// arc(mode[, arctype], x, y, r, angle1, angle2[, segments])
static void lh_arc(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
				   LhatValue *answers, int *answerCount)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, args[0], mode))
		return;
	Graphics::ArcMode arcmode = Graphics::ARC_PIE;
	size_t at = 1;
	if (count >= 2 && !lhat_is_number(args[1]))
	{
		std::string name = lh::optEnum(machine, args, count, 1, "love.graphics", "ArcMode", "pie");
		if (!Graphics::getConstant(name.c_str(), arcmode))
		{
			lh::raise(machine, "Invalid arc mode: " + name);
			return;
		}
		at = 2;
	}
	float x = (float) lh::optNumber(args, count, at, 0), y = (float) lh::optNumber(args, count, at + 1, 0);
	float r = (float) lh::optNumber(args, count, at + 2, 0);
	float a1 = (float) lh::optNumber(args, count, at + 3, 0), a2 = (float) lh::optNumber(args, count, at + 4, 0);
	lh::guard(machine, [&]() {
		if (count > at + 5)
			instance()->arc(mode, arcmode, x, y, r, a1, a2, (int) lh::optNumber(args, count, at + 5, 10));
		else
			instance()->arc(mode, arcmode, x, y, r, a1, a2);
	});
}

// ---------------------------------------------------------------------------
// Information
// ---------------------------------------------------------------------------

static void lh_getRendererInfo(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	Graphics::RendererInfo info = instance()->getRendererInfo();
	answers[0] = stringValue(machine, info.name.c_str());
	answers[1] = stringValue(machine, info.version.c_str());
	answers[2] = stringValue(machine, info.vendor.c_str());
	answers[3] = stringValue(machine, info.device.c_str());
	*answerCount = 4;
}

// getStats() -> a table of the frame's counts.
static void lh_getStats(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	Graphics::Stats stats = instance()->getStats();
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	struct { const char *name; int64_t value; } fields[] = {
		{"drawcalls", stats.drawCalls},
		{"drawcallsbatched", stats.drawCallsBatched},
		{"canvasswitches", stats.renderTargetSwitches},
		{"shaderswitches", stats.shaderSwitches},
		{"textures", stats.textures},
		{"fonts", stats.fonts},
		{"buffers", stats.buffers},
		{"texturememory", stats.textureMemory},
		{"buffermemory", stats.bufferMemory},
	};
	for (const auto &f : fields)
	{
		LhatValue key = lhat_nil();
		bool refused = false;
		if (lh::makeString(machine, f.name, &key))
			lhat_table_set(t, key, lhat_integer(f.value), &refused);
	}
	answers[0] = table;
	*answerCount = 1;
}

static void lh_getDPIScale(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) machine;
	(void) args;
	(void) count;
	answers[0] = lhat_real(instance()->getScreenDPIScale());
	*answerCount = 1;
}

static void lh_getPixelDimensions(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) args;
	(void) count;
	float values[2] = {(float) instance()->getPixelWidth(), (float) instance()->getPixelHeight()};
	numberTuple(values, 2, answers, answerCount);
}

// readbackTexture(texture[, x, y, w, h]) -> ImageData with the pixels.
static void lh_readbackTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture *t = checkTexture(machine, args, count, 0);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	Rect rect = {0, 0, t->getPixelWidth(0), t->getPixelHeight(0)};
	if (count >= 5)
	{
		rect.x = (int) lh::optNumber(args, count, 1, 0);
		rect.y = (int) lh::optNumber(args, count, 2, 0);
		rect.w = (int) lh::optNumber(args, count, 3, rect.w);
		rect.h = (int) lh::optNumber(args, count, 4, rect.h);
	}
	lh::guard(machine, [&]() {
		StrongRef<love::image::ImageData> data(instance()->readbackTexture(t, 0, 0, rect, nullptr, 0, 0), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, data.get());
		*answerCount = 1;
	});
}

// ---------------------------------------------------------------------------
// Texture extras
// ---------------------------------------------------------------------------

#define TEXTURE_SELF() Texture *t = checkTexture(machine, args, count, 0); if (t == nullptr) return

static void lh_Texture_isCanvas(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	answers[0] = lhat_bool(t->isRenderTarget());
	*answerCount = 1;
}

static void lh_Texture_isReadable(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	answers[0] = lhat_bool(t->isReadable());
	*answerCount = 1;
}

static void lh_Texture_getFormat(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	const char *name = "";
	getConstant(t->getPixelFormat(), name);
	answers[0] = lh::pushEnum(machine, "love.graphics", "PixelFormat", name);
	*answerCount = 1;
}

static void lh_Texture_getDPIScale(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	answers[0] = lhat_real(t->getDPIScale());
	*answerCount = 1;
}

static void lh_Texture_getPixelDimensions(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										  LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	float values[2] = {(float) t->getPixelWidth(0), (float) t->getPixelHeight(0)};
	numberTuple(values, 2, answers, answerCount);
}

static void lh_Texture_getMipmapCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	answers[0] = lhat_integer(t->getMipmapCount());
	*answerCount = 1;
}

static void lh_Texture_getFilter(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	const SamplerState &s = t->getSamplerState();
	const char *minstr = "linear";
	const char *magstr = "linear";
	SamplerState::getConstant(s.minFilter, minstr);
	SamplerState::getConstant(s.magFilter, magstr);
	answers[0] = lh::pushEnum(machine, "love.graphics", "FilterMode", minstr);
	answers[1] = lh::pushEnum(machine, "love.graphics", "FilterMode", magstr);
	answers[2] = lhat_integer(s.maxAnisotropy);
	*answerCount = 3;
}

// setWrap(horiz[, vert])
static void lh_Texture_setWrap(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	SamplerState s = t->getSamplerState();
	std::string u = lh::optEnum(machine, args, count, 1, "love.graphics", "WrapMode", "clamp");
	std::string v = lh::optEnum(machine, args, count, 2, "love.graphics", "WrapMode", u);
	if (!SamplerState::getConstant(u.c_str(), s.wrapU))
	{
		lh::raise(machine, "Invalid wrap mode: " + u);
		return;
	}
	if (!SamplerState::getConstant(v.c_str(), s.wrapV))
	{
		lh::raise(machine, "Invalid wrap mode: " + v);
		return;
	}
	lh::guard(machine, [&]() {
		t->setSamplerState(s);
	});
}

static void lh_Texture_getWrap(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	const SamplerState &s = t->getSamplerState();
	const char *u = "clamp";
	const char *v = "clamp";
	SamplerState::getConstant(s.wrapU, u);
	SamplerState::getConstant(s.wrapV, v);
	answers[0] = lh::pushEnum(machine, "love.graphics", "WrapMode", u);
	answers[1] = lh::pushEnum(machine, "love.graphics", "WrapMode", v);
	*answerCount = 2;
}

static void lh_Texture_generateMipmaps(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	lh::guard(machine, [&]() {
		t->generateMipmaps();
	});
}

// replacePixels(imagedata[, x, y]): writes the pixels into the texture.
static void lh_Texture_replacePixels(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	TEXTURE_SELF();
	auto *data = count > 1 ? lh::checkObject<love::image::ImageData>(args[1], *binding.registry) : nullptr;
	if (data == nullptr)
	{
		lh::raise(machine, "Expected an ImageData");
		return;
	}
	int x = (int) lh::optNumber(args, count, 2, 0);
	int y = (int) lh::optNumber(args, count, 3, 0);
	lh::guard(machine, [&]() {
		t->replacePixels(data, 0, 0, x, y, true);
	});
}

bool lhGraphicsState(lh::Context &ctx)
{
	if (ctx.types())
		return true;
	const char *m = LH_GRAPHICS;
	const char *T = "Texture";
	return ctx.func(m, "newCanvas", "p^ -> love.graphics.Texture;", lh_newCanvas, nullptr)
		&& ctx.func(m, "newCanvas", "p^number^, number^ -> love.graphics.Texture;", lh_newCanvas, nullptr)
		&& ctx.func(m, "newCanvas", "p^number^, number^, t^{ msaa : number^ } -> love.graphics.Texture;", lh_newCanvas, nullptr)
		&& ctx.func(m, "setCanvas", "p^...;", lh_setCanvas, nullptr)
		&& ctx.func(m, "getCanvas", "p^ -> love.graphics.Texture|nil^;", lh_getCanvas, nullptr)
		&& ctx.func(m, "setShader", "p^;", lh_setShader, nullptr)
		&& ctx.func(m, "setShader", "p^love.graphics.Shader;", lh_setShader, nullptr)
		&& ctx.func(m, "getShader", "p^ -> love.graphics.Shader|nil^;", lh_getShader, nullptr)
		&& ctx.func(m, "setBlendMode", "p^love.graphics.BlendMode;", lh_setBlendMode, nullptr)
		&& ctx.func(m, "setBlendMode", "p^love.graphics.BlendMode, love.graphics.BlendAlphaMode;", lh_setBlendMode, nullptr)
		&& ctx.func(m, "getBlendMode", "f^ -> (love.graphics.BlendMode, love.graphics.BlendAlphaMode);", lh_getBlendMode, nullptr)
		&& ctx.func(m, "setScissor", "p^;", lh_setScissor, nullptr)
		&& ctx.func(m, "setScissor", "p^number^, number^, number^, number^;", lh_setScissor, nullptr)
		&& ctx.func(m, "intersectScissor", "p^number^, number^, number^, number^;", lh_intersectScissor, nullptr)
		&& ctx.func(m, "getScissor", "f^ -> (number^, number^, number^, number^);", lh_getScissor, nullptr)
		&& ctx.func(m, "setStencilMode", "p^;", lh_setStencilMode, nullptr)
		&& ctx.func(m, "setStencilMode", "p^love.graphics.StencilMode;", lh_setStencilMode, nullptr)
		&& ctx.func(m, "setStencilMode", "p^love.graphics.StencilMode, number^;", lh_setStencilMode, nullptr)
		&& ctx.func(m, "getStencilMode", "f^ -> (love.graphics.StencilMode, number^);", lh_getStencilMode, nullptr)
		&& ctx.func(m, "setColorMask", "p^;", lh_setColorMask, nullptr)
		&& ctx.func(m, "setColorMask", "p^bool^, bool^, bool^, bool^;", lh_setColorMask, nullptr)
		&& ctx.func(m, "getColorMask", "f^ -> (bool^, bool^, bool^, bool^);", lh_getColorMask, nullptr)
		&& ctx.func(m, "setDefaultFilter", "p^love.graphics.FilterMode;", lh_setDefaultFilter, nullptr)
		&& ctx.func(m, "setDefaultFilter", "p^love.graphics.FilterMode, love.graphics.FilterMode;", lh_setDefaultFilter, nullptr)
		&& ctx.func(m, "setDefaultFilter", "p^love.graphics.FilterMode, love.graphics.FilterMode, number^;", lh_setDefaultFilter, nullptr)
		&& ctx.func(m, "getDefaultFilter", "f^ -> (love.graphics.FilterMode, love.graphics.FilterMode, number^);", lh_getDefaultFilter, nullptr)
		&& ctx.func(m, "setLineStyle", "p^love.graphics.LineStyle;", lh_setLineStyle, nullptr)
		&& ctx.func(m, "getLineStyle", "f^ -> love.graphics.LineStyle;", lh_getLineStyle, nullptr)
		&& ctx.func(m, "setLineJoin", "p^love.graphics.LineJoin;", lh_setLineJoin, nullptr)
		&& ctx.func(m, "getLineJoin", "f^ -> love.graphics.LineJoin;", lh_getLineJoin, nullptr)
		&& ctx.func(m, "setWireframe", "p^bool^;", lh_setWireframe, nullptr)
		&& ctx.func(m, "isWireframe", "f^ -> bool^;", lh_isWireframe, nullptr)
		&& ctx.func(m, "getPointSize", "f^ -> number^;", lh_getPointSize, nullptr)
		&& ctx.func(m, "reset", "p^;", lh_reset, nullptr)
		&& ctx.func(m, "shear", "p^number^, number^;", lh_shear, nullptr)
		&& ctx.func(m, "applyTransform", "p^love.math.Transform;", lh_applyTransform, nullptr)
		&& ctx.func(m, "replaceTransform", "p^love.math.Transform;", lh_replaceTransform, nullptr)
		&& ctx.func(m, "transformPoint", "f^number^, number^ -> (number^, number^);", lh_transformPoint, nullptr)
		&& ctx.func(m, "inverseTransformPoint", "f^number^, number^ -> (number^, number^);", lh_inverseTransformPoint, nullptr)
		&& ctx.func(m, "getStackDepth", "f^ -> number^;", lh_getStackDepth, nullptr)
		&& ctx.func(m, "ellipse", "p^love.graphics.DrawMode, number^, number^, number^, ...;", lh_ellipse, nullptr)
		&& ctx.func(m, "arc", "p^love.graphics.DrawMode, number^, number^, number^, number^, number^, ...;", lh_arc, nullptr)
		&& ctx.func(m, "arc", "p^love.graphics.DrawMode, love.graphics.ArcMode, number^, number^, number^, number^, number^, ...;", lh_arc, nullptr)
		&& ctx.func(m, "getRendererInfo", "f^ -> (string^, string^, string^, string^);", lh_getRendererInfo, nullptr)
		&& ctx.func(m, "getStats", "f^ -> t^{ drawcalls : number^, drawcallsbatched : number^, canvasswitches : number^, shaderswitches : number^, textures : number^, fonts : number^, buffers : number^, texturememory : number^, buffermemory : number^ };", lh_getStats, nullptr)
		&& ctx.func(m, "getDPIScale", "f^ -> number^;", lh_getDPIScale, nullptr)
		&& ctx.func(m, "getPixelDimensions", "f^ -> (number^, number^);", lh_getPixelDimensions, nullptr)
		&& ctx.func(m, "readbackTexture", "p^love.graphics.Texture -> love.image.ImageData;", lh_readbackTexture, nullptr)
		&& ctx.func(m, "readbackTexture", "p^love.graphics.Texture, number^, number^, number^, number^ -> love.image.ImageData;", lh_readbackTexture, nullptr)
		&& ctx.member(m, T, "isCanvas", "f^self^ -> bool^;", lh_Texture_isCanvas, nullptr)
		&& ctx.member(m, T, "isReadable", "f^self^ -> bool^;", lh_Texture_isReadable, nullptr)
		&& ctx.member(m, T, "getFormat", "f^self^ -> love.graphics.PixelFormat;", lh_Texture_getFormat, nullptr)
		&& ctx.member(m, T, "getDPIScale", "f^self^ -> number^;", lh_Texture_getDPIScale, nullptr)
		&& ctx.member(m, T, "getPixelDimensions", "f^self^ -> (number^, number^);", lh_Texture_getPixelDimensions, nullptr)
		&& ctx.member(m, T, "getMipmapCount", "f^self^ -> number^;", lh_Texture_getMipmapCount, nullptr)
		&& ctx.member(m, T, "getFilter", "f^self^ -> (love.graphics.FilterMode, love.graphics.FilterMode, number^);", lh_Texture_getFilter, nullptr)
		&& ctx.member(m, T, "setWrap", "p^self^, love.graphics.WrapMode;", lh_Texture_setWrap, nullptr)
		&& ctx.member(m, T, "setWrap", "p^self^, love.graphics.WrapMode, love.graphics.WrapMode;", lh_Texture_setWrap, nullptr)
		&& ctx.member(m, T, "getWrap", "f^self^ -> (love.graphics.WrapMode, love.graphics.WrapMode);", lh_Texture_getWrap, nullptr)
		&& ctx.member(m, T, "generateMipmaps", "p^self^;", lh_Texture_generateMipmaps, nullptr)
		&& ctx.member(m, T, "replacePixels", "p^self^, love.image.ImageData;", lh_Texture_replacePixels, nullptr)
		&& ctx.member(m, T, "replacePixels", "p^self^, love.image.ImageData, number^, number^;", lh_Texture_replacePixels, nullptr);
}

} // graphics
} // love

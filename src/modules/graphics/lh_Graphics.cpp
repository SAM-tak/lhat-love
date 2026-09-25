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

// love.graphics for L^: the immediate-mode subset (clear, shapes, text,
// color and transform state), textures from files and fonts. The reference
// is wrap_Graphics.cpp beside this file; Canvas, Shader, Mesh, SpriteBatch
// and the rest arrive with later milestones.
//
// Optional trailing arguments are spelled as overloads (one registration
// per arity, 02 の 14.12) where the set is small, and as a variadic tail
// where it is the nine-number transform every draw call takes.

#include "lh_Graphics.h"

#include "Font.h"
#include "Shader.h"
#include "Mesh.h"
#include "SpriteBatch.h"
#include "ParticleSystem.h"
#include "TextBatch.h"
#include "Video.h"
#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/FileData.h"
#include "modules/font/Font.h"
#include "modules/image/Image.h"
#include "modules/image/ImageData.h"

#include <vector>

namespace love
{
namespace graphics
{

#define instance() graphicsInstance()

GraphicsBinding graphicsBinding;
#define binding graphicsBinding

// The x, y, r, sx, sy, ox, oy, kx, ky tail of a draw call, from `first` on.
Matrix4 transformOf(const LhatValue *args, size_t count, size_t first)
{
	float x = (float) lh::optNumber(args, count, first + 0, 0.0);
	float y = (float) lh::optNumber(args, count, first + 1, 0.0);
	float a = (float) lh::optNumber(args, count, first + 2, 0.0);
	float sx = (float) lh::optNumber(args, count, first + 3, 1.0);
	float sy = (float) lh::optNumber(args, count, first + 4, sx);
	float ox = (float) lh::optNumber(args, count, first + 5, 0.0);
	float oy = (float) lh::optNumber(args, count, first + 6, 0.0);
	float kx = (float) lh::optNumber(args, count, first + 7, 0.0);
	float ky = (float) lh::optNumber(args, count, first + 8, 0.0);
	return Matrix4(x, y, a, sx, sy, ox, oy, kx, ky);
}

bool drawModeOf(LhatMachine *machine, LhatValue value, Graphics::DrawMode &mode)
{
	const char *name = lh::stringOf(value);
	if (name == nullptr || !Graphics::getConstant(name, mode))
	{
		lh::raise(machine, std::string("Invalid draw mode: ") + (name != nullptr ? name : "(not a string)"));
		return false;
	}
	return true;
}

Colorf colorOf(const LhatValue *args, size_t count, size_t first)
{
	return Colorf(
		(float) lh::optNumber(args, count, first + 0, 0.0),
		(float) lh::optNumber(args, count, first + 1, 0.0),
		(float) lh::optNumber(args, count, first + 2, 0.0),
		(float) lh::optNumber(args, count, first + 3, 1.0));
}

// 05 の 8.7: several answers go into the room the machine handed over.
void numberTuple(const float *values, size_t count, LhatValue *answers, int *answerCount)
{
	for (size_t i = 0; i < count; i++)
		answers[i] = lhat_real(values[i]);
	*answerCount = (int) count;
}

void colorTuple(Colorf c, LhatValue *answers, int *answerCount)
{
	answers[0] = lhat_real(c.r);
	answers[1] = lhat_real(c.g);
	answers[2] = lhat_real(c.b);
	answers[3] = lhat_real(c.a);
	*answerCount = 4;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

// clear() clears to the background color; clear(r, g, b[, a]) to that.
static void lh_clear(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) context;
	OptionalColorD color;
	if (count >= 3)
	{
		Colorf c = colorOf(arguments, count, 0);
		color.set(ColorD(c.r, c.g, c.b, c.a));
	}
	else
	{
		Colorf c = instance()->getBackgroundColor();
		color.set(ColorD(c.r, c.g, c.b, c.a));
	}
	lh::guard(machine, [&]() {
		instance()->clear(color, OptionalInt(0), OptionalDouble(1.0));
	});
}

static void lh_present(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	lh::guard(machine, [&]() {
		instance()->present(nullptr);
	});
}

static void lh_isActive(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->isActive());
	*answerCount = 1;
}

static void lh_getWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_integer(instance()->getWidth());
	*answerCount = 1;
}

static void lh_getHeight(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_integer(instance()->getHeight());
	*answerCount = 1;
}

static void lh_getDimensions(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_integer(instance()->getWidth());
	answers[1] = lhat_integer(instance()->getHeight());
	*answerCount = 2;
}

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------

static void lh_setColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setColor(colorOf(arguments, count, 0));
}

static void lh_getColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	colorTuple(instance()->getColor(), answers, answerCount);
}

static void lh_setBackgroundColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setBackgroundColor(colorOf(arguments, count, 0));
}

static void lh_getBackgroundColor(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	colorTuple(instance()->getBackgroundColor(), answers, answerCount);
}

static void lh_setLineWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setLineWidth((float) lh::optNumber(arguments, count, 0, 1.0));
}

static void lh_getLineWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_real(instance()->getLineWidth());
	*answerCount = 1;
}

// ---------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------

// rectangle(mode, x, y, w, h[, rx[, ry[, segments]]])
static void lh_rectangle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, arguments[0], mode))
	{
		return;
	}
	float x = (float)lh::optNumber(arguments, count, 1, 0.0);
	float y = (float) lh::optNumber(arguments, count, 2, 0.0);
	float w = (float) lh::optNumber(arguments, count, 3, 0.0);
	float h = (float) lh::optNumber(arguments, count, 4, 0.0);
	lh::guard(machine, [&]() {
		if (count <= 5)
		{
			instance()->rectangle(mode, x, y, w, h);
		}
		else
		{
			float rx = (float) lh::optNumber(arguments, count, 5, 0.0);
			float ry = (float) lh::optNumber(arguments, count, 6, rx);
			if (count <= 7)
				instance()->rectangle(mode, x, y, w, h, rx, ry);
			else
				instance()->rectangle(mode, x, y, w, h, rx, ry, (int) lh::optNumber(arguments, count, 7, 10));
		}
	});
}

// circle(mode, x, y, radius[, segments])
static void lh_circle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, arguments[0], mode))
	{
		return;
	}
	float x = (float)lh::optNumber(arguments, count, 1, 0.0);
	float y = (float) lh::optNumber(arguments, count, 2, 0.0);
	float radius = (float) lh::optNumber(arguments, count, 3, 0.0);
	lh::guard(machine, [&]() {
		if (count <= 4)
			instance()->circle(mode, x, y, radius);
		else
			instance()->circle(mode, x, y, radius, (int) lh::optNumber(arguments, count, 4, 10));
	});
}

// Reads x1, y1, x2, y2, ... from `first` on. An odd count drops the last.
std::vector<Vector2> verticesOf(const LhatValue *args, size_t count, size_t first)
{
	std::vector<Vector2> vertices;
	for (size_t i = first; i + 1 < count; i += 2)
		vertices.emplace_back((float) lh::optNumber(args, count, i, 0.0), (float) lh::optNumber(args, count, i + 1, 0.0));
	return vertices;
}

// line(x1, y1, x2, y2, ...)
static void lh_line(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<Vector2> vertices = verticesOf(arguments, count, 0);
	if (vertices.size() < 2)
	{
		lh::raise(machine, "Need at least two vertices to draw a line");
		return;
	}
	lh::guard(machine, [&]() {
		instance()->polyline(vertices.data(), vertices.size());
	});
}

// polygon(mode, x1, y1, x2, y2, ...)
static void lh_polygon(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	Graphics::DrawMode mode;
	if (count < 1 || !drawModeOf(machine, arguments[0], mode))
		return;
	std::vector<Vector2> vertices = verticesOf(arguments, count, 1);
	if (vertices.size() < 3)
	{
		lh::raise(machine, "Need at least three vertices to draw a polygon");
		return;
	}
	// The closing vertex, as wrap_Graphics.cpp adds it.
	vertices.push_back(vertices[0]);
	lh::guard(machine, [&]() {
		instance()->polygon(mode, vertices.data(), vertices.size());
	});
}

// points(x1, y1, x2, y2, ...)
static void lh_points(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<Vector2> vertices = verticesOf(arguments, count, 0);
	if (vertices.empty())
	{
		return;
	}
	lh::guard(machine, [&]() {
		instance()->points(vertices.data(), nullptr, vertices.size());
	});
}

static void lh_setPointSize(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	lh::guard(machine, [&]() {
		instance()->setPointSize((float) lh::optNumber(arguments, count, 0, 1.0));
	});
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

// The Font a print call may pass right after its text. print(text, x, ...)
// and print(text, font, x, ...) are two arms, told apart at the second
// position; one host function serves both and finds the font here.
static Font *fontInTail(const LhatValue *arguments, size_t count, size_t index)
{
	if (index >= count || !lhat_is_object_kind(arguments[index], LHAT_OBJECT_HOSTDATA))
		return nullptr;
	return lh::checkObject<Font>(arguments[index], *binding.registry);
}

// print(text[, font], x, y, r, sx, sy, ox, oy, kx, ky) -- all but text optional.
static void lh_print(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<love::font::ColoredString> text;
	text.push_back({lh::optString(arguments, count, 0, ""), instance()->getColor()});
	Font *font = fontInTail(arguments, count, 1);
	Matrix4 m = transformOf(arguments, count, font != nullptr ? 2 : 1);
	lh::guard(machine, [&]() {
		if (font != nullptr)
			instance()->print(text, font, m);
		else
			instance()->print(text, m);
	});
}

// printf(text[, font], x, y, limit[, align, r, sx, sy, ox, oy, kx, ky])
static void lh_printf(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<love::font::ColoredString> text;
	text.push_back({lh::optString(arguments, count, 0, ""), instance()->getColor()});
	Font *font = fontInTail(arguments, count, 1);
	size_t at = font != nullptr ? 2 : 1;
	float x = (float) lh::optNumber(arguments, count, at + 0, 0.0);
	float y = (float) lh::optNumber(arguments, count, at + 1, 0.0);
	float wrap = (float) lh::optNumber(arguments, count, at + 2, 0.0);

	Font::AlignMode align = Font::ALIGN_LEFT;
	size_t rest = at + 3;
	if (count > rest && lh::stringOf(arguments[rest]) != nullptr)
	{
		const char *name = lh::stringOf(arguments[rest]);
		if (!Font::getConstant(name, align))
		{
			lh::raise(machine, std::string("Invalid align mode: ") + name);
			return;
		}
		rest = rest + 1;
	}
	float a = (float) lh::optNumber(arguments, count, rest + 0, 0.0);
	float sx = (float) lh::optNumber(arguments, count, rest + 1, 1.0);
	float sy = (float) lh::optNumber(arguments, count, rest + 2, sx);
	float ox = (float) lh::optNumber(arguments, count, rest + 3, 0.0);
	float oy = (float) lh::optNumber(arguments, count, rest + 4, 0.0);
	float kx = (float) lh::optNumber(arguments, count, rest + 5, 0.0);
	float ky = (float) lh::optNumber(arguments, count, rest + 6, 0.0);
	Matrix4 m(x, y, a, sx, sy, ox, oy, kx, ky);
	lh::guard(machine, [&]() {
		if (font != nullptr)
			instance()->printf(text, font, wrap, align, m);
		else
			instance()->printf(text, wrap, align, m);
	});
}

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------

static void lh_push(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	lh::guard(machine, [&]() {
		instance()->push();
	});
}

static void lh_pop(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
				   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	lh::guard(machine, [&]() {
		instance()->pop();
	});
}

static void lh_translate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->translate((float) lh::optNumber(arguments, count, 0, 0.0), (float) lh::optNumber(arguments, count, 1, 0.0));
}

static void lh_rotate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->rotate((float) lh::optNumber(arguments, count, 0, 0.0));
}

static void lh_scale(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	float sx = (float) lh::optNumber(arguments, count, 0, 1.0);
	instance()->scale(sx, (float) lh::optNumber(arguments, count, 1, sx));
}

static void lh_origin(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	instance()->origin();
}

// ---------------------------------------------------------------------------
// Objects: Texture (love.graphics.newImage) and Font
// ---------------------------------------------------------------------------

Texture *checkTexture(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index)
{
	Texture *texture = index < count ? lh::checkObject<Texture>(arguments[index], *binding.registry) : nullptr;
	if (texture == nullptr)
		lh::raise(machine, "Expected a Texture");
	return texture;
}

Quad *checkQuad(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index)
{
	Quad *quad = index < count ? lh::checkObject<Quad>(arguments[index], *binding.registry) : nullptr;
	if (quad == nullptr)
		lh::raise(machine, "Expected a Quad");
	return quad;
}

bool numbersOf(LhatValue table, std::vector<float> &out)
{
	if (!lhat_is_object_kind(table, LHAT_OBJECT_TABLE))
		return false;
	const LhatTable *t = (const LhatTable *) lhat_as_object(table);
	size_t n = lhat_table_length(t);
	out.clear();
	out.reserve(n);
	for (size_t i = 0; i < n; i++)
	{
		LhatValue v = lhat_table_get(t, lhat_integer((int64_t) i));
		out.push_back((float) lh::optNumber(&v, 1, 0, 0.0));
	}
	return true;
}

static Font *checkFont(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index)
{
	Font *font = index < count ? lh::checkObject<Font>(arguments[index], *binding.registry) : nullptr;
	if (font == nullptr)
		lh::raise(machine, "Expected a Font");
	return font;
}

// newImage(path) / newImage(imagedata): a 2D texture with one mip level.
// wrap_Graphics.cpp takes layers, mipmap tables, compressed data and
// settings as well -- those arrive with the later milestones.
static void lh_newImage(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	auto imagemodule = Module::getInstance<love::image::Image>(Module::M_IMAGE);
	auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
	if (imagemodule == nullptr)
	{
		lh::raise(machine, "Cannot load images without the love.image module.");
		return;
	}

	StrongRef<love::image::ImageData> idata;
	if (count > 0 && lhat_is_object_kind(arguments[0], LHAT_OBJECT_HOSTDATA))
	{
		idata.set(lh::checkObject<love::image::ImageData>(arguments[0], *binding.registry));
		if (idata.get() == nullptr)
		{
			lh::raise(machine, "Expected an ImageData");
			return;
		}
	}
	else
	{
		std::string path = lh::optString(arguments, count, 0, "");
		if (fs == nullptr)
		{
			lh::raise(machine, "love.filesystem is not loaded.");
			return;
		}
		try
		{
			StrongRef<love::filesystem::FileData> file(fs->read(path.c_str()), Acquire::NORETAIN);
			idata.set(imagemodule->newImageData(file.get()), Acquire::NORETAIN);
		}
		catch (const love::Exception &e)
		{
			answers[0] = lh::fail(machine, binding.errors->graphicsCouldNotLoad, e.what());
			*answerCount = 1;
			return;
		}
	}

	lh::guard(machine, [&]() {
		Texture::Settings settings;
		settings.type = TEXTURE_2D;
		Texture::Slices slices(TEXTURE_2D);
		slices.set(0, 0, idata.get());
		StrongRef<Texture> texture(instance()->newTexture(settings, &slices), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, texture.get());
		*answerCount = 1;
	});
}

static void lh_Texture_getWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture *t = checkTexture(machine, arguments, count, 0);
	answers[0] = lhat_integer(t != nullptr ? t->getWidth() : 0);
	*answerCount = 1;
}

static void lh_Texture_getHeight(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture *t = checkTexture(machine, arguments, count, 0);
	answers[0] = lhat_integer(t != nullptr ? t->getHeight() : 0);
	*answerCount = 1;
}

static void lh_Texture_getDimensions(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture *t = checkTexture(machine, arguments, count, 0);
	if (t == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_integer(t->getWidth());
	answers[1] = lhat_integer(t->getHeight());
	*answerCount = 2;
}

// texture.setFilter(min[, mag])
static void lh_Texture_setFilter(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture *t = checkTexture(machine, arguments, count, 0);
	if (t == nullptr)
		return;
	SamplerState s = t->getSamplerState();
	std::string minstr = lh::optString(arguments, count, 1, "linear");
	std::string magstr = lh::optString(arguments, count, 2, minstr);
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
	lh::guard(machine, [&]() {
		t->setSamplerState(s);
	});
}

// draw(drawable, x, y, r, sx, sy, ox, oy, kx, ky) / draw(texture, quad, x, y, ...)
static void lh_draw(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) context;
	Drawable *drawable = count > 0 ? lh::checkObject<Drawable>(arguments[0], *binding.registry) : nullptr;
	if (drawable == nullptr)
	{
		lh::raise(machine, "Expected a Drawable");
		return;
	}
	if (count > 1 && lhat_is_object_kind(arguments[1], LHAT_OBJECT_HOSTDATA))
	{
		Quad *quad = checkQuad(machine, arguments, count, 1);
		Texture *texture = lh::checkObject<Texture>(arguments[0], *binding.registry);
		if (quad == nullptr)
			return;
		if (texture == nullptr)
		{
			lh::raise(machine, "Only a Texture can be drawn with a Quad");
			return;
		}
		Matrix4 m = transformOf(arguments, count, 2);
		lh::guard(machine, [&]() {
			instance()->draw(texture, quad, m);
		});
		return;
	}
	Matrix4 m = transformOf(arguments, count, 1);
	lh::guard(machine, [&]() {
		instance()->draw(drawable, m);
	});
}

// newFont(size) -- the default face; newFont(path, size) -- a TrueType file.
static void lh_newFont(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	auto fontmodule = Module::getInstance<love::font::Font>(Module::M_FONT);
	if (fontmodule == nullptr)
	{
		lh::raise(machine, "Cannot create fonts without the love.font module.");
		return;
	}

	love::font::TrueTypeRasterizer::Settings settings;
	StrongRef<love::font::Rasterizer> rasterizer;
	if (count > 0 && lhat_is_number(arguments[0]))
	{
		int size = (int) lh::optNumber(arguments, count, 0, 12);
		try
		{
			rasterizer.set(fontmodule->newTrueTypeRasterizer(size, settings), Acquire::NORETAIN);
		}
		catch (const love::Exception &e)
		{
			lh::raise(machine, e.what());
			return;
		}
	}
	else
	{
		std::string path = lh::optString(arguments, count, 0, "");
		int size = (int) lh::optNumber(arguments, count, 1, 12);
		auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
		if (fs == nullptr)
		{
			lh::raise(machine, "love.filesystem is not loaded.");
			return;
		}
		try
		{
			StrongRef<love::filesystem::FileData> file(fs->read(path.c_str()), Acquire::NORETAIN);
			rasterizer.set(fontmodule->newTrueTypeRasterizer(file.get(), size, settings), Acquire::NORETAIN);
		}
		catch (const love::Exception &e)
		{
			answers[0] = lh::fail(machine, binding.errors->graphicsCouldNotLoad, e.what());
			*answerCount = 1;
			return;
		}
	}

	lh::guard(machine, [&]() {
		StrongRef<Font> font(instance()->newFont(rasterizer.get()), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, font.get());
		*answerCount = 1;
	});
}

static void lh_setFont(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	Font *font = checkFont(machine, arguments, count, 0);
	if (font == nullptr)
		return;
	lh::guard(machine, [&]() {
		instance()->setFont(font);
	});
}

static void lh_getFont(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	lh::guard(machine, [&]() {
		answers[0] = lh::pushObject(machine, *binding.registry, instance()->getFont());
		*answerCount = 1;
	});
}

static void lh_Font_getWidth(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	Font *font = checkFont(machine, arguments, count, 0);
	if (font == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::string text = lh::optString(arguments, count, 1, "");
	lh::guard(machine, [&]() {
		answers[0] = lhat_real(font->getWidth(text));
		*answerCount = 1;
	});
}

static void lh_Font_getHeight(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	Font *font = checkFont(machine, arguments, count, 0);
	answers[0] = lhat_real(font != nullptr ? font->getHeight() : 0.0f);
	*answerCount = 1;
}

static void lh_Font_getLineHeight(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	Font *font = checkFont(machine, arguments, count, 0);
	answers[0] = lhat_real(font != nullptr ? font->getLineHeight() : 0.0f);
	*answerCount = 1;
}


} // graphics

namespace lh
{

bool lhopen_love_graphics(Context &ctx)
{
	using namespace love::graphics;
	const char *m = "love.graphics";

	// 04 の 2.4: what love.graphics can fail at, declared where it fails.
	if (ctx.types())
	{
		static const char *const variants[] = {"CouldNotLoad", "ShaderFailed"};
		const LhatErrorKind *kinds[2] = {nullptr, nullptr};
		if (!ctx.errorKind(m, variants, 2, kinds))
			return false;
		ctx.errors->graphicsCouldNotLoad = kinds[0];
		ctx.errors->graphicsShaderFailed = kinds[1];
	}
	if (!ctx.objectType(m, "Drawable", Drawable::type) || !ctx.objectType(m, "Texture", Texture::type, m, "Drawable") || !ctx.objectType(m, "Font", Font::type))
		return false;
	// The other types first, in both phases: the draw arms below name them.
	if (!lhGraphicsQuad(ctx) || !lhGraphicsShader(ctx) || !lhGraphicsMesh(ctx) || !lhGraphicsSpriteBatch(ctx)
		|| !lhGraphicsParticleSystem(ctx) || !lhGraphicsTextBatch(ctx) || !lhGraphicsVideo(ctx) || !lhGraphicsState(ctx))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;

	return ctx.func(m, "clear", "p^;", lh_clear, nullptr)
		&& ctx.func(m, "clear", "p^number^, number^, number^;", lh_clear, nullptr)
		&& ctx.func(m, "clear", "p^number^, number^, number^, number^;", lh_clear, nullptr)
		&& ctx.func(m, "present", "p^;", lh_present, nullptr)
		&& ctx.func(m, "isActive", "f^ -> bool^;", lh_isActive, nullptr)
		&& ctx.func(m, "getWidth", "f^ -> number^;", lh_getWidth, nullptr)
		&& ctx.func(m, "getHeight", "f^ -> number^;", lh_getHeight, nullptr)
		&& ctx.func(m, "getDimensions", "f^ -> (number^, number^);", lh_getDimensions, nullptr)
		&& ctx.func(m, "setColor", "p^number^, number^, number^;", lh_setColor, nullptr)
		&& ctx.func(m, "setColor", "p^number^, number^, number^, number^;", lh_setColor, nullptr)
		&& ctx.func(m, "getColor", "f^ -> (number^, number^, number^, number^);", lh_getColor, nullptr)
		&& ctx.func(m, "setBackgroundColor", "p^number^, number^, number^;", lh_setBackgroundColor, nullptr)
		&& ctx.func(m, "setBackgroundColor", "p^number^, number^, number^, number^;", lh_setBackgroundColor, nullptr)
		&& ctx.func(m, "getBackgroundColor", "f^ -> (number^, number^, number^, number^);", lh_getBackgroundColor, nullptr)
		&& ctx.func(m, "setLineWidth", "p^number^;", lh_setLineWidth, nullptr)
		&& ctx.func(m, "getLineWidth", "f^ -> number^;", lh_getLineWidth, nullptr)
		&& ctx.func(m, "rectangle", "p^string^, number^, number^, number^, number^, ...;", lh_rectangle, nullptr)
		&& ctx.func(m, "circle", "p^string^, number^, number^, number^, ...;", lh_circle, nullptr)
		&& ctx.func(m, "line", "p^number^, number^, number^, number^, ...;", lh_line, nullptr)
		&& ctx.func(m, "polygon", "p^string^, ...;", lh_polygon, nullptr)
		&& ctx.func(m, "points", "p^number^, number^, ...;", lh_points, nullptr)
		&& ctx.func(m, "setPointSize", "p^number^;", lh_setPointSize, nullptr)
		&& ctx.func(m, "print", "p^string^;", lh_print, nullptr)
		&& ctx.func(m, "print", "p^string^, number^, ...;", lh_print, nullptr)
		&& ctx.func(m, "print", "p^string^, love.graphics.Font, ...;", lh_print, nullptr)
		&& ctx.func(m, "printf", "p^string^, number^, number^, number^, ...;", lh_printf, nullptr)
		&& ctx.func(m, "printf", "p^string^, love.graphics.Font, number^, number^, number^, ...;", lh_printf, nullptr)
		&& ctx.func(m, "push", "p^;", lh_push, nullptr)
		&& ctx.func(m, "pop", "p^;", lh_pop, nullptr)
		&& ctx.func(m, "translate", "p^number^, number^;", lh_translate, nullptr)
		&& ctx.func(m, "rotate", "p^number^;", lh_rotate, nullptr)
		&& ctx.func(m, "scale", "p^number^;", lh_scale, nullptr)
		&& ctx.func(m, "scale", "p^number^, number^;", lh_scale, nullptr)
		&& ctx.func(m, "origin", "p^;", lh_origin, nullptr)
		&& ctx.func(m, "newImage", "p^string^ -> love.graphics.Texture|love.graphics.Error.CouldNotLoad;", lh_newImage, nullptr)
		&& ctx.func(m, "newImage", "p^love.image.ImageData -> love.graphics.Texture;", lh_newImage, nullptr)
		&& ctx.member(m, "Texture", "getWidth", "f^self^ -> number^;", lh_Texture_getWidth, nullptr)
		&& ctx.member(m, "Texture", "getHeight", "f^self^ -> number^;", lh_Texture_getHeight, nullptr)
		&& ctx.member(m, "Texture", "getDimensions", "f^self^ -> (number^, number^);", lh_Texture_getDimensions, nullptr)
		&& ctx.member(m, "Texture", "setFilter", "p^self^, string^, ...;", lh_Texture_setFilter, nullptr)
		&& ctx.func(m, "draw", "p^love.graphics.Drawable;", lh_draw, nullptr)
		&& ctx.func(m, "draw", "p^love.graphics.Drawable, number^, ...;", lh_draw, nullptr)
		&& ctx.func(m, "draw", "p^love.graphics.Texture, love.graphics.Quad, ...;", lh_draw, nullptr)
		&& ctx.func(m, "newFont", "p^number^ -> love.graphics.Font;", lh_newFont, nullptr)
		&& ctx.func(m, "newFont", "p^string^, number^ -> love.graphics.Font|love.graphics.Error.CouldNotLoad;", lh_newFont, nullptr)
		&& ctx.func(m, "setFont", "p^love.graphics.Font;", lh_setFont, nullptr)
		&& ctx.func(m, "getFont", "p^ -> love.graphics.Font;", lh_getFont, nullptr)
		&& ctx.member(m, "Font", "getWidth", "f^self^, string^ -> number^;", lh_Font_getWidth, nullptr)
		&& ctx.member(m, "Font", "getHeight", "f^self^ -> number^;", lh_Font_getHeight, nullptr)
		&& ctx.member(m, "Font", "getLineHeight", "f^self^ -> number^;", lh_Font_getLineHeight, nullptr);
}

} // lh
} // love

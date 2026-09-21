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

// love.window for L^. The reference is wrap_Window.cpp beside this file.
// Milestone M1: the settings table of setMode/getMode waits for conf.lh.

#include "lh_Window.h"
#include "lh/lh.h"

namespace love
{
namespace window
{

#define instance() (Module::getInstance<Window>(Module::M_WINDOW))

bool readWindowSettings(LhatMachine *machine, LhatValue table, WindowSettings &settings, std::string &error)
{
	if (!lhat_is_object_kind(table, LHAT_OBJECT_TABLE))
		return true;

	std::string fstype = lh::fieldString(machine, table, "fullscreentype", "");
	if (!fstype.empty() && !Window::getConstant(fstype.c_str(), settings.fstype))
	{
		error = "Invalid fullscreen type: " + fstype;
		return false;
	}
	settings.fullscreen = lh::fieldBool(machine, table, "fullscreen", settings.fullscreen);
	settings.vsync = (int) lh::fieldNumber(machine, table, "vsync", settings.vsync);
	settings.msaa = (int) lh::fieldNumber(machine, table, "msaa", settings.msaa);
	settings.stencil = lh::fieldBool(machine, table, "stencil", settings.stencil);
	settings.depth = lh::fieldBool(machine, table, "depth", settings.depth);
	settings.resizable = lh::fieldBool(machine, table, "resizable", settings.resizable);
	settings.minwidth = (int) lh::fieldNumber(machine, table, "minwidth", settings.minwidth);
	settings.minheight = (int) lh::fieldNumber(machine, table, "minheight", settings.minheight);
	settings.borderless = lh::fieldBool(machine, table, "borderless", settings.borderless);
	settings.centered = lh::fieldBool(machine, table, "centered", settings.centered);
	settings.displayindex = (int) lh::fieldNumber(machine, table, "displayindex", settings.displayindex);
	settings.usedpiscale = lh::fieldBool(machine, table, "usedpiscale", settings.usedpiscale);
	settings.refreshrate = lh::fieldNumber(machine, table, "refreshrate", settings.refreshrate);
	if (lh::fieldIs(machine, table, "x", LHAT_VALUE_INTEGER) || lh::fieldIs(machine, table, "x", LHAT_VALUE_REAL))
	{
		settings.useposition = true;
		settings.x = (int) lh::fieldNumber(machine, table, "x", 0);
		settings.y = (int) lh::fieldNumber(machine, table, "y", 0);
	}
	return true;
}

static void lh_setMode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	int w = (int) lh::optNumber(arguments, count, 0, 800);
	int h = (int) lh::optNumber(arguments, count, 1, 600);
	WindowSettings settings;
	std::string error;
	if (count > 2 && !readWindowSettings(machine, arguments[2], settings, error))
	{
		lh::raise(machine, error);
		return;
	}
	lh::guard(machine, [&]() {
		answers[0] = lhat_bool(instance()->setWindow(w, h, &settings));
		*answerCount = 1;
	});
}

static void lh_getMode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	int w = 0, h = 0;
	WindowSettings settings;
	instance()->getWindow(w, h, settings);
	answers[0] = lhat_integer(w);
	answers[1] = lhat_integer(h);
	*answerCount = 2;
}

static void lh_setTitle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setWindowTitle(lh::optString(arguments, count, 0, ""));
}

static void lh_getTitle(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getWindowTitle(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_isOpen(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->isOpen());
	*answerCount = 1;
}

static void lh_close(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	lh::guard(machine, [&]() {
		instance()->close();
	});
}

static void lh_setFullscreen(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) context;
	bool fullscreen = lh::optBool(arguments, count, 0, false);
	lh::guard(machine, [&]() {
		answers[0] = lhat_bool(instance()->setFullscreen(fullscreen));
		*answerCount = 1;
	});
}

static void lh_getDPIScale(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_real(instance()->getDPIScale());
	*answerCount = 1;
}

static void lh_hasFocus(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->hasFocus());
	*answerCount = 1;
}

static void lh_hasMouseFocus(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->hasMouseFocus());
	*answerCount = 1;
}

static void lh_isVisible(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->isVisible());
	*answerCount = 1;
}

static void lh_setVSync(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setVSync((int) lh::optNumber(arguments, count, 0, 1));
}

static void lh_getVSync(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_integer(instance()->getVSync());
	*answerCount = 1;
}

} // window

namespace lh
{

bool lhopen_love_window(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::window;
	const char *m = "love.window";
	return ctx.func(m, "setMode", "p^number^, number^ -> bool^;", lh_setMode, nullptr)
		&& ctx.func(m, "setMode", "p^number^, number^, t^{} -> bool^;", lh_setMode, nullptr)
		&& ctx.func(m, "getMode", "f^ -> (number^, number^);", lh_getMode, nullptr)
		&& ctx.func(m, "setTitle", "p^string^;", lh_setTitle, nullptr)
		&& ctx.func(m, "getTitle", "f^ -> string^;", lh_getTitle, nullptr)
		&& ctx.func(m, "isOpen", "f^ -> bool^;", lh_isOpen, nullptr)
		&& ctx.func(m, "close", "p^;", lh_close, nullptr)
		&& ctx.func(m, "setFullscreen", "p^bool^ -> bool^;", lh_setFullscreen, nullptr)
		&& ctx.func(m, "getDPIScale", "f^ -> number^;", lh_getDPIScale, nullptr)
		&& ctx.func(m, "hasFocus", "f^ -> bool^;", lh_hasFocus, nullptr)
		&& ctx.func(m, "hasMouseFocus", "f^ -> bool^;", lh_hasMouseFocus, nullptr)
		&& ctx.func(m, "isVisible", "f^ -> bool^;", lh_isVisible, nullptr)
		&& ctx.func(m, "setVSync", "p^number^;", lh_setVSync, nullptr)
		&& ctx.func(m, "getVSync", "f^ -> number^;", lh_getVSync, nullptr);
}

} // lh
} // love

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

// love.mouse for L^. The reference is wrap_Mouse.cpp beside this file.

#include "Mouse.h"
#include "lh_Mouse.h"
#include "lh/lh.h"

#include <cstring>
#include <string>
#include <vector>

namespace love
{
namespace mouse
{

#define instance() (Module::getInstance<Mouse>(Module::M_MOUSE))

static void lh_getPosition(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	double x = 0.0, y = 0.0;
	instance()->getPosition(x, y);
	answers[0] = lhat_real(x);
	answers[1] = lhat_real(y);
	*answerCount = 2;
}

static void lh_getX(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	double x = 0.0, y = 0.0;
	instance()->getPosition(x, y);
	answers[0] = lhat_real(x);
	*answerCount = 1;
}

static void lh_getY(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	double x = 0.0, y = 0.0;
	instance()->getPosition(x, y);
	answers[0] = lhat_real(y);
	*answerCount = 1;
}

static void lh_setPosition(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setPosition(lh::optNumber(arguments, count, 0, 0.0), lh::optNumber(arguments, count, 1, 0.0));
}

// love.mouse.Button. Each member's .value is LOVE's number for the button
// -- 1 left, 2 right, 3 middle, then SDL's X1 and X2 -- and none is 0, which
// no button is. X1 is the one a browser calls back and X2 forward, so the
// order here is not the order of the numbers.
struct ButtonName
{
	const char *member;
	int number;
};

static const ButtonName buttonNames[] =
{
	{"none", 0},
	{"left", 1},
	{"right", 2},
	{"middle", 3},
	{"forward", 5},
	{"back", 4},
	{"extra1", 6},
	{"extra2", 7},
	{"extra3", 8},
};

static const char *const M = "love.mouse";

// isDown(button, ...): true if any of the buttons is down. none never is.
static void lh_isDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<int> buttons;
	buttons.reserve(count);
	for (size_t i = 0; i < count; i++)
	{
		const char *member = lh::enumName(machine, arguments[i], M, "Button");
		if (*member == '\0')
			return;
		for (const ButtonName &b : buttonNames)
			if (std::strcmp(b.member, member) == 0)
				buttons.push_back(b.number);
	}
	answers[0] = lhat_bool(instance()->isDown(buttons));
	*answerCount = 1;
}

static void lh_setVisible(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setVisible(lh::optBool(arguments, count, 0, true));
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

} // mouse

namespace lh
{

LhatValue pushMouseButton(LhatMachine *machine, int number)
{
	using namespace love::mouse;
	// A button past the ones named here -- a mouse may have more -- is none.
	const char *member = "none";
	for (const ButtonName &b : buttonNames)
		if (b.number == number)
			member = b.member;
	return pushEnum(machine, M, "Button", member);
}

bool lhopen_love_mouse(Context &ctx)
{
	using namespace love::mouse;
	if (ctx.types())
	{
		std::vector<std::string> members;
		std::vector<int64_t> values;
		for (const ButtonName &b : buttonNames)
		{
			members.emplace_back(b.member);
			values.push_back(b.number);
		}
		return ctx.enumType(M, "Button", members, values);
	}

	const char *m = M;
	return ctx.func(m, "getPosition", "f^ -> (number^, number^);", lh_getPosition, nullptr)
		&& ctx.func(m, "getX", "f^ -> number^;", lh_getX, nullptr)
		&& ctx.func(m, "getY", "f^ -> number^;", lh_getY, nullptr)
		&& ctx.func(m, "setPosition", "p^number^, number^;", lh_setPosition, nullptr)
		&& ctx.func(m, "isDown", "f^love.mouse.Button, ... -> bool^;", lh_isDown, nullptr)
		&& ctx.func(m, "setVisible", "p^bool^;", lh_setVisible, nullptr)
		&& ctx.func(m, "isVisible", "f^ -> bool^;", lh_isVisible, nullptr);
}

} // lh
} // love

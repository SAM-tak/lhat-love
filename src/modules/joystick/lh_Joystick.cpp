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

// love.joystick for L^. The reference is wrap_JoystickModule.cpp and
// wrap_Joystick.cpp beside this file. Gamepad mappings, sensors on sticks
// and the power/connection queries come later.

#include "JoystickModule.h"
#include "Joystick.h"
#include "lh/lh.h"

#include <vector>

namespace love
{
namespace joystick
{

#define instance() (Module::getInstance<JoystickModule>(Module::M_JOYSTICK))

struct JoystickBinding
{
	lh::Errors *errors;
	lh::TypeRegistry *registry;
};

static JoystickBinding binding;

static Joystick *checkJoystick(LhatMachine *machine, const LhatValue *arguments, size_t count)
{
	Joystick *j = count > 0 ? lh::checkObject<Joystick>(arguments[0], *binding.registry) : nullptr;
	if (j == nullptr)
		lh::raise(machine, "Expected a Joystick");
	return j;
}

static void lh_getJoysticks(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	int n = instance()->getJoystickCount();
	for (int i = 0; i < n; i++)
	{
		Joystick *stick = instance()->getJoystick(i);
		bool refused = false;
		lhat_table_set(t, lhat_integer(i), lh::pushObject(machine, *binding.registry, stick), &refused);
	}
	answers[0] = table;
	*answerCount = 1;
}

static void lh_getJoystickCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_integer(instance()->getJoystickCount());
	*answerCount = 1;
}

static void lh_Joystick_isConnected(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	answers[0] = lhat_bool(j != nullptr && j->isConnected());
	*answerCount = 1;
}

static void lh_Joystick_getName(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	LhatValue out = lhat_nil();
	if (j != nullptr)
		lh::makeString(machine, j->getName(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_Joystick_getID(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	answers[0] = lhat_integer(j != nullptr ? j->getID() : 0);
	*answerCount = 1;
}

static void lh_Joystick_getGUID(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	LhatValue out = lhat_nil();
	if (j != nullptr)
		lh::makeString(machine, j->getGUID(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_Joystick_getAxisCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	answers[0] = lhat_integer(j != nullptr ? j->getAxisCount() : 0);
	*answerCount = 1;
}

static void lh_Joystick_getButtonCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	answers[0] = lhat_integer(j != nullptr ? j->getButtonCount() : 0);
	*answerCount = 1;
}

static void lh_Joystick_getHatCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	answers[0] = lhat_integer(j != nullptr ? j->getHatCount() : 0);
	*answerCount = 1;
}

// getAxis(index) -- from 0, as L^ counts (lhat 5529b14). The Lua API
// counted from 1; LOVE's own C++ always counted from 0, so no shift is left.
static void lh_Joystick_getAxis(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	answers[0] = lhat_real(j != nullptr ? j->getAxis((int) lh::optNumber(arguments, count, 1, 0)) : 0.0f);
	*answerCount = 1;
}

static void lh_Joystick_getHat(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	if (j == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	const char *name = "c";
	Joystick::getConstant(j->getHat((int) lh::optNumber(arguments, count, 1, 0)), name);
	LhatValue out = lhat_nil();
	lh::makeString(machine, name, &out);
	answers[0] = out;
	*answerCount = 1;
}

// isDown(button, ...) -- buttons counted from 0, as an index is in L^; true
// if any is down.
static void lh_Joystick_isDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	if (j == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::vector<int> buttons;
	for (size_t i = 1; i < count; i++)
		buttons.push_back((int) lh::optNumber(arguments, count, i, 0));
	answers[0] = lhat_bool(j->isDown(buttons));
	*answerCount = 1;
}

static void lh_Joystick_isGamepad(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	answers[0] = lhat_bool(j != nullptr && j->isGamepad());
	*answerCount = 1;
}

static void lh_Joystick_getGamepadAxis(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	if (j == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::string name = lh::optString(arguments, count, 1, "");
	Joystick::GamepadAxis axis;
	if (!Joystick::getConstant(name.c_str(), axis))
	{
		lh::raise(machine, "Invalid gamepad axis: " + name);
		return;
	}
	answers[0] = lhat_real(j->getGamepadAxis(axis));
	*answerCount = 1;
}

static void lh_Joystick_isGamepadDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	if (j == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::vector<Joystick::GamepadButton> buttons;
	for (size_t i = 1; i < count; i++)
	{
		std::string name = lh::optString(arguments, count, i, "");
		Joystick::GamepadButton button;
		if (!Joystick::getConstant(name.c_str(), button))
		{
			lh::raise(machine, "Invalid gamepad button: " + name);
			return;
		}
		buttons.push_back(button);
	}
	answers[0] = lhat_bool(j->isGamepadDown(buttons));
	*answerCount = 1;
}

// setVibration(left, right[, duration]) / setVibration() to stop.
static void lh_Joystick_setVibration(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									 LhatValue *answers, int *answerCount)
{
	(void) context;
	Joystick *j = checkJoystick(machine, arguments, count);
	if (j == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	if (count < 3)
	{
		answers[0] = lhat_bool(j->setVibration());
		*answerCount = 1;
		return;
	}
	float left = (float) lh::optNumber(arguments, count, 1, 0.0);
	float right = (float) lh::optNumber(arguments, count, 2, left);
	float duration = (float) lh::optNumber(arguments, count, 3, -1.0);
	answers[0] = lhat_bool(j->setVibration(left, right, duration));
	*answerCount = 1;
}

} // joystick

namespace lh
{

bool lhopen_love_joystick(Context &ctx)
{
	using namespace love::joystick;
	const char *m = "love.joystick";

	if (!ctx.objectType(m, "Joystick", Joystick::type))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;

	return ctx.func(m, "getJoysticks", "p^ -> t^{love.joystick.Joystick[]};", lh_getJoysticks, nullptr)
		&& ctx.func(m, "getJoystickCount", "f^ -> number^;", lh_getJoystickCount, nullptr)
		&& ctx.member(m, "Joystick", "isConnected", "f^self^ -> bool^;", lh_Joystick_isConnected, nullptr)
		&& ctx.member(m, "Joystick", "getName", "f^self^ -> string^;", lh_Joystick_getName, nullptr)
		&& ctx.member(m, "Joystick", "getID", "f^self^ -> number^;", lh_Joystick_getID, nullptr)
		&& ctx.member(m, "Joystick", "getGUID", "f^self^ -> string^;", lh_Joystick_getGUID, nullptr)
		&& ctx.member(m, "Joystick", "getAxisCount", "f^self^ -> number^;", lh_Joystick_getAxisCount, nullptr)
		&& ctx.member(m, "Joystick", "getButtonCount", "f^self^ -> number^;", lh_Joystick_getButtonCount, nullptr)
		&& ctx.member(m, "Joystick", "getHatCount", "f^self^ -> number^;", lh_Joystick_getHatCount, nullptr)
		&& ctx.member(m, "Joystick", "getAxis", "f^self^, number^ -> number^;", lh_Joystick_getAxis, nullptr)
		&& ctx.member(m, "Joystick", "getHat", "f^self^, number^ -> string^;", lh_Joystick_getHat, nullptr)
		&& ctx.member(m, "Joystick", "isDown", "f^self^, number^, ... -> bool^;", lh_Joystick_isDown, nullptr)
		&& ctx.member(m, "Joystick", "isGamepad", "f^self^ -> bool^;", lh_Joystick_isGamepad, nullptr)
		&& ctx.member(m, "Joystick", "getGamepadAxis", "f^self^, string^ -> number^;", lh_Joystick_getGamepadAxis, nullptr)
		&& ctx.member(m, "Joystick", "isGamepadDown", "f^self^, string^, ... -> bool^;", lh_Joystick_isGamepadDown, nullptr)
		&& ctx.member(m, "Joystick", "setVibration", "p^self^, ... -> bool^;", lh_Joystick_setVibration, nullptr);
}

} // lh
} // love

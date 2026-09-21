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

// love.system for L^. The reference is wrap_System.cpp beside this file.

#include "System.h"
#include "lh/lh.h"

#include <vector>

namespace love
{
namespace system
{

#define instance() (Module::getInstance<System>(Module::M_SYSTEM))

static void lh_getOS(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, System::getOS(), &out);
	answers[0] = out;
	*answerCount = 1;
}

static void lh_getProcessorCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_integer(instance()->getProcessorCount());
	*answerCount = 1;
}

static void lh_setClipboardText(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setClipboardText(lh::optString(arguments, count, 0, ""));
}

static void lh_getClipboardText(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	LhatValue out = lhat_nil();
	lh::makeString(machine, instance()->getClipboardText(), &out);
	answers[0] = out;
	*answerCount = 1;
}

// getPowerInfo() -> (state, seconds, percent); -1 where unknown.
static void lh_getPowerInfo(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	int seconds = -1, percent = -1;
	System::PowerState state = instance()->getPowerInfo(seconds, percent);
	const char *name = "unknown";
	System::getConstant(state, name);
	answers[0] = lhat_nil();
	lh::makeString(machine, name, &answers[0]);
	answers[1] = lhat_integer(seconds);
	answers[2] = lhat_integer(percent);
	*answerCount = 3;
}

static void lh_openURL(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	answers[0] = lhat_bool(instance()->openURL(lh::optString(arguments, count, 0, "")));
	*answerCount = 1;
}

static void lh_vibrate(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->vibrate(lh::optNumber(arguments, count, 0, 0.5));
}

static void lh_hasBackgroundMusic(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->hasBackgroundMusic());
	*answerCount = 1;
}

static void lh_getPreferredLocales(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	std::vector<std::string> locales = instance()->getPreferredLocales();
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < locales.size(); i++)
	{
		LhatValue value = lhat_nil();
		bool refused = false;
		lh::makeString(machine, locales[i], &value);
		lhat_table_set(t, lhat_integer((int64_t) i), value, &refused);
	}
	answers[0] = table;
	*answerCount = 1;
}

} // system

namespace lh
{

bool lhopen_love_system(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::system;
	const char *m = "love.system";
	return ctx.func(m, "getOS", "f^ -> string^;", lh_getOS, nullptr)
		&& ctx.func(m, "getProcessorCount", "f^ -> number^;", lh_getProcessorCount, nullptr)
		&& ctx.func(m, "setClipboardText", "p^string^;", lh_setClipboardText, nullptr)
		&& ctx.func(m, "getClipboardText", "p^ -> string^;", lh_getClipboardText, nullptr)
		&& ctx.func(m, "getPowerInfo", "p^ -> (string^, number^, number^);", lh_getPowerInfo, nullptr)
		&& ctx.func(m, "openURL", "p^string^ -> bool^;", lh_openURL, nullptr)
		&& ctx.func(m, "vibrate", "p^number^;", lh_vibrate, nullptr)
		&& ctx.func(m, "hasBackgroundMusic", "f^ -> bool^;", lh_hasBackgroundMusic, nullptr)
		&& ctx.func(m, "getPreferredLocales", "p^ -> t^{string^[]};", lh_getPreferredLocales, nullptr);
}

} // lh
} // love

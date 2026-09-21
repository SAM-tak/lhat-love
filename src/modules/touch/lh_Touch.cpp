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

// love.touch for L^. The reference is wrap_Touch.cpp beside this file. A
// touch id is the integer its light userdata carried in Lua.

#include "Touch.h"
#include "lh/lh.h"

namespace love
{
namespace touch
{

#define instance() (Module::getInstance<Touch>(Module::M_TOUCH))

static void lh_getTouches(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) arguments;
	(void) count;
	const std::vector<Touch::TouchInfo> &touches = instance()->getTouches();
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return;
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < touches.size(); i++)
	{
		bool refused = false;
		lhat_table_set(t, lhat_integer((int64_t) i), lhat_integer(touches[i].id), &refused);
	}
	answers[0] = table;
	*answerCount = 1;
}

static void lh_getPosition(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	int64 id = (int64) lh::optNumber(arguments, count, 0, 0);
	lh::guard(machine, [&]() {
		const Touch::TouchInfo &info = instance()->getTouch(id);
		answers[0] = lhat_real(info.x);
		answers[1] = lhat_real(info.y);
		*answerCount = 2;
	});
}

static void lh_getPressure(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	int64 id = (int64) lh::optNumber(arguments, count, 0, 0);
	lh::guard(machine, [&]() {
		answers[0] = lhat_real(instance()->getTouch(id).pressure);
		*answerCount = 1;
	});
}

} // touch

namespace lh
{

bool lhopen_love_touch(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::touch;
	const char *m = "love.touch";
	return ctx.func(m, "getTouches", "f^ -> t^{number^[]};", lh_getTouches, nullptr)
		&& ctx.func(m, "getPosition", "f^number^ -> (number^, number^);", lh_getPosition, nullptr)
		&& ctx.func(m, "getPressure", "f^number^ -> number^;", lh_getPressure, nullptr);
}

} // lh
} // love

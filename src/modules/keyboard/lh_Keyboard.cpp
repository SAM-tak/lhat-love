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

// love.keyboard for L^. The reference is wrap_Keyboard.cpp beside this file.

#include "Keyboard.h"
#include "lh_Keyboard.h"
#include "lh/lh.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace love
{
namespace keyboard
{

#define instance() (Module::getInstance<Keyboard>(Module::M_KEYBOARD))

static const char *const M = "love.keyboard";

// LOVE's spelling -> the member's, for the names that are not identifiers.
// Each takes SDL's name for its key (the KEY_ / SCANCODE_ constant in
// Keyboard.h), lowercased, with the underscores dropped and "digit" before a
// bare number. Every other name is its own member.
struct Rename
{
	const char *love;
	const char *member;
};

static const Rename keyRenames[] =
{
	{"!", "exclaim"},
	{"\"", "quotedbl"},
	{"#", "hash"},
	{"%", "percent"},
	{"$", "dollar"},
	{"&", "ampersand"},
	{"'", "quote"},
	{"(", "leftparen"},
	{")", "rightparen"},
	{"*", "asterisk"},
	{"+", "plus"},
	{",", "comma"},
	{"-", "minus"},
	{".", "period"},
	{"/", "slash"},
	{"0", "digit0"},
	{"1", "digit1"},
	{"2", "digit2"},
	{"3", "digit3"},
	{"4", "digit4"},
	{"5", "digit5"},
	{"6", "digit6"},
	{"7", "digit7"},
	{"8", "digit8"},
	{"9", "digit9"},
	{":", "colon"},
	{";", "semicolon"},
	{"<", "less"},
	{"=", "equals"},
	{">", "greater"},
	{"?", "question"},
	{"@", "at"},
	{"[", "leftbracket"},
	{"\\", "backslash"},
	{"]", "rightbracket"},
	{"^", "caret"},
	{"_", "underscore"},
	{"`", "backquote"},
	{"kp/", "kpdivide"},
	{"kp*", "kpmultiply"},
	{"kp-", "kpminus"},
	{"kp+", "kpplus"},
	{"kp.", "kpperiod"},
	{"kp,", "kpcomma"},
	{"kp=", "kpequals"},
};

static const Rename scancodeRenames[] =
{
	{"1", "digit1"},
	{"2", "digit2"},
	{"3", "digit3"},
	{"4", "digit4"},
	{"5", "digit5"},
	{"6", "digit6"},
	{"7", "digit7"},
	{"8", "digit8"},
	{"9", "digit9"},
	{"0", "digit0"},
	{"-", "minus"},
	{"=", "equals"},
	{"[", "leftbracket"},
	{"]", "rightbracket"},
	{"\\", "backslash"},
	{"nonus#", "nonushash"},
	{";", "semicolon"},
	{"'", "apostrophe"},
	{"`", "grave"},
	{",", "comma"},
	{".", "period"},
	{"/", "slash"},
	{"kp/", "kpdivide"},
	{"kp*", "kpmultiply"},
	{"kp-", "kpminus"},
	{"kp+", "kpplus"},
	{"kp.", "kpperiod"},
	{"kp=", "kpequals"},
	{"kp,", "kpcomma"},
	{"kp=400", "kpequalsas400"},
	{"kp(", "kpleftparen"},
	{"kp)", "kprightparen"},
	{"kp{", "kpleftbrace"},
	{"kp}", "kprightbrace"},
	{"kp%", "kppercent"},
	{"kp<", "kpless"},
	{"kp>", "kpgreater"},
	{"kp&", "kpampersand"},
	{"kp&&", "kpdblampersand"},
	{"kp|", "kpverticalbar"},
	{"kp||", "kpdblverticalbar"},
	{"kp:", "kpcolon"},
	{"kp#", "kphash"},
	{"kp ", "kpspace"},
	{"kp@", "kpat"},
	{"kp!", "kpexclam"},
	{"kpmem+", "kpmemadd"},
	{"kpmem-", "kpmemsubtract"},
	{"kpmem*", "kpmemmultiply"},
	{"kpmem/", "kpmemdivide"},
	{"kp+-", "kpplusminus"},
};
class Names
{
public:
	template <size_t N>
	explicit Names(const Rename (&renames)[N])
	{
		for (const Rename &r : renames)
		{
			toMember[r.love] = r.member;
			toLove[r.member] = r.love;
		}
	}

	const char *member(const char *love) const
	{
		auto it = toMember.find(love);
		return it != toMember.end() ? it->second : love;
	}

	const char *love(const char *member) const
	{
		auto it = toLove.find(member);
		return it != toLove.end() ? it->second : member;
	}

	std::vector<std::string> members(const std::vector<std::string> &loveNames) const
	{
		std::vector<std::string> out;
		out.reserve(loveNames.size());
		for (const std::string &name : loveNames)
			out.emplace_back(member(name.c_str()));
		return out;
	}

private:
	std::unordered_map<std::string, const char *> toMember;
	std::unordered_map<std::string, const char *> toLove;
};

static const Names &keyNames()
{
	static const Names names(keyRenames);
	return names;
}

static const Names &scancodeNames()
{
	static const Names names(scancodeRenames);
	return names;
}

// Whether LOVE names this key in its own table. Asked by name rather than
// through Keyboard::getConstant, which would add the character to that table.
static bool isBuiltinKey(const char *love)
{
	static const std::unordered_set<std::string> builtin = []
	{
		std::vector<std::string> names = Keyboard::getConstants(Keyboard::Key {});
		return std::unordered_set<std::string>(names.begin(), names.end());
	}();
	return builtin.count(love) != 0;
}

static bool keyArgument(LhatMachine *machine, LhatValue value, Keyboard::Key &out)
{
	const char *member = lh::enumName(machine, value, M, "Key");
	return *member != '\0' && Keyboard::getConstant(keyNames().love(member), out);
}

static bool scancodeArgument(LhatMachine *machine, LhatValue value, Keyboard::Scancode &out)
{
	const char *member = lh::enumName(machine, value, M, "Scancode");
	return *member != '\0' && Keyboard::getConstant(scancodeNames().love(member), out);
}

// isDown(key, ...): true if any of the keys is down.
static void lh_isDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<Keyboard::Key> keys;
	keys.reserve(count);
	for (size_t i = 0; i < count; i++)
	{
		Keyboard::Key k;
		if (!keyArgument(machine, arguments[i], k))
			return;
		keys.push_back(k);
	}
	answers[0] = lhat_bool(instance()->isDown(keys));
	*answerCount = 1;
}

static void lh_isScancodeDown(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<Keyboard::Scancode> codes;
	codes.reserve(count);
	for (size_t i = 0; i < count; i++)
	{
		Keyboard::Scancode s;
		if (!scancodeArgument(machine, arguments[i], s))
			return;
		codes.push_back(s);
	}
	answers[0] = lhat_bool(instance()->isScancodeDown(codes));
	*answerCount = 1;
}

static void lh_getKeyFromScancode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) count;
	Keyboard::Scancode s;
	if (!scancodeArgument(machine, arguments[0], s))
		return;
	const char *name = nullptr;
	if (!Keyboard::getConstant(instance()->getKeyFromScancode(s), name))
		name = "unknown";
	answers[0] = lh::pushKey(machine, name);
	*answerCount = 1;
}

static void lh_getScancodeFromKey(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	(void) count;
	Keyboard::Key k;
	if (!keyArgument(machine, arguments[0], k))
		return;
	const char *name = nullptr;
	if (!Keyboard::getConstant(instance()->getScancodeFromKey(k), name))
		name = "unknown";
	answers[0] = lh::pushScancode(machine, name);
	*answerCount = 1;
}

static void lh_setKeyRepeat(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setKeyRepeat(lh::optBool(arguments, count, 0, false));
}

static void lh_hasKeyRepeat(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->hasKeyRepeat());
	*answerCount = 1;
}

static void lh_setTextInput(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setTextInput(lh::optBool(arguments, count, 0, false));
}

static void lh_hasTextInput(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_bool(instance()->hasTextInput());
	*answerCount = 1;
}

} // keyboard

namespace lh
{

LhatValue pushKey(LhatMachine *machine, const char *loveName)
{
	using namespace love::keyboard;
	// A character outside the declared list is still a key, but not a
	// member: it arrives as unknown.
	const char *known = loveName != nullptr && isBuiltinKey(loveName)
		? keyNames().member(loveName) : "unknown";
	return pushEnum(machine, M, "Key", known);
}

LhatValue pushScancode(LhatMachine *machine, const char *loveName)
{
	using namespace love::keyboard;
	Keyboard::Scancode code;
	const char *known = loveName != nullptr && Keyboard::getConstant(loveName, code)
		? scancodeNames().member(loveName) : "unknown";
	return pushEnum(machine, M, "Scancode", known);
}

bool lhopen_love_keyboard(Context &ctx)
{
	using namespace love::keyboard;
	if (ctx.types())
		return ctx.enumType(M, "Key", keyNames().members(Keyboard::getConstants(Keyboard::Key {})))
			&& ctx.enumType(M, "Scancode", scancodeNames().members(Keyboard::getConstants(Keyboard::Scancode {})));

	return ctx.func(M, "isDown", "f^love.keyboard.Key, ... -> bool^;", lh_isDown, nullptr)
		&& ctx.func(M, "isScancodeDown", "f^love.keyboard.Scancode, ... -> bool^;", lh_isScancodeDown, nullptr)
		&& ctx.func(M, "getKeyFromScancode", "f^love.keyboard.Scancode -> love.keyboard.Key;", lh_getKeyFromScancode, nullptr)
		&& ctx.func(M, "getScancodeFromKey", "f^love.keyboard.Key -> love.keyboard.Scancode;", lh_getScancodeFromKey, nullptr)
		&& ctx.func(M, "setKeyRepeat", "p^bool^;", lh_setKeyRepeat, nullptr)
		&& ctx.func(M, "hasKeyRepeat", "f^ -> bool^;", lh_hasKeyRepeat, nullptr)
		&& ctx.func(M, "setTextInput", "p^bool^;", lh_setTextInput, nullptr)
		&& ctx.func(M, "hasTextInput", "f^ -> bool^;", lh_hasTextInput, nullptr);
}

} // lh
} // love

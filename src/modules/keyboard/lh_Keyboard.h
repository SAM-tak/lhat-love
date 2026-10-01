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

#ifndef LOVE_KEYBOARD_LH_KEYBOARD_H
#define LOVE_KEYBOARD_LH_KEYBOARD_H

// love.keyboard's two enums, for the event module as well: keypressed and
// keyreleased hand the game a KeyConstant and a Scancode.
//
// The members are LOVE's constant names, except where a name is not an
// identifier ("1", "-", "kp+", "nonus#"): those take SDL's name for the same
// key, lowercased ("digit1", "minus", "kpplus", "nonushash"). A key outside
// the declared list -- any character a layout produces can be a key -- is
// KeyConstant.unknown; the scancode says where it is, textinput what it types.

#include "lh/lh.h"

namespace love
{
namespace lh
{

LhatValue pushKeyConstant(LhatMachine *machine, const char *loveName);
LhatValue pushScancode(LhatMachine *machine, const char *loveName);

} // lh
} // love

#endif // LOVE_KEYBOARD_LH_KEYBOARD_H

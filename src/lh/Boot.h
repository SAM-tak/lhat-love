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

#ifndef LOVE_LH_BOOT_H
#define LOVE_LH_BOOT_H

#include "common/config.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Runs a game: the L^ successor of boot.lua + callbacks.lua + the Lua-state
// setup that src/love.cpp used to do. Exported from liblove so the thin
// love/lovec executables stay in love.cpp.
//
// `console` says the caller is lovec (or any build that writes to a terminal
// of its own): problems go to stderr only, never to a message box.
//
// Answers the process exit code, or LOVE_LH_RESTART when the game asked to
// be restarted (love.event.restart): call again with the same arguments.
// What the game handed restart() waits in between as a carried value the
// next run uncarries for love.restartValue(); a game path the no-game
// screen was given by a drop replaces the arguments.
#define LOVE_LH_RESTART (-0x4c48)

LOVE_EXPORT int love_lh_boot(int argc, char **argv, bool console);

// Gives back what a registration interned for the process. 05 の 8.7 makes a
// registration a declaration, and lhat keeps one identity per declaration for
// as long as the process runs -- a restart builds a new program but not new
// tags. May be called only when no LhatProgram is left, which is once the
// restart loop around love_lh_boot has run out.
LOVE_EXPORT void love_lh_shutdown(void);

#ifdef __cplusplus
}

#include "common/Variant.h"
#include <string>

namespace love
{
namespace lh
{

// What a restart carries over, set by love.event's dispatch when a quit
// message says "restart" and read by the next boot. The payload is a
// Variant so that a carried L^ value (lh::Carried) or a scalar survives
// the machine; a LOVE object is refused at love.event.restart.
void setRestartPayload(const Variant &payload);
void setRestartGamePath(const std::string &path);

// What the run before handed love.event.restart -- nil on a first run.
// love.event.restartValue() answers this.
const Variant &restartPayload();

// love.event.reload: reads every changed unit again and, when they check
// and compile, asks for the game to be started over at the next frame
// boundary -- its main unit run afresh, a new handlers table, a new run --
// without a new program, window or modules. Units nothing changed keep
// their state. Answers the empty string when the reload was accepted, and
// the diagnostics otherwise; the running game is left as it was then.
std::string requestReload();

} // lh
} // love
#endif

#endif // LOVE_LH_BOOT_H

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

#include "common/version.h"
#include "modules/love/love.h"
#include "lh/Boot.h"

#include <SDL3/SDL.h>

#ifdef LOVE_BUILD_EXE

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>

#ifdef LOVE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // LOVE_WINDOWS

#ifdef LOVE_WINDOWS
extern "C"
{

// Prefer the higher performance GPU on Windows systems that use nvidia Optimus.
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
// TODO: Re-evaluate in the future when the average integrated GPU in Optimus
// systems is less mediocre?
LOVE_EXPORT DWORD NvOptimusEnablement = 1;

// Same with AMD GPUs.
// https://community.amd.com/thread/169965
LOVE_EXPORT DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif // LOVE_WINDOWS

static void print_usage()
{
	printf("lhatove is LOVE with L^ as its scripting language\n"
		"https://github.com/SAM-tak/lhatove\n"
		"\n"
		"usage:\n"
		"    love --version                  prints the version and quits\n"
		"    love --help                     prints this message and quits\n"
		"    love --dump-host-api [file]     writes the L^ host API (lhat-host.json) for the language server and quits\n"
		"    lovec --compile -o DIR file.lh  compiles the source and its required units without running them\n"
		"    lovec --compile -o DIR file.lton  compiles LTON data without running it\n"
		"    lovec --compile-game DIR GAME  compiles a whole game and copies its configuration and assets\n"
		"    love --no-error-screen GAME    reports errors to stderr and exits instead of waiting on an error screen\n"
		"    --debug-names                  keeps local and captured names in compiled units\n"
		"    --jobs N                       LTON compiler threads for --compile-game (0: physical cores on Windows, 1: serial)\n"
		"    love path/to/gamedir            runs the game from the given directory which contains a main.lh file\n"
		"    love path/to/packagedgame.love  runs the packaged game from the provided .love file\n"
		"    love path/to/file.lh            runs the game from the given .lh file\n"
		);
}

int main(int argc, char **argv)
{
	if (strcmp(LOVE_VERSION_STRING, love_version()) != 0)
	{
		printf("Version mismatch detected!\nLOVE binary is version %s\n"
			   "LOVE library is version %s\n", LOVE_VERSION_STRING, love_version());
		return 1;
	}

	// Oh, you just want the version? Okay!
	if (argc > 1 && strcmp(argv[1], "--version") == 0)
	{
#ifdef LOVE_LEGENDARY_CONSOLE_IO_HACK
		const char *err = nullptr;
		love_openConsole(err);
#endif
		printf("LOVE %s (%s)\n", love_version(), love_codename());
		return 0;
	}

	if (argc > 1 && strcmp(argv[1], "--help") == 0)
	{
		print_usage();
		return 0;
	}

	// The whole run lives in liblove; a restart comes back here and goes
	// round again.
	int code;
	do
	{
#ifdef LOVE_LH_CONSOLE_EXE
		code = love_lh_boot(argc, argv, true);
#else
		code = love_lh_boot(argc, argv, false);
#endif
	} while (code == LOVE_LH_RESTART);

	// No program is left now, which is what lets the interned tags go.
	love_lh_shutdown();
	return code;
}

#endif // LOVE_BUILD_EXE

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

// love.audio for L^: Sources from files, the module-level play/stop/pause,
// and the listener volume. The reference is wrap_Audio.cpp / wrap_Source.cpp
// beside this file. Effects, filters and queueable sources come later.

#include "Audio.h"
#include "Source.h"
#include "lh/lh.h"

#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/File.h"
#include "modules/sound/Sound.h"
#include "modules/sound/SoundData.h"
#include "modules/sound/Decoder.h"

#include <vector>

namespace love
{
namespace audio
{

#define instance() (Module::getInstance<Audio>(Module::M_AUDIO))

struct AudioBinding
{
	lh::Errors *errors;
	lh::TypeRegistry *registry;
};

static AudioBinding binding;

static Source *checkSource(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index = 0)
{
	Source *source = index < count ? lh::checkObject<Source>(arguments[index], *binding.registry) : nullptr;
	if (source == nullptr)
		lh::raise(machine, "Expected a Source");
	return source;
}

// newSource(path, "static" | "stream") / newSource(sounddata): wrap_Audio's
// path through love.sound -- a decoder over the file, and for a static
// source the whole of it decoded up front.
static void lh_newSource(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) context;
	auto sound = Module::getInstance<love::sound::Sound>(Module::M_SOUND);
	auto fs = Module::getInstance<love::filesystem::Filesystem>(Module::M_FILESYSTEM);
	if (sound == nullptr)
	{
		lh::raise(machine, "Cannot create sources without the love.sound module.");
		return;
	}

	if (count > 0 && lhat_is_object_kind(arguments[0], LHAT_OBJECT_HOSTDATA))
	{
		auto data = lh::checkObject<love::sound::SoundData>(arguments[0], *binding.registry);
		if (data == nullptr)
		{
			lh::raise(machine, "Expected a SoundData");
			return;
		}
		lh::guard(machine, [&]() {
			StrongRef<Source> source(instance()->newSource(data), Acquire::NORETAIN);
			answers[0] = lh::pushObject(machine, *binding.registry, source.get());
			*answerCount = 1;
		});
		return;
	}

	std::string path = lh::optString(arguments, count, 0, "");
	std::string typestr = lh::optEnum(machine, arguments, count, 1, "love.audio", "SourceType", "stream");
	Source::Type stype = Source::TYPE_STREAM;
	if (!Source::getConstant(typestr.c_str(), stype) || stype == Source::TYPE_QUEUE)
	{
		lh::raise(machine, "Invalid source type: " + typestr);
		return;
	}
	if (fs == nullptr)
	{
		lh::raise(machine, "love.filesystem is not loaded.");
		return;
	}

	lh::catchexcept(machine, binding.errors->audioCouldNotLoad, [&]() {
		StrongRef<love::filesystem::File> file(fs->openFile(path.c_str(), love::filesystem::File::MODE_READ), Acquire::NORETAIN);
		StrongRef<love::sound::Decoder> decoder(sound->newDecoder(file.get(), love::sound::Decoder::DEFAULT_BUFFER_SIZE), Acquire::NORETAIN);
		StrongRef<Source> source;
		if (stype == Source::TYPE_STATIC)
		{
			StrongRef<love::sound::SoundData> data(sound->newSoundData(decoder.get()), Acquire::NORETAIN);
			source.set(instance()->newSource(data.get()), Acquire::NORETAIN);
		}
		else
			source.set(instance()->newSource(decoder.get()), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, source.get());
		*answerCount = 1;
	}, answers, answerCount);
}

// play(source, ...) plays each; play() with nothing is a mistake in Lua, a
// no-op here.
static void lh_play(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) context;
	std::vector<Source *> sources;
	for (size_t i = 0; i < count; i++)
	{
		Source *s = checkSource(machine, arguments, count, i);
		if (s == nullptr)
		{
			answers[0] = lhat_nil();
			*answerCount = 1;
			return;
		}
		sources.push_back(s);
	}
	lh::guard(machine, [&]() {
		answers[0] = lhat_bool(sources.size() == 1 ? instance()->play(sources[0]) : instance()->play(sources));
		*answerCount = 1;
	});
}

static void lh_stop(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count == 0)
	{
		instance()->stop();
		return;
	}
	std::vector<Source *> sources;
	for (size_t i = 0; i < count; i++)
	{
		Source *s = checkSource(machine, arguments, count, i);
		if (s == nullptr)
			return;
		sources.push_back(s);
	}
	instance()->stop(sources);
}

static void lh_pause(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
					 LhatValue *answers, int *answerCount)
{
	(void) context;
	if (count == 0)
	{
		instance()->pause();
		return;
	}
	std::vector<Source *> sources;
	for (size_t i = 0; i < count; i++)
	{
		Source *s = checkSource(machine, arguments, count, i);
		if (s == nullptr)
			return;
		sources.push_back(s);
	}
	instance()->pause(sources);
}

static void lh_setVolume(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	instance()->setVolume((float) lh::optNumber(arguments, count, 0, 1.0));
}

static void lh_getVolume(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						 LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_real(instance()->getVolume());
	*answerCount = 1;
}

static void lh_getActiveSourceCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) machine;
	(void) context;
	(void) arguments;
	(void) count;
	answers[0] = lhat_integer(instance()->getActiveSourceCount());
	*answerCount = 1;
}

// ---------------------------------------------------------------------------
// Source
// ---------------------------------------------------------------------------

static void lh_Source_play(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	if (s == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() { answers[0] = lhat_bool(s->play()); *answerCount = 1; });
}

static void lh_Source_stop(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	if (s != nullptr)
		s->stop();
}

static void lh_Source_pause(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	if (s != nullptr)
		s->pause();
}

static void lh_Source_isPlaying(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	answers[0] = lhat_bool(s != nullptr && s->isPlaying());
	*answerCount = 1;
}

static void lh_Source_setLooping(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	if (s == nullptr)
		return;
	lh::guard(machine, [&]() {
		s->setLooping(lh::optBool(arguments, count, 1, false));
	});
}

static void lh_Source_isLooping(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	answers[0] = lhat_bool(s != nullptr && s->isLooping());
	*answerCount = 1;
}

static void lh_Source_setVolume(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	if (s != nullptr)
		s->setVolume((float) lh::optNumber(arguments, count, 1, 1.0));
}

static void lh_Source_getVolume(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	answers[0] = lhat_real(s != nullptr ? s->getVolume() : 0.0f);
	*answerCount = 1;
}

static void lh_Source_setPitch(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	if (s == nullptr)
		return;
	float pitch = (float) lh::optNumber(arguments, count, 1, 1.0);
	if (pitch <= 0.0f)
	{
		lh::raise(machine, "Pitch has to be a positive number.");
		return;
	}
	s->setPitch(pitch);
}

static void lh_Source_getPitch(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	answers[0] = lhat_real(s != nullptr ? s->getPitch() : 0.0f);
	*answerCount = 1;
}

static bool unitOf(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index, Source::Unit &unit)
{
	unit = Source::UNIT_SECONDS;
	if (index >= count)
		return true;
	std::string name = lh::optEnum(machine, arguments, count, index, "love.audio", "TimeUnit", "seconds");
	if (!Source::getConstant(name.c_str(), unit))
	{
		lh::raise(machine, "Invalid time unit: " + name);
		return false;
	}
	return true;
}

static void lh_Source_seek(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	Source::Unit unit;
	if (s == nullptr || !unitOf(machine, arguments, count, 2, unit))
		return;
	lh::guard(machine, [&]() {
		s->seek(lh::optNumber(arguments, count, 1, 0.0), unit);
	});
}

static void lh_Source_tell(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
						   LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	Source::Unit unit;
	if (s == nullptr || !unitOf(machine, arguments, count, 1, unit))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_real(s->tell(unit));
	*answerCount = 1;
}

static void lh_Source_getDuration(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
								  LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	Source::Unit unit;
	if (s == nullptr || !unitOf(machine, arguments, count, 1, unit))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	answers[0] = lhat_real(s->getDuration(unit));
	*answerCount = 1;
}

static void lh_Source_clone(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
							LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	if (s == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		StrongRef<Source> copy(s->clone(), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, copy.get());
		*answerCount = 1;
	});
}

static void lh_Source_getChannelCount(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	Source *s = checkSource(machine, arguments, count);
	answers[0] = lhat_integer(s != nullptr ? s->getChannelCount() : 0);
	*answerCount = 1;
}

} // audio

namespace lh
{

bool lhopen_love_audio(Context &ctx)
{
	using namespace love::audio;
	if (ctx.types())
	{
		if (!ctx.enumType("love.audio", "SourceType", Source::getConstants(Source::Type{})))
			return false;
		if (!ctx.enumType("love.audio", "TimeUnit", Source::getConstants(Source::Unit{})))
			return false;
	}

	const char *m = "love.audio";

	// 04 の 2.4: what love.audio can fail at, declared where it fails.
	if (ctx.types())
	{
		static const char *const variants[] = {"CouldNotLoad"};
		const LhatErrorKind *kinds[1] = {nullptr};
		if (!ctx.errorKind(m, variants, 1, kinds))
			return false;
		ctx.errors->audioCouldNotLoad = kinds[0];
	}
	if (!ctx.objectType(m, "Source", Source::type))
		return false;
	if (ctx.types())
		return true;

	binding.errors = ctx.errors;
	binding.registry = ctx.registry;

	return ctx.func(m, "newSource", "p^string^ -> love.audio.Source|love.audio.Error;", lh_newSource, nullptr)
		&& ctx.func(m, "newSource", "p^string^, love.audio.SourceType -> love.audio.Source|love.audio.Error;", lh_newSource, nullptr)
		&& ctx.func(m, "newSource", "p^love.sound.SoundData -> love.audio.Source;", lh_newSource, nullptr)
		&& ctx.func(m, "play", "p^love.audio.Source, ... -> bool^;", lh_play, nullptr)
		&& ctx.func(m, "stop", "p^...;", lh_stop, nullptr)
		&& ctx.func(m, "pause", "p^...;", lh_pause, nullptr)
		&& ctx.func(m, "setVolume", "p^number^;", lh_setVolume, nullptr)
		&& ctx.func(m, "getVolume", "f^ -> number^;", lh_getVolume, nullptr)
		&& ctx.func(m, "getActiveSourceCount", "f^ -> number^;", lh_getActiveSourceCount, nullptr)
		&& ctx.member(m, "Source", "play", "p^self^ -> bool^;", lh_Source_play, nullptr)
		&& ctx.member(m, "Source", "stop", "p^self^;", lh_Source_stop, nullptr)
		&& ctx.member(m, "Source", "pause", "p^self^;", lh_Source_pause, nullptr)
		&& ctx.member(m, "Source", "isPlaying", "f^self^ -> bool^;", lh_Source_isPlaying, nullptr)
		&& ctx.member(m, "Source", "setLooping", "p^self^, bool^;", lh_Source_setLooping, nullptr)
		&& ctx.member(m, "Source", "isLooping", "f^self^ -> bool^;", lh_Source_isLooping, nullptr)
		&& ctx.member(m, "Source", "setVolume", "p^self^, number^;", lh_Source_setVolume, nullptr)
		&& ctx.member(m, "Source", "getVolume", "f^self^ -> number^;", lh_Source_getVolume, nullptr)
		&& ctx.member(m, "Source", "setPitch", "p^self^, number^;", lh_Source_setPitch, nullptr)
		&& ctx.member(m, "Source", "getPitch", "f^self^ -> number^;", lh_Source_getPitch, nullptr)
		&& ctx.member(m, "Source", "seek", "p^self^, number^;", lh_Source_seek, nullptr)
		&& ctx.member(m, "Source", "seek", "p^self^, number^, love.audio.TimeUnit;", lh_Source_seek, nullptr)
		&& ctx.member(m, "Source", "tell", "f^self^ -> number^;", lh_Source_tell, nullptr)
		&& ctx.member(m, "Source", "tell", "f^self^, love.audio.TimeUnit -> number^;", lh_Source_tell, nullptr)
		&& ctx.member(m, "Source", "getDuration", "f^self^ -> number^;", lh_Source_getDuration, nullptr)
		&& ctx.member(m, "Source", "getDuration", "f^self^, love.audio.TimeUnit -> number^;", lh_Source_getDuration, nullptr)
		&& ctx.member(m, "Source", "clone", "p^self^ -> love.audio.Source;", lh_Source_clone, nullptr)
		&& ctx.member(m, "Source", "getChannelCount", "f^self^ -> number^;", lh_Source_getChannelCount, nullptr);
}

} // lh
} // love

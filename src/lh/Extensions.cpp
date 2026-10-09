#include "Extensions.h"
#include "common/Object.h"
#include "modules/filesystem/Filesystem.h"
#include "modules/filesystem/FileData.h"
#include <SDL3/SDL_loadso.h>
#include <SDL3/SDL_error.h>
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace love {
namespace lh {
namespace {

// Only the platform loader is host-specific; L^ owns the module cache and ABI.
void *openLibrary(void *, const char *path) { return SDL_LoadObject(path); }
LhatExtensionSymbol findSymbol(void *, void *library, const char *name)
{
    return SDL_LoadFunction(static_cast<SDL_SharedObject *>(library), name);
}
void closeLibrary(void *, void *library) { SDL_UnloadObject(static_cast<SDL_SharedObject *>(library)); }
const char *loaderError(void *) { return SDL_GetError(); }

LhatExtensions *pool = nullptr;

std::string trim(const std::string &text)
{
	auto first = text.find_first_not_of(" \t\r");
	return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

} // namespace

std::string Extensions::readManifest(filesystem::Filesystem *fs)
{
	paths.clear();
	try
	{
		filesystem::Filesystem::Info info = {};
		if (!fs->getInfo("extensions.txt", info))
			return {};
		if (fs->getRealDirectory("extensions.txt") != std::string(fs->getSource()))
			return "extensions.txt must come from the game source";
		if (info.type != filesystem::Filesystem::FILETYPE_FILE)
			return "extensions.txt must be a file";
#if defined(LOVE_ANDROID) || defined(LOVE_IOS)
		return "Native extensions are currently supported on desktop platforms only";
#endif
		StrongRef<filesystem::FileData> data(fs->read("extensions.txt"), Acquire::NORETAIN);
		std::string text(static_cast<const char *>(data->getData()), data->getSize());
		if (text.compare(0, 3, "\xef\xbb\xbf") == 0)
			text.erase(0, 3);
		if (text.find('\0') != std::string::npos)
			return "extensions.txt contains a NUL byte";
		auto source = std::filesystem::u8path(fs->getSource());
		auto base = std::filesystem::is_directory(source) ? source : source.parent_path();
		base = std::filesystem::canonical(base);
		std::istringstream lines(text);
		std::string line;
		size_t number = 0;
		while (std::getline(lines, line))
		{
			number++;
			line = trim(line);
			if (line.empty() || line[0] == '#')
				continue;
			auto relative = std::filesystem::u8path(line);
			bool invalid = relative.has_root_path()
				|| line.find_first_of(":\\") != std::string::npos;
			for (const auto &part : relative)
				invalid = invalid || part == ".." || part == "." || part.empty();
			if (invalid)
				return "extensions.txt:" + std::to_string(number) + ": expected a relative library path without '..' (use '/' separators)";
			auto suffix = relative.extension().u8string();
			if (suffix != ".dll" && suffix != ".so" && suffix != ".dylib")
			{
#if defined(LOVE_WINDOWS)
				relative += ".dll";
#elif defined(LOVE_MACOS)
				relative += ".dylib";
#else
				relative += ".so";
#endif
			}
			// Resolve an absolute path, never the OS loader's ambient search path.
			auto absolute = std::filesystem::weakly_canonical(base / relative);
			auto within = absolute.lexically_relative(base);
			if (within.empty() || *within.begin() == "..")
				return "extensions.txt:" + std::to_string(number) + ": library path escapes the game directory";
			std::string path = absolute.u8string();
			if (std::find(paths.begin(), paths.end(), path) != paths.end())
				return "extensions.txt:" + std::to_string(number) + ": duplicate library " + line;
			paths.push_back(path);
		}
		return {};
	}
	catch (const std::exception &e)
	{
		return std::string("Could not read extensions.txt: ") + e.what();
	}
}

std::string Extensions::install(LhatProgram *program) const
{
	if (paths.empty())
		return {};
	if (pool == nullptr)
	{
		const LhatExtensionLoader loader = {nullptr, openLibrary, findSymbol, closeLibrary, loaderError};
		pool = lhat_extensions_new(&loader);
		if (pool == nullptr)
			return "Could not create the native extension pool";
	}
	std::vector<const LhatExtensionModule *> modules;
	for (const auto &path : paths)
	{
		const auto *module = lhat_extensions_load(pool, path.c_str());
		if (module == nullptr)
			return lhat_extensions_error(pool);
		modules.push_back(module);
	}
	if (!lhat_extensions_register(pool, program, modules.data(), modules.size()))
		return lhat_extensions_error(pool);
	return {};
}

void Extensions::shutdown()
{
	lhat_extensions_free(pool);
	pool = nullptr;
}

} // lh
} // love

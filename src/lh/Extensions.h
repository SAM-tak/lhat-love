// Native extensions selected by the game source, before checking any L^ unit.
#ifndef LOVE_LH_EXTENSIONS_H
#define LOVE_LH_EXTENSIONS_H

#include <lhat/extension.h>
#include <string>
#include <vector>

namespace love {
namespace filesystem { class Filesystem; }
namespace lh {

class Extensions
{
public:

	// Called before the save directory is mounted. Only the game source may
	// supply the manifest; there is no adjacent-file or save-directory fallback.
	std::string readManifest(filesystem::Filesystem *fs);
	std::string install(LhatProgram *program) const;
	// Libraries outlive every program and the process-wide type registry,
	// including restarts. Call after lhat_registry_dispose, not at boot exit.
	static void shutdown();

private:
	std::vector<std::string> paths;
};

} // lh
} // love
#endif

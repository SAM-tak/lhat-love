// This extension links no copy of L^: every call uses the host's function table.
#include <lhat/extension.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PROBE_PEER
#define MODULE "peer"
#else
#define MODULE "probe"
#endif

#ifdef PROBE_BAD_SIGNATURES
static const uint8_t probe_signatures[] = {0};
#define probe_signatures_length 1
#elif defined(PROBE_SIGNATURE_HEADER)
#include PROBE_SIGNATURE_HEADER
#else
static const uint8_t *probe_signatures = NULL;
#define probe_signatures_length 0
#endif

typedef struct State {
    const LhatExtensionAPI *host;
} State;

static void finish(void *context)
{
    State *state = (State *) context;
    // Test that program cleanup runs while the library is still loaded.
    const char *path = getenv("LHAT_EXTENSION_TEST_LOG");
    if (path) {
        FILE *file = fopen(path, "a");
        if (file) { fprintf(file, "%s disposed\n", MODULE); fclose(file); }
    }
    state->host->lhat_free(state);
}

static void answer(LhatMachine *machine, void *context, const LhatValue *args,
                   size_t count, LhatValue *out, int *answers)
{
    (void) machine; (void) context; (void) args; (void) count;
    out[0] = lhat_integer(42);
    *answers = 1;
}

static void message(LhatMachine *machine, void *context, const LhatValue *args,
                    size_t count, LhatValue *out, int *answers)
{
    State *state = (State *) context;
    (void) args; (void) count;
    state->host->lhat_machine_make_string(machine, "native", 6, out);
    *answers = 1;
}

static const char *install(const LhatExtensionAPI *host, LhatProgram *program,
                           uint32_t phase, void **context)
{
    if (host->abi_version != LHAT_EXTENSION_ABI || host->struct_size < sizeof(*host))
        return "unsupported host API";
    if (phase == LHAT_EXTENSION_TYPES) {
        State *state = host->lhat_alloc(sizeof(*state));
        if (!state) return "out of memory";
        state->host = host;
        if (!host->lhat_program_on_dispose(program, finish, state)) {
            host->lhat_free(state);
            return "could not register cleanup";
        }
        *context = state;
        return host->lhat_register_type(program, MODULE, "Marker") ? NULL : "type registration failed";
    }
    if (!host->lhat_register_func(program, MODULE, "answer", "f^ -> number^;", answer, *context)
        || !host->lhat_register_func(program, MODULE, "message", "f^ -> string^;", message, *context))
        return "function registration failed";
#ifdef PROBE_PEER
    // Listed first in the manifest: this requires the later DLL's TYPES
    // pass to finish before this DLL starts registering its members.
    if (!host->lhat_register_func(program, MODULE, "accept", "f^probe.Marker -> number^;", answer, *context))
        return "cross-extension type registration failed";
#endif
    return NULL;
}

LHAT_EXTENSION_EXPORT const LhatExtension *lhat_extension_v1(void)
{
    static LhatExtension extension = {
#ifdef PROBE_BAD_ABI
        999,
#else
        LHAT_EXTENSION_ABI,
#endif
        sizeof(LhatExtension),
#ifdef PROBE_BAD_VERSION
        "wrong-version",
#else
        LHAT_VERSION,
#endif
        sizeof(LhatValue), MODULE, NULL, 0, install
    };
    extension.signatures = probe_signatures;
    extension.signatures_size = probe_signatures_length;
    return &extension;
}

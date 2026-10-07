#include "src/frontend/core_options.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int failures = 0;
void check(bool ok, const char* what, const char* key) {
    if (ok) return;
    std::fprintf(stderr, "%s: %s\n", key, what);
    failures++;
}
}  // namespace

int main() {
    using namespace rs::coreopt;
    const char* cores[] = {"gambatte", "gpsp",     "pcefast", "picodrive",
                           "quicknes", "smsplus",  "snes9x2005"};

    for (const Option& o : detail::ALL) {
        /* Every default must be one of its own values. */
        bool found = false;
        for (int i = 0; i < o.count; i++)
            found |= std::strcmp(o.values[i].value, o.def) == 0;
        check(found, "default is not in the value list", o.key);
        check(indexOf(o, nullptr) >= 0 && indexOf(o, "bogus") ==
                  indexOf(o, o.def), "unknown values fall back to the default",
              o.key);
        check(o.live == (std::strcmp(o.core, "*") == 0),
              "only the shared RetroShell options apply live", o.key);
    }

    for (const char* core : cores) {
        /* Nothing may be dropped by the MAX_PER_CORE cut-off. */
        int all = 0;
        for (const Option& o : detail::ALL)
            if (std::strcmp(o.core, "*") == 0 || std::strcmp(o.core, core) == 0)
                all++;
        const Option* opts[MAX_PER_CORE];
        const int n = forCore(core, opts);
        check(n == all, "options exceed MAX_PER_CORE", core);
        check(find(opts, n, FRAMESKIP_KEY) == 0, "frame skip comes first", core);
        check(find(opts, n, AUDIO_BUFFER_KEY) == 1, "audio buffer comes second",
              core);
        check(n > 2, "the core offers none of its own options", core);
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                check(std::strcmp(opts[i]->key, opts[j]->key) != 0,
                      "duplicate key", opts[i]->key);
    }

    /* A fixed skip parses to its frame count; Auto to 0. */
    const Option* opts[MAX_PER_CORE];
    const int n = forCore("gambatte", opts);
    const Option& skip = *opts[find(opts, n, FRAMESKIP_KEY)];
    check(std::atoi(skip.values[0].value) == 0, "Auto parses to 0", skip.key);
    for (int i = 1; i < skip.count; i++)
        check(std::atoi(skip.values[i].value) == i, "fixed skip parses", skip.key);

    if (failures) return 1;
    std::puts("core_options_test: ok");
    return 0;
}

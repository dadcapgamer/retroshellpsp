#include "src/frontend/launch_notice.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace rs::launch;

namespace {
int failures = 0;
void expect(const char* raw, Kind kind, Action action) {
    const Notice n = classify(raw);
    if (n.kind != kind || n.action != action || !*n.title || !*n.message) {
        std::fprintf(stderr, "'%s': kind %d action %d\n", raw, int(n.kind),
                     int(n.action));
        failures++;
    }
}
}  // namespace

int main() {
    expect("rom missing", Kind::RomMissing, Action::Rescan);
    expect("storage unavailable", Kind::StorageUnavailable, Action::None);
    expect("core not installed", Kind::CoreMissing, Action::ChooseCore);
    expect("core 'gambatte' not installed", Kind::CoreMissing,
           Action::ChooseCore);
    expect("FrogGBA needs system/gba_bios.bin", Kind::BiosMissing, Action::None);
    expect("froggba: required BIOS is missing", Kind::BiosMissing, Action::None);
    expect("froggba: ROM path is too long", Kind::PathInvalid, Action::Rescan);
    expect("rom exceeds memory budget", Kind::OutOfMemory, Action::ChooseCore);
    expect("zip open failed", Kind::ArchiveInvalid, Action::None);
    expect("core rejected rom", Kind::CoreRejected, Action::ChooseCore);
    expect("core init failed", Kind::CoreRejected, Action::ChooseCore);
    expect("something unforeseen", Kind::Generic, Action::None);
    expect("", Kind::Generic, Action::None);
    expect(nullptr, Kind::Generic, Action::None);

    /* No raw status codes or hex ever reach the user. */
    const char* raws[] = {"rom missing", "core init failed", "unknown 0x80020149"};
    for (const char* raw : raws) {
        const Notice n = classify(raw);
        if (std::strstr(n.title, "0x") || std::strstr(n.message, "0x"))
            failures++;
    }
    if (!*actionLabel(Action::Rescan) || *actionLabel(Action::None)) failures++;
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

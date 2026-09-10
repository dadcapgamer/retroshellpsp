#include "src/frontend/native_adapter_protocol.h"

#include <cassert>
#include <cstring>
#include <string>

int main() {
    using namespace rs::nativeemu::protocol;
    char receipt[256]{};
    assert(formatReceipt(receipt, sizeof receipt, "froggba", 0x1234abcd,
                         "launched"));
    assert(std::strstr(receipt, "romHash=1234abcd\n"));

    char adapter[49]{};
    char state[17]{};
    assert(parseReceipt(receipt, adapter, sizeof adapter,
                        state, sizeof state));
    assert(std::strcmp(adapter, "froggba") == 0);
    assert(std::strcmp(state, "launched") == 0);
    assert(!formatReceipt(receipt, sizeof receipt, "bad\nname", 0,
                          "returned"));
    assert(!formatReceipt(receipt, sizeof receipt, "froggba", 0,
                          "unknown"));
    assert(!parseReceipt("RETROSHELL_ADAPTER_SESSION=2\n", adapter,
                         sizeof adapter, state, sizeof state));
    assert(!parseReceipt(std::string(1025, 'x'), adapter, sizeof adapter,
                         state, sizeof state));
    assert(safeSessionPath(SESSION_PATH));
    assert(!safeSessionPath("ms0:/RETROSHELL/session/../bad"));
    return 0;
}

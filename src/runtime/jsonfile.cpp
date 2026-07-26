#include "runtime/jsonfile.h"

#include "platform/psp/fs_psp.h"

#include <cstring>
#include <vector>

namespace rs::json {

cJSON* parseFile(const char* path) {
    constexpr u32 MAX_JSON_BYTES = 256u * 1024u;
    std::vector<u8> buf;
    if (!fs::readFile(path, buf, MAX_JSON_BYTES)) return nullptr;
    buf.push_back(0);
    return cJSON_Parse(reinterpret_cast<const char*>(buf.data()));
}

bool writeFile(const char* path, cJSON* root) {
    char* text = cJSON_Print(root);
    if (!text) return false;
    const size_t len = std::strlen(text);
    const bool ok = len <= 256u * 1024u &&
                    fs::writeFileAtomic(path, text, u32(len));
    cJSON_free(text);
    return ok;
}

}  // namespace rs::json

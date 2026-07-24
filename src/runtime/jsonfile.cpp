#include "runtime/jsonfile.h"

#include "platform/psp/fs_psp.h"

#include <cstring>
#include <vector>

namespace rs::json {

cJSON* parseFile(const char* path) {
    std::vector<u8> buf;
    if (!fs::readFile(path, buf)) return nullptr;
    buf.push_back(0);
    return cJSON_Parse(reinterpret_cast<const char*>(buf.data()));
}

bool writeFile(const char* path, cJSON* root) {
    char* text = cJSON_Print(root);
    if (!text) return false;
    const bool ok = fs::writeFile(path, text, u32(std::strlen(text)));
    cJSON_free(text);
    return ok;
}

}  // namespace rs::json

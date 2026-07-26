#include "cJSON.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 256u * 1024u) return 0;
    cJSON* value = cJSON_ParseWithLength((const char*)data, size);
    cJSON_Delete(value);
    return 0;
}

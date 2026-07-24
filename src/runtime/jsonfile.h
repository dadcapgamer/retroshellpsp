/** Whole-file cJSON helpers used by config, themes, metadata, the library
 * and the core registry. Keeps the read-terminate-parse / print-write
 * dance in one place.
 */
#pragma once

#include "cJSON.h"

namespace rs::json {

/* Returns a parsed document (caller owns, cJSON_Delete) or nullptr. */
cJSON* parseFile(const char* path);

/* Pretty-prints `root` over `path`. */
bool writeFile(const char* path, cJSON* root);

}  // namespace rs::json

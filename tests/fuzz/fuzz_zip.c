#include "miniz.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 4u * 1024u * 1024u) return 0;
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, data, size, 0)) return 0;
    mz_uint count = mz_zip_reader_get_num_files(&zip);
    if (count > 4096) count = 4096;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) break;
        if (stat.m_uncomp_size <= 64u * 1024u &&
            stat.m_comp_size &&
            stat.m_uncomp_size / stat.m_comp_size <= 200) {
            uint8_t output[64u * 1024u];
            (void)mz_zip_reader_extract_to_mem(&zip, i, output,
                                                (size_t)stat.m_uncomp_size, 0);
        }
    }
    mz_zip_reader_end(&zip);
    return 0;
}

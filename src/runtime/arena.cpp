#include "runtime/arena.h"
#include "runtime/bounds.h"
#include "runtime/log.h"

#include <pspkernel.h>
#include <pspsysmem.h>

namespace rs::mem {

namespace {
/* Kept out of the arena so threads, IO buffers and the GU list still have
 * room to come from the partition after we grab our block. */
constexpr u32 SLACK_BYTES = 1536 * 1024;

SceUID s_block  = -1;
u8*    s_base   = nullptr;
u32    s_size   = 0;
u32    s_cursor = 0;
u32    s_high   = 0;
u32    s_failures = 0;
}  // namespace

bool init() {
    const u32 maxFree = u32(sceKernelMaxFreeMemSize());
    if (maxFree <= SLACK_BYTES) {
        RS_LOGE("arena: only %u bytes free, cannot reserve", unsigned(maxFree));
        return false;
    }
    s_size  = maxFree - SLACK_BYTES;
    s_block = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER,
                                            "rs_arena", PSP_SMEM_Low,
                                            SceSize(s_size), nullptr);
    if (s_block < 0) {
        RS_LOGE("arena: AllocPartitionMemory failed (%08x)", unsigned(s_block));
        return false;
    }
    s_base   = static_cast<u8*>(sceKernelGetBlockHeadAddr(s_block));
    s_cursor = 0;
    s_high   = 0;
    s_failures = 0;
    RS_LOGI("arena: reserved %u KB at %p", unsigned(s_size / 1024),
            (void*)s_base);
    return true;
}

void shutdown() {
    if (s_block >= 0) sceKernelFreePartitionMemory(s_block);
    s_block = -1;
    s_base  = nullptr;
    s_size = s_cursor = s_high = 0;
}

void* alloc(u32 size, u32 align) {
    if (align == 0) align = 16;
    if (!bounds::powerOfTwo(align) || align > 4096) {
        s_failures++;
        RS_LOGW("arena: invalid alignment %u", unsigned(align));
        return nullptr;
    }
    if (!s_base) {
        s_failures++;
        RS_LOGW("arena: allocation attempted while unavailable");
        return nullptr;
    }
    if (s_cursor > 0xFFFFFFFFu - (align - 1)) {
        s_failures++;
        return nullptr;
    }
    const u32 start = (s_cursor + align - 1) & ~(align - 1);
    /* Overflow-safe: `start + size` could wrap for a huge (e.g. corrupt
     * zip-declared) size and pass a naive `start + size > s_size` check. */
    if (start > s_size || size > s_size - start) {
        s_failures++;
        RS_LOGW("arena: out of memory (want %u, have %u)", unsigned(size),
                unsigned(s_size - s_cursor));
        return nullptr;
    }
    s_cursor = start + size;
    if (s_cursor > s_high) s_high = s_cursor;
    return s_base + start;
}

Marker marker() { return s_cursor; }

void reset(Marker m) {
    if (m <= s_cursor) s_cursor = m;
}

u32 available() { return s_size - s_cursor; }
u32 used() { return s_cursor; }
u32 totalSize() { return s_size; }
u32 highWater() { return s_high; }
u32 allocationFailures() { return s_failures; }

}  // namespace rs::mem

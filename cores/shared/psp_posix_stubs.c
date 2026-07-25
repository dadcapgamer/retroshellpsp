/* POSIX shims for libretro-common on bare PSP newlib.
 *
 * Some libretro-common file backends reference POSIX calls the PSP libc
 * doesn't provide. RetroSuite hands cores raw ROM data and persists saves
 * through the host API, so these file paths are never exercised at
 * runtime — the stubs exist only to satisfy the linker. Add this file to a
 * core's sources when its vfs backend pulls one of these in. */
#include <sys/types.h>

int ftruncate(int fd, off_t length) {
    (void)fd;
    (void)length;
    return 0;
}

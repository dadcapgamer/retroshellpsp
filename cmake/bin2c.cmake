# Script mode: cmake -DIN=<file> -DOUT_C=<file.c> -DOUT_H=<file.h> -DSYM=<symbol> -P bin2c.cmake
# Converts a binary file into a C array so assets can be embedded in the EBOOT.

file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" hexlen)
math(EXPR n "${hexlen} / 2")

# Insert a comma after every byte and a newline every 32 bytes for sane files.
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "(([^,]+,){32})" "\\1\n" bytes "${bytes}")

get_filename_component(hdr "${OUT_H}" NAME)

file(WRITE "${OUT_H}"
"/* Auto-generated from ${IN} — do not edit. */
#pragma once
#ifdef __cplusplus
extern \"C\" {
#endif
extern const unsigned char ${SYM}[];
extern const unsigned int ${SYM}_len;
#ifdef __cplusplus
}
#endif
")

file(WRITE "${OUT_C}"
"/* Auto-generated from ${IN} — do not edit. */
#include \"${hdr}\"
const unsigned char ${SYM}[] __attribute__((aligned(16))) = {
${bytes}
};
const unsigned int ${SYM}_len = ${n}u;
")

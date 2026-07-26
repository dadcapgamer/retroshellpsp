#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int main(void) {
    const size_t maximum = 4u * 1024u * 1024u;
    uint8_t* data = (uint8_t*)malloc(maximum);
    if (!data) return 2;
    const size_t size = fread(data, 1, maximum, stdin);
    const int result = LLVMFuzzerTestOneInput(data, size);
    free(data);
    return result;
}

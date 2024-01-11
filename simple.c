#include <stdint.h>
#include <stdio.h>

unsigned char __afl_map_ptr[1 << 16];

int factorial(int i) {
    if (i == 0) {
        return 1;
    }

    return i * factorial(i - 1);
}

int main() {
    printf("%i\n", factorial(5));
    for (int i = 0; i < (1 << 16); i++) {
        if (__afl_map_ptr[i] == 0) {
            continue;
        }

        printf("afl_map[%i]: %u\n", i, __afl_map_ptr[i]);
    }
    return 0;
}
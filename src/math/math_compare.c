#include "math_backend.h"

uint64_t math_ucomisd(const vec128 *a, const vec128 *b) {
    uint64_t temp_flags = 0;
    double val1 = a->f64[0];
    double val2 = b->f64[0];
    __asm__ volatile (
        "ucomisd %1, %2\n\t"
        "pushfq\n\t"
        "popq %0\n\t"
        : "=r"(temp_flags)
        : "x"(val2), "x"(val1)
        : "cc"
    );
    return temp_flags;
}

uint64_t math_ucomiss(const vec128 *a, const vec128 *b) {
    uint64_t temp_flags = 0;
    float val1 = a->f32[0];
    float val2 = b->f32[0];
    __asm__ volatile (
        "ucomiss %1, %2\n\t"
        "pushfq\n\t"
        "popq %0\n\t"
        : "=r"(temp_flags)
        : "x"(val2), "x"(val1)
        : "cc"
    );
    return temp_flags;
}

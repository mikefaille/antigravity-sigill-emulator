#include <stdio.h>
#include <time.h>
#include <stdint.h>

int main() {
    uint32_t iterations = 1000000;
    
    printf("[*] Starting benchmark: running %u iterations of 'aesenc xmm0, xmm0'...\n", iterations);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (uint32_t i = 0; i < iterations; i++) {
        __asm__ __volatile__ (
            "aesenc %%xmm0, %%xmm0"
            :
            :
            : "xmm0"
        );
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double traps_per_sec = iterations / elapsed_sec;
    double avg_ns = (elapsed_sec / iterations) * 1e9;
    
    printf("[*] Benchmark complete:\n");
    printf("    Elapsed time:       %.6f seconds\n", elapsed_sec);
    printf("    Traps per second:   %.2f\n", traps_per_sec);
    printf("    Avg latency/trap:   %.2f ns (%.3f us)\n", avg_ns, avg_ns / 1000.0);
    
    return 0;
}

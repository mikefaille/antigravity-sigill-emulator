#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <setjmp.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>

static volatile sig_atomic_t custom_handler_called = 0;
static sigjmp_buf jmp_env;

static void custom_sigill_handler(int sig, siginfo_t *si, void *ctx) {
    (void)si;
    (void)ctx;
    if (sig == SIGILL) {
        custom_handler_called++;
    }
}

static void custom_sigtrap_handler(int sig, siginfo_t *si, void *ctx) {
    (void)si;
    (void)ctx;
    if (sig == SIGTRAP) {
        custom_handler_called++;
    }
}

static void ordinary_sigill_handler(int sig) {
    if (sig == SIGILL) {
        custom_handler_called++;
    }
}

static void siglongjmp_sigill_handler(int sig, siginfo_t *si, void *ctx) {
    (void)si;
    (void)ctx;
    if (sig == SIGILL) {
        custom_handler_called++;
        siglongjmp(jmp_env, custom_handler_called);
    }
}

// Sub-test 1: SIG_DFL
static void run_sig_dfl(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    
    raise(SIGILL);
    _exit(0); // Should not reach (will be terminated by signal)
}

// Sub-test 2: SIG_IGN
static void run_sig_ign(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    
    raise(SIGILL);
    _exit(42); // Should reach here safely because SIGILL is ignored
}

// Sub-test 3: Ordinary handler
static void run_ordinary_handler_test(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ordinary_sigill_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    
    raise(SIGILL);
    if (custom_handler_called == 1) {
        _exit(43);
    }
    _exit(132);
}

// Sub-test 4: SA_SIGINFO handler
static void run_siginfo_handler_test(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = custom_sigill_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    
    raise(SIGILL);
    if (custom_handler_called == 1) {
        _exit(44);
    }
    _exit(132);
}

// Sub-test 5: siglongjmp safety
static void run_siglongjmp_test(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = siglongjmp_sigill_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    
    int val = sigsetjmp(jmp_env, 1);
    if (val == 0) {
        raise(SIGILL);
    } else if (val == 1) {
        // First jump succeeded, clear guard and try a second time to prove no guard poisoning
        raise(SIGILL);
    } else if (val == 2) {
        // Second jump succeeded, recursion guard is perfectly functional and unpoisoned
        _exit(45);
    }
    _exit(132);
}

static void unsupported_vex_sigill_handler(int sig, siginfo_t *si, void *ctx) {
    (void)si;
    (void)ctx;
    if (sig == SIGILL) {
        _exit(46);
    }
}

// Sub-test 6: Unsupported VEX chaining
static void run_unsupported_vex_test(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = unsupported_vex_sigill_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    
    // Execute an invalid VEX prefix pattern to trigger SIGILL
    __asm__ __volatile__ (
        ".byte 0xc5, 0xf0, 0x01, 0xc0\n" // Invalid VEX instruction op=0x01
    );
    
    _exit(132);
}

// Sub-test 7: Handled VEX not chaining
static void run_supported_vex_test(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = custom_sigill_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    
    // Execute a supported VEX instruction (e.g. vxorps)
    __asm__ __volatile__ (
        "vxorps %%xmm0, %%xmm0, %%xmm0"
        :
        :
        : "xmm0"
    );
    
    if (custom_handler_called == 0) {
        _exit(47);
    }
    _exit(132);
}

// Sub-test 8: SIGTRAP chaining
static void run_sigtrap_chaining_test(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = custom_sigtrap_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTRAP, &sa, NULL);
    
    raise(SIGTRAP);
    if (custom_handler_called == 1) {
        _exit(48);
    }
    _exit(132);
}

// Sub-test 9: Concurrent replacement of virtualized handler
static volatile int replacement_stop = 0;

static void *replacement_thread(void *arg) {
    (void)arg;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ordinary_sigill_handler;
    sigemptyset(&sa.sa_mask);
    
    while (!replacement_stop) {
        sigaction(SIGILL, &sa, NULL);
        sched_yield();
    }
    return NULL;
}

static void run_concurrent_replacement_test(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, replacement_thread, NULL);
    pthread_create(&t2, NULL, replacement_thread, NULL);
    
    for (int i = 0; i < 10000; i++) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = custom_sigill_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGILL, &sa, NULL);
        
        raise(SIGILL);
    }
    
    replacement_stop = 1;
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    _exit(49);
}

static int run_forked_test(void (*test_func)(void), int expected_status, const char *name) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        test_func();
        _exit(0);
    }
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (expected_status == -sig) {
            printf("[+] PASSED: %s (terminated by expected signal %d)\n", name, sig);
            return 0;
        } else {
            printf("[-] FAILED: %s (terminated by unexpected signal %d, expected status/signal %d)\n", name, sig, expected_status);
            return 1;
        }
    } else if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == expected_status) {
            printf("[+] PASSED: %s (exited with expected code %d)\n", name, code);
            return 0;
        } else {
            printf("[-] FAILED: %s (exited with unexpected code %d, expected %d)\n", name, code, expected_status);
            return 1;
        }
    }
    return 1;
}

static void run_simd_math_test(void) {
    // Double precision inputs
    double d1[4] __attribute__((aligned(32))) = { 1.5, -2.5, 3.5, -4.5 };
    double d2[4] __attribute__((aligned(32))) = { -1.5, 2.5, -3.5, 4.5 };
    double d_res[4] __attribute__((aligned(32))) = { 0 };

    // Float inputs
    float f1[8] __attribute__((aligned(32))) = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
    float f2[8] __attribute__((aligned(32))) = { 0.5f, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f };
    float f_res[8] __attribute__((aligned(32))) = { 0 };

    // --- Logical family ---
    // vxorpd (256-bit)
    __asm__ __volatile__ (
        "vmovupd %1, %%ymm0\n"
        "vmovupd %2, %%ymm1\n"
        "vxorpd %%ymm1, %%ymm0, %%ymm2\n"
        "vmovupd %%ymm2, %0\n"
        "vzeroupper\n"
        : "=m"(d_res)
        : "m"(d1), "m"(d2)
        : "ymm0", "ymm1", "ymm2"
    );
    printf("[+] Math subtest: vxorpd 256-bit completed\n");

    // vandpd (128-bit)
    __asm__ __volatile__ (
        "vmovupd %1, %%xmm0\n"
        "vmovupd %2, %%xmm1\n"
        "vandpd %%xmm1, %%xmm0, %%xmm2\n"
        "vmovupd %%xmm2, %0\n"
        : "=m"(d_res)
        : "m"(d1), "m"(d2)
        : "xmm0", "xmm1", "xmm2"
    );
    printf("[+] Math subtest: vandpd 128-bit completed\n");

    // --- Shuffles ---
    // vmovshdup (256-bit)
    __asm__ __volatile__ (
        "vmovups %1, %%ymm0\n"
        "vmovshdup %%ymm0, %%ymm1\n"
        "vmovups %%ymm1, %0\n"
        "vzeroupper\n"
        : "=m"(f_res)
        : "m"(f1)
        : "ymm0", "ymm1"
    );
    if (f_res[0] != 2.0f || f_res[1] != 2.0f || f_res[2] != 4.0f || f_res[3] != 4.0f) {
        _exit(101);
    }
    printf("[+] Math subtest: vmovshdup completed\n");

    // vmovsldup (256-bit)
    __asm__ __volatile__ (
        "vmovups %1, %%ymm0\n"
        "vmovsldup %%ymm0, %%ymm1\n"
        "vmovups %%ymm1, %0\n"
        "vzeroupper\n"
        : "=m"(f_res)
        : "m"(f1)
        : "ymm0", "ymm1"
    );
    if (f_res[0] != 1.0f || f_res[1] != 1.0f || f_res[2] != 3.0f || f_res[3] != 3.0f) {
        _exit(102);
    }
    printf("[+] Math subtest: vmovsldup completed\n");

    // --- Unpacks ---
    // vunpcklps (256-bit)
    __asm__ __volatile__ (
        "vmovups %1, %%ymm0\n"
        "vmovups %2, %%ymm1\n"
        "vunpcklps %%ymm1, %%ymm0, %%ymm2\n"
        "vmovups %%ymm2, %0\n"
        "vzeroupper\n"
        : "=m"(f_res)
        : "m"(f1), "m"(f2)
        : "ymm0", "ymm1", "ymm2"
    );
    if (f_res[0] != 1.0f || f_res[1] != 0.5f || f_res[2] != 2.0f || f_res[3] != 1.5f) {
        _exit(103);
    }
    printf("[+] Math subtest: vunpcklps completed\n");

    // --- Add/Sub ---
    // vaddsubps (256-bit)
    __asm__ __volatile__ (
        "vmovups %1, %%ymm0\n"
        "vmovups %2, %%ymm1\n"
        "vaddsubps %%ymm1, %%ymm0, %%ymm2\n"
        "vmovups %%ymm2, %0\n"
        "vzeroupper\n"
        : "=m"(f_res)
        : "m"(f1), "m"(f2)
        : "ymm0", "ymm1", "ymm2"
    );
    if (f_res[0] != 0.5f || f_res[1] != 3.5f || f_res[2] != 0.5f || f_res[3] != 7.5f) {
        _exit(104);
    }
    printf("[+] Math subtest: vaddsubps completed\n");

    // --- Comparisons ---
    // vcmpps (256-bit, EQ_OQ predicate = 0)
    __asm__ __volatile__ (
        "vmovups %1, %%ymm0\n"
        "vmovups %2, %%ymm1\n"
        "vcmpps $0, %%ymm1, %%ymm0, %%ymm2\n"
        "vmovups %%ymm2, %0\n"
        "vzeroupper\n"
        : "=m"(f_res)
        : "m"(f1), "m"(f1)
        : "ymm0", "ymm1", "ymm2"
    );
    uint32_t *f_res_u32 = (uint32_t *)f_res;
    if (f_res_u32[0] != 0xFFFFFFFF || f_res_u32[1] != 0xFFFFFFFF) {
        _exit(105);
    }
    printf("[+] Math subtest: vcmpps EQ completed\n");

    _exit(55);
}

int main(void) {
    printf("=========================================================\n");
    printf("             AVX EMULATOR SIGNAL FALLBACK TESTS         \n");
    printf("=========================================================\n");
    
    int failures = 0;
    
    failures += run_forked_test(run_sig_dfl, -SIGILL, "SIG_DFL Handler");
    failures += run_forked_test(run_sig_ign, 42, "SIG_IGN Handler");
    failures += run_forked_test(run_ordinary_handler_test, 43, "Ordinary One-Argument Handler");
    failures += run_forked_test(run_siginfo_handler_test, 44, "SA_SIGINFO Three-Argument Handler");
    failures += run_forked_test(run_siglongjmp_test, 45, "siglongjmp Safety and Unpoisoned guard");
    failures += run_forked_test(run_unsupported_vex_test, 46, "Unsupported VEX Fallback Chaining");
    failures += run_forked_test(run_supported_vex_test, 47, "Handled VEX Skipping Chaining");
    failures += run_forked_test(run_sigtrap_chaining_test, 48, "SIGTRAP Chaining Fallback");
    failures += run_forked_test(run_concurrent_replacement_test, 49, "Concurrent virtualization replacement");
    failures += run_forked_test(run_simd_math_test, 55, "SIMDe Vector Math and Comparisons");
    
    printf("---------------------------------------------------------\n");
    if (failures == 0) {
        printf("[+] ALL SIGNAL FALLBACK TESTS PASSED SUCCESSFULLY!\n");
        return 0;
    } else {
        printf("[-] TEST SUITE FAILED WITH %d FAILURES.\n", failures);
        return 1;
    }
}

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>

struct config {
    const char *name;
    const char *lib_name;
    const char *mode;
};

int file_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

const char *resolve_library(const char *lib_name, char *resolved_path, size_t max_len) {
    // Check current directory first
    snprintf(resolved_path, max_len, "./%s", lib_name);
    if (file_exists(resolved_path)) {
        return resolved_path;
    }
    // Check HOME environment variable next
    const char *home = getenv("HOME");
    if (home != NULL) {
        snprintf(resolved_path, max_len, "%s/%s", home, lib_name);
        if (file_exists(resolved_path)) {
            return resolved_path;
        }
    }
    return NULL;
}

int run_config(const struct config *cfg, double *out_elapsed, double *out_traps, double *out_latency, char *out_error, size_t err_len) {
    char lib_path[512];
    if (cfg->lib_name != NULL) {
        if (!resolve_library(cfg->lib_name, lib_path, sizeof(lib_path))) {
            snprintf(out_error, err_len, "Library %s not found (compile first)", cfg->lib_name);
            return -1;
        }
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        snprintf(out_error, err_len, "Failed to create pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        snprintf(out_error, err_len, "Failed to fork");
        return -1;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end

        // Redirect stdout to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        
        // Redirect stderr to /dev/null to keep screen clean from debugging/construction output
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null != -1) {
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }
        close(pipefd[1]);

        // Configure environment
        if (cfg->lib_name != NULL) {
            setenv("LD_PRELOAD", lib_path, 1);
            setenv("EMU_MODE", cfg->mode, 1);
        } else {
            unsetenv("LD_PRELOAD");
        }
        setenv("BENCHMARK_CHILD", "1", 1);

        char *argv[] = { "/proc/self/exe", NULL };
        execv(argv[0], argv);

        perror("execv failed");
        _exit(127);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    char buffer[2048];
    size_t total_read = 0;
    ssize_t n;
    while ((n = read(pipefd[0], buffer + total_read, sizeof(buffer) - total_read - 1)) > 0) {
        total_read += n;
    }
    buffer[total_read] = '\0';
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGILL && cfg->lib_name == NULL) {
            snprintf(out_error, err_len, "Crashed (SIGILL as expected)");
        } else {
            snprintf(out_error, err_len, "Crashed (Signal %d: %s)", sig, strsignal(sig));
        }
        return -1;
    }

    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
        if (exit_code == 132 && cfg->lib_name == NULL) {
            snprintf(out_error, err_len, "Crashed (SIGILL as expected)");
        } else {
            snprintf(out_error, err_len, "Exited with code %d", exit_code);
        }
        return -1;
    }

    char *result_line = strstr(buffer, "BENCHMARK_RESULT:");
    if (!result_line) {
        snprintf(out_error, err_len, "No benchmark output parsed");
        return -1;
    }

    if (sscanf(result_line, "BENCHMARK_RESULT: elapsed_sec=%lf, traps_per_sec=%lf, avg_ns=%lf",
               out_elapsed, out_traps, out_latency) != 3) {
        snprintf(out_error, err_len, "Failed to parse benchmark metrics");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    char *child_env = getenv("BENCHMARK_CHILD");
    if (child_env && strcmp(child_env, "1") == 0) {
        // Child Mode: execute the hot loop of unsupported instructions
        uint32_t iterations = 1000000;
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

        printf("BENCHMARK_RESULT: elapsed_sec=%.6f, traps_per_sec=%.2f, avg_ns=%.2f\n",
               elapsed_sec, traps_per_sec, avg_ns);
        return 0;
    }

    // Parent Mode: orchestrate the comparison
    struct config configs[] = {
        {"Baseline (No Emulator)",             NULL,                    "none"},
        {"Emulator (Nehalem)   - Safe",         "sigill_emulator.so",    "safe"},
        {"Emulator (Nehalem)   - Experimental", "sigill_emulator.so",    "experimental"}
    };
    int num_configs = sizeof(configs) / sizeof(configs[0]);

    printf("========================================================================================\n");
    printf("                  SIGILL EMULATOR MULTI-TARGET COMPARISON BENCHMARK                     \n");
    printf("========================================================================================\n");
    printf(" Running 1,000,000 iterations of 'aesenc xmm0, xmm0' per configuration...\n\n");

    printf("----------------------------------------------------------------------------------------\n");
    printf(" %-35s | %-15s | %-16s | %-16s\n", "Configuration Target", "Elapsed Time", "Traps / Sec", "Avg Latency");
    printf("----------------------------------------------------------------------------------------\n");

    for (int i = 0; i < num_configs; i++) {
        double elapsed = 0.0, traps = 0.0, latency = 0.0;
        char err_msg[256];
        memset(err_msg, 0, sizeof(err_msg));

        printf(" Running: %-30s... ", configs[i].name);
        fflush(stdout);

        int rc = run_config(&configs[i], &elapsed, &traps, &latency, err_msg, sizeof(err_msg));

        // Erase the "Running..." status line
        printf("\r");
        
        if (rc == 0) {
            printf(" %-35s | %12.6f s | %14.2f | %12.2f ns\n",
                   configs[i].name, elapsed, traps, latency);
        } else {
            printf(" %-35s | %-49s\n", configs[i].name, err_msg);
        }
    }
    printf("----------------------------------------------------------------------------------------\n");
    printf(" Note: Option B (Experimental) dynamically rewrites invalid instructions to bypass the\n");
    printf(" kernel context-switch entirely, while Option A uses lock-free metadata caching.\n");
    printf("========================================================================================\n");

    return 0;
}

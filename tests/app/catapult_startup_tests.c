#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_pass;
static int g_fail;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        g_fail++; \
        return; \
    } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static FILE *start_probe(pid_t *child, const char *video_driver) {
    int log_pipe[2];
    if (pipe(log_pipe) != 0) return NULL;
    *child = fork();
    if (*child == 0) {
        close(log_pipe[0]);
        if (dup2(log_pipe[1], STDERR_FILENO) < 0) _exit(126);
        close(log_pipe[1]);
        if (video_driver && setenv("SDL_VIDEODRIVER", video_driver, 1) != 0)
            _exit(126);
        execl("./build/catapult_demo", "catapult_demo", "--startup-probe", (char *)NULL);
        _exit(127);
    }
    close(log_pipe[1]);
    if (*child >= 0) {
        FILE *log = fdopen(log_pipe[0], "r");
        if (log) return log;
        kill(*child, SIGTERM);
        waitpid(*child, NULL, 0);
    }
    close(log_pipe[0]);
    return NULL;
}

static void test_startup_probe_reports_launch_before_graphics_failure(void) {
    pid_t child;
    FILE *log = start_probe(&child, "dummy");
    ASSERT_TRUE(log);
    char line[512];
    int started = 0, presented = 0;
    while (fgets(line, sizeof(line), log)) {
        fputs(line, stdout);
        if (strcmp(line, "Catapult startup probe begins\n") == 0) started++;
        if (strstr(line, "Catapult startup first present:")) presented++;
    }
    fclose(log);
    int status;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_INT_EQ(WEXITSTATUS(status), 1);
    ASSERT_INT_EQ(started, 1);
    ASSERT_INT_EQ(presented, 0);
    PASS();
}

static void test_startup_probe_profiles_through_first_present(void) {
    static const char *const phases[] = {
        "GPU setup", "ground upload", "thermal setup", "structural placement",
        "structural settle", "launcher base", "wood arm", "coal load",
        "revolute joints", "gas setup", "combustion setup", "renderer setup",
        "initial state", "first present", "total"
    };
    const size_t count = sizeof(phases) / sizeof(phases[0]);
    pid_t child;
    FILE *log = start_probe(&child, NULL);
    ASSERT_TRUE(log);

    size_t phase = 0;
    double sum_ms = 0., total_ms = 0.;
    int ready = 0, presented = 0, trace_valid = 1;
    char line[512];
    while (fgets(line, sizeof(line), log)) {
        fputs(line, stdout);
        char name[80], units[8];
        double ms;
        if (sscanf(line, "Catapult startup %79[^:]: %lf %7s", name, &ms, units) == 3) {
            if (phase >= count || strcmp(name, phases[phase]) != 0 ||
                strcmp(units, "ms") != 0 || !isfinite(ms) || ms < 0.) {
                trace_valid = 0;
                continue;
            }
            ++phase;
            if (phase < count) sum_ms += ms;
            else total_ms = ms;
        }
        if (strstr(line, "256x128 game view ready")) ready++;
        unsigned frame, frames;
        if (sscanf(line, "Catapult demo: frame %u/%u", &frame, &frames) == 2) {
            presented++;
            if (frame != 1u || frames != 1u || phase != count) {
                kill(child, SIGTERM);
                break;
            }
        }
    }
    fclose(log);
    int status;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    if (WIFSIGNALED(status))
        printf("Catapult process ended with signal %d after %zu phases\n",
               WTERMSIG(status), phase);
    ASSERT_EQ(phase, count);
    ASSERT_TRUE(trace_valid && WIFEXITED(status));
    ASSERT_INT_EQ(WEXITSTATUS(status), 0);
    ASSERT_INT_EQ(ready, 1);
    ASSERT_INT_EQ(presented, 1);
    ASSERT_TRUE(total_ms > 0. && fabs(total_ms - sum_ms) < .001 * count);
    PASS();
}

int main(void) {
    RUN(test_startup_probe_reports_launch_before_graphics_failure);
    if (!getenv("DC_CATAPULT_STARTUP_TEST_HOST_ONLY")) {
        RUN(test_startup_probe_profiles_through_first_present);
    }
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}

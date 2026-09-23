#include "retr01_emu/machine.h"

#include <stdio.h>

int main(int argc, char **argv) {
    const char *path;
    R01eMachine m;
    char err[256];
    int i;
    int f;

    if (argc < 2 || !argv[1] || !argv[1][0]) {
        fprintf(stderr, "skip: provide cart path argv\n");
        return 77;
    }
    path = argv[1];
    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }
    if (r01e_play_start(&m) != 1) {
        fprintf(stderr, "FAIL play start\n");
        r01e_machine_shutdown(&m);
        return 1;
    }

    /* Boot MAP stream / first publishes settle before the busy check. */
    for (i = 0; i < 8; i++) {
        (void)r01e_machine_frame(&m);
    }
    for (f = 0; f < 20; f++) {
        uint64_t busy;
        (void)r01e_machine_frame(&m);
        busy = m.prof_last_active + m.prof_last_vblank;
        if (busy > R01E_CYCLES_PER_FRAME) {
            fprintf(stderr,
                    "FAIL example_01 busy %llu exceeds CRT frame %llu (active=%llu vblank=%llu idle=%llu)\n",
                    (unsigned long long)busy, (unsigned long long)R01E_CYCLES_PER_FRAME,
                    (unsigned long long)m.prof_last_active, (unsigned long long)m.prof_last_vblank,
                    (unsigned long long)m.prof_last_idle);
            r01e_machine_shutdown(&m);
            return 1;
        }
    }

    printf("ok frame-time busy<=%llu\n", (unsigned long long)R01E_CYCLES_PER_FRAME);
    r01e_machine_shutdown(&m);
    return 0;
}

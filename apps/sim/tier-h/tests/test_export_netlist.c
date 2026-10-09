#include "retr01_sim/board.h"
#include "retr01_sim/board_netlist.h"
#include "retr01_sim/island_builder.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

static int read_tmp_json(R01sPinNetlist *nl, char *buf, size_t cap) {
    FILE *f;
    size_t n;

    if (!nl || !buf || cap < 2) {
        return -1;
    }
    f = tmpfile();
    if (!f) {
        return -1;
    }
    if (r01s_pin_netlist_write_json(nl, f) != 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return (int)n;
}

int main(void) {
    R01sBoard board;
    R01sIslandBuilder builder;
    char json[98304];
    int n;

    memset(&board, 0, sizeof(board));
    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    n = read_tmp_json(&board.pin_netlist, json, sizeof(json));
    expect_true(n > 256, "json size");
    expect_true(strstr(json, "preliminary_pcb_illustrative_only") != NULL, "disclaimer meta");
    expect_true(strstr(json, "\"fabrication_ready\": false") != NULL, "fabrication flag");
    expect_true(strstr(json, "\"U04\"") != NULL, "U04 in export");
    expect_true(strstr(json, "\"Y1\"") != NULL, "Y1 crystal in export");
    expect_true(strstr(json, "\"U725\"") != NULL, "AD724 in export");
    expect_true(strstr(json, "COMPOSITE_OUT") != NULL, "composite net name");
    expect_true(strstr(json, "AUDIO_OUT") != NULL, "audio net name");
    expect_true(strstr(json, "HDR-2x4") != NULL, "J2 2x4 header in export");
    expect_true(strstr(json, "\"J10\"") != NULL, "J10 cart program header in export");
    expect_true(strstr(json, "SW-DIP-2") != NULL, "SW10 DIP in export");
    expect_true(strstr(json, "CART_PROG_DATA") != NULL, "cart program data net");
    expect_true(strstr(json, "CART_ARM") != NULL, "CART_ARM net");
    expect_true(strstr(json, "\"+5V\"") != NULL, "named +5V rail");
    r01s_island_builder_shutdown(&builder);
    return test_done("test_export_netlist");
}

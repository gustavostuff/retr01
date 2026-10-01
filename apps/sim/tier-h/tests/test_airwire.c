#include "retr01_sim/airwire.h"
#include "retr01_sim/entity.h"
#include "test_common.h"

#include <string.h>

static void test_mst_square(void) {
    R01sAirPt pts[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    R01sAirSeg seg[3];
    int n;
    int i;
    n = r01s_air_mst(pts, 4, seg, 3);
    expect_true(n == 3, "square mst has n-1 segments");
    for (i = 0; i < n; i++) {
        int dx = pts[seg[i].a].x - pts[seg[i].b].x;
        int dy = pts[seg[i].a].y - pts[seg[i].b].y;
        expect_true(dx * dx + dy * dy <= 100, "square mst skips the diagonal");
    }
}

static void test_dist_mm(void) {
    expect_true(NS_PX_PER_MM == 2, "canvas scale");
    expect_true(r01s_air_dist_mm(0, 0, 10, 0) == 5, "10 px is 5 mm");
    expect_true(!r01s_air_too_long(5, 5), "limit is exclusive");
    expect_true(r01s_air_too_long(6, 5), "6 mm blinks a 5 mm cap");
    expect_true(!r01s_air_too_long(80, 0), "limit 0 does not blink");
}

static void test_style(void) {
    R01sAirStyle st;
    R01sAirEnd color[2];
    R01sAirEnd cap;
    R01sAirEnd vdd;
    color[0].refdes = "UPLDV";
    color[0].pin = "A3";
    color[1].refdes = "U24";
    color[1].pin = "A3";
    r01s_air_style("IDX", color, 2, &st);
    expect_true(st.layer == R01S_AIR_LAYER_NOISY, "color index is layer 1");
    expect_true(st.limit_mm == 25, "color index 25 mm");

    r01s_air_style("GND", NULL, 0, &st);
    expect_true(st.layer == R01S_AIR_LAYER_GND, "GND layer");
    expect_true(r01s_air_hop_limit_mm(&st, NULL, NULL) == 0, "GND does not length-blink");

    r01s_air_style("+5V", NULL, 0, &st);
    expect_true(st.pwr, "+5V flag");
    cap.refdes = "C1";
    cap.pin = "1";
    vdd.refdes = "U1";
    vdd.pin = "VDD";
    expect_true(r01s_air_hop_limit_mm(&st, &cap, &vdd) == 5, "bypass hop is 5 mm");
    cap.pin = "2";
    expect_true(r01s_air_hop_limit_mm(&st, &cap, &vdd) == 0, "cap ground side is not a bypass hop");

    r01s_air_style("XTAL_CPU_IN", NULL, 0, &st);
    expect_true(st.layer == R01S_AIR_LAYER_QUIET && st.limit_mm == 20, "pierce loop is quiet 20 mm");
}

static void test_zones(void) {
    expect_true(r01s_air_zone_for_ref("U3") == R01S_ZONE_Z1, "U3 is zone 1");
    expect_true(r01s_air_zone_for_ref("C1") == R01S_ZONE_CPU, "C1 stays with the CPU");
    expect_true(r01s_air_zone_for_ref("C10") == R01S_ZONE_Z2, "C10 is not C1");
    expect_true(r01s_air_zone_for_ref("U40") == -1, "cart flash stays off the outline");
    expect_true(r01s_air_zone_for_ref("J1") == R01S_ZONE_J8, "J1 on the rear strip");
    expect_true(r01s_air_zone_for_ref("J9") == R01S_ZONE_J8, "J9 on the rear strip");
    expect_true(r01s_air_zone_for_ref("J2") == R01S_ZONE_Z2, "J2 sits in zone 2");
    expect_true(strcmp(r01s_air_zone_name(R01S_ZONE_J8), "Rear") == 0, "rear strip name");
    expect_true(r01s_air_outside_px(0, 0, 100, 40, 10, 10, 20, 10) == 0, "body inside");
    expect_true(r01s_air_outside_px(0, 0, 100, 40, -8, 0, 20, 10) == 8, "body 8 px past the left edge");
    expect_true(strcmp(r01s_air_zone_name(R01S_ZONE_Z4), "Zone 4") == 0, "zone 4 name");
}

int main(void) {
    test_mst_square();
    test_dist_mm();
    test_style();
    test_zones();
    return test_done("test_airwire");
}

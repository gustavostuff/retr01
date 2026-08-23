#include "retr01_sim/gamepad.h"
#include "test_common.h"

int main(void) {
    expect_true(r01s_gamepad_stick_bits(0, 0) == 0, "neutral");
    expect_true(r01s_gamepad_stick_bits(0, -20) == R01S_PAD_UP, "up");
    expect_true(r01s_gamepad_stick_bits(20, 0) == R01S_PAD_RIGHT, "right");
    expect_true(r01s_gamepad_stick_bits(-20, -20) == (R01S_PAD_UP | R01S_PAD_LEFT), "up-left");
    expect_true(r01s_gamepad_stick_bits(20, 20) == (R01S_PAD_DOWN | R01S_PAD_RIGHT), "down-right");
    expect_true(r01s_gamepad_stick_bits(3, 3) == 0, "inside deadzone");

    {
        R01sGamepadInput gp;
        r01s_gamepad_input_clear(&gp);
        gp.stick_x = 20;
        gp.stick_y = -20;
        gp.btn_x = 1;
        gp.btn_start = 1;
        expect_true(r01s_gamepad_encode(&gp) == (R01S_PAD_UP | R01S_PAD_RIGHT | R01S_PAD_X | R01S_PAD_START),
                    "encode");
    }

    return test_done("test_gamepad");
}

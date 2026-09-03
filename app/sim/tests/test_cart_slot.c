#include "retr01_sim/cart_module.h"
#include "retr01_sim/cart_slot.h"
#include "test_common.h"

int main(void) {
    R01sCartModule mod;
    R01sCartSlotMgr mgr;

    r01s_cart_module_init(&mod);
    r01s_cart_slot_reset(&mgr);

    expect_true(r01s_cart_slot_insert(&mgr, &mod, R01S_CART_SLOT_FLASHER) == 0, "flasher insert");
    expect_true(r01s_cart_slot_insert(&mgr, &mod, R01S_CART_SLOT_MOBO) != 0, "dual insert blocked");
    expect_true(r01s_cart_slot_remove(&mgr, R01S_CART_SLOT_FLASHER) == 0, "flasher remove");
    expect_true(r01s_cart_slot_insert(&mgr, &mod, R01S_CART_SLOT_MOBO) == 0, "mobo insert");
    expect_true(r01s_cart_slot_check_read(&mgr, R01S_CART_SLOT_MOBO, "test") == 0, "read ok");
    expect_true(r01s_cart_slot_remove(&mgr, R01S_CART_SLOT_MOBO) == 0, "mobo remove");
    expect_true(r01s_cart_slot_check_read(&mgr, R01S_CART_SLOT_MOBO, "empty") != 0, "empty read warn");

    return test_done("test_cart_slot");
}

#include <stdio.h>

#include "TXTUI.h"
#include "keycodes.h"

char hi[] = "press escape";

int main(int argc, char** argv) {
    char keystates[32];
    memset(keystates, 0, 32);

    init_keyboard();

    clear_screen(0x03);
    draw_box(10,10, 18, 3, 0x03);
    draw_box(30,20, 18, 4, 0x04);
    draw_box(5,1, 18, 5, 0x05);
    while(!test_keybuf(keystates, KEY_ESC)) {
        clear_keybuf(keystates);
        get_keys_hit(keystates);
    }
    deinit_keyboard();
    return 0;
}


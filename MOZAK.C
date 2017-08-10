#include <stdio.h>

#include "TXTUI.h"
#include "keycodes.h"

#define BOTTOM_NOTE 21

#define NOTE_CHAR 13

char hi[] = "press escape";

int meter_num = 4;
int meter_dnm = 4;
int tempo = 120;

int file_dirty = 0;

int steps_per_beat = 1;
int scroll_beat = 0;
int cursor_beat = 0;
int cursor_channel = 1;
int cursor_note = 57;

int black_notes[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

char meter_str[8];
char tempo_str[8];
char cursor_str[10];

typedef struct note {
    unsigned int pitch;
    unsigned int step;
    unsigned char channel;
    unsigned int duration;
    unsigned char volume;
} note;

note song_buf[512];
int song_buf_size = 512;

/* notenum halfsteps from C0 */
int is_black_note(int notenum) {
    return black_notes[notenum % 12];
}

void render_piano_roll() {
    /*starting at line 2, draw dotted lines every (meter_num * steps_per_beat) rows*/
    int x, y, i;
    paint_box(0, 2, 80, 23, 0x07);
    for(y = 2 + (scroll_beat % (meter_num * steps_per_beat)); y < 25; y+= meter_num * steps_per_beat) {
        for(x = 0; x < 80; x++) {
            charat(x, y) = 0xFA;
        }
    }

    for(i = 0; i < song_buf_size; i++) {
        if(song_buf[i].channel != 0) {
            if(song_buf[i].step >= scroll_beat) {
                if(song_buf[i].step < scroll_beat+23) {
                    if(song_buf[i].pitch > BOTTOM_NOTE && song_buf[i].pitch < BOTTOM_NOTE+80) {
                        x = song_buf[i].pitch - BOTTOM_NOTE;
                        y = 2 + song_buf[i].step - scroll_beat;
                        charat(x, y) = NOTE_CHAR;
                        colorat(x, y) = 0x03 + song_buf[i].channel; 
                    }
                }
            }
        }
    }
}

void render_piano_keyboard() {
    /*draw piano starting from A1*/
    int i;
    paint_box(0, 1, 80, 1, 0x70);
    for(i = 0; i < 80; i++) {
        charat(i, 1) = is_black_note(i+BOTTOM_NOTE) * 0xDF;
    }
}

void render_top_bar() {

    sprintf(meter_str, "%02d/%02d ", meter_num, meter_dnm);
    sprintf(tempo_str, "%03d   ", tempo);
    sprintf(cursor_str, "%04d:%03d", (cursor_beat / steps_per_beat) / meter_num, (cursor_beat / steps_per_beat) % meter_num);

    write_string("UNTITLED.MZ", 0, 0, 11, 0);
    write_string("0123456789", 24 , 0, 10, 0);
    write_string(meter_str, 56, 0, 6, 0);
    write_string(tempo_str, 64, 0, 4, 0);
    write_string(cursor_str, 72, 0, 8, 0);
    paint_box(0, 0, 80, 1, 0x30);
    colorat(24, 0) = 0x34;
    colorat(25, 0) = 0x35;
    colorat(26, 0) = 0x36;
    colorat(27, 0) = 0x37;
    colorat(28, 0) = 0x38;
    colorat(29, 0) = 0x39;
    colorat(30, 0) = 0x3A;
    colorat(31, 0) = 0x3B;
    colorat(32, 0) = 0x3C;
    colorat(33, 0) = 0x3D;

    colorat(24 + (cursor_channel+9) % 10, 0) += 0x80;
}

int main(int argc, char** argv) {
    char keystates[32];
    int i;
    note default_note;

    default_note.channel = 0x01;
    default_note.pitch = 57;
    default_note.step = 0;
    default_note.volume = 0xFF;

    memset(keystates, 0, 32);

    init_keyboard();
    if(song_buf != NULL) {
        for(i = 0; i < song_buf_size; i++) {
            song_buf[i].channel = 0;
        }
    } else {
        printf("error, song_buf malloced as null");
    }

    clear_screen(0x07);

    render_top_bar();
    render_piano_keyboard();

    song_buf[0] = default_note;
    song_buf[0].pitch = 57;
    song_buf[0].step = 0;

    song_buf[1] = default_note;
    song_buf[1].pitch = 60;
    song_buf[1].step = 1;

    song_buf[2] = default_note;
    song_buf[2].pitch = 64;
    song_buf[2].step = 2;

    song_buf[3] = default_note;
    song_buf[3].pitch = 69;
    song_buf[3].step = 3;

    song_buf[4] = default_note;
    song_buf[4].pitch = 45;
    song_buf[4].step = 8;

    song_buf[5] = default_note;
    song_buf[5].pitch = 48;
    song_buf[5].step = 9;

    song_buf[6] = default_note;
    song_buf[6].pitch = 52;
    song_buf[6].step = 10;

    song_buf[7] = default_note;
    song_buf[7].pitch = 57;
    song_buf[7].step = 11;

    render_piano_roll();
    while(!test_keybuf(keystates, KEY_ESC)) {
        clear_keybuf(keystates);
        get_keys_hit(keystates);
    }
    deinit_keyboard();
    return 0;
}


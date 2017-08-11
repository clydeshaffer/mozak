#include <stdio.h>
#include <dos.h>

#include "TXTUI.h"
#include "keycodes.h"

#define BOTTOM_NOTE 21

#define NOTE_CHAR 13

char keystates[32];
char old_keystates[32];

int meter_num = 4;
int meter_dnm = 4;
int tempo = 120;

int file_dirty = 0;

int steps_per_beat = 1;
int scroll_beat = 0;
int cursor_beat = 0;
int cursor_channel = 1;
int highlight_channels=0;
int cursor_note = 57;

int black_notes[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

char meter_str[8];
char tempo_str[8];
char cursor_str[10];

char filename_str[8] = "UNTITLED";

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

int get_note_index(int pitch, int step, int channel) {
    int i;
    for(i = 0; i < song_buf_size; i++) {
        if(song_buf[i].channel == channel && song_buf[i].step == step && song_buf[i].pitch == pitch) {
            return i;
        }  
    }
    return -1;
}

int create_note(note newnote) {
    int i;
    for(i = 0; i < song_buf_size; i++) {
        if(song_buf[i].channel == 0) {
            song_buf[i] = newnote;
            return i;
        }  
    }
    printf("NOTE LIMIT REACHED\n");
    return -1;
}

note create_default_note() {
    note default_note;
    default_note.channel = 0x01;
    default_note.pitch = 57;
    default_note.step = 0;
    default_note.volume = 0xFF;
    return default_note;
}

void create_cursor_note() {
    int new_note_ind = create_note(create_default_note());
    if(new_note_ind != -1) {
        song_buf[new_note_ind].pitch = cursor_note;
        song_buf[new_note_ind].step = cursor_beat;
        song_buf[new_note_ind].channel = cursor_channel;    
    }
}

void render_piano_roll() {
    /*starting at line 2, draw dotted lines every (meter_num * steps_per_beat) rows*/
    int x, y, i;
    clear_box(0, 2, 80, 23);
    paint_box(0, 2, 80, 23, 0x07);
    for(y = 2 + (scroll_beat % (meter_num * steps_per_beat)); y < 25; y+= meter_num * steps_per_beat) {
        for(x = 0; x < 80; x++) {
            charat(x, y) = 0xFA;
        }
    }

    highlight_channels = 0;
    for(i = 0; i < song_buf_size; i++) {
        if(song_buf[i].channel != 0) {
            if(song_buf[i].step >= scroll_beat) {
                if(song_buf[i].step < scroll_beat+23) {
                    if(song_buf[i].pitch > BOTTOM_NOTE && song_buf[i].pitch < BOTTOM_NOTE+80) {
                        x = song_buf[i].pitch - BOTTOM_NOTE;
                        y = 2 + song_buf[i].step - scroll_beat;
                        if(charat(x,y) == NOTE_CHAR || charat(x,y) == NOTE_CHAR+1) {
                            charat(x, y) = NOTE_CHAR+1;
                        } else {
                            charat(x, y) = NOTE_CHAR;
                        }
                        colorat(x, y) = 0x03 + song_buf[i].channel;
                        if(song_buf[i].pitch == cursor_note && song_buf[i].step == cursor_beat) {
                            highlight_channels |= 1 << (song_buf[i].channel - 1);
                        } 
                    }
                }
            }
        }
    }

    if(cursor_beat >= scroll_beat && cursor_beat < scroll_beat+23) {
        colorat(cursor_note - BOTTOM_NOTE, cursor_beat - scroll_beat + 2) &= 0x0F;
        colorat(cursor_note - BOTTOM_NOTE, cursor_beat - scroll_beat + 2) |= 0x10;
    }
}

void render_piano_keyboard() {
    /*draw piano starting from A1*/
    int i;
    paint_box(0, 1, 80, 1, 0x0F);
    for(i = 0; i < 80; i++) {
        charat(i, 1) = is_black_note(i+BOTTOM_NOTE) ? 0xDC : 0xDB;
        if(i == (cursor_note - BOTTOM_NOTE)) {
            colorat(i, 1) = 0x4C;
        }
    }
}

void render_top_bar() {
    int i = 0;
    clear_box(0, 0, 80, 1);
    sprintf(meter_str, "%02d/%02d ", meter_num, meter_dnm);
    sprintf(tempo_str, "%03d   ", tempo);
    sprintf(cursor_str, "%04d:%03d", (cursor_beat / steps_per_beat) / meter_num, (cursor_beat / steps_per_beat) % meter_num);

    while(i < 8 && filename_str[i] != 0) {
        i++;
    }
    write_string(filename_str, 0, 0, i, 0);
    write_string(".MZ", i, 0, 3, 0);
    if(file_dirty)
        write_string("*", i+3, 0, 1, 0);
    write_string("1234567890", 24 , 0, 10, 0);
    write_string(meter_str, 56, 0, 6, 0);
    write_string(tempo_str, 64, 0, 4, 0);
    write_string(cursor_str, 72, 0, 8, 0);
    paint_box(0, 0, 80, 1, 0x30);
    for(i = 0; i < 10; i ++) {
        colorat(24+i, 0) = 0x34+i;
        if(highlight_channels & (1 << i)) {
            colorat(24+i, 0) &= 0x0F;
            colorat(24+i,0) |= 0x10;
        }
    }

    colorat(24 + (cursor_channel+9) % 10, 0) += 0x80;
}

int just_pressed(int keycode) {
    return test_keybuf(keystates, keycode) && !test_keybuf(old_keystates, keycode);
}

int main(int argc, char** argv) {
    int i, redraw_roll = 0, redraw_top = 0, redraw_keys = 0;

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

    
    render_piano_keyboard();
    render_piano_roll();
    render_top_bar();

    clear_keybuf(keystates);
    while(!test_keybuf(keystates, KEY_ESC)) {
        memcpy(old_keystates, keystates, 32);
        get_keys_hit(keystates);
        redraw_top = 0;
        redraw_roll = 0;
        redraw_keys = 0;
        for(i = 0; i < 10; i++) {
            if(just_pressed(KEY_1 + i)) {
                cursor_channel = i+1;
                redraw_top = 1;
            }
        }
        if(just_pressed(KEY_UP)) {
                    if(cursor_beat > 0) {
                        cursor_beat--;
                        if(test_keybuf(keystates, KEY_LSHIFT)) {
                            cursor_beat -= meter_num - 1;
                            if(cursor_beat < 0) cursor_beat = 0;
                        }
                        if(cursor_beat < scroll_beat) {
                            scroll_beat = cursor_beat;
                        }
                        redraw_roll = 1;
                        redraw_top = 1;
                    }
            }
            if(just_pressed(KEY_DOWN)) {
                cursor_beat++;
                if(test_keybuf(keystates, KEY_LSHIFT)) cursor_beat+= meter_num - 1;
                if(cursor_beat >= scroll_beat+23) {
                    scroll_beat = cursor_beat - 22;
                }
                redraw_roll = 1;
                redraw_top = 1;
            }
            if(just_pressed(KEY_LEFT)) {
                cursor_note--;
                redraw_roll = 1;
                redraw_top = 1;
                redraw_keys = 1;
            }
            if(just_pressed(KEY_RIGHT)) {
                cursor_note++;
                redraw_roll = 1;
                redraw_top = 1;
                redraw_keys = 1;
            }

            if(just_pressed(KEY_SPACE)) {
                char cursor_char = charat(cursor_note - BOTTOM_NOTE, cursor_beat - scroll_beat + 2);
                if(cursor_char == NOTE_CHAR || cursor_char == NOTE_CHAR+1) {
                    int noteind = get_note_index(cursor_note, cursor_beat, cursor_channel);
                    if(noteind == -1) {
                        /*marked note belongs to another channel
                            so add one from cursor channel*/
                        create_cursor_note();
                    } else {
                        /*remove the note found*/
                        song_buf[noteind].channel = 0;
                    }
                } else {
                    /* no notes should be here so add one to list*/
                    create_cursor_note();
                }
                redraw_roll = 1;
                redraw_top = 1;
            }

            if(redraw_roll) render_piano_roll();
            if(redraw_top) render_top_bar();
            if(redraw_keys) render_piano_keyboard();
    }
    deinit_keyboard();
    return 0;
}


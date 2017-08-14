#include <stdio.h>
#include <dos.h>

#include "TXTUI.h"
#include "keycodes.h"
#include "blaster.h"

#define BOTTOM_NOTE 21

#define NOTE_CHAR 13

#define COMPOSE_MODE 0
#define PARAMS_MODE 1
#define PLAY_MODE 2
#define QUIT_MODE 3

#define PARAMS_COUNT 6
#define OPERATOR_COUNT 2

char format_version = 0x03;

char keystates[32];
char old_keystates[32];

const char *param_names[] = {
    "HARMONIC",
    "VOLUME  ",
    "ATK/DECY",
    "SUST/REL",
    "WAVEFORM",
    "FEEDBACK"
};

int prev_mode = COMPOSE_MODE;
int interface_mode = COMPOSE_MODE;

unsigned long fast_timer = 0;

static void interrupt (far *oldtimer)(void);

int tempo = 120;
unsigned char steps_per_beat = 1;
unsigned char timer_count = 0;

void set_timer_divisor(unsigned short d) {
    unsigned char upper = (unsigned char) (d >> 8);
    unsigned char lower = (unsigned char) (d & 0xFF);
    outportb(0x43, 0x36);
    outportb(0x40, lower);
    outportb(0x40, upper);
}

void interrupt sb_timer() {
    fast_timer+=tempo*steps_per_beat;
    timer_count++;
    if(timer_count % 8 == 0) oldtimer();
    else {
        asm mov al, 20h
        asm out 20h, al
    }
}

void switch_mode(int state) {
    if(state == interface_mode) return;
    prev_mode = interface_mode;
    interface_mode = state;
}

int meter_num = 4;
int meter_dnm = 4;

int file_dirty = 0;

int scroll_beat = 0;
int cursor_beat = 0;
int cursor_channel = 1;
int highlight_channels=0;
int cursor_note = 57;
int cursor_duration = 1;
int cursor_param = 0;

int black_notes[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

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
int note_count = 0;

FM_Instrument instruments[10];

/* notenum halfsteps from C0 */
int is_black_note(int notenum) {
    return black_notes[notenum % 12];
}

int get_note_index(int step, int channel) {
    int i;
    for(i = 0; i < song_buf_size; i++) {
        if(song_buf[i].channel == channel && song_buf[i].step == step) {
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
            note_count++;
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
    default_note.duration = 1;
    return default_note;
}

void create_cursor_note() {
    int new_note_ind = create_note(create_default_note());
    if(new_note_ind != -1) {
        song_buf[new_note_ind].pitch = cursor_note;
        song_buf[new_note_ind].step = cursor_beat;
        song_buf[new_note_ind].channel = cursor_channel;   
        song_buf[new_note_ind].duration = cursor_duration; 
    }
}

void render_piano_roll() {
    /*starting at line 2, draw dotted lines every (meter_num * steps_per_beat) rows*/
    int x, y, i;
    clear_box(0, 2, 80, 23);
    paint_box(0, 2, 80, 23, 0x07);
    for(y = 2 - (scroll_beat % (meter_num * steps_per_beat)); y < 25; y+= meter_num * steps_per_beat) {
        if(y < 2) continue;
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
    char meter_str[8];
    char tempo_str[8];
    char cursor_str[10];
    char duration_str[5];
    char count_str[5];
    clear_box(0, 0, 80, 1);
    sprintf(meter_str, "%02d/%02d ", meter_num, meter_dnm);
    sprintf(tempo_str, "%03d   ", tempo);
    sprintf(cursor_str, "%04d:%03d", (cursor_beat / steps_per_beat) / meter_num, (cursor_beat / steps_per_beat) % meter_num);
    sprintf(duration_str, "  %02d", cursor_duration);
    sprintf(count_str ,"%04d", note_count);
    while(i < 8 && filename_str[i] != 0) {
        i++;
    }
    write_string(filename_str, 0, 0, i, 0);
    write_string(".MZ", i, 0, 3, 0);
    if(file_dirty)
        write_string("*", i+3, 0, 1, 0);
    write_string(count_str, 16, 0, 4, 0);
    charat(20, 0) = NOTE_CHAR;
    write_string("1234567890", 24 , 0, 10, 0);
    write_string(duration_str, 36, 0, 4, 0);
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

int render_quit_confirm() {
    /*26 chars long*/
    char confirm_text[] = "QUIT WITHOUT SAVING? (Y/N)";
    draw_box(26, 11, 28, 3, 0x30);
    write_string(confirm_text, 27, 12, 26, 0x30);
}

char hex_char(int num) {
    if(num < 10) {
        return '0' + num;
    } else {
        return 'A' + (num - 10);
    }
}

void render_params_window() {
    int i, j;
    char bytenum[4];
    clear_box(64, 1, 16, 24);
    paint_box(64, 1, 16, 24, 0x30);
    draw_box(64, 1, 16, 24, 0x33 + cursor_channel);

    cursor_param = cursor_param % (OPERATOR_COUNT * PARAMS_COUNT);

    for(i = 0; i < PARAMS_COUNT; i ++) {
        write_string(param_names[i], 66, 2 + i * (OPERATOR_COUNT + 1), 8, (cursor_param / OPERATOR_COUNT == i) ? 0x1F : 0x30);
        for(j = 0; j < OPERATOR_COUNT; j++) {
            unsigned char *inst_param = (char*) &instruments[cursor_channel-1];
            inst_param += i * OPERATOR_COUNT + j;
            paint_box(76, 2 + i * (OPERATOR_COUNT + 1) + j, 2, 1, cursor_param == (i * OPERATOR_COUNT + j) ? 0x0F : 0x07);
            charat(76, 2 + i * (OPERATOR_COUNT + 1) + j) = hex_char(*inst_param >> 4);
            charat(77, 2 + i * (OPERATOR_COUNT + 1) + j) = hex_char(*inst_param % 16);
        }
    }
}

void render_options_window() {

}

void adjust_instrument(int amount) {
    char *inst_param = (char*) &instruments[cursor_channel-1];
    inst_param += cursor_param;
    *inst_param += amount;
    Sb_FM_Set_Voice(cursor_channel-1, &instruments[cursor_channel-1]);
}

int just_pressed(int keycode) {
    return test_keybuf(keystates, keycode) && !test_keybuf(old_keystates, keycode);
}

int just_released(int keycode) {
    return !test_keybuf(keystates, keycode) && test_keybuf(old_keystates, keycode);
}

void swap_notes(int a, int b) {
    note holder = song_buf[b];
    song_buf[b] = song_buf[a];
    song_buf[a] = holder;
}

/*1 if a>b 0 otherwise*/
int compare_notes(int a, int b) {
    if(song_buf[a].channel == 0 && song_buf[b].channel != 0) return 1;
    if(song_buf[a].channel != 0 && song_buf[b].channel == 0) return 0;
    if(song_buf[a].step > song_buf[b].step) return 1;
    return 0;
}

void sort_song() {
    int i, j, didswap;
    for(i=0; i < song_buf_size-1; i++) {
        didswap=0;
        for(j=0; j<song_buf_size-i-1;j++) {
            if(compare_notes(j, j+1)) {
                swap_notes(j, j+1);
                didswap=1;
            }
        }
        if(!didswap) break;
    }
}

void save_song() {
    FILE *write_ptr;
    char filename_str_ext[12];
    int i;
    for(i = 0; i < 8; i++) {
        if(filename_str[i] == 0) break;
        filename_str_ext[i] = filename_str[i];
    }
    filename_str_ext[i++] = '.';
    filename_str_ext[i++] = 'M';
    filename_str_ext[i++] = 'Z';
    filename_str_ext[i++] = 0;
    sort_song();
    for(i = 0; i < song_buf_size; i ++) {
        if(song_buf[i].channel == 0) break;
    }
    write_ptr = fopen(filename_str_ext, "wb");
    fwrite(&format_version, sizeof(format_version), 1, write_ptr);
    fwrite(&tempo, sizeof(tempo), 1, write_ptr);
    fwrite(&steps_per_beat, sizeof(steps_per_beat), 1, write_ptr);
    fwrite(instruments, sizeof(FM_Instrument), 10, write_ptr);
    fwrite(song_buf, sizeof(note), i, write_ptr);
    fclose(write_ptr);
    file_dirty = 0;
}

void load_song() {
    FILE *read_ptr = NULL;
    char filename_str_ext[12];
    int i;
    unsigned char loadversion;
    printf("lets load the song\n");
    for(i = 0; i < 8; i++) {
        if(filename_str[i] == 0) break;
        filename_str_ext[i] = filename_str[i];
    }
    printf("ok, so title is %d chars long\n", i);
    filename_str_ext[i++] = '.';
    filename_str_ext[i++] = 'M';
    filename_str_ext[i++] = 'Z';
    filename_str_ext[i++] = 0;
    printf("filename is %s\n", filename_str_ext);
    read_ptr = fopen(filename_str_ext, "rb");
    if(read_ptr == NULL) return;
    for(i = 0; i < song_buf_size; i++) {
        song_buf[i].channel = 0;
        note_count--;
    }
    printf("song memory cleared, let's begin\n");

    /*check file format version*/
    fread(&loadversion, sizeof(loadversion), 1, read_ptr);

    /*REMEMBER! When adding things here, increment version
    and only load params from correct versions*/

    fread(&tempo, sizeof(tempo), 1, read_ptr);

    if(loadversion >= 0x03) {
        fread(&steps_per_beat, sizeof(steps_per_beat), 1, read_ptr);
    }

    fread(instruments, sizeof(FM_Instrument), 10, read_ptr);
    for(i = 0; i < song_buf_size; i++) {
        if(fread(&song_buf[i], sizeof(note), 1, read_ptr) == 0) break;
    }
    note_count = i;
    printf("loaded %d notes\n", i);
    fclose(read_ptr);

    for(i = 0; i < 10; i ++) {
        Sb_FM_Set_Voice(i, &instruments[i]);
    }
}



int main(int argc, char** argv) {
    int i, redraw_roll = 0, 
            redraw_top = 0, 
            redraw_keys = 0, 
            redraw_params = 0, 
            redraw_quit = 0,
            steptimer = 0, 
            playindex = 0;
    unsigned short start;
    unsigned short *my_clock = (unsigned short *)0x0000046C;
    unsigned short frame_duration;
    int do_exit = 0;

    start = *my_clock;
    /*read and clean up song name argument*/
    if(argc == 2) {
        for(i = 0; i < 8; i ++) {
            if(argv[1][i] == '.') {
                filename_str[i] = 0;
                break;
            }
            filename_str[i] = argv[1][i];
        }
    }


    memset(keystates, 0, 32);

    init_keyboard();

    for(i = 0; i < song_buf_size; i++) {
        song_buf[i].channel = 0;
    }

    Sb_Get_Params();

    Sb_FM_Reset();

    WriteFM(0, 1, 1 << 5);

    oldtimer = getvect(8);
    setvect(8, sb_timer);
    set_timer_divisor(8192);
    WriteFM(0, 2, 0x00);
    WriteFM(0, 4, 1 & (1 << 7));


    for(i = 0; i < 10; i++) {
        FM_Instrument defaultstrument = {
            0x00, 0x01, 0x10, 0x00,
            0xf0, 0xf8, 0x77, 0x77,
            0x00, 0x00, 0x00, 0x00,
            0x98, 0x00, 0x00, 0x31 
        };
        instruments[i] = defaultstrument;
        Sb_FM_Set_Voice(i, &instruments[i]);
    }

    /*attempt load song*/
    load_song();

    clear_screen(0x07);

    

    render_piano_keyboard();
    render_piano_roll();
    render_top_bar();

    clear_keybuf(keystates);
    while(!do_exit) {
        int shift_held;
        frame_duration = (*my_clock - start); /*IN UNITS OF 18.2 PER SECOND*/
        start = *my_clock;
        memcpy(old_keystates, keystates, 32);
        get_keys_hit(keystates);
        shift_held = test_keybuf(keystates, KEY_LSHIFT) || test_keybuf(keystates, KEY_RSHIFT);
        redraw_top = 0;
        redraw_roll = 0;
        redraw_keys = 0;
        redraw_params = 0;
        redraw_quit = 0;
        if(interface_mode == COMPOSE_MODE) {
            for(i = 0; i < 10; i++) {
                if(just_pressed(KEY_1 + i)) {
                    cursor_channel = i+1;
                    redraw_top = 1;
                }
            }
            if(just_pressed(KEY_UP)) {
                    if(cursor_beat > 0) {
                        cursor_beat--;
                        if(shift_held) {
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
                if(shift_held) cursor_beat+= meter_num - 1;
                if(cursor_beat >= scroll_beat+23) {
                    scroll_beat = cursor_beat - 22;
                }
                redraw_roll = 1;
                redraw_top = 1;
            }
            if(just_pressed(KEY_LEFT)) {
                cursor_note--;
                if(shift_held) cursor_note -= 11;
                if(cursor_note < BOTTOM_NOTE) cursor_note = BOTTOM_NOTE;
                redraw_roll = 1;
                redraw_top = 1;
                redraw_keys = 1;
            }
            if(just_pressed(KEY_RIGHT)) {
                cursor_note++;
                if(shift_held) cursor_note += 11;
                if(cursor_note >= BOTTOM_NOTE + 80) cursor_note = BOTTOM_NOTE + 79;
                redraw_roll = 1;
                redraw_top = 1;
                redraw_keys = 1;
            }

            if(just_pressed(KEY_EQUALS)) {
                if(shift_held) {
                    if(tempo < 999) tempo++;
                } else {
                    if(cursor_duration < 16) cursor_duration++;
                }
                file_dirty = 1;
                redraw_top = 1;
            }

            if(just_pressed(KEY_MINUS)) {
                if(shift_held) {
                    if(tempo > 10) tempo --;
                } else {
                    if(cursor_duration > 1) cursor_duration--;
                }
                file_dirty = 1;
                redraw_top = 1;
            }

            if(just_pressed(KEY_SPACE)) {
                int noteind = get_note_index(cursor_beat, cursor_channel);
                if(noteind == -1) {
                    create_cursor_note();
                    Sb_FM_Key_Off(cursor_channel-1);
                    Sb_FM_Key_On(cursor_channel-1, note_fnums[cursor_note], note_octaves[cursor_note]);
                } else {
                    if(cursor_note == song_buf[noteind].pitch) {
                        song_buf[noteind].channel = 0;
                    } else {
                        song_buf[noteind].pitch = cursor_note;
                        Sb_FM_Key_Off(cursor_channel-1);
                        Sb_FM_Key_On(cursor_channel-1, note_fnums[cursor_note], note_octaves[cursor_note]);
                    }
                    
                }
                redraw_roll = 1;
                redraw_top = 1;
                file_dirty = 1;
            }

            if(just_pressed(KEY_F2) && file_dirty) {
                save_song();
                redraw_top = 1;
            }

            if(just_pressed(KEY_TAB)) {
                switch_mode(PARAMS_MODE);
                redraw_params = 1;
            }

            if(just_pressed(KEY_ENTER)) {
                switch_mode(PLAY_MODE);
                if(shift_held) scroll_beat = 0;
                cursor_beat = scroll_beat;
                sort_song();
                fast_timer = 0;
                steptimer = 0;
                playindex = 0;
                file_dirty = 1;;
            }
        } else if(interface_mode == PARAMS_MODE) {
            if(just_pressed(KEY_TAB)) {
                switch_mode(COMPOSE_MODE);
                redraw_roll = 1;
                redraw_keys = 1;
            }
            for(i = 0; i < 10; i++) {
                if(just_pressed(KEY_1 + i)) {
                    cursor_channel = i+1;
                    redraw_top = 1;
                    redraw_params = 1;
                }
            }
            if(just_pressed(KEY_UP)) {
                if(cursor_param > 0) {
                    cursor_param --;
                    redraw_params = 1;
                }
            }
            if(just_pressed(KEY_DOWN)) {
                if(cursor_param < (PARAMS_COUNT * OPERATOR_COUNT - 1)) {
                    cursor_param ++;
                    redraw_params = 1;
                }
            }

            if(just_pressed(KEY_LEFT)) {
                int adjust_amount = -1;
                if(shift_held) {
                    adjust_amount = -16;
                }
                adjust_instrument(adjust_amount);
                redraw_params = 1;
            }

            if(just_pressed(KEY_RIGHT)) {
                int adjust_amount = 1;
                if(shift_held) {
                    adjust_amount = 16;
                }
                adjust_instrument(adjust_amount);
                redraw_params = 1;
            }

            if(just_pressed(KEY_SPACE)) {
                Sb_FM_Key_On(cursor_channel-1, note_fnums[cursor_note], note_octaves[cursor_note]);
            } else if(just_released(KEY_SPACE)) {
                Sb_FM_Key_Off(cursor_channel-1);
            }
        } else if(interface_mode == PLAY_MODE) {
            if(just_pressed(KEY_ENTER)) {
                switch_mode(COMPOSE_MODE);
            }

            while((song_buf[playindex].step < cursor_beat) && (song_buf[playindex].channel != 0)) {
                playindex++;
            }
            while((song_buf[playindex].step == cursor_beat) && (song_buf[playindex].channel != 0)) {
                Sb_FM_Key_Off(song_buf[playindex].channel-1);
                Sb_FM_Key_On(song_buf[playindex].channel-1, note_fnums[song_buf[playindex].pitch], note_octaves[song_buf[playindex].pitch]);
                playindex++;
                
            }

            if(song_buf[playindex].channel == 0) {
                    switch_mode(COMPOSE_MODE);
            }

            if(fast_timer > 8736) {
                fast_timer -= 8736;
                cursor_beat++;
                scroll_beat++;
                redraw_top = 1;
                redraw_roll = 1;
            }
        } else if(interface_mode == QUIT_MODE) {
            if(file_dirty) {
                if(just_pressed(KEY_Y)) {
                    do_exit = 1;
                }
                if(just_pressed(KEY_N)) {
                    do_exit = 0;
                    switch_mode(prev_mode);
                    redraw_roll = 1;
                }
            } else {
                do_exit = 1;
            }
        }

        if(interface_mode != QUIT_MODE) {
            if(just_pressed(KEY_ESC)) {
                switch_mode(QUIT_MODE);
                redraw_quit = file_dirty;
            }
        }

        if(redraw_roll) render_piano_roll();
        if(redraw_top) render_top_bar();
        if(redraw_keys) render_piano_keyboard();
        if(redraw_params) render_params_window();
        if(redraw_quit) render_quit_confirm();
    }
    deinit_keyboard();
    set_timer_divisor(0);
    setvect(8, oldtimer);
    for(i = 0; i < 25; i ++) {
        printf("\n");
    }
    printf("Thanks for using MOZAK!");
    return 0;
}


#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dolphin/dolphin.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

enum state {
    paused = 0,
    running = 1,
    stopped = 4,
};

typedef struct {
    Gui* gui;
    enum state state;
    uint8_t width;
    uint8_t height;
    uint8_t grid[128][64];
    uint8_t cursor_x;
    uint8_t cursor_y;
    uint8_t scale;
    uint8_t cursor_blink;
} Conway;

typedef struct {
    int8_t x;
    int8_t y;
} pos;

static Conway* init_conway() {
    Conway* conway = malloc(sizeof(Conway));

    conway->width = 128;
    conway->height = 64;
    conway->cursor_x = 0;
    conway->cursor_y = 0;
    conway->state = paused;
    conway->scale = 1;

    conway->gui = furi_record_open(RECORD_GUI);

    for(uint8_t x = 0; x < 128; x++) {
        for(uint8_t y = 0; y < 64; y++) {
            conway->grid[x][y] = 0;
        }
    }

    return conway;
}

static uint32_t apply_scale(uint8_t value, uint8_t scale) {
    return value * scale;
}

static void draw_callback(Canvas* canvas, void* context) {
    Conway* conway = context;

    for(uint8_t x = 0; x < conway->width; x++) {
        for(uint8_t y = 0; y < conway->height; y++) {
            if(conway->grid[x][y] == 1) {
                // Ignore the cursor
                if(conway->cursor_x == x && conway->cursor_y == y) {
                    continue;
                }

                canvas_draw_box(
                    canvas,
                    apply_scale(x, conway->scale),
                    apply_scale(y, conway->scale),
                    conway->scale,
                    conway->scale);
            }
        }
    }

    if(conway->cursor_blink) {
        // Draw cursor without scaling
        canvas_draw_box(
            canvas,
            apply_scale(conway->cursor_x, conway->scale),
            apply_scale(conway->cursor_y, conway->scale),
            conway->scale,
            conway->scale);
    }
}

static void blink_cursor(Conway* conway) {
    conway->cursor_blink ^= 1;
}

void move_cursor(Conway* conway, int8_t dx, int8_t dy) {
    int x = conway->cursor_x + dx;
    int y = conway->cursor_y + dy;

    if(x >= 0 && x < conway->width) {
        conway->cursor_x = x;
    }

    if(y >= 0 && y < conway->height) {
        conway->cursor_y = y;
    }
}

static void input_callback(InputEvent* event, void* context) {
    if(context == NULL) {
        return;
    }

    Conway* conway = context;

    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        conway->state = stopped;
        return;
    }

    if(event->type != InputTypePress || event->type == InputTypeRepeat) {
        return;
    }

    switch(event->key) {
    case InputKeyOk:
        conway->grid[conway->cursor_x][conway->cursor_y] ^= 1;
        break;
    case InputKeyUp:
        move_cursor(conway, 0, -1);
        break;
    case InputKeyDown:
        move_cursor(conway, 0, 1);
        break;
    case InputKeyLeft:
        move_cursor(conway, -1, 0);
        break;
    case InputKeyRight:
        move_cursor(conway, 1, 0);
        break;
    case InputKeyBack:
        conway->state ^= running;
        break;
    default:
        break;
    }
}

int count_neighbours(Conway* conway, uint8_t x, uint8_t y) {
    pos offsets[] = {
        {
            -1,
            -1,
        },
        {0, -1},
        {1, -1},
        {-1, 0},
        {1, 0},
        {-1, 1},
        {0, 1},
        {1, 1}};

    int neighours = 0;

    for(int p = 0; p < 8; p++) {
        pos pos = offsets[p];
        if(conway->height - 1 < y + pos.y || (y + pos.y) < 0) {
            continue;
        }
        if(conway->width - 1 < x + pos.x || (x + pos.x) < 0) {
            continue;
        }

        if(conway->grid[y + pos.y][x + pos.x] == 1) {
            neighours++;
        }
    }
    return neighours;
}

void simulate(Conway* conway) {
    if(conway->state == paused) {
        return;
    }

    uint8_t buffer[conway->width][conway->height];

    for(int y = 0; y < conway->height; y++) {
        for(int x = 0; x < conway->width; x++) {
            int neighours = count_neighbours(conway, x, y);
            if(neighours == 0 || neighours == 1) {
                buffer[y][x] = 0;
            } else if(neighours == 3) {
                buffer[y][x] = 1;
            } else if(neighours >= 4) {
                buffer[y][x] = 0;
            }
        }
    }

    for(int y = 0; y < conway->height - 1; y++) {
        for(int x = 0; x < conway->width - 1; x++) {
            conway->grid[y][x] = buffer[y][x];
        }
    }

    return;
}

void game_loop(Conway* conway, ViewPort* viewPort) {
    while(conway->state != stopped) {
        blink_cursor(conway);
        view_port_update(viewPort);
        furi_delay_ms(250);
        simulate(conway);
        blink_cursor(conway);
        view_port_update(viewPort);
        furi_delay_ms(250);
    }
    return;
}

int32_t conway_main(void* p) {
    UNUSED(p);

    Conway* conway = init_conway();

    conway->scale = 4;

    ViewPort* viewPort = view_port_alloc();
    view_port_draw_callback_set(viewPort, draw_callback, conway);
    gui_add_view_port(conway->gui, viewPort, GuiLayerFullscreen);

    view_port_input_callback_set(viewPort, input_callback, conway);

    game_loop(conway, viewPort);

    gui_remove_view_port(conway->gui, viewPort);
    view_port_free(viewPort);

    furi_record_close(RECORD_GUI);
    free(conway);

    return 0;
}

#include "common.h"

void put_pixel_raw(u8 *fb, int screen_w, int screen_h, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= screen_w || y >= screen_h) return;

    int offset = ((x * screen_h) + (screen_h - 1 - y)) * 3;
    fb[offset + 0] = c.b;
    fb[offset + 1] = c.g;
    fb[offset + 2] = c.r;
}

void fill_rect_raw(u8 *fb, int screen_w, int screen_h, int x0, int y0, int x1, int y1, Color c) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > screen_w) x1 = screen_w;
    if (y1 > screen_h) y1 = screen_h;
    if (x1 <= x0 || y1 <= y0) return;

    for (int x = x0; x < x1; x++) {
        int offset = ((x * screen_h) + (screen_h - 1 - y0)) * 3;
        for (int y = y0; y < y1; y++) {
            fb[offset + 0] = c.b;
            fb[offset + 1] = c.g;
            fb[offset + 2] = c.r;
            offset -= 3;
        }
    }
}

Color shade_color(Color base, float shade) {
    Color out;
    if (shade < 0.0f) shade = 0.0f;
    if (shade > 1.3f) shade = 1.3f;
    int r = (int)(base.r * shade);
    int g = (int)(base.g * shade);
    int b = (int)(base.b * shade);
    out.r = (uint8_t)(r > 255 ? 255 : r);
    out.g = (uint8_t)(g > 255 ? 255 : g);
    out.b = (uint8_t)(b > 255 ? 255 : b);
    return out;
}

const uint8_t DIGITS_3X5[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
};

void draw_digit(u8 *fb, int sw, int sh, int x, int y, int d, Color c, int scale) {
    if (d < 0 || d > 9) return;
    for (int yy = 0; yy < 5; yy++) {
        uint8_t row = DIGITS_3X5[d][yy];
        for (int xx = 0; xx < 3; xx++) {
            if (row & (1 << (2 - xx))) {
                fill_rect_raw(fb, sw, sh, x + xx * scale, y + yy * scale,
                              x + (xx + 1) * scale, y + (yy + 1) * scale, c);
            }
        }
    }
}

void draw_number(u8 *fb, int sw, int sh, int x, int y, int value, Color c, int scale) {
    if (value < 0) value = 0;
    if (value >= 1000) value = 999;
    if (value >= 100) {
        draw_digit(fb, sw, sh, x, y, value / 100, c, scale);
        x += 4 * scale;
    }
    if (value >= 10) {
        draw_digit(fb, sw, sh, x, y, (value / 10) % 10, c, scale);
        x += 4 * scale;
    }
    draw_digit(fb, sw, sh, x, y, value % 10, c, scale);
}

uint8_t glyph3x5_row(char ch, int row) {
    static const uint8_t letters[26][5] = {
        {2,5,7,5,5}, {6,5,6,5,6}, {7,4,4,4,7}, {6,5,5,5,6}, {7,4,6,4,7}, {7,4,6,4,4},
        {7,4,5,5,7}, {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,7}, {5,5,6,5,5}, {4,4,4,4,7},
        {5,7,7,5,5}, {5,7,7,7,5}, {7,5,5,5,7}, {7,5,7,4,4}, {7,5,5,7,1}, {7,5,7,6,5},
        {7,4,7,1,7}, {7,2,2,2,2}, {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5},
        {5,5,2,2,2}, {7,1,2,4,7}
    };
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    if (ch >= '0' && ch <= '9') return DIGITS_3X5[ch - '0'][row];
    if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'][row];
    switch (ch) {
        case ' ': return 0;
        case ':': { static const uint8_t g[5] = {0,2,0,2,0}; return g[row]; }
        case '-': { static const uint8_t g[5] = {0,0,7,0,0}; return g[row]; }
        case '/': { static const uint8_t g[5] = {1,1,2,4,4}; return g[row]; }
        case '.': { static const uint8_t g[5] = {0,0,0,0,2}; return g[row]; }
        case '!': { static const uint8_t g[5] = {2,2,2,0,2}; return g[row]; }
        case '?': { static const uint8_t g[5] = {7,1,3,0,2}; return g[row]; }
        case '+': { static const uint8_t g[5] = {0,2,7,2,0}; return g[row]; }
        case '=': { static const uint8_t g[5] = {0,7,0,7,0}; return g[row]; }
        case '<': { static const uint8_t g[5] = {1,2,4,2,1}; return g[row]; }
        case '>': { static const uint8_t g[5] = {4,2,1,2,4}; return g[row]; }
        default:  { static const uint8_t g[5] = {7,1,3,0,2}; return g[row]; }
    }
}

void draw_char3x5(u8 *fb, int sw, int sh, int x, int y, char ch, Color c, int scale) {
    for (int yy = 0; yy < 5; yy++) {
        uint8_t bits = glyph3x5_row(ch, yy);
        for (int xx = 0; xx < 3; xx++) {
            if (bits & (1 << (2 - xx))) {
                fill_rect_raw(fb, sw, sh, x + xx * scale, y + yy * scale,
                              x + (xx + 1) * scale, y + (yy + 1) * scale, c);
            }
        }
    }
}

void draw_text3x5(u8 *fb, int sw, int sh, int x, int y, const char *text, Color c, int scale) {
    if (!text) return;
    int start_x = x;
    while (*text) {
        char ch = *text++;
        if (ch == '\n') {
            x = start_x;
            y += 7 * scale;
            continue;
        }
        draw_char3x5(fb, sw, sh, x, y, ch, c, scale);
        x += 4 * scale;
        if (x > sw - 4 * scale) break;
    }
}

void draw_text_number(u8 *fb, int sw, int sh, int x, int y, const char *label, int value, Color c, int scale) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%d", label ? label : "", value);
    draw_text3x5(fb, sw, sh, x, y, buf, c, scale);
}

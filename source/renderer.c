#include "common.h"

void draw_sky_floor(u8 *fb) {
    fill_rect_raw(fb, TOP_W, TOP_H, 0, 0, TOP_W, TOP_H / 2, (Color){60, 92, 145});
    fill_rect_raw(fb, TOP_W, TOP_H, 0, TOP_H / 2, TOP_W, TOP_H, (Color){45, 42, 38});
}

void render_raycast(const Level *lv) {
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;
    u8 *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (!fb) return;
    draw_sky_floor(fb);

    float pos_x = lv->player_x;
    float pos_y = lv->player_y;
    float cam_z = lv->player_z + EYE_HEIGHT;
    float angle = g_render_angle_override ? g_render_angle : lv->player_angle;

    float dir_x = cosf(angle);
    float dir_y = sinf(angle);
    float plane_len = tanf((FOV_DEGREES * PI_F / 180.0f) * 0.5f);
    float plane_x = -dir_y * plane_len;
    float plane_y = dir_x * plane_len;

    int max_steps = lv->width + lv->height + 8;
    float center_y = RENDER_H * 0.5f;

    for (int x = 0; x < RENDER_W; x++) {
        float camera_x = (2.0f * (float)x / (float)RENDER_W) - 1.0f;
        float ray_x = dir_x + plane_x * camera_x;
        float ray_y = dir_y + plane_y * camera_x;

        int map_x = (int)pos_x;
        int map_y = (int)pos_y;

        float delta_x = (fabsf(ray_x) < 0.000001f) ? 1.0e30f : fabsf(1.0f / ray_x);
        float delta_y = (fabsf(ray_y) < 0.000001f) ? 1.0e30f : fabsf(1.0f / ray_y);

        int step_x;
        int step_y;
        float side_x;
        float side_y;

        if (ray_x < 0.0f) {
            step_x = -1;
            side_x = (pos_x - (float)map_x) * delta_x;
        } else {
            step_x = 1;
            side_x = ((float)map_x + 1.0f - pos_x) * delta_x;
        }

        if (ray_y < 0.0f) {
            step_y = -1;
            side_y = (pos_y - (float)map_y) * delta_y;
        } else {
            step_y = 1;
            side_y = ((float)map_y + 1.0f - pos_y) * delta_y;
        }

        uint8_t hit_tile = 0;
        int side = 0;

        for (int s = 0; s < max_steps; s++) {
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = 0;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = 1;
            }

            if (map_x < 0 || map_y < 0 || map_x >= lv->width || map_y >= lv->height) {
                hit_tile = 1;
                break;
            }

            hit_tile = lv->tiles[tile_index(lv, map_x, map_y)];
            if (hit_tile) break;
        }

        if (!hit_tile) continue;

        float perp;
        if (side == 0) {
            float denom = fabsf(ray_x) > 0.000001f ? ray_x : 0.000001f;
            perp = ((float)map_x - pos_x + (1.0f - (float)step_x) * 0.5f) / denom;
        } else {
            float denom = fabsf(ray_y) > 0.000001f ? ray_y : 0.000001f;
            perp = ((float)map_y - pos_y + (1.0f - (float)step_y) * 0.5f) / denom;
        }
        if (perp < 0.001f) perp = 0.001f;

        float z0 = 0.0f;
        float z1 = 1.0f;
        if (hit_tile == PLATFORM_TILE) {
            z0 = PLATFORM_BOTTOM;
            z1 = PLATFORM_TOP;
        }

        float proj = (float)RENDER_H / perp;
        int draw_start = (int)(center_y + (cam_z - z1) * proj);
        int draw_end = (int)(center_y + (cam_z - z0) * proj);

        if (draw_start < 0) draw_start = 0;
        if (draw_end >= RENDER_H) draw_end = RENDER_H - 1;
        if (draw_end <= draw_start) continue;

        Color base = WALL_COLORS[hit_tile & 7];
        float shade = 1.0f / (1.0f + perp * 0.12f);
        if (side == 1) shade *= 0.72f;
        if (hit_tile == PLATFORM_TILE) shade *= 0.95f;
        Color c = shade_color(base, shade);

        int sx0 = x * PIXEL_SCALE;
        int sx1 = sx0 + PIXEL_SCALE;
        int sy0 = draw_start * PIXEL_SCALE;
        int sy1 = (draw_end + 1) * PIXEL_SCALE;
        fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy1, c);

        if (hit_tile == PLATFORM_TILE) {
            Color edge = c;
            edge.r = (edge.r > 215) ? 255 : edge.r + 40;
            edge.g = (edge.g > 215) ? 255 : edge.g + 40;
            edge.b = (edge.b > 215) ? 255 : edge.b + 40;
            fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy0 + PIXEL_SCALE, edge);
        }
    }

    Color white = {225, 225, 225};
    fill_rect_raw(fb, TOP_W, TOP_H, TOP_W / 2 - 6, TOP_H / 2, TOP_W / 2 + 7, TOP_H / 2 + 1, white);
    fill_rect_raw(fb, TOP_W, TOP_H, TOP_W / 2, TOP_H / 2 - 6, TOP_W / 2 + 1, TOP_H / 2 + 7, white);

    if (g_render_world_hud) {
        fill_rect_raw(fb, TOP_W, TOP_H, 4, 4, TOP_W - 4, 26, (Color){5, 5, 5});
        draw_text3x5(fb, TOP_W, TOP_H, 8, 8, g_edit_mode ? "EDITOR" : "PLAY", g_edit_mode ? (Color){120,240,120} : (Color){120,190,255}, 2);
        draw_text_number(fb, TOP_W, TOP_H, 82, 8, g_dirty ? "SLOT* " : "SLOT ", g_slot, (Color){240, 240, 240}, 2);
        draw_text_number(fb, TOP_W, TOP_H, 160, 8, "TILE ", g_selected_tile, WALL_COLORS[g_selected_tile & 7], 2);
        draw_text3x5(fb, TOP_W, TOP_H, 246, 8, g_status, (Color){230, 230, 160}, 1);
    }
}

#define EDITOR_MAP_Y 8
#define EDITOR_MAP_H 204
#define PALETTE_Y 220
#define PALETTE_X 4
#define PALETTE_STEP 22
#define PALETTE_W 18
#define PALETTE_H 16

void editor_layout(const Level *lv, int *cell, int *ox, int *oy) {
    int c_x = (BOT_W - 8) / lv->width;
    int c_y = EDITOR_MAP_H / lv->height;
    int c = c_x < c_y ? c_x : c_y;
    if (c > 3) c = 3;
    if (c < 1) c = 1;
    if (cell) *cell = c;
    if (ox) *ox = (BOT_W - lv->width * c) / 2;
    if (oy) *oy = EDITOR_MAP_Y;
}

Color map_tile_color(uint8_t tile) {
    if (tile == 0) return (Color){28, 28, 28};
    if (tile == PLATFORM_TILE) return (Color){40, 115, 120};
    Color base = WALL_COLORS[tile & 7];
    return (Color){(uint8_t)(base.r / 2 + 55), (uint8_t)(base.g / 2 + 55), (uint8_t)(base.b / 2 + 55)};
}

void draw_tile_palette(u8 *fb) {
    for (int t = 0; t < 8; t++) {
        int x0 = PALETTE_X + t * PALETTE_STEP;
        int y0 = PALETTE_Y;
        Color border = (t == g_selected_tile) ? (Color){255, 255, 255} : (Color){60, 60, 60};
        fill_rect_raw(fb, BOT_W, BOT_H, x0 - 1, y0 - 1, x0 + PALETTE_W + 1, y0 + PALETTE_H + 1, border);
        fill_rect_raw(fb, BOT_W, BOT_H, x0, y0, x0 + PALETTE_W, y0 + PALETTE_H, map_tile_color((uint8_t)t));
        draw_digit(fb, BOT_W, BOT_H, x0 + 6, y0 + 5, t, (t == 0) ? (Color){230,230,230} : (Color){8,8,8}, 1);
    }
}

void render_bottom_map(void) {
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    if (!fb) return;
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){12, 12, 12});

    const Level *lv = &g_level;
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;

    int cell, ox, oy;
    editor_layout(lv, &cell, &ox, &oy);

    fill_rect_raw(fb, BOT_W, BOT_H, ox - 2, oy - 2, ox + lv->width * cell + 2, oy + lv->height * cell + 2, (Color){4, 4, 4});

    for (int y = 0; y < lv->height; y++) {
        for (int x = 0; x < lv->width; x++) {
            uint8_t tile = tile_at(lv, x, y);
            Color c = map_tile_color(tile);
            fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + (y + 1) * cell, c);
        }
    }

    int px = ox + (int)(lv->player_x * cell);
    int py = oy + (int)(lv->player_y * cell);
    fill_rect_raw(fb, BOT_W, BOT_H, px - 2, py - 2, px + 3, py + 3, (Color){255, 60, 60});
    int ax = px + (int)(cosf(lv->player_angle) * cell * 3.0f);
    int ay = py + (int)(sinf(lv->player_angle) * cell * 3.0f);
    fill_rect_raw(fb, BOT_W, BOT_H, ax - 1, ay - 1, ax + 2, ay + 2, (Color){255, 160, 160});

    fill_rect_raw(fb, BOT_W, BOT_H, 0, 212, BOT_W, BOT_H, (Color){3, 3, 3});
    draw_tile_palette(fb);

    if (g_edit_mode) {
        draw_text3x5(fb, BOT_W, BOT_H, 4, 213, "EDIT", (Color){120, 240, 120}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 28, 213, g_dirty ? "S*" : "S", g_slot, (Color){245, 245, 245}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 48, 213, "T", g_selected_tile, WALL_COLORS[g_selected_tile & 7], 1);
        draw_text3x5(fb, BOT_W, BOT_H, 172, 213, "A SAVE START MENU", (Color){220, 220, 220}, 1);
    } else {
        draw_text3x5(fb, BOT_W, BOT_H, 4, 213, "PLAY", (Color){120, 190, 255}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 30, 213, g_dirty ? "S*" : "S", g_slot, (Color){245, 245, 245}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 176, 213, "START MENU", (Color){220, 220, 220}, 1);
    }

    draw_text3x5(fb, BOT_W, BOT_H, 170, 226, g_edit_mode ? "B ERASE X SPAWN SEL SIZE" : "B SPRINT A JUMP", (Color){200, 200, 200}, 1);
}

const char *menu_action_name(int action) {
    switch (action) {
        case MENU_ACTION_PLAY: return "PLAY";
        case MENU_ACTION_EDIT: return "EDIT";
        case MENU_ACTION_RESIZE: return "RESIZE";
        case MENU_ACTION_DUPLICATE: return "DUP";
        case MENU_ACTION_RENAME: return "NAME";
        case MENU_ACTION_DELETE: return "DELETE";
        default: return "?";
    }
}

void render_world_menu(void) {
    g_render_angle_override = true;
    g_render_angle = g_preview_spin;
    g_render_world_hud = false;
    render_raycast(&g_preview_level);
    g_render_world_hud = true;
    g_render_angle_override = false;

    u8 *top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (top) {
        fill_rect_raw(top, TOP_W, TOP_H, 0, 0, TOP_W, 34, (Color){4, 4, 8});
        draw_text3x5(top, TOP_W, TOP_H, 8, 7, "3DCASTER LEVEL SELECT", (Color){235,235,235}, 2);
        draw_text3x5(top, TOP_W, TOP_H, 8, 24, g_preview_valid ? g_slots[g_slot].name : "EMPTY SLOT", g_preview_valid ? (Color){230,230,160} : (Color){180,180,180}, 1);
        draw_text_number(top, TOP_W, TOP_H, 168, 24, "SLOT ", g_slot, (Color){190,220,255}, 1);
        if (g_preview_valid) {
            draw_text_number(top, TOP_W, TOP_H, 220, 24, "W", g_slots[g_slot].width, (Color){210,210,210}, 1);
            draw_text_number(top, TOP_W, TOP_H, 272, 24, "H", g_slots[g_slot].height, (Color){210,210,210}, 1);
        }
        draw_text3x5(top, TOP_W, TOP_H, 8, 226, g_status, (Color){240, 240, 170}, 1);
    }

    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    if (!fb) return;

    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){10, 10, 14});
    draw_text3x5(fb, BOT_W, BOT_H, 8, 8, "WORLD / LEVELS", (Color){235,235,235}, 2);

    int y = 32;
    for (int i = 0; i < SLOT_COUNT; i++) {
        Color row = (i == g_slot) ? (Color){45, 58, 84} : (Color){18, 18, 24};
        fill_rect_raw(fb, BOT_W, BOT_H, 8, y - 2, BOT_W - 8, y + 15, row);
        draw_text_number(fb, BOT_W, BOT_H, 14, y + 2, "S", i, (Color){210,230,255}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 38, y + 2, g_slots[i].exists ? g_slots[i].name : "EMPTY", g_slots[i].exists ? (Color){235,235,220} : (Color){120,120,120}, 1);
        if (g_slots[i].exists) {
            draw_text_number(fb, BOT_W, BOT_H, 230, y + 2, "W", g_slots[i].width, (Color){180,180,180}, 1);
            draw_text_number(fb, BOT_W, BOT_H, 272, y + 2, "H", g_slots[i].height, (Color){180,180,180}, 1);
        }
        y += 18;
    }

    fill_rect_raw(fb, BOT_W, BOT_H, 0, 182, BOT_W, BOT_H, (Color){4, 4, 8});

    if (g_resize_menu) {
        draw_text3x5(fb, BOT_W, BOT_H, 8, 188, "RESIZE / NEW LEVEL", (Color){255,220,140}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 8, 202, "WIDTH ", g_resize_w, (Color){230,230,230}, 2);
        draw_text_number(fb, BOT_W, BOT_H, 140, 202, "HEIGHT ", g_resize_h, (Color){230,230,230}, 2);
        draw_text3x5(fb, BOT_W, BOT_H, 8, 224, "DLEFT/RIGHT W  DUP/DDOWN H  A APPLY  B CANCEL", (Color){190,190,190}, 1);
    } else if (g_duplicate_menu) {
        draw_text_number(fb, BOT_W, BOT_H, 8, 188, "DUP FROM S", g_dup_source, (Color){255,220,140}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 8, 204, "TARGET S", g_dup_target, (Color){230,230,230}, 2);
        draw_text3x5(fb, BOT_W, BOT_H, 8, 224, "DUP/DDOWN TARGET  A COPY  B CANCEL", (Color){190,190,190}, 1);
    } else {
        if (g_delete_armed) {
            draw_text3x5(fb, BOT_W, BOT_H, 8, 188, "DELETE ARMED - PRESS A AGAIN", (Color){255,120,120}, 1);
        } else {
            draw_text3x5(fb, BOT_W, BOT_H, 8, 188, "ACTION", (Color){200,200,200}, 1);
            draw_text3x5(fb, BOT_W, BOT_H, 64, 186, menu_action_name(g_menu_action), (Color){255,220,140}, 2);
        }
        draw_text3x5(fb, BOT_W, BOT_H, 8, 210, "DUP/DDOWN SLOT  DLEFT/RIGHT ACTION", (Color){190,190,190}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 8, 224, "A SELECT  B CANCEL DEL  START EXIT APP", (Color){190,190,190}, 1);
    }
}

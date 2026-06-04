#include "common.h"

void draw_sky_floor(u8 *fb) {
    fill_rect_raw(fb, TOP_W, TOP_H, 0, 0, TOP_W, TOP_H / 2, (Color){60, 92, 145});
    fill_rect_raw(fb, TOP_W, TOP_H, 0, TOP_H / 2, TOP_W, TOP_H, (Color){45, 42, 38});
}

static Color blend_color(Color a, Color b, float t) {
    t = clampf32(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    Color out;
    out.r = (uint8_t)(a.r * inv + b.r * t + 0.5f);
    out.g = (uint8_t)(a.g * inv + b.g * t + 0.5f);
    out.b = (uint8_t)(a.b * inv + b.b * t + 0.5f);
    return out;
}

static Color apply_dof_fade(Color c, float distance) {
    if (!g_dof_enabled) return c;

    float start = clampf32(g_dof_start, 4.0f, 32.0f);
    float strength = clampf32(g_dof_strength, 0.10f, 1.00f);
    if (distance <= start) return c;

    float fade = clampf32((distance - start) / 18.0f, 0.0f, 1.0f) * strength;
    return blend_color(c, (Color){22, 24, 30}, fade);
}

static void draw_column_aa(u8 *fb, int sx0, int sx1, int sy0, int sy1, Color c,
                           int prev_sy0, int prev_sy1, Color prev_c) {
    if (!g_antialiasing) return;

    Color edge = blend_color(c, (Color){8, 8, 8}, 0.35f);
    if (sy1 - sy0 > 4) {
        fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy0 + 1, edge);
        fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy1 - 1, sx1, sy1, edge);
    }

    if (prev_sy1 > prev_sy0 && (abs(sy0 - prev_sy0) > 2 || abs(sy1 - prev_sy1) > 2)) {
        int seam_y0 = sy0 < prev_sy0 ? sy0 : prev_sy0;
        int seam_y1 = sy1 > prev_sy1 ? sy1 : prev_sy1;
        if (seam_y0 < 0) seam_y0 = 0;
        if (seam_y1 > TOP_H) seam_y1 = TOP_H;
        if (seam_y1 > seam_y0) {
            Color seam = blend_color(c, prev_c, 0.50f);
            fill_rect_raw(fb, TOP_W, TOP_H, sx0, seam_y0, sx0 + 1, seam_y1, seam);
        }
    }
}

static void render_raycast_eye(const Level *lv, gfx3dSide_t side, float eye_offset) {
    u8 *fb = gfxGetFramebuffer(GFX_TOP, side, NULL, NULL);
    if (!fb) return;
    draw_sky_floor(fb);

    const int map_w = lv->width;
    const int map_h = lv->height;
    const uint8_t *tiles = lv->tiles;

    float angle = g_render_angle_override ? g_render_angle : lv->player_angle;
    float dir_x = cosf(angle);
    float dir_y = sinf(angle);
    float right_x = -dir_y;
    float right_y = dir_x;

    float pos_x = lv->player_x + right_x * eye_offset;
    float pos_y = lv->player_y + right_y * eye_offset;
    float depth = clampf32(g_level_depth, 0.50f, 2.00f);
    float cam_z = (lv->player_z + EYE_HEIGHT) * depth;

    float fov = clampf32(g_fov_degrees, FOV_MIN_DEGREES, FOV_MAX_DEGREES);
    float plane_len = tanf((fov * PI_F / 180.0f) * 0.5f);
    float plane_x = -dir_y * plane_len;
    float plane_y = dir_x * plane_len;

    int max_steps = map_w + map_h + 8;
    int column_step = g_fast_render ? 2 : 1;
    if (g_antialiasing) column_step = 1;

    float bob_y = 0.0f;
    if (!g_render_angle_override && g_view_bob) {
        bob_y = sinf(g_bob_phase) * g_bob_amount;
    }
    float center_y = RENDER_H * 0.5f + (g_render_angle_override ? 0.0f : g_camera_pitch) + bob_y;

    float ray_x = dir_x - plane_x;
    float ray_y = dir_y - plane_y;
    float ray_step_x = (2.0f * plane_x / (float)RENDER_W) * (float)column_step;
    float ray_step_y = (2.0f * plane_y / (float)RENDER_W) * (float)column_step;

    int prev_sy0 = -1;
    int prev_sy1 = -1;
    Color prev_c = {0, 0, 0};

    for (int x = 0; x < RENDER_W; x += column_step, ray_x += ray_step_x, ray_y += ray_step_y) {
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
        int side_hit = 0;

        for (int s = 0; s < max_steps; s++) {
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side_hit = 0;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side_hit = 1;
            }

            if (map_x < 0 || map_y < 0 || map_x >= map_w || map_y >= map_h) {
                hit_tile = 1;
                break;
            }

            hit_tile = tiles[map_y * map_w + map_x];
            if (hit_tile) break;
        }

        if (!hit_tile) continue;

        float perp = (side_hit == 0) ? (side_x - delta_x) : (side_y - delta_y);
        if (perp < 0.001f) perp = 0.001f;

        float z0 = 0.0f;
        float z1 = 1.0f;
        if (hit_tile == PLATFORM_TILE) {
            z0 = PLATFORM_BOTTOM;
            z1 = PLATFORM_TOP;
        }
        z0 *= depth;
        z1 *= depth;

        float proj = (float)RENDER_H / perp;
        int draw_start = (int)(center_y + (cam_z - z1) * proj);
        int draw_end = (int)(center_y + (cam_z - z0) * proj);

        if (draw_start < 0) draw_start = 0;
        if (draw_end >= RENDER_H) draw_end = RENDER_H - 1;
        if (draw_end <= draw_start) continue;

        Color base = WALL_COLORS[hit_tile & 7];
        float shade = 1.0f / (1.0f + perp * 0.12f);
        if (side_hit == 1) shade *= 0.72f;
        if (hit_tile == PLATFORM_TILE) shade *= 0.95f;
        Color c = apply_dof_fade(shade_color(base, shade), perp);

        int sx0 = x * PIXEL_SCALE;
        int sx1 = sx0 + PIXEL_SCALE * column_step;
        if (sx1 > TOP_W) sx1 = TOP_W;
        int sy0 = draw_start * PIXEL_SCALE;
        int sy1 = (draw_end + 1) * PIXEL_SCALE;
        fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy1, c);
        draw_column_aa(fb, sx0, sx1, sy0, sy1, c, prev_sy0, prev_sy1, prev_c);

        if (hit_tile == PLATFORM_TILE) {
            Color edge = c;
            edge.r = (edge.r > 215) ? 255 : edge.r + 40;
            edge.g = (edge.g > 215) ? 255 : edge.g + 40;
            edge.b = (edge.b > 215) ? 255 : edge.b + 40;
            fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy0 + PIXEL_SCALE, edge);
        }

        prev_sy0 = sy0;
        prev_sy1 = sy1;
        prev_c = c;
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

void render_raycast(const Level *lv) {
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;

    bool stereo = g_3d_enabled && !g_render_angle_override;
    gfxSet3D(stereo);

    if (stereo) {
        render_raycast_eye(lv, GFX_LEFT, -0.025f);
        render_raycast_eye(lv, GFX_RIGHT, 0.025f);
    } else {
        render_raycast_eye(lv, GFX_LEFT, 0.0f);
    }
}

void editor_layout(const Level *lv, int *cell, int *ox, int *oy) {
    int vx, vy, vw, vh;
    editor_view_bounds(lv, &vx, &vy, &vw, &vh);
    (void)vx;
    (void)vy;

    if (vw < 1) vw = 1;
    if (vh < 1) vh = 1;

    int c_x = (BOT_W - 8) / vw;
    int c_y = EDITOR_MAP_H / vh;
    int c = c_x < c_y ? c_x : c_y;
    if (c < 1) c = 1;

    if (cell) *cell = c;
    if (ox) *ox = (BOT_W - vw * c) / 2;
    if (oy) *oy = EDITOR_MAP_Y + (EDITOR_MAP_H - vh * c) / 2;
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

static void draw_editor_button(u8 *fb, int x0, int y0, int x1, int y1, const char *label, bool active) {
    Color border = active ? (Color){90, 120, 170} : (Color){45, 45, 55};
    Color fill = active ? (Color){22, 30, 46} : (Color){14, 14, 18};
    Color text = active ? (Color){230, 238, 255} : (Color){115, 115, 125};
    fill_rect_raw(fb, BOT_W, BOT_H, x0, y0, x1, y1, border);
    fill_rect_raw(fb, BOT_W, BOT_H, x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill);
    int len = 0;
    while (label && label[len]) len++;
    int tx = x0 + ((x1 - x0) - len * 4) / 2;
    int ty = y0 + ((y1 - y0) - 5) / 2;
    if (tx < x0 + 2) tx = x0 + 2;
    draw_text3x5(fb, BOT_W, BOT_H, tx, ty, label, text, 1);
}

void render_bottom_map(void) {
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    if (!fb) return;
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){12, 12, 12});

    const Level *lv = &g_level;
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;

    editor_clamp_view(lv);

    int cell, ox, oy;
    int vx, vy, vw, vh;
    editor_layout(lv, &cell, &ox, &oy);
    editor_view_bounds(lv, &vx, &vy, &vw, &vh);

    fill_rect_raw(fb, BOT_W, BOT_H, ox - 2, oy - 2, ox + vw * cell + 2, oy + vh * cell + 2, (Color){4, 4, 4});

    for (int y = 0; y < vh; y++) {
        for (int x = 0; x < vw; x++) {
            int tx = vx + x;
            int ty = vy + y;
            uint8_t tile = tile_at(lv, tx, ty);
            Color c = map_tile_color(tile);
            fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + (y + 1) * cell, c);

            if (cell >= 10) {
                fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + y * cell + 1, (Color){8,8,8});
                fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + x * cell + 1, oy + (y + 1) * cell, (Color){8,8,8});
            }
        }
    }

    if ((int)lv->player_x >= vx && (int)lv->player_x < vx + vw && (int)lv->player_y >= vy && (int)lv->player_y < vy + vh) {
        int px = ox + (int)((lv->player_x - (float)vx) * cell);
        int py = oy + (int)((lv->player_y - (float)vy) * cell);
        int pr = cell >= 10 ? 3 : 2;
        fill_rect_raw(fb, BOT_W, BOT_H, px - pr, py - pr, px + pr + 1, py + pr + 1, (Color){255, 60, 60});
        int ax = px + (int)(cosf(lv->player_angle) * cell * 1.8f);
        int ay = py + (int)(sinf(lv->player_angle) * cell * 1.8f);
        fill_rect_raw(fb, BOT_W, BOT_H, ax - 1, ay - 1, ax + 2, ay + 2, (Color){255, 160, 160});
    }

    fill_rect_raw(fb, BOT_W, BOT_H, 0, EDITOR_CTRL_Y, BOT_W, BOT_H, (Color){3, 3, 3});

    if (g_edit_mode) {
        draw_text3x5(fb, BOT_W, BOT_H, 4, 213, "EDIT", (Color){120, 240, 120}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 28, 213, g_dirty ? "S*" : "S", g_slot, (Color){245, 245, 245}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 52, 213, "T", g_selected_tile, WALL_COLORS[g_selected_tile & 7], 1);
        if (g_editor_zoom_tiles > 0) {
            char zoom_buf[24];
            snprintf(zoom_buf, sizeof(zoom_buf), "Z%d X%d Y%d", g_editor_zoom_tiles, vx, vy);
            draw_text3x5(fb, BOT_W, BOT_H, 82, 213, zoom_buf, (Color){220, 230, 255}, 1);
        } else {
            draw_text3x5(fb, BOT_W, BOT_H, 82, 213, "FIT", (Color){220, 230, 255}, 1);
        }

        draw_tile_palette(fb);

        bool can_pan = g_editor_zoom_tiles > 0;
        draw_editor_button(fb, 184, 221, 203, 240, "-", true);
        draw_editor_button(fb, 206, 221, 225, 240, "+", true);
        draw_editor_button(fb, 228, 221, 253, 240, "FIT", true);
        draw_editor_button(fb, 258, 221, 276, 240, "L", can_pan);
        draw_editor_button(fb, 279, 212, 297, 226, "U", can_pan);
        draw_editor_button(fb, 279, 226, 297, 240, "D", can_pan);
        draw_editor_button(fb, 300, 221, 320, 240, "R", can_pan);
    } else {
        draw_text3x5(fb, BOT_W, BOT_H, 4, 214, "PLAY", (Color){120, 190, 255}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 30, 214, g_dirty ? "S*" : "S", g_slot, (Color){245, 245, 245}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 4, 224, "CPAD MOVE", (Color){205, 225, 255}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 96, 224, "LR/C LOOK", (Color){205, 225, 255}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 188, 224, "TOUCH SIDE", (Color){205, 225, 255}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 4, 232, "B SPRINT", (Color){230, 230, 210}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 96, 232, "A JUMP", (Color){230, 230, 210}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 188, 232, "START MENU", (Color){230, 230, 210}, 1);
    }
}

const char *menu_action_name(int action) {
    switch (action) {
        case MENU_ACTION_PLAY: return "PLAY";
        case MENU_ACTION_EDIT: return "EDIT";
        case MENU_ACTION_RESIZE: return "RESIZE";
        case MENU_ACTION_DUPLICATE: return "DUP";
        case MENU_ACTION_RENAME: return "NAME";
        case MENU_ACTION_DELETE: return "DELETE";
        case MENU_ACTION_SETTINGS: return "SETTINGS";
        default: return "?";
    }
}

static void draw_settings_row(u8 *fb, int row, const char *label, const char *value) {
    int y = 176 + row * 7;
    Color bg = (g_settings_cursor == row) ? (Color){50, 66, 96} : (Color){12, 14, 24};
    Color fg = (g_settings_cursor == row) ? (Color){255, 230, 150} : (Color){225, 225, 225};
    fill_rect_raw(fb, BOT_W, BOT_H, 8, y - 1, BOT_W - 8, y + 6, bg);
    draw_text3x5(fb, BOT_W, BOT_H, 14, y, label, fg, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 170, y, value, fg, 1);
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

    if (g_settings_menu) {
        char fov_buf[24];
        char depth_buf[24];
        char dof_start_buf[24];
        char dof_str_buf[24];
        snprintf(fov_buf, sizeof(fov_buf), "%d", (int)(g_fov_degrees + 0.5f));
        snprintf(depth_buf, sizeof(depth_buf), "%d PCT", (int)(g_level_depth * 100.0f + 0.5f));
        snprintf(dof_start_buf, sizeof(dof_start_buf), "%d", (int)(g_dof_start + 0.5f));
        snprintf(dof_str_buf, sizeof(dof_str_buf), "%d PCT", (int)(g_dof_strength * 100.0f + 0.5f));

        fill_rect_raw(fb, BOT_W, BOT_H, 0, 166, BOT_W, BOT_H, (Color){4, 4, 8});
        draw_text3x5(fb, BOT_W, BOT_H, 8, 166, "SETTINGS  D<> CHANGE  A TOGGLE  X RESET  B BACK", (Color){255,220,140}, 1);
        draw_settings_row(fb, 0, "FOV", fov_buf);
        draw_settings_row(fb, 1, "LEVEL DEPTH", depth_buf);
        draw_settings_row(fb, 2, "VIEW BOB", g_view_bob ? "ON" : "OFF");
        draw_settings_row(fb, 3, "3D SUPPORT", g_3d_enabled ? "ON" : "OFF");
        draw_settings_row(fb, 4, "DOF FADE", g_dof_enabled ? "ON" : "OFF");
        draw_settings_row(fb, 5, "DOF DIST", dof_start_buf);
        draw_settings_row(fb, 6, "DOF STR", dof_str_buf);
        draw_settings_row(fb, 7, "ANTI ALIAS", g_antialiasing ? "ON" : "OFF");
        draw_settings_row(fb, 8, "FAST RENDER", g_fast_render ? "ON" : "OFF");
    } else if (g_resize_menu) {
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

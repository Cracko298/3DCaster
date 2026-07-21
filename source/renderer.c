#include "common.h"

static Color floor_texture_color(void);
static const char *room_class_short_name(uint8_t room);

void draw_sky_floor(u8 *fb) {
    fill_rect_raw(fb, TOP_W, TOP_H, 0, 0, TOP_W, TOP_H / 2, (Color){60, 92, 145});
    fill_rect_raw(fb, TOP_W, TOP_H, 0, TOP_H / 2, TOP_W, TOP_H, floor_texture_color());
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

static Color texture_tint_color(uint8_t tex, Color base) {
    tex &= MAX_TEXTURE_ID;
    switch (tex) {
        case 1: return blend_color(base, (Color){255,255,255}, 0.18f);
        case 2: return blend_color(base, (Color){255,95,75}, 0.22f);
        case 3: return blend_color(base, (Color){85,175,255}, 0.22f);
        case 4: return blend_color(base, (Color){80,225,105}, 0.20f);
        case 5: return blend_color(base, (Color){255,225,90}, 0.24f);
        case 6: return blend_color(base, (Color){185,95,255}, 0.23f);
        case 7: return blend_color(base, (Color){95,235,235}, 0.22f);
        default: return base;
    }
}

static Color wall_texture_color(uint8_t tile, int x, int y) {
    Color base = WALL_COLORS[tile & 7];
    return texture_tint_color(wall_texture_at(&g_level, x, y), base);
}

static Color door_texture_color(int x, int y) {
    return texture_tint_color(door_texture_at(x, y), (Color){150, 100, 45});
}

static Color floor_texture_color(void) {
    return texture_tint_color(g_floor_textures[0] & MAX_TEXTURE_ID, (Color){45, 42, 38});
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




static bool project_world_point_to_render(float cam_x, float cam_y,
                                          float dir_x, float dir_y,
                                          float plane_x, float plane_y,
                                          float center_y,
                                          float wx, float wy, float wz,
                                          int *out_x, int *out_y, float *out_depth);

static float g_wall_pixel_zbuf[RENDER_W * RENDER_H];
static float g_sprite_pixel_zbuf[RENDER_W * RENDER_H];

static inline int rz_index(int x, int y) {
    return y * RENDER_W + x;
}

static void reset_render_pixel_zbufs(void) {
    for (int i = 0; i < RENDER_W * RENDER_H; i++) {
        g_wall_pixel_zbuf[i] = 1.0e30f;
        g_sprite_pixel_zbuf[i] = 1.0e30f;
    }
}

static void mark_wall_depth_rect(int x0, int x1, int y0, int y1, float depth) {
    if (x0 < 0) x0 = 0;
    if (x1 > RENDER_W) x1 = RENDER_W;
    if (y0 < 0) y0 = 0;
    if (y1 > RENDER_H) y1 = RENDER_H;
    if (x1 <= x0 || y1 <= y0) return;

    for (int y = y0; y < y1; y++) {
        int base = y * RENDER_W;
        for (int x = x0; x < x1; x++) {
            float *d = &g_wall_pixel_zbuf[base + x];
            if (depth < *d) *d = depth;
        }
    }
}

static bool sprite_pixel_can_draw(int sx, int sy, float depth) {
    if (sx < 0 || sy < 0 || sx >= RENDER_W || sy >= RENDER_H) return false;
    int idx = rz_index(sx, sy);

    /* Walls/platforms/doors only occlude pixels they actually draw.
       This prevents low platforms and transparent sprite space from hiding enemies. */
    if (depth > g_wall_pixel_zbuf[idx] + 0.02f) return false;

    /* Sprites occlude per-pixel, not per-column. This fixes coins/items turning their
       transparent area into an invisible wall for enemies and NPCs behind them. */
    if (depth > g_sprite_pixel_zbuf[idx] + 0.02f) return false;
    return true;
}

static void mark_sprite_pixel_depth(int sx, int sy, float depth) {
    if (sx < 0 || sy < 0 || sx >= RENDER_W || sy >= RENDER_H) return;
    int idx = rz_index(sx, sy);
    if (depth < g_sprite_pixel_zbuf[idx]) g_sprite_pixel_zbuf[idx] = depth;
}

static void mark_sprite_depth_rect_pixels(int x0, int y0, int x1, int y1, float depth) {
    /* Text labels are foreground UI attached to the entity, not part of the
       entity shadow. Mark their render-space pixels so later shadows cannot
       dither over the letters/background and look like the shadow is following
       the text box. */
    int rx0 = x0 / PIXEL_SCALE;
    int ry0 = y0 / PIXEL_SCALE;
    int rx1 = (x1 + PIXEL_SCALE - 1) / PIXEL_SCALE;
    int ry1 = (y1 + PIXEL_SCALE - 1) / PIXEL_SCALE;
    if (rx0 < 0) rx0 = 0;
    if (ry0 < 0) ry0 = 0;
    if (rx1 > RENDER_W) rx1 = RENDER_W;
    if (ry1 > RENDER_H) ry1 = RENDER_H;
    if (rx1 <= rx0 || ry1 <= ry0) return;
    for (int y = ry0; y < ry1; y++) {
        for (int x = rx0; x < rx1; x++) {
            mark_sprite_pixel_depth(x, y, depth);
        }
    }
}


static float current_render_depth(void) {
    return clampf32(g_level_depth, 0.50f, 2.00f);
}

static float current_camera_z_scaled(void) {
    return (g_level.player_z + EYE_HEIGHT) * current_render_depth();
}


static float sprite_base_z_at(float wx, float wy) {
    if (g_render_angle_override) return 0.0f;
    return ground_height_at(&g_level, wx, wy, PLATFORM_TOP);
}

static int project_world_z_to_render_y(float center_y, float transform_y, float world_z) {
    if (transform_y < 0.001f) transform_y = 0.001f;
    float proj = (float)RENDER_H / transform_y;
    float depth = current_render_depth();
    return (int)(center_y + (current_camera_z_scaled() - world_z * depth) * proj);
}

static bool world_text_visible_at(int sx, int sy, float depth) {
    if (sx < 0 || sy < 0 || sx >= RENDER_W || sy >= RENDER_H) return false;
    return depth <= g_wall_pixel_zbuf[rz_index(sx, sy)] + 0.05f;
}

static bool entity_in_front_and_near(float cam_x, float cam_y, float dir_x, float dir_y,
                                     float wx, float wy, float max_dist2) {
    float dx = wx - cam_x;
    float dy = wy - cam_y;
    float d2 = dx * dx + dy * dy;
    if (d2 > max_dist2) return false;
    float front = dx * dir_x + dy * dir_y;
    return front > 0.04f;
}


static float mask8_visible_width_ratio(const uint8_t *mask8) {
    if (!mask8) return 1.0f;
    int min_u = 8;
    int max_u = -1;
    for (int y = 0; y < 8; y++) {
        uint8_t row = mask8[y];
        for (int u = 0; u < 8; u++) {
            if (row & (uint8_t)(0x80u >> u)) {
                if (u < min_u) min_u = u;
                if (u > max_u) max_u = u;
            }
        }
    }
    if (max_u < min_u) return 1.0f;
    return clampf32(((float)(max_u - min_u + 1)) / 8.0f, 0.20f, 1.0f);
}

static float mask16_visible_width_ratio(const uint16_t *mask16) {
    if (!mask16) return 1.0f;
    int min_u = ENEMY_SPRITE_W;
    int max_u = -1;
    for (int y = 0; y < ENEMY_SPRITE_H; y++) {
        uint16_t row = mask16[y];
        for (int u = 0; u < ENEMY_SPRITE_W; u++) {
            if (row & (uint16_t)(1u << (ENEMY_SPRITE_W - 1 - u))) {
                if (u < min_u) min_u = u;
                if (u > max_u) max_u = u;
            }
        }
    }
    if (max_u < min_u) return 1.0f;
    return clampf32(((float)(max_u - min_u + 1)) / (float)ENEMY_SPRITE_W, 0.20f, 1.0f);
}

static float mask32_visible_width_ratio(const uint32_t *mask32) {
    if (!mask32) return 1.0f;
    int min_u = BOSS_SPRITE_W;
    int max_u = -1;
    for (int y = 0; y < BOSS_SPRITE_H; y++) {
        uint32_t row = mask32[y];
        for (int u = 0; u < BOSS_SPRITE_W; u++) {
            if (row & (uint32_t)(1u << (BOSS_SPRITE_W - 1 - u))) {
                if (u < min_u) min_u = u;
                if (u > max_u) max_u = u;
            }
        }
    }
    if (max_u < min_u) return 1.0f;
    return clampf32(((float)(max_u - min_u + 1)) / (float)BOSS_SPRITE_W, 0.20f, 1.0f);
}

static const uint8_t KEY_SPRITE_8X8[8] = {
    0x1C, 0x22, 0x22, 0x1C, 0x08, 0x0E, 0x08, 0x0C
};

static const uint8_t DOT_SPRITE_8X8[8] = {
    0x00, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x00
};

static const uint8_t HEART_SPRITE_8X8[8] = {
    0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00
};

static const uint8_t ARROW_SPRITE_8X8[8] = {
    0x10, 0x18, 0x1C, 0xFE, 0xFE, 0x1C, 0x18, 0x10
};

static const uint8_t ORB_SPRITE_8X8[8] = {
    0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0x7E, 0x3C, 0x00
};

static const uint8_t WAVE_SPRITE_8X8[8] = {
    0x18, 0x3C, 0x7E, 0xDB, 0xDB, 0x7E, 0x3C, 0x18
};

static int typed_visible_len(const char *text, uint8_t speed, float elapsed) {
    if (!text || !text[0]) return 0;
    int len = 0;
    while (text[len] && len < 180) len++;
    if (speed == TEXT_SPEED_INSTANT) return len;
    if (elapsed >= 99.0f) return len;
    float cps = 14.0f;
    if (speed == TEXT_SPEED_SLOW) cps = 7.0f;
    else if (speed == TEXT_SPEED_MEDIUM) cps = 14.0f;
    else if (speed == TEXT_SPEED_FAST) cps = 28.0f;
    int shown = (int)(elapsed * cps) + 1;
    if (shown < 1) shown = 1;
    if (shown > len) shown = len;
    return shown;
}

static void make_typed_text(char *out, size_t out_size, const char *text, uint8_t speed, float elapsed) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;
    int n = typed_visible_len(text, speed, elapsed);
    if (n >= (int)out_size) n = (int)out_size - 1;
    memcpy(out, text, (size_t)n);
    out[n] = '\0';
}

static int wrap_world_text(const char *text, char lines[6][25], int *widest) {
    if (widest) *widest = 0;
    for (int i = 0; i < 6; i++) lines[i][0] = '\0';
    if (!text || !text[0]) return 0;

    int line = 0;
    int pos = 0;
    int last_space = -1;
    int src_line_start = 0;
    char cur[25];
    cur[0] = '\0';

    for (int i = 0; text[i] && line < 6; i++) {
        char ch = text[i];
        if (ch == '|') ch = '\n';
        if (ch == '\r') continue;

        if (ch == '\n') {
            cur[pos] = '\0';
            snprintf(lines[line], 25, "%s", cur);
            if (widest && pos > *widest) *widest = pos;
            line++;
            pos = 0;
            last_space = -1;
            src_line_start = i + 1;
            cur[0] = '\0';
            continue;
        }

        if (pos < 24) {
            cur[pos++] = ch;
            cur[pos] = '\0';
            if (ch == ' ') last_space = pos - 1;
            continue;
        }

        int break_pos = (last_space >= 8) ? last_space : 24;
        char out[25];
        memcpy(out, cur, (size_t)break_pos);
        out[break_pos] = '\0';
        while (break_pos > 0 && out[break_pos - 1] == ' ') out[--break_pos] = '\0';
        snprintf(lines[line], 25, "%s", out);
        if (widest && break_pos > *widest) *widest = break_pos;
        line++;
        if (line >= 4) break;

        int carry_start = (last_space >= 8) ? (last_space + 1) : 24;
        int carry_len = pos - carry_start;
        if (carry_len < 0) carry_len = 0;
        if (carry_len > 23) carry_len = 23;
        memmove(cur, cur + carry_start, (size_t)carry_len);
        pos = carry_len;
        cur[pos++] = ch;
        if (pos > 24) pos = 24;
        cur[pos] = '\0';
        last_space = -1;
        for (int k = 0; k < pos; k++) if (cur[k] == ' ') last_space = k;
        src_line_start = i;
        (void)src_line_start;
    }

    if (line < 6 && pos > 0) {
        cur[pos] = '\0';
        snprintf(lines[line], 25, "%s", cur);
        if (widest && pos > *widest) *widest = pos;
        line++;
    }

    return line;
}

static void draw_world_text_label(u8 *fb,
                                  float cam_x, float cam_y,
                                  float dir_x, float dir_y,
                                  float plane_x, float plane_y,
                                  float center_y,
                                  const float *zbuf,
                                  float wx, float wy,
                                  float height_scale,
                                  const char *text,
                                  Color fg,
                                  Color bg) {
    if (!text || !text[0]) return;
    float rx = wx - cam_x;
    float ry = wy - cam_y;
    float det = plane_x * dir_y - dir_x * plane_y;
    if (fabsf(det) < 0.000001f) return;

    float inv_det = 1.0f / det;
    float transform_x = inv_det * (dir_y * rx - dir_x * ry);
    float transform_y = inv_det * (-plane_y * rx + plane_x * ry);
    if (transform_y <= 0.08f) return;

    int sx = (int)((RENDER_W * 0.5f) * (1.0f + transform_x / transform_y));
    if (sx < 0 || sx >= RENDER_W) return;

    int sprite_h = (int)fabsf((RENDER_H / transform_y) * height_scale * clampf32(g_level_depth, 0.50f, 2.00f));
    if (sprite_h < 8) sprite_h = 8;
    if (sprite_h > RENDER_H * 2) sprite_h = RENDER_H * 2;

    char lines[6][25];
    int widest = 0;
    int line_count = wrap_world_text(text, lines, &widest);
    if (line_count <= 0 || widest <= 0) return;

    float base_z = sprite_base_z_at(wx, wy);
    int label_y = project_world_z_to_render_y(center_y, transform_y, base_z) - sprite_h;
    int px = sx * PIXEL_SCALE;
    int py = label_y * PIXEL_SCALE - 4;
    int text_ry = py / PIXEL_SCALE;
    if (!world_text_visible_at(sx, text_ry, transform_y)) return;

    int tw = widest * 4;
    int th = line_count * 7;
    int x0 = px - tw / 2 - 3;
    int y0 = py - th - 2;
    if (x0 < 0) x0 = 0;
    if (x0 + tw + 6 > TOP_W) x0 = TOP_W - tw - 6;
    if (y0 < 2) y0 = 2;
    if (y0 > TOP_H - th - 4) y0 = TOP_H - th - 4;

    fill_rect_raw(fb, TOP_W, TOP_H, x0, y0, x0 + tw + 6, y0 + th + 4, bg);
    for (int li = 0; li < line_count; li++) {
        draw_text3x5(fb, TOP_W, TOP_H, x0 + 3, y0 + 2 + li * 7, lines[li], fg, 1);
    }
    mark_sprite_depth_rect_pixels(x0, y0, x0 + tw + 6, y0 + th + 4, transform_y);
    (void)zbuf;
}

static void draw_world_text_label_typed(u8 *fb,
                                        float cam_x, float cam_y,
                                        float dir_x, float dir_y,
                                        float plane_x, float plane_y,
                                        float center_y,
                                        const float *zbuf,
                                        float wx, float wy,
                                        float height_scale,
                                        const char *text,
                                        uint8_t speed,
                                        float elapsed,
                                        Color fg,
                                        Color bg) {
    char tmp[192];
    make_typed_text(tmp, sizeof(tmp), text, speed, elapsed);
    draw_world_text_label(fb, cam_x, cam_y, dir_x, dir_y, plane_x, plane_y,
                          center_y, zbuf, wx, wy, height_scale, tmp, fg, bg);
}


static const uint8_t WEAPON_SPRITE_8X8[8] = {
    0x10, 0x38, 0x10, 0x10, 0x10, 0x54, 0x38, 0x10
};

static Color npc_color_for(uint8_t id) {
    static const Color colors[8] = {
        {80, 210, 255}, {255, 120, 180}, {120, 255, 120}, {255, 220, 90},
        {170, 120, 255}, {255, 120, 80}, {120, 220, 210}, {235, 235, 235}
    };
    return colors[id & 7];
}

static void draw_billboard_masked_sprite(u8 *fb,
                                         float cam_x, float cam_y,
                                         float dir_x, float dir_y,
                                         float plane_x, float plane_y,
                                         float center_y,
                                         float *zbuf,
                                         float sprite_x, float sprite_y,
                                         float height_scale,
                                         const uint8_t *mask8,
                                         Color base) {
    float rx = sprite_x - cam_x;
    float ry = sprite_y - cam_y;
    float det = plane_x * dir_y - dir_x * plane_y;
    if (fabsf(det) < 0.000001f) return;

    float inv_det = 1.0f / det;
    float transform_x = inv_det * (dir_y * rx - dir_x * ry);
    float transform_y = inv_det * (-plane_y * rx + plane_x * ry);
    if (transform_y <= 0.08f) return;

    int screen_x = (int)((RENDER_W * 0.5f) * (1.0f + transform_x / transform_y));
    int sprite_h = (int)fabsf((RENDER_H / transform_y) * height_scale * clampf32(g_level_depth, 0.50f, 2.00f));
    if (sprite_h < 5) sprite_h = 5;
    if (sprite_h > 56) sprite_h = 56;

    int sprite_w = sprite_h;
    float base_z = sprite_base_z_at(sprite_x, sprite_y);
    int draw_y1 = project_world_z_to_render_y(center_y, transform_y, base_z);
    int draw_y0 = draw_y1 - sprite_h;
    int draw_x0 = screen_x - sprite_w / 2;
    int draw_x1 = screen_x + sprite_w / 2;

    if (draw_y0 < 0) draw_y0 = 0;
    if (draw_y1 >= RENDER_H) draw_y1 = RENDER_H - 1;
    if (draw_x0 < 0) draw_x0 = 0;
    if (draw_x1 >= RENDER_W) draw_x1 = RENDER_W - 1;
    if (draw_x1 <= draw_x0 || draw_y1 <= draw_y0) return;

    float shade = 1.0f / (1.0f + transform_y * 0.08f);
    Color c = apply_dof_fade(shade_color(base, shade), transform_y);
    Color shine = blend_color(c, (Color){255, 255, 255}, 0.40f);

    int w = draw_x1 - draw_x0 + 1;
    int h = draw_y1 - draw_y0 + 1;
    for (int sy = draw_y0; sy <= draw_y1; sy++) {
        int v = ((sy - draw_y0) * 8) / h;
        if (v < 0) v = 0;
        if (v > 7) v = 7;

        for (int sx = draw_x0; sx <= draw_x1; sx++) {
            int u = ((sx - draw_x0) * 8) / w;
            if (u < 0) u = 0;
            if (u > 7) u = 7;
            if (!(mask8[v] & (uint8_t)(0x80u >> u))) continue;
            if (!sprite_pixel_can_draw(sx, sy, transform_y)) continue;

            Color out = (v < 3 && u < 5) ? shine : c;
            int px0 = sx * PIXEL_SCALE;
            int px1 = px0 + PIXEL_SCALE;
            int py0 = sy * PIXEL_SCALE;
            int py1 = py0 + PIXEL_SCALE;
            fill_rect_raw(fb, TOP_W, TOP_H, px0, py0, px1, py1, out);
            mark_sprite_pixel_depth(sx, sy, transform_y);
        }
    }
    (void)zbuf;
}


static void draw_ground_shadow(u8 *fb,
                               float cam_x, float cam_y,
                               float dir_x, float dir_y,
                               float plane_x, float plane_y,
                               float center_y,
                               float wx, float wy,
                               float world_z,
                               float sprite_height_scale,
                               float sprite_width_ratio) {
    int sx, sy;
    float depth;
    /* Project the shadow as a floor decal at the entity base, not as a
       screen-space blob attached to the text/sprite rectangle. */
    if (!project_world_point_to_render(cam_x, cam_y, dir_x, dir_y, plane_x, plane_y,
                                       center_y, wx, wy, world_z + 0.006f, &sx, &sy, &depth)) return;
    if (sx < -40 || sx >= RENDER_W + 40 || sy < -20 || sy >= RENDER_H + 24) return;

    if (sprite_height_scale <= 0.0f) sprite_height_scale = 0.25f;
    if (sprite_width_ratio <= 0.0f) sprite_width_ratio = 1.0f;

    int projected_h = (int)fabsf((RENDER_H / depth) * sprite_height_scale * clampf32(g_level_depth, 0.50f, 2.00f));
    if (projected_h < 2) projected_h = 2;
    if (projected_h > 72) projected_h = 72;

    /* Width is based on the visible opaque sprite columns supplied by the
       caller, then enlarged slightly so it reads as a soft contact shadow. */
    int visible_w = (int)((float)projected_h * sprite_width_ratio + 0.5f);
    int diameter = (int)((float)visible_w * 1.28f + 0.5f);
    if (diameter < 4) diameter = 4;
    if (diameter > 60) diameter = 60;

    int rx_max = diameter / 2;
    int ry_max = rx_max / 3;
    if (ry_max < 1) ry_max = 1;
    if (ry_max > 9) ry_max = 9;

    /* Put the decal slightly below the foot point so it feels stuck to the
       floor rather than hovering in the middle of the sprite. */
    int center_sy = sy + (ry_max / 2);
    Color dark = {10, 8, 7};
    Color soft = {6, 5, 5};

    for (int yy = -ry_max; yy <= ry_max; yy++) {
        int py = center_sy + yy;
        if (py < 0 || py >= RENDER_H) continue;
        float ny = (float)yy / (float)ry_max;
        for (int xx = -rx_max; xx <= rx_max; xx++) {
            int px = sx + xx;
            if (px < 0 || px >= RENDER_W) continue;
            float nx = (float)xx / (float)rx_max;
            float ed = nx * nx + ny * ny;
            if (ed > 1.0f) continue;

            int idx = rz_index(px, py);
            /* Decal rule: never draw onto pixels already occupied by walls,
               platforms, doors, text, or rendered sprite pixels.  This keeps
               shadows on the visible floor only. */
            if (g_wall_pixel_zbuf[idx] < 1.0e29f) continue;
            if (g_sprite_pixel_zbuf[idx] < 1.0e29f) continue;

            /* Transparent/dithered: dense at contact center, sparse near edge. */
            if (ed > 0.25f && (((px + py) & 1) != 0)) continue;
            if (ed > 0.62f && (((px ^ (py * 3)) & 3) != 0)) continue;

            Color sh = (ed < 0.28f) ? dark : soft;
            fill_rect_raw(fb, TOP_W, TOP_H, px * PIXEL_SCALE, py * PIXEL_SCALE,
                          px * PIXEL_SCALE + PIXEL_SCALE, py * PIXEL_SCALE + PIXEL_SCALE, sh);
        }
    }
}


static void draw_billboard_masked_sprite16_centered_z(u8 *fb,
                                                       float cam_x, float cam_y,
                                                       float dir_x, float dir_y,
                                                       float plane_x, float plane_y,
                                                       float center_y,
                                                       float *zbuf,
                                                       float wx, float wy, float wz,
                                                       float scale,
                                                       const uint16_t *mask16,
                                                       Color color) {
    (void)zbuf;
    if (!mask16) return;
    float rx = wx - cam_x;
    float ry = wy - cam_y;
    float det = plane_x * dir_y - dir_x * plane_y;
    if (fabsf(det) < 0.000001f) return;
    float inv_det = 1.0f / det;
    float transform_x = inv_det * (dir_y * rx - dir_x * ry);
    float transform_y = inv_det * (-plane_y * rx + plane_x * ry);
    if (transform_y <= 0.08f) return;
    int screen_x = (int)((RENDER_W * 0.5f) * (1.0f + transform_x / transform_y));
    int center_screen_y = project_world_z_to_render_y(center_y, transform_y, wz);
    int sprite_h = (int)fabsf((RENDER_H / transform_y) * scale * clampf32(g_level_depth, 0.50f, 2.00f));
    if (sprite_h < 6) sprite_h = 6;
    if (sprite_h > 64) sprite_h = 64;
    int sprite_w = (int)((float)sprite_h * 0.54f);
    if (sprite_w < 6) sprite_w = 6;
    int draw_y0 = center_screen_y - sprite_h / 2;
    int draw_y1 = center_screen_y + sprite_h / 2;
    int draw_x0 = screen_x - sprite_w / 2;
    int draw_x1 = screen_x + sprite_w / 2;
    if (draw_y0 < 0) draw_y0 = 0;
    if (draw_y1 >= RENDER_H) draw_y1 = RENDER_H - 1;
    if (draw_x0 < 0) draw_x0 = 0;
    if (draw_x1 >= RENDER_W) draw_x1 = RENDER_W - 1;
    if (draw_x1 <= draw_x0 || draw_y1 <= draw_y0) return;
    float shade = 1.0f / (1.0f + transform_y * 0.10f);
    Color c = apply_dof_fade(shade_color(color, shade), transform_y);
    Color edge = blend_color(c, (Color){255,255,255}, 0.22f);
    int w = draw_x1 - draw_x0 + 1;
    int h = draw_y1 - draw_y0 + 1;
    for (int sy = draw_y0; sy <= draw_y1; sy++) {
        int v = ((sy - draw_y0) * ENEMY_SPRITE_H) / h;
        if (v < 0) v = 0;
        if (v >= ENEMY_SPRITE_H) v = ENEMY_SPRITE_H - 1;
        for (int sx = draw_x0; sx <= draw_x1; sx++) {
            int u = ((sx - draw_x0) * ENEMY_SPRITE_W) / w;
            if (u < 0) u = 0;
            if (u >= ENEMY_SPRITE_W) u = ENEMY_SPRITE_W - 1;
            if ((mask16[v] & (uint16_t)(1u << (ENEMY_SPRITE_W - 1 - u))) == 0) continue;
            if (!sprite_pixel_can_draw(sx, sy, transform_y)) continue;
            Color pc = (u == 0 || u == ENEMY_SPRITE_W - 1 || v == 0) ? edge : c;
            fill_rect_raw(fb, TOP_W, TOP_H, sx * PIXEL_SCALE, sy * PIXEL_SCALE, sx * PIXEL_SCALE + PIXEL_SCALE, sy * PIXEL_SCALE + PIXEL_SCALE, pc);
            mark_sprite_pixel_depth(sx, sy, transform_y);
        }
    }
}

static void draw_enemy_sprite_with_hp(u8 *fb,
                                      float cam_x, float cam_y,
                                      float dir_x, float dir_y,
                                      float plane_x, float plane_y,
                                      float center_y,
                                      float *zbuf,
                                      const Enemy *e,
                                      Color base) {
    if (!e || !e->active) return;
    float rx = e->x - cam_x;
    float ry = e->y - cam_y;
    float det = plane_x * dir_y - dir_x * plane_y;
    if (fabsf(det) < 0.000001f) return;
    float inv_det = 1.0f / det;
    float transform_x = inv_det * (dir_y * rx - dir_x * ry);
    float transform_y = inv_det * (-plane_y * rx + plane_x * ry);
    if (transform_y <= 0.08f) return;
    int screen_x = (int)((RENDER_W * 0.5f) * (1.0f + transform_x / transform_y));
    float hit_t = clampf32(e->hit_timer / 0.28f, 0.0f, 1.0f);
    float death_t = e->dying ? clampf32(1.0f - (e->death_timer / 0.48f), 0.0f, 1.0f) : 0.0f;
    if (hit_t > 0.0f) screen_x += (int)(sinf(hit_t * 32.0f) * 4.0f);
    bool boss_sprite = (e->ai_rank == AI_RANK_BOSS);
    const uint16_t *anim16 = boss_sprite ? NULL : enemy_anim_frame_for(e);
    int mask_w = boss_sprite ? BOSS_SPRITE_W : ENEMY_SPRITE_W;
    int mask_h = boss_sprite ? BOSS_SPRITE_H : ENEMY_SPRITE_H;
    int size_pct = e->size_pct ? e->size_pct : (boss_sprite ? 118 : 100);
    if (!boss_sprite && size_pct > 115) size_pct = 115;
    if (boss_sprite && size_pct < 108) size_pct = 108;
    float hscale = (boss_sprite ? 1.12f : 0.92f) * ((float)size_pct / 100.0f);
    float wscale = 1.0f;
    if (e->dying) { hscale *= (1.0f - death_t * 0.82f); wscale += death_t * 1.10f; }
    else if (hit_t > 0.0f) { hscale *= (1.0f - hit_t * 0.16f); wscale += hit_t * 0.26f; }
    int sprite_h = (int)fabsf((RENDER_H / transform_y) * hscale * clampf32(g_level_depth, 0.50f, 2.00f));
    if (sprite_h < 4) sprite_h = 4;
    if (sprite_h > (boss_sprite ? 82 : 68)) sprite_h = boss_sprite ? 82 : 68;
    int sprite_w = (int)((float)sprite_h * (boss_sprite ? 0.70f : 0.54f) * wscale);
    if (sprite_w < (boss_sprite ? 10 : 5)) sprite_w = boss_sprite ? 10 : 5;
    float base_z = sprite_base_z_at(e->x, e->y);
    int draw_y1 = project_world_z_to_render_y(center_y, transform_y, base_z);
    int draw_y0 = draw_y1 - sprite_h;
    int draw_x0 = screen_x - sprite_w / 2;
    int draw_x1 = screen_x + sprite_w / 2;
    if (draw_y0 < 0) draw_y0 = 0;
    if (draw_y1 >= RENDER_H) draw_y1 = RENDER_H - 1;
    if (draw_x0 < 0) draw_x0 = 0;
    if (draw_x1 >= RENDER_W) draw_x1 = RENDER_W - 1;
    if (draw_x1 <= draw_x0 || draw_y1 <= draw_y0) return;
    float shade = 1.0f / (1.0f + transform_y * 0.10f);
    Color c = apply_dof_fade(shade_color(base, shade), transform_y);
    Color edge = blend_color(c, (Color){255, 255, 255}, 0.25f);
    int w = draw_x1 - draw_x0 + 1;
    int h = draw_y1 - draw_y0 + 1;
    for (int sy = draw_y0; sy <= draw_y1; sy++) {
        int v = ((sy - draw_y0) * mask_h) / h;
        if (v < 0) v = 0;
        if (v >= mask_h) v = mask_h - 1;
        for (int sx = draw_x0; sx <= draw_x1; sx++) {
            int u = ((sx - draw_x0) * mask_w) / w;
            if (u < 0) u = 0;
            if (u >= mask_w) u = mask_w - 1;
            bool on = boss_sprite ? ((e->boss_sprite[v] & (uint32_t)(1u << (BOSS_SPRITE_W - 1 - u))) != 0)
                                  : (((anim16 ? anim16[v] : e->sprite16[v]) & (uint16_t)(1u << (ENEMY_SPRITE_W - 1 - u))) != 0);
            if (!on) continue;
            if (!sprite_pixel_can_draw(sx, sy, transform_y)) continue;
            Color pc = c;
            if (e->dying) pc = blend_color(pc, (Color){35,20,18}, death_t * 0.65f);
            if (u == 0 || u == mask_w - 1 || v == 0) pc = edge;
            fill_rect_raw(fb, TOP_W, TOP_H, sx * PIXEL_SCALE, sy * PIXEL_SCALE, sx * PIXEL_SCALE + PIXEL_SCALE, sy * PIXEL_SCALE + PIXEL_SCALE, pc);
            mark_sprite_pixel_depth(sx, sy, transform_y);
        }
    }
    float ddx = e->x - g_level.player_x;
    float ddy = e->y - g_level.player_y;
    if ((ddx * ddx + ddy * ddy) <= ENEMY_HP_BAR_DIST2 && world_text_visible_at(screen_x, draw_y0, transform_y)) {
        int hp_max = e->hp_max > 0 ? e->hp_max : 7;
        int hp = e->hp;
        if (hp < 0) hp = 0;
        if (hp > hp_max) hp = hp_max;
        int bx0 = draw_x0 * PIXEL_SCALE;
        int bx1 = (draw_x1 + 1) * PIXEL_SCALE;
        int by0 = draw_y0 * PIXEL_SCALE - 5;
        if (by0 < 2) by0 = 2;
        fill_rect_raw(fb, TOP_W, TOP_H, bx0, by0, bx1, by0 + 3, (Color){35,10,10});
        int fill_w = ((bx1 - bx0) * hp) / hp_max;
        fill_rect_raw(fb, TOP_W, TOP_H, bx0, by0, bx0 + fill_w, by0 + 3, (Color){90,255,90});
    }
    (void)zbuf;
}

static void draw_slash_point(u8 *fb, int x, int y, Color core, Color glow, int thick) {
    if (thick > 1) {
        fill_rect_raw(fb, TOP_W, TOP_H, x - thick, y - thick, x + thick + 1, y + thick + 1, glow);
    }
    fill_rect_raw(fb, TOP_W, TOP_H, x, y, x + 2, y + 2, core);
}


static bool project_world_point_to_render(float cam_x, float cam_y,
                                          float dir_x, float dir_y,
                                          float plane_x, float plane_y,
                                          float center_y,
                                          float wx, float wy, float wz,
                                          int *out_x, int *out_y, float *out_depth) {
    float rx = wx - cam_x;
    float ry = wy - cam_y;
    float det = plane_x * dir_y - dir_x * plane_y;
    if (fabsf(det) < 0.000001f) return false;

    float inv_det = 1.0f / det;
    float transform_x = inv_det * (dir_y * rx - dir_x * ry);
    float transform_y = inv_det * (-plane_y * rx + plane_x * ry);
    if (transform_y <= 0.08f) return false;

    int sx = (int)((RENDER_W * 0.5f) * (1.0f + transform_x / transform_y));
    int sy = project_world_z_to_render_y(center_y, transform_y, wz);
    if (out_x) *out_x = sx;
    if (out_y) *out_y = sy;
    if (out_depth) *out_depth = transform_y;
    return true;
}

static void draw_line_raw(u8 *fb, int x0, int y0, int x1, int y1, Color c) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        fill_rect_raw(fb, TOP_W, TOP_H, x0 - 1, y0 - 1, x0 + 2, y0 + 2, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_success_floor_marker(u8 *fb,
                                      float cam_x, float cam_y,
                                      float dir_x, float dir_y,
                                      float plane_x, float plane_y,
                                      float center_y,
                                      float sx_world, float sy_world) {
    float z = sprite_base_z_at(sx_world, sy_world) + 0.025f;
    float x0 = floorf(sx_world);
    float y0 = floorf(sy_world);
    float x1 = x0 + 1.0f;
    float y1 = y0 + 1.0f;

    int px[4], py[4];
    float d[4];
    if (!project_world_point_to_render(cam_x, cam_y, dir_x, dir_y, plane_x, plane_y, center_y, x0, y0, z, &px[0], &py[0], &d[0])) return;
    if (!project_world_point_to_render(cam_x, cam_y, dir_x, dir_y, plane_x, plane_y, center_y, x1, y0, z, &px[1], &py[1], &d[1])) return;
    if (!project_world_point_to_render(cam_x, cam_y, dir_x, dir_y, plane_x, plane_y, center_y, x1, y1, z, &px[2], &py[2], &d[2])) return;
    if (!project_world_point_to_render(cam_x, cam_y, dir_x, dir_y, plane_x, plane_y, center_y, x0, y1, z, &px[3], &py[3], &d[3])) return;

    float center_depth = 0.0f;
    int cxr = 0, cyr = 0;
    if (!project_world_point_to_render(cam_x, cam_y, dir_x, dir_y, plane_x, plane_y, center_y, sx_world, sy_world, z, &cxr, &cyr, &center_depth)) return;
    if (cxr < 0 || cxr >= RENDER_W || cyr < 0 || cyr >= RENDER_H) return;
    if (!world_text_visible_at(cxr, cyr, center_depth)) return;

    for (int i = 0; i < 4; i++) {
        px[i] *= PIXEL_SCALE;
        py[i] *= PIXEL_SCALE;
    }

    Color glow = (Color){20, 95, 35};
    Color edge = (Color){95, 255, 115};
    draw_line_raw(fb, px[0], py[0], px[1], py[1], edge);
    draw_line_raw(fb, px[1], py[1], px[2], py[2], edge);
    draw_line_raw(fb, px[2], py[2], px[3], py[3], edge);
    draw_line_raw(fb, px[3], py[3], px[0], py[0], edge);
    draw_line_raw(fb, px[0], py[0], px[2], py[2], glow);
    draw_line_raw(fb, px[1], py[1], px[3], py[3], glow);
}

static void draw_billboard_masked_sprite_centered_z(u8 *fb,
                                         float cam_x, float cam_y,
                                         float dir_x, float dir_y,
                                         float plane_x, float plane_y,
                                         float center_y,
                                         float *zbuf,
                                         float sprite_x, float sprite_y,
                                         float world_center_z,
                                         float height_scale,
                                         const uint8_t *mask8,
                                         Color base) {
    float rx = sprite_x - cam_x;
    float ry = sprite_y - cam_y;
    float det = plane_x * dir_y - dir_x * plane_y;
    if (fabsf(det) < 0.000001f) return;

    float inv_det = 1.0f / det;
    float transform_x = inv_det * (dir_y * rx - dir_x * ry);
    float transform_y = inv_det * (-plane_y * rx + plane_x * ry);
    if (transform_y <= 0.08f) return;

    int screen_x = (int)((RENDER_W * 0.5f) * (1.0f + transform_x / transform_y));
    int sprite_h = (int)fabsf((RENDER_H / transform_y) * height_scale * clampf32(g_level_depth, 0.50f, 2.00f));
    if (sprite_h < 5) sprite_h = 5;
    if (sprite_h > 48) sprite_h = 48;

    int sprite_w = sprite_h;
    int center_screen_y = project_world_z_to_render_y(center_y, transform_y, world_center_z);
    int draw_y0 = center_screen_y - sprite_h / 2;
    int draw_y1 = center_screen_y + sprite_h / 2;
    int draw_x0 = screen_x - sprite_w / 2;
    int draw_x1 = screen_x + sprite_w / 2;

    if (draw_y0 < 0) draw_y0 = 0;
    if (draw_y1 >= RENDER_H) draw_y1 = RENDER_H - 1;
    if (draw_x0 < 0) draw_x0 = 0;
    if (draw_x1 >= RENDER_W) draw_x1 = RENDER_W - 1;
    if (draw_x1 <= draw_x0 || draw_y1 <= draw_y0) return;

    float shade = 1.0f / (1.0f + transform_y * 0.08f);
    Color c = apply_dof_fade(shade_color(base, shade), transform_y);
    Color shine = blend_color(c, (Color){255, 255, 255}, 0.40f);

    int w = draw_x1 - draw_x0 + 1;
    int h = draw_y1 - draw_y0 + 1;
    for (int sy = draw_y0; sy <= draw_y1; sy++) {
        int v = ((sy - draw_y0) * 8) / h;
        if (v < 0) v = 0;
        if (v > 7) v = 7;
        for (int sx = draw_x0; sx <= draw_x1; sx++) {
            int u = ((sx - draw_x0) * 8) / w;
            if (u < 0) u = 0;
            if (u > 7) u = 7;
            if (!(mask8[v] & (uint8_t)(0x80u >> u))) continue;
            if (!sprite_pixel_can_draw(sx, sy, transform_y)) continue;

            Color out = (v < 3 && u < 5) ? shine : c;
            int px0 = sx * PIXEL_SCALE;
            int px1 = px0 + PIXEL_SCALE;
            int py0 = sy * PIXEL_SCALE;
            int py1 = py0 + PIXEL_SCALE;
            fill_rect_raw(fb, TOP_W, TOP_H, px0, py0, px1, py1, out);
            mark_sprite_pixel_depth(sx, sy, transform_y);
        }
    }
    (void)zbuf;
}

static bool draw_raycast_column_slice(u8 *fb,
                                      int x, int column_step,
                                      float perp, uint8_t hit_tile, int side_hit,
                                      int map_x, int map_y,
                                      float ray_x, float ray_y,
                                      float pos_x, float pos_y,
                                      float cam_z, float center_y,
                                      float depth,
                                      int *prev_sy0, int *prev_sy1, Color *prev_c,
                                      float *zbuf) {
    if (!hit_tile || perp < 0.001f) return false;

    float z0 = 0.0f;
    float z1 = 1.0f;
    if (hit_tile == PLATFORM_TILE) {
        z0 = PLATFORM_BOTTOM;
        z1 = PLATFORM_TOP;
    } else if (hit_tile == TILE_DOOR) {
        float open = door_open_fraction_at(map_x, map_y);
        DoorMeta *dm = door_meta_find_at(map_x, map_y);
        uint8_t move = dm ? dm->move_dir : DOOR_MOVE_UP;
        float slide = open * 1.12f;
        if (move == DOOR_MOVE_DOWN) {
            z0 = -slide;
            z1 = 1.0f - slide;
            if (z1 <= -0.05f) return false;
        } else if (move == DOOR_MOVE_UP) {
            z0 = slide;
            z1 = 1.0f + slide;
            if (z0 >= 1.05f) return false;
        } else {
            float wall_x_test = (side_hit == 0) ? (pos_y + perp * ray_y) : (pos_x + perp * ray_x);
            wall_x_test -= floorf(wall_x_test);
            if (move == DOOR_MOVE_RIGHT && wall_x_test < open) return false;
            if (move == DOOR_MOVE_LEFT && wall_x_test > 1.0f - open) return false;
        }
    }
    z0 *= depth;
    z1 *= depth;

    float proj = (float)RENDER_H / perp;
    int draw_start = (int)(center_y + (cam_z - z1) * proj);
    int draw_end = (int)(center_y + (cam_z - z0) * proj);

    if (draw_start < 0) draw_start = 0;
    if (draw_end >= RENDER_H) draw_end = RENDER_H - 1;
    if (draw_end <= draw_start) return false;

    Color base = (hit_tile == TILE_DOOR) ? door_texture_color(map_x, map_y) : wall_texture_color(hit_tile, map_x, map_y);
    float shade = 1.0f / (1.0f + perp * 0.12f);
    if (side_hit == 1) shade *= 0.72f;
    if (hit_tile == PLATFORM_TILE) shade *= 0.95f;
    if (hit_tile == TILE_DOOR) shade *= 1.08f;
    Color c = apply_dof_fade(shade_color(base, shade), perp);

    for (int zi = x; zi < x + column_step && zi < RENDER_W; zi++) {
        if (perp < zbuf[zi]) zbuf[zi] = perp;
    }
    mark_wall_depth_rect(x, x + column_step, draw_start, draw_end + 1, perp);

    int sx0 = x * PIXEL_SCALE;
    int sx1 = sx0 + PIXEL_SCALE * column_step;
    if (sx1 > TOP_W) sx1 = TOP_W;
    int sy0 = draw_start * PIXEL_SCALE;
    int sy1 = (draw_end + 1) * PIXEL_SCALE;
    fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy1, c);

    if (hit_tile != TILE_DOOR && hit_tile != PLATFORM_TILE) {
        uint8_t tex = wall_texture_at(&g_level, map_x, map_y);
        if (tex) {
            Color line = apply_dof_fade(shade_color(blend_color(c, (Color){0,0,0}, 0.35f), shade), perp);
            if ((tex & 1) != 0 && sy1 - sy0 > 20) {
                int ymid = sy0 + (sy1 - sy0) / 2;
                fill_rect_raw(fb, TOP_W, TOP_H, sx0, ymid, sx1, ymid + 1, line);
            }
            if ((tex & 2) != 0) {
                fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx0 + 1, sy1, line);
            }
            if ((tex & 4) != 0 && sy1 - sy0 > 12) {
                int yq = sy0 + (sy1 - sy0) / 4;
                fill_rect_raw(fb, TOP_W, TOP_H, sx0, yq, sx1, yq + 1, line);
            }
        }
    }

    if (hit_tile == TILE_DOOR) {
        float wall_x = (side_hit == 0) ? (pos_y + perp * ray_y) : (pos_x + perp * ray_x);
        wall_x -= floorf(wall_x);
        int u = (int)(wall_x * 16.0f);
        Color dark = apply_dof_fade(shade_color((Color){75, 42, 22}, shade), perp);
        Color light = apply_dof_fade(shade_color((Color){205, 150, 78}, shade), perp);
        if (u <= 1 || u >= 14 || u == 7 || u == 8) {
            fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy1, dark);
        } else if (u == 3 || u == 12) {
            fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy1, light);
        }
        int h = sy1 - sy0;
        if (h > 18) {
            int r1 = sy0 + h / 3;
            int r2 = sy0 + (h * 2) / 3;
            fill_rect_raw(fb, TOP_W, TOP_H, sx0, r1, sx1, r1 + 2, dark);
            fill_rect_raw(fb, TOP_W, TOP_H, sx0, r2, sx1, r2 + 2, dark);
        }
    }

    draw_column_aa(fb, sx0, sx1, sy0, sy1, c, *prev_sy0, *prev_sy1, *prev_c);

    if (hit_tile == PLATFORM_TILE) {
        Color edge = c;
        edge.r = (edge.r > 215) ? 255 : edge.r + 40;
        edge.g = (edge.g > 215) ? 255 : edge.g + 40;
        edge.b = (edge.b > 215) ? 255 : edge.b + 40;
        fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy0 + PIXEL_SCALE, edge);
    }

    *prev_sy0 = sy0;
    *prev_sy1 = sy1;
    *prev_c = c;
    return true;
}

static void draw_crosshair_and_slash(u8 *fb) {
    int cx = TOP_W / 2;
    int cy = TOP_H / 2;

    if (g_slash_timer > 0.0f && !g_render_angle_override) {
        const float total = (g_slash_total > 0.05f) ? g_slash_total : 0.26f;
        float age = 1.0f - clampf32(g_slash_timer / total, 0.0f, 1.0f);
        int head = (int)(age * 42.0f);
        int tail = head - 18;
        if (tail < 0) tail = 0;
        if (head > 41) head = 41;

        Color core = (Color){255, 248, 205};
        if (g_current_weapon >= 0 && g_current_weapon < MAX_WEAPONS) core = npc_color_for(g_weapons[g_current_weapon].color_id);
        Color mid = (Color){(uint8_t)((core.r + 255) / 2), (uint8_t)((core.g + 235) / 2), (uint8_t)((core.b + 170) / 2)};
        Color glow = (Color){(uint8_t)(core.r / 4), (uint8_t)(core.g / 4), (uint8_t)(core.b / 4)};

        for (int i = tail; i <= head; i++) {
            float u = ((float)i) / 41.0f;
            float k = sinf(u * PI_F);
            int x = cx;
            int y = cy;

            switch (g_slash_type % 3) {
                default:
                case 0:
                    /* Rising left-to-right cut, centered on the crosshair. */
                    x = cx - 48 + (int)(u * 96.0f);
                    y = cy + 34 - (int)(u * 68.0f) - (int)(k * 10.0f);
                    break;
                case 1:
                    /* Falling left-to-right cut. */
                    x = cx - 50 + (int)(u * 100.0f);
                    y = cy - 36 + (int)(u * 72.0f) + (int)(k * 8.0f);
                    break;
                case 2:
                    /* Short crescent sweep across the center. */
                    x = cx - 42 + (int)(u * 84.0f);
                    y = cy + (int)(cosf((u * 1.35f + 0.08f) * PI_F) * 26.0f);
                    break;
            }

            /* Checker-skip the glow so it feels light/transparent instead of a solid block. */
            int thick = ((i + g_slash_type) & 3) == 0 ? 2 : 1;
            Color use_core = (i > head - 5) ? core : mid;
            if (((i + g_slash_type) & 1) == 0) draw_slash_point(fb, x, y, use_core, glow, thick);
            else fill_rect_raw(fb, TOP_W, TOP_H, x, y, x + 2, y + 2, use_core);
        }
    }

    /* Draw the crosshair last so the slash never drags it right or covers it. */
    fill_rect_raw(fb, TOP_W, TOP_H, cx - 9, cy - 1, cx + 10, cy + 2, (Color){20, 20, 20});
    fill_rect_raw(fb, TOP_W, TOP_H, cx - 1, cy - 9, cx + 2, cy + 10, (Color){20, 20, 20});
    fill_rect_raw(fb, TOP_W, TOP_H, cx - 7, cy, cx + 8, cy + 1, (Color){235, 235, 235});
    fill_rect_raw(fb, TOP_W, TOP_H, cx, cy - 7, cx + 1, cy + 8, (Color){235, 235, 235});
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
    if (!g_render_angle_override && g_screen_shake_enabled && g_screen_shake_timer > 0.0f) {
        float shake = g_screen_shake_timer / 0.30f;
        int jitter = (int)((g_frame_counter * 1103515245u + 12345u) >> 28) & 7;
        center_y += ((float)jitter - 3.5f) * shake * 1.6f;
    }

    float ray_x = dir_x - plane_x;
    float ray_y = dir_y - plane_y;
    float ray_step_x = (2.0f * plane_x / (float)RENDER_W) * (float)column_step;
    float ray_step_y = (2.0f * plane_y / (float)RENDER_W) * (float)column_step;

    int prev_sy0 = -1;
    int prev_sy1 = -1;
    Color prev_c = {0, 0, 0};
    float zbuf[RENDER_W];
    for (int zi = 0; zi < RENDER_W; zi++) zbuf[zi] = 1.0e30f;
    reset_render_pixel_zbufs();

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
        bool platform_hit = false;
        int platform_side = 0;
        int platform_x = 0;
        int platform_y = 0;
        float platform_perp = 0.0f;

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

            hit_tile = tiles[map_y * map_w + map_x] & MAX_TILE_ID;
            if (hit_tile == PLATFORM_TILE) {
                if (!platform_hit) {
                    platform_hit = true;
                    platform_side = side_hit;
                    platform_x = map_x;
                    platform_y = map_y;
                    platform_perp = (side_hit == 0) ? (side_x - delta_x) : (side_y - delta_y);
                    if (platform_perp < 0.001f) platform_perp = 0.001f;
                }
                hit_tile = 0;
                continue;
            }
            if (hit_tile == TILE_DOOR && !door_blocks_at(map_x, map_y)) { hit_tile = 0; continue; }
            if (tile_blocks_raycast(hit_tile)) break;
            hit_tile = 0;
        }

        float perp = 0.0f;
        if (hit_tile) {
            perp = (side_hit == 0) ? (side_x - delta_x) : (side_y - delta_y);
            if (perp < 0.001f) perp = 0.001f;
        }

        if (!hit_tile && !platform_hit) continue;

        /* Draw the far wall first, then overlay the nearer platform strip.
           This keeps platforms visible while still allowing walls behind them to show. */
        if (hit_tile) {
            draw_raycast_column_slice(fb, x, column_step, perp, hit_tile, side_hit, map_x, map_y,
                                      ray_x, ray_y, pos_x, pos_y, cam_z, center_y, depth,
                                      &prev_sy0, &prev_sy1, &prev_c, zbuf);
        }
        if (platform_hit) {
            draw_raycast_column_slice(fb, x, column_step, platform_perp, PLATFORM_TILE, platform_side, platform_x, platform_y,
                                      ray_x, ray_y, pos_x, pos_y, cam_z, center_y, depth,
                                      &prev_sy0, &prev_sy1, &prev_c, zbuf);
        }
        continue;

    }

    if (!g_render_angle_override) {
        bool low_detail = g_fast_render || (g_fps_smooth < 45.0f);
        int item_budget = 0;
        int npc_budget = 0;
        int enemy_budget = 0;
        int max_item_budget = low_detail ? 10 : 18;
        int max_npc_budget = low_detail ? 6 : 10;
        int max_enemy_budget = low_detail ? 12 : 20;
        for (int ci = 0; ci < g_collectible_count && item_budget < max_item_budget; ci++) {
            const Collectible *c = &g_collectibles[ci];
            if (!c->active) continue;
            if (!entity_in_front_and_near(pos_x, pos_y, dir_x, dir_y, c->fx, c->fy, low_detail ? (14.0f * 14.0f) : ITEM_RENDER_DIST2)) continue;
            item_budget++;
            float item_base_z = sprite_base_z_at(c->fx, c->fy);
            /* draw pickup shadows next to the matching sprite scale below */
            if (c->kind == REWARD_KEY) {
                float cz = item_base_z + EYE_HEIGHT;
                draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, c->fx, c->fy, item_base_z, 0.42f, mask8_visible_width_ratio(KEY_SPRITE_8X8));
                draw_billboard_masked_sprite_centered_z(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             c->fx, c->fy, cz, 0.42f, KEY_SPRITE_8X8, (Color){245, 205, 55});
            } else if (c->kind == REWARD_HEALTH) {
                float cz = item_base_z + EYE_HEIGHT * 0.88f;
                draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, c->fx, c->fy, item_base_z, 0.34f, mask8_visible_width_ratio(HEART_SPRITE_8X8));
                draw_billboard_masked_sprite_centered_z(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             c->fx, c->fy, cz, 0.34f, HEART_SPRITE_8X8, (Color){255,80,105});
            } else if (c->kind >= REWARD_WEAPON_BASE && c->kind < REWARD_WEAPON_BASE + MAX_WEAPONS) {
                int wi = c->kind - REWARD_WEAPON_BASE;
                Color wc = npc_color_for(g_weapons[wi].color_id);
                const uint8_t *wsp = g_weapons[wi].sprite;
                if (!wsp[0] && !wsp[1] && !wsp[2] && !wsp[3] && !wsp[4] && !wsp[5] && !wsp[6] && !wsp[7]) wsp = WEAPON_SPRITE_8X8;
                draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, c->fx, c->fy, item_base_z, 0.44f, mask8_visible_width_ratio(wsp));
                draw_billboard_masked_sprite_centered_z(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             c->fx, c->fy, item_base_z + EYE_HEIGHT * 0.76f, 0.44f, wsp, wc);
            } else {
                Color cc = (c->kind == REWARD_PINK) ? (Color){255, 105, 190} : ((c->kind == REWARD_PURPLE) ? (Color){175, 85, 255} : (Color){245, 205, 55});
                float scale = (c->kind == REWARD_PURPLE) ? 0.34f : ((c->kind == REWARD_PINK) ? 0.30f : 0.26f);
                float cz = item_base_z + EYE_HEIGHT;
                draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, c->fx, c->fy, item_base_z, scale, mask8_visible_width_ratio(DOT_SPRITE_8X8));
                draw_billboard_masked_sprite_centered_z(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             c->fx, c->fy, cz, scale, DOT_SPRITE_8X8, cc);
            }
        }
        for (int ni = 0; ni < g_npc_count && npc_budget < max_npc_budget; ni++) {
            const NPC *n = &g_npcs[ni];
            if (!n->active) continue;
            const uint16_t *nsp16 = npc_anim_frame_for(n);
            if (!nsp16) nsp16 = n->sprite16;
            bool npc16_empty = true;
            for (int si = 0; si < ENEMY_SPRITE_ROWS; si++) if (nsp16[si]) npc16_empty = false;
            float nx = (float)n->x + 0.5f;
            float ny = (float)n->y + 0.5f;
            if (!entity_in_front_and_near(pos_x, pos_y, dir_x, dir_y, nx, ny, low_detail ? (18.0f * 18.0f) : NPC_RENDER_DIST2)) continue;
            npc_budget++;
            if (!npc16_empty) {
                draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, nx, ny, sprite_base_z_at(nx, ny), 0.82f, mask16_visible_width_ratio(nsp16));
                draw_billboard_masked_sprite16_centered_z(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                                         nx, ny, sprite_base_z_at(nx, ny) + 0.42f, 0.82f, nsp16, npc_color_for(n->color_id));
            } else {
                const uint8_t *nsp = n->sprite;
                if (!nsp[0] && !nsp[1] && !nsp[2] && !nsp[3] && !nsp[4] && !nsp[5] && !nsp[6] && !nsp[7]) nsp = KEY_SPRITE_8X8;
                draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, nx, ny, sprite_base_z_at(nx, ny), 0.82f, mask8_visible_width_ratio(nsp));
                draw_billboard_masked_sprite(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             nx, ny, 0.82f, nsp, npc_color_for(n->color_id));
            }
            float ndx = nx - pos_x;
            float ndy = ny - pos_y;
            float nd2 = ndx * ndx + ndy * ndy;
            bool show_text = false;
            float text_radius = (n->text_mode == TEXT_MODE_ALWAYS) ? 16.0f : 10.0f;
            if (nd2 <= text_radius * text_radius) show_text = true;
            if (!g_edit_mode && show_text) {
                float telapsed = n->talk_timer > 0.0f ? n->talk_timer : 999.0f;
                draw_world_text_label_typed(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                      nx, ny, 0.65f,
                                      n->text, n->text_speed, telapsed, (Color){245,245,245}, (Color){15,20,28});
            }
        }
        for (int pi = 0; pi < g_projectile_count; pi++) {
            const Projectile *pr = &g_projectiles[pi];
            if (!pr->active) continue;
            if (!entity_in_front_and_near(pos_x, pos_y, dir_x, dir_y, pr->x, pr->y, ENEMY_RENDER_DIST2)) continue;
            const uint8_t *psp = (pr->style == 1) ? ORB_SPRITE_8X8 : ((pr->style == 2) ? WAVE_SPRITE_8X8 : ARROW_SPRITE_8X8);
            float psz = 0.25f;
            if (pr->anim) psz += 0.035f * sinf((float)g_frame_counter * 0.22f + (float)pi);
            draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, pr->x, pr->y, sprite_base_z_at(pr->x, pr->y), psz * 0.80f, mask8_visible_width_ratio(psp));
            draw_billboard_masked_sprite_centered_z(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                         pr->x, pr->y, pr->z, psz, psp, npc_color_for(pr->color_id));
        }
        if (g_has_success) {
            draw_success_floor_marker(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y,
                                      g_success_x, g_success_y);
        }
        for (int ei = 0; ei < g_enemy_count && enemy_budget < max_enemy_budget; ei++) {
            Enemy *e = &g_enemies[ei];
            if (!e->active) continue;
            if (!entity_in_front_and_near(pos_x, pos_y, dir_x, dir_y, e->x, e->y, low_detail ? (22.0f * 22.0f) : ENEMY_RENDER_DIST2)) continue;
            enemy_budget++;
            float edx = e->x - pos_x;
            float edy = e->y - pos_y;
            float ed2 = edx * edx + edy * edy;
            Color ec = e->dying ? (Color){135, 70, 50} : (e->state ? (Color){255, 75, 65} : npc_color_for(e->color_id));
            if (!e->dying && g_current_weapon >= 0 && g_current_weapon < MAX_WEAPONS && g_player_weapons[g_current_weapon]) {
                float wr = 0.85f + (float)g_weapons[g_current_weapon].range * 0.42f;
                float fx = cosf(g_level.player_angle);
                float fy = sinf(g_level.player_angle);
                float dot = 0.0f;
                if (ed2 > 0.0001f) dot = ((edx * fx) + (edy * fy)) / sqrtf(ed2);
                if (ed2 <= wr * wr && dot > 0.40f) {
                    ec = blend_color(ec, (Color){105, 220, 255}, 0.30f);
                }
            }
            bool boss_shadow = (e->ai_rank == AI_RANK_BOSS);
            int shadow_size_pct = e->size_pct ? e->size_pct : (boss_shadow ? 118 : 100);
            if (!boss_shadow && shadow_size_pct > 115) shadow_size_pct = 115;
            if (boss_shadow && shadow_size_pct < 108) shadow_size_pct = 108;
            float hit_t_shadow = clampf32(e->hit_timer / 0.28f, 0.0f, 1.0f);
            float death_t_shadow = e->dying ? clampf32(1.0f - (e->death_timer / 0.48f), 0.0f, 1.0f) : 0.0f;
            float shadow_hscale = (boss_shadow ? 1.12f : 0.92f) * ((float)shadow_size_pct / 100.0f);
            float shadow_wratio = boss_shadow ? 0.70f : 0.54f;
            shadow_wratio *= boss_shadow ? mask32_visible_width_ratio(e->boss_sprite) : mask16_visible_width_ratio(e->sprite16);
            if (e->dying) { shadow_hscale *= (1.0f - death_t_shadow * 0.82f); shadow_wratio *= (1.0f + death_t_shadow * 1.10f); }
            else if (hit_t_shadow > 0.0f) { shadow_hscale *= (1.0f - hit_t_shadow * 0.16f); shadow_wratio *= (1.0f + hit_t_shadow * 0.26f); }
            draw_ground_shadow(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, e->x, e->y, sprite_base_z_at(e->x, e->y), shadow_hscale, shadow_wratio);
            draw_enemy_sprite_with_hp(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf, e, ec);
            const char *eline = NULL;
            bool enemy_text_near = ed2 <= 6.0f * 6.0f;
            if (!e->dying && e->text_count > 0 && (e->text_timer > 0.0f || (e->state && enemy_text_near))) eline = e->text[e->text_index % e->text_count];
            else if (!e->dying && e->state && enemy_text_near) eline = "!!!";
            if (!g_edit_mode && eline && eline[0]) {
                float eelapsed = e->text_timer > 0.0f ? (1.1f - e->text_timer) : 999.0f;
                draw_world_text_label_typed(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                      e->x, e->y, 0.74f, eline, e->text_speed, eelapsed, (Color){255,235,210}, (Color){35,8,8});
            }
        }
    }

    draw_crosshair_and_slash(fb);

    if (g_level_won && !g_render_angle_override) {
        char buf[64];
        fill_rect_raw(fb, TOP_W, TOP_H, 78, 54, 322, 168, (Color){3, 20, 8});
        fill_rect_raw(fb, TOP_W, TOP_H, 82, 58, 318, 164, (Color){8, 45, 18});
        draw_text3x5(fb, TOP_W, TOP_H, 142, 64, "RESULTS", (Color){150,255,155}, 2);
        snprintf(buf, sizeof(buf), "SCORE %d", g_player_score);
        draw_text3x5(fb, TOP_W, TOP_H, 98, 86, buf, (Color){245,245,210}, 1);
        snprintf(buf, sizeof(buf), "ENEMIES %d/%d", g_enemies_killed, g_enemies_total);
        draw_text3x5(fb, TOP_W, TOP_H, 98, 98, buf, (Color){255,205,190}, 1);
        snprintf(buf, sizeof(buf), "COINS BANK %d/%d", g_coins_bank, g_coins_total);
        draw_text3x5(fb, TOP_W, TOP_H, 98, 110, buf, (Color){245,220,90}, 1);
        snprintf(buf, sizeof(buf), "MISSIONS %d/%d", g_missions_done, g_missions_total);
        draw_text3x5(fb, TOP_W, TOP_H, 98, 122, buf, (Color){190,225,255}, 1);
        snprintf(buf, sizeof(buf), "TOTAL %d%%", g_success_percent);
        draw_text3x5(fb, TOP_W, TOP_H, 98, 138, buf, (Color){180,255,180}, 2);
        if (g_random_play) draw_text3x5(fb, TOP_W, TOP_H, 98, 156, "A NEXT  START MENU", (Color){235,235,235}, 1);
        else draw_text3x5(fb, TOP_W, TOP_H, 98, 156, "START MENU", (Color){235,235,235}, 1);
    }

    if (g_player_dead && !g_render_angle_override) {
        char dbuf[64];
        fill_rect_raw(fb, TOP_W, TOP_H, 58, 54, 342, 176, (Color){28, 4, 4});
        fill_rect_raw(fb, TOP_W, TOP_H, 64, 60, 336, 170, (Color){72, 10, 12});
        draw_text3x5(fb, TOP_W, TOP_H, 142, 68, "YOU FELL", (Color){255, 160, 145}, 2);
        snprintf(dbuf, sizeof(dbuf), "KILLED BY %s", g_death_killer[0] ? g_death_killer : "AN ENEMY");
        draw_text3x5(fb, TOP_W, TOP_H, 86, 98, dbuf, (Color){255, 220, 210}, 1);
        draw_text3x5(fb, TOP_W, TOP_H, 100, 122, "A RETRY FROM START", (Color){245, 245, 210}, 1);
        draw_text3x5(fb, TOP_W, TOP_H, 100, 138, "START RETURN TO MENU", (Color){245, 245, 210}, 1);
    }

    if (g_render_world_hud) {
        fill_rect_raw(fb, TOP_W, TOP_H, 4, 4, TOP_W - 4, 26, (Color){5, 5, 5});
        draw_text3x5(fb, TOP_W, TOP_H, 8, 8, g_edit_mode ? "EDITOR" : (g_random_play ? "RANDOM" : "PLAY"), g_edit_mode ? (Color){120,240,120} : (Color){120,190,255}, 2);
        draw_text_number(fb, TOP_W, TOP_H, 82, 8, g_dirty ? "SLOT* " : "SLOT ", g_slot, (Color){240, 240, 240}, 2);
        int status_x = 246;
        if (g_edit_mode) {
            draw_text_number(fb, TOP_W, TOP_H, 160, 8, "TILE ", g_selected_tile, map_tile_color(g_selected_tile), 2);
            draw_text3x5(fb, TOP_W, TOP_H, 230, 8, editor_tool_name(g_editor_tool), (Color){190,220,255}, 1);
            draw_text3x5(fb, TOP_W, TOP_H, 286, 8, room_class_short_name(g_selected_room), room_class_color(g_selected_room), 1);
            status_x = 332;
        } else {
            draw_text_number(fb, TOP_W, TOP_H, 160, 8, "KEY ", g_player_keys, (Color){245, 205, 55}, 2);
            draw_text_number(fb, TOP_W, TOP_H, 218, 8, "SCR ", g_player_score, (Color){230, 230, 160}, 2);
            draw_text3x5(fb, TOP_W, TOP_H, 292, 8, weapon_name(g_current_weapon), (Color){190, 220, 255}, 1);
            status_x = 344;
        }
        draw_text3x5(fb, TOP_W, TOP_H, status_x, 8, g_status, (Color){230, 230, 160}, 1);

        if (g_debug_overlay && !g_render_angle_override) {
            char dbg[96];
            snprintf(dbg, sizeof(dbg), "FPS %d SPD %d EN %d FOV %d", (int)(g_fps_smooth + 0.5f),
                     (int)(g_camera_speed * 100.0f + 0.5f), g_enemy_count, (int)(g_fov_degrees + 0.5f));
            fill_rect_raw(fb, TOP_W, TOP_H, 4, 28, TOP_W - 4, 50, (Color){5, 5, 5});
            draw_text3x5(fb, TOP_W, TOP_H, 8, 32, dbg, (Color){180, 240, 180}, 1);
            snprintf(dbg, sizeof(dbg), "POS %d %d Z%d RAYS %d AA %d 3D %d", (int)lv->player_x, (int)lv->player_y,
                     (int)(lv->player_z * 100.0f), g_fast_render ? 100 : 200, g_antialiasing ? 1 : 0, g_3d_enabled ? 1 : 0);
            draw_text3x5(fb, TOP_W, TOP_H, 8, 42, dbg, (Color){180, 220, 255}, 1);
        }
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
    tile &= MAX_TILE_ID;
    if (tile == 0) return (Color){28, 28, 28};
    if (tile == PLATFORM_TILE) return (Color){40, 115, 120};
    if (tile == TILE_DOT) return (Color){245, 205, 55};
    if (tile == TILE_PINK) return (Color){255, 105, 190};
    if (tile == TILE_PURPLE) return (Color){175, 85, 255};
    if (tile == TILE_NPC) return (Color){80, 210, 255};
    if (tile == TILE_AI_SPAWN) return (Color){190, 65, 210};
    if (tile == TILE_SUCCESS) return (Color){65, 235, 95};
    if (tile == TILE_KEY) return (Color){245, 205, 55};
    if (tile == TILE_DOOR) return (Color){150, 100, 45};
    if (tile >= 8) return (Color){90, 90, 110};
    Color base = WALL_COLORS[tile & 7];
    return (Color){(uint8_t)(base.r / 2 + 55), (uint8_t)(base.g / 2 + 55), (uint8_t)(base.b / 2 + 55)};
}

static void draw_tile_label(u8 *fb, int x, int y, int t, Color c) {
    char label[2];
    if (t == PLATFORM_TILE) label[0] = 'P';
    else if (t == TILE_DOT) label[0] = '.';
    else if (t == TILE_PINK) label[0] = '5';
    else if (t == TILE_PURPLE) label[0] = 'A';
    else if (t == TILE_NPC) label[0] = 'N';
    else if (t == TILE_AI_SPAWN) label[0] = 'E';
    else if (t == TILE_SUCCESS) label[0] = 'S';
    else if (t == TILE_KEY) label[0] = 'K';
    else if (t == TILE_DOOR) label[0] = 'D';
    else label[0] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
    label[1] = '\0';
    draw_text3x5(fb, BOT_W, BOT_H, x, y, label, c, 1);
}

void draw_tile_palette(u8 *fb) {
    for (int t = 0; t <= MAX_TILE_ID; t++) {
        int row = t / 4;
        int col = t & 3;
        int x0 = 4 + col * 23;
        int y0 = 188 + row * 12;
        Color border = (t == g_selected_tile) ? (Color){255, 255, 255} : (Color){60, 60, 60};
        fill_rect_raw(fb, BOT_W, BOT_H, x0 - 1, y0 - 1, x0 + 20, y0 + 11, border);
        fill_rect_raw(fb, BOT_W, BOT_H, x0, y0, x0 + 19, y0 + 10, map_tile_color((uint8_t)t));
        draw_tile_label(fb, x0 + 7, y0 + 3, t, (t == 0) ? (Color){230,230,230} : (Color){8,8,8});
    }
}


static const char *room_class_short_name(uint8_t room) {
    switch (room) {
        case ROOM_TREASURE: return "TRSR";
        case ROOM_BOSS: return "BOSS";
        case ROOM_TRAP: return "TRAP";
        case ROOM_ENEMY: return "ENMY";
        case ROOM_CORRIDOR: return "HALL";
        case ROOM_SAFE: return "SAFE";
        case ROOM_PUZZLE: return "PUZL";
        case ROOM_SECRET: return "SECR";
        default: return "NONE";
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

static void draw_icon8_bottom(u8 *fb, int x, int y, const uint8_t *mask, Color c, int scale) {
    if (!mask || scale < 1) return;
    for (int yy = 0; yy < 8; yy++) {
        uint8_t row = mask[yy];
        for (int xx = 0; xx < 8; xx++) {
            if (row & (uint8_t)(0x80u >> xx)) {
                fill_rect_raw(fb, BOT_W, BOT_H, x + xx * scale, y + yy * scale,
                              x + (xx + 1) * scale, y + (yy + 1) * scale, c);
            }
        }
    }
}

static void draw_bottom_health_bar(u8 *fb, int x, int y, int w, int h) {
    int maxhp = g_player_health_max > 0 ? g_player_health_max : PLAYER_HEALTH_DEFAULT;
    int hp = clampi32(g_player_health, 0, maxhp);
    fill_rect_raw(fb, BOT_W, BOT_H, x - 1, y - 1, x + w + 1, y + h + 1, (Color){90, 35, 35});
    fill_rect_raw(fb, BOT_W, BOT_H, x, y, x + w, y + h, (Color){28, 10, 12});
    int fill = (w * hp) / maxhp;
    Color hc = g_player_dead ? (Color){90, 90, 90} : (Color){75, 230, 95};
    if (g_player_hurt_timer > 0.0f && !g_player_dead) hc = (Color){255, 90, 80};
    fill_rect_raw(fb, BOT_W, BOT_H, x, y, x + fill, y + h, hc);
}

static const char *quest_hud_text(const NPC *n) {
    static char buf[48];
    if (!n) return "";
    if (n->completed) snprintf(buf, sizeof(buf), "DONE: %.22s", n->text);
    else if (n->quest_type == QUEST_NONE) snprintf(buf, sizeof(buf), "TALK: %.22s", n->text);
    else if (n->quest_type == QUEST_COINS) snprintf(buf, sizeof(buf), "COINS %d/%d", g_player_score, n->quest_target);
    else if (n->quest_type == QUEST_KEY) snprintf(buf, sizeof(buf), "KEYS %d/%d", g_player_keys, n->quest_target);
    else snprintf(buf, sizeof(buf), "NPCS %d/%d", g_missions_done, n->quest_target);
    return buf;
}

static void draw_play_hud(u8 *fb) {
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){6, 7, 12});
    draw_text3x5(fb, BOT_W, BOT_H, 8, 7, "PLAYER", (Color){120,190,255}, 2);
    draw_bottom_health_bar(fb, 84, 10, 90, 9);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d/%d", g_player_health, g_player_health_max);
    draw_text3x5(fb, BOT_W, BOT_H, 180, 11, buf, (Color){230,245,230}, 1);

    draw_text_number(fb, BOT_W, BOT_H, 8, 30, "COINS ", g_coins_bank, (Color){245,220,80}, 1);
    draw_text_number(fb, BOT_W, BOT_H, 78, 30, "KEYS ", g_player_keys, (Color){245,205,55}, 1);
    draw_text_number(fb, BOT_W, BOT_H, 140, 30, "SCORE ", g_player_score, (Color){230,230,160}, 1);

    fill_rect_raw(fb, BOT_W, BOT_H, 206, 8, 312, 70, (Color){28, 32, 48});
    fill_rect_raw(fb, BOT_W, BOT_H, 210, 12, 248, 50, (Color){8, 9, 14});
    int bounce = 0;
    if (g_weapon_bounce_timer > 0.0f) {
        float t = clampf32(g_weapon_bounce_timer / 0.22f, 0.0f, 1.0f);
        bounce = -(int)(sinf((1.0f - t) * PI_F) * 5.0f);
    }
    if (g_current_weapon >= 0 && g_current_weapon < MAX_WEAPONS && g_player_weapons[g_current_weapon]) {
        Color wc = npc_color_for(g_weapons[g_current_weapon].color_id);
        draw_icon8_bottom(fb, 217, 19 + bounce, g_weapons[g_current_weapon].sprite, wc, 3);
        snprintf(buf, sizeof(buf), "%s", weapon_name(g_current_weapon));
        draw_text3x5(fb, BOT_W, BOT_H, 252, 14, buf, (Color){190,220,255}, 1);
        draw_text_number(fb, BOT_W, BOT_H, 252, 28, "DMG ", g_weapons[g_current_weapon].damage, (Color){255,210,160}, 1);
    } else {
        draw_text3x5(fb, BOT_W, BOT_H, 216, 26, "NO WEAPON", (Color){160,160,170}, 1);
    }

    int ix = 210;
    for (int wi = 0; wi < MAX_WEAPONS; wi++) {
        if (!g_player_weapons[wi]) continue;
        Color wc = npc_color_for(g_weapons[wi].color_id);
        fill_rect_raw(fb, BOT_W, BOT_H, ix - 1, 55, ix + 10, 66, wi == g_current_weapon ? (Color){245,245,245} : (Color){55,55,65});
        draw_icon8_bottom(fb, ix, 56, g_weapons[wi].sprite, wc, 1);
        ix += 13;
        if (ix > 300) break;
    }

    draw_text3x5(fb, BOT_W, BOT_H, 8, 54, "QUESTS", (Color){220,220,255}, 1);
    int qy = 68;
    int shown = 0;
    for (int i = 0; i < g_npc_count && shown < 6; i++) {
        const NPC *n = &g_npcs[i];
        if (!n->active || n->completed || !n->known) continue;
        snprintf(buf, sizeof(buf), "%d %s", shown + 1, quest_hud_text(n));
        draw_text3x5(fb, BOT_W, BOT_H, 8, qy, buf, (Color){230,230,210}, 1);
        qy += 13;
        shown++;
    }
    if (shown == 0) {
        draw_text3x5(fb, BOT_W, BOT_H, 8, qy, "NO ACTIVE QUESTS", (Color){145,145,155}, 1);
        qy += 13;
    }

    draw_text_number(fb, BOT_W, BOT_H, 8, 156, "MISSIONS ", g_missions_done, (Color){190,225,255}, 1);
    draw_text_number(fb, BOT_W, BOT_H, 92, 156, "/", g_missions_total, (Color){190,225,255}, 1);
    draw_text_number(fb, BOT_W, BOT_H, 8, 171, "ENEMIES ", g_enemies_killed, (Color){255,205,190}, 1);
    draw_text_number(fb, BOT_W, BOT_H, 92, 171, "/", g_enemies_total, (Color){255,205,190}, 1);

    if (g_player_dead) draw_text3x5(fb, BOT_W, BOT_H, 8, 198, "DEAD - A RETRY / START MENU", (Color){255,120,110}, 1);
    else draw_text3x5(fb, BOT_W, BOT_H, 8, 198, "X ATTACK  Y WEAPON  SELECT TALK", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 8, 220, g_status, (Color){240,240,170}, 1);
}

void render_bottom_map(void) {
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    if (!fb) return;
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){12, 12, 12});

    const Level *lv = &g_level;
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;

    if (g_entity_edit_mode != EDIT_MODE_NONE) {
        render_entity_editor(fb);
        return;
    }

    if (!g_edit_mode && !g_random_play) {
        draw_play_hud(fb);
        return;
    }

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
            uint8_t room = room_at(lv, tx, ty);
            if (g_edit_mode && room != ROOM_NONE) {
                Color rc = room_class_color(room);
                c.r = (uint8_t)(((int)c.r + (int)rc.r) / 2);
                c.g = (uint8_t)(((int)c.g + (int)rc.g) / 2);
                c.b = (uint8_t)(((int)c.b + (int)rc.b) / 2);
            }
            fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + (y + 1) * cell, c);
            if (g_edit_mode && room != ROOM_NONE && cell >= 6) {
                Color rc = room_class_color(room);
                fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell + 1, oy + y * cell + 1, ox + (x + 1) * cell - 1, oy + y * cell + 2, rc);
            }

            if (cell >= 10) {
                fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + y * cell + 1, (Color){8,8,8});
                fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + x * cell + 1, oy + (y + 1) * cell, (Color){8,8,8});
            }
        }
    }

    for (int ci = 0; ci < g_collectible_count; ci++) {
        const Collectible *c = &g_collectibles[ci];
        if (!c->active) continue;
        int cx = (int)c->fx;
        int cy = (int)c->fy;
        if (cx >= vx && cx < vx + vw && cy >= vy && cy < vy + vh) {
            int sx = ox + (int)((c->fx - (float)vx) * cell);
            int sy = oy + (int)((c->fy - (float)vy) * cell);
            Color cc = (c->kind == REWARD_KEY) ? (Color){255,230,80} : ((c->kind == REWARD_HEALTH) ? (Color){255,80,105} : (Color){245,205,55});
            int cr = cell >= 10 ? 2 : 1;
            fill_rect_raw(fb, BOT_W, BOT_H, sx - cr, sy - cr, sx + cr + 1, sy + cr + 1, cc);
        }
    }

    for (int di = 0; di < g_door_count; di++) {
        const Door *d = &g_doors[di];
        if (!d->active) continue;
        if (d->x >= vx && d->x < vx + vw && d->y >= vy && d->y < vy + vh) {
            int dx = ox + (d->x - vx) * cell + cell / 2;
            int dy = oy + (d->y - vy) * cell + cell / 2;
            Color dc = (d->door_type == DOOR_TYPE_KEY) ? (Color){255,210,70} : ((d->door_type == DOOR_TYPE_TOGGLE) ? (Color){120,220,255} : ((d->door_type == DOOR_TYPE_SWITCH) ? (Color){120,255,150} : (Color){200,150,80}));
            int dr = cell >= 10 ? 3 : 2;
            fill_rect_raw(fb, BOT_W, BOT_H, dx - dr, dy - dr, dx + dr + 1, dy + dr + 1, dc);
        }
    }

    for (int ei = 0; ei < g_enemy_count; ei++) {
        const Enemy *e = &g_enemies[ei];
        if (!e->active) continue;
        if ((int)e->x >= vx && (int)e->x < vx + vw && (int)e->y >= vy && (int)e->y < vy + vh) {
            int ex = ox + (int)((e->x - (float)vx) * cell);
            int ey = oy + (int)((e->y - (float)vy) * cell);
            int er = cell >= 10 ? 3 : 2;
            fill_rect_raw(fb, BOT_W, BOT_H, ex - er, ey - er, ex + er + 1, ey + er + 1,
                          e->state ? (Color){255, 60, 60} : (Color){210, 70, 220});
        }
    }

    for (int ni = 0; ni < g_npc_count; ni++) {
        const NPC *n = &g_npcs[ni];
        if (!n->active) continue;
        if (n->x >= vx && n->x < vx + vw && n->y >= vy && n->y < vy + vh) {
            int nx = ox + (int)(((float)n->x + 0.5f - (float)vx) * cell);
            int ny = oy + (int)(((float)n->y + 0.5f - (float)vy) * cell);
            int nr = cell >= 10 ? 3 : 2;
            fill_rect_raw(fb, BOT_W, BOT_H, nx - nr, ny - nr, nx + nr + 1, ny + nr + 1, npc_color_for(n->color_id));
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
        draw_tile_palette(fb);

        bool can_pan = g_editor_zoom_tiles > 0;
        draw_text3x5(fb, BOT_W, BOT_H, 104, 184, "ZOOM", (Color){180, 210, 255}, 1);
        draw_editor_button(fb, 108, 188, 144, 206, "-", true);
        draw_editor_button(fb, 148, 188, 184, 206, "+", true);
        draw_editor_button(fb, 108, 210, 184, 232, "FIT", true);
        draw_editor_button(fb, 188, 188, 216, 206, editor_tool_name(g_editor_tool), true);
        draw_editor_button(fb, 188, 210, 216, 232, room_class_short_name(g_selected_room), g_editor_tool == EDITOR_TOOL_ROOM);
        draw_text3x5(fb, BOT_W, BOT_H, 224, 184, "PAN", (Color){180, 210, 255}, 1);
        draw_editor_button(fb, 218, 214, 248, 236, "L", can_pan);
        draw_editor_button(fb, 252, 188, 286, 210, "U", can_pan);
        draw_editor_button(fb, 252, 214, 286, 236, "D", can_pan);
        draw_editor_button(fb, 290, 214, 320, 236, "R", can_pan);
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


/* Bottom-screen entity/sprite editor moved to source/editor.c. */

const char *menu_action_name(int action) {
    switch (action) {
        case MENU_ACTION_PLAY: return "PLAY";
        case MENU_ACTION_RANDOM: return "RANDOM";
        case MENU_ACTION_EDIT: return "EDIT";
        case MENU_ACTION_RESIZE: return "RESIZE";
        case MENU_ACTION_DUPLICATE: return "DUP";
        case MENU_ACTION_RENAME: return "NAME";
        case MENU_ACTION_DELETE: return "DELETE";
        case MENU_ACTION_WEAPONS: return "WEAPONS";
        case MENU_ACTION_SETTINGS: return "SETTINGS";
        default: return "?";
    }
}

static void draw_settings_row(u8 *fb, int row, const char *label, const char *value) {
    const int visible = 10;
    if (row < g_settings_scroll || row >= g_settings_scroll + visible) return;
    int y = 171 + (row - g_settings_scroll) * 6;
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
    if (g_entity_edit_mode != EDIT_MODE_NONE) {
        render_entity_editor(fb);
        return;
    }
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
        draw_settings_row(fb, 9, "DEBUG", g_debug_overlay ? "ON" : "OFF");
        draw_settings_row(fb, 10, "SCREEN SHAKE", g_screen_shake_enabled ? "ON" : "OFF");
        char npcbuf[16];
        snprintf(npcbuf, sizeof(npcbuf), "%d", g_default_npc_color & 7);
        draw_settings_row(fb, 11, "DEFAULT NPC", npcbuf);
        draw_settings_row(fb, 12, "DEFAULT ART", "EDIT ON NPC");
        char hpbuf[16];
        snprintf(hpbuf, sizeof(hpbuf), "%d", g_player_health_max);
        draw_settings_row(fb, 13, "PLAYER HP", hpbuf);
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

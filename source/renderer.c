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


static void draw_billboard_sprite(u8 *fb,
                                  float cam_x, float cam_y,
                                  float dir_x, float dir_y,
                                  float plane_x, float plane_y,
                                  float center_y,
                                  const float *zbuf,
                                  float sprite_x, float sprite_y,
                                  float height_scale,
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
    if (sprite_h < 4) sprite_h = 4;
    if (sprite_h > RENDER_H * 2) sprite_h = RENDER_H * 2;

    int sprite_w = sprite_h / 2;
    if (sprite_w < 3) sprite_w = 3;

    int draw_y0 = (int)(center_y - (float)sprite_h * 0.55f);
    int draw_y1 = draw_y0 + sprite_h;
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

    for (int sx = draw_x0; sx <= draw_x1; sx++) {
        if (transform_y >= zbuf[sx]) continue;
        int px0 = sx * PIXEL_SCALE;
        int px1 = px0 + PIXEL_SCALE;
        int py0 = draw_y0 * PIXEL_SCALE;
        int py1 = (draw_y1 + 1) * PIXEL_SCALE;
        fill_rect_raw(fb, TOP_W, TOP_H, px0, py0, px1, py1, c);
        if (sx == draw_x0 || sx == draw_x1) fill_rect_raw(fb, TOP_W, TOP_H, px0, py0, px1, py1, edge);
    }
}

static const uint8_t KEY_SPRITE_8X8[8] = {
    0x1C, 0x22, 0x22, 0x1C, 0x08, 0x0E, 0x08, 0x0C
};

static const uint8_t DOT_SPRITE_8X8[8] = {
    0x00, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x00
};

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
    if (transform_y >= zbuf[sx]) return;

    int sprite_h = (int)fabsf((RENDER_H / transform_y) * height_scale * clampf32(g_level_depth, 0.50f, 2.00f));
    if (sprite_h < 8) sprite_h = 8;
    if (sprite_h > RENDER_H * 2) sprite_h = RENDER_H * 2;

    char buf[24];
    int len = 0;
    while (text[len] && len < (int)sizeof(buf) - 1) {
        buf[len] = text[len];
        len++;
    }
    buf[len] = '\0';
    if (len <= 0) return;

    int px = sx * PIXEL_SCALE;
    int py = (int)((center_y - (float)sprite_h * 0.72f) * PIXEL_SCALE) - 10;
    int tw = len * 4;
    int x0 = px - tw / 2 - 2;
    int y0 = py - 2;
    if (x0 < 0) x0 = 0;
    if (x0 + tw + 4 > TOP_W) x0 = TOP_W - tw - 4;
    if (y0 < 2) y0 = 2;
    if (y0 > TOP_H - 10) y0 = TOP_H - 10;
    fill_rect_raw(fb, TOP_W, TOP_H, x0, y0, x0 + tw + 4, y0 + 9, bg);
    draw_text3x5(fb, TOP_W, TOP_H, x0 + 2, y0 + 2, buf, fg, 1);
}

static void draw_billboard_masked_sprite(u8 *fb,
                                         float cam_x, float cam_y,
                                         float dir_x, float dir_y,
                                         float plane_x, float plane_y,
                                         float center_y,
                                         const float *zbuf,
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
    if (sprite_h > RENDER_H) sprite_h = RENDER_H;

    int sprite_w = sprite_h;
    int draw_y0 = (int)(center_y - (float)sprite_h * 0.40f);
    int draw_y1 = draw_y0 + sprite_h;
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
            if (transform_y >= zbuf[sx]) continue;

            int u = ((sx - draw_x0) * 8) / w;
            if (u < 0) u = 0;
            if (u > 7) u = 7;
            if (!(mask8[v] & (uint8_t)(0x80u >> u))) continue;

            Color out = (v < 3 && u < 5) ? shine : c;
            int px0 = sx * PIXEL_SCALE;
            int px1 = px0 + PIXEL_SCALE;
            int py0 = sy * PIXEL_SCALE;
            int py1 = py0 + PIXEL_SCALE;
            fill_rect_raw(fb, TOP_W, TOP_H, px0, py0, px1, py1, out);
        }
    }
}


static void draw_enemy_sprite_with_hp(u8 *fb,
                                      float cam_x, float cam_y,
                                      float dir_x, float dir_y,
                                      float plane_x, float plane_y,
                                      float center_y,
                                      const float *zbuf,
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
    int sprite_h = (int)fabsf((RENDER_H / transform_y) * 0.95f * clampf32(g_level_depth, 0.50f, 2.00f));
    if (sprite_h < 8) sprite_h = 8;
    if (sprite_h > RENDER_H * 2) sprite_h = RENDER_H * 2;
    int sprite_w = sprite_h / 2;
    if (sprite_w < 4) sprite_w = 4;

    int draw_y0 = (int)(center_y - (float)sprite_h * 0.55f);
    int draw_y1 = draw_y0 + sprite_h;
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
        int v = ((sy - draw_y0) * 8) / h;
        if (v < 0) v = 0;
        if (v > 7) v = 7;
        for (int sx = draw_x0; sx <= draw_x1; sx++) {
            if (transform_y >= zbuf[sx]) continue;
            int u = ((sx - draw_x0) * 8) / w;
            if (u < 0) u = 0;
            if (u > 7) u = 7;
            if (!(e->sprite[v] & (1 << (7 - u)))) continue;
            int px0 = sx * PIXEL_SCALE;
            int px1 = px0 + PIXEL_SCALE;
            int py0 = sy * PIXEL_SCALE;
            int py1 = py0 + PIXEL_SCALE;
            Color pc = c;
            if (u == 0 || u == 7 || v == 0) pc = edge;
            fill_rect_raw(fb, TOP_W, TOP_H, px0, py0, px1, py1, pc);
        }
    }

    int hp_max = e->hp_max > 0 ? e->hp_max : 7;
    int hp = e->hp;
    if (hp < 0) hp = 0;
    if (hp > hp_max) hp = hp_max;
    int bx0 = draw_x0 * PIXEL_SCALE;
    int bx1 = (draw_x1 + 1) * PIXEL_SCALE;
    int by0 = draw_y0 * PIXEL_SCALE - 5;
    if (by0 < 2) by0 = 2;
    fill_rect_raw(fb, TOP_W, TOP_H, bx0, by0, bx1, by0 + 3, (Color){35, 10, 10});
    int fill_w = ((bx1 - bx0) * hp) / hp_max;
    fill_rect_raw(fb, TOP_W, TOP_H, bx0, by0, bx0 + fill_w, by0 + 3, (Color){90, 255, 90});
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
    float zbuf[RENDER_W];
    for (int zi = 0; zi < RENDER_W; zi++) zbuf[zi] = 1.0e30f;

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

            hit_tile = tiles[map_y * map_w + map_x] & MAX_TILE_ID;
            if (tile_blocks_raycast(hit_tile)) break;
            hit_tile = 0;
        }

        if (!hit_tile) continue;

        float perp = (side_hit == 0) ? (side_x - delta_x) : (side_y - delta_y);
        if (perp < 0.001f) perp = 0.001f;
        for (int zi = x; zi < x + column_step && zi < RENDER_W; zi++) zbuf[zi] = perp;

        float z0 = 0.0f;
        float z1 = 1.0f;
        if (hit_tile == PLATFORM_TILE) {
            z0 = PLATFORM_BOTTOM;
            z1 = PLATFORM_TOP;
        } else if (hit_tile == TILE_DOOR) {
            float open = door_open_fraction_at(map_x, map_y);
            float lower = open * 1.12f;
            z0 = -lower;
            z1 = 1.0f - lower;
            if (z1 <= -0.05f) continue;
        }
        z0 *= depth;
        z1 *= depth;

        float proj = (float)RENDER_H / perp;
        int draw_start = (int)(center_y + (cam_z - z1) * proj);
        int draw_end = (int)(center_y + (cam_z - z0) * proj);

        if (draw_start < 0) draw_start = 0;
        if (draw_end >= RENDER_H) draw_end = RENDER_H - 1;
        if (draw_end <= draw_start) continue;

        Color base = (hit_tile == TILE_DOOR) ? (Color){150, 100, 45} : WALL_COLORS[hit_tile & 7];
        float shade = 1.0f / (1.0f + perp * 0.12f);
        if (side_hit == 1) shade *= 0.72f;
        if (hit_tile == PLATFORM_TILE) shade *= 0.95f;
        if (hit_tile == TILE_DOOR) shade *= 1.08f;
        Color c = apply_dof_fade(shade_color(base, shade), perp);

        int sx0 = x * PIXEL_SCALE;
        int sx1 = sx0 + PIXEL_SCALE * column_step;
        if (sx1 > TOP_W) sx1 = TOP_W;
        int sy0 = draw_start * PIXEL_SCALE;
        int sy1 = (draw_end + 1) * PIXEL_SCALE;
        fill_rect_raw(fb, TOP_W, TOP_H, sx0, sy0, sx1, sy1, c);

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

    if (!g_render_angle_override && !g_edit_mode) {
        for (int ci = 0; ci < g_collectible_count; ci++) {
            const Collectible *c = &g_collectibles[ci];
            if (!c->active) continue;
            if (c->kind == REWARD_KEY) {
                draw_billboard_masked_sprite(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             c->fx, c->fy, 0.42f, KEY_SPRITE_8X8, (Color){245, 205, 55});
            } else if (c->kind >= REWARD_WEAPON_BASE && c->kind < REWARD_WEAPON_BASE + MAX_WEAPONS) {
                int wi = c->kind - REWARD_WEAPON_BASE;
                Color wc = npc_color_for(g_weapons[wi].color_id);
                const uint8_t *wsp = g_weapons[wi].sprite;
                if (!wsp[0] && !wsp[1] && !wsp[2] && !wsp[3] && !wsp[4] && !wsp[5] && !wsp[6] && !wsp[7]) wsp = WEAPON_SPRITE_8X8;
                draw_billboard_masked_sprite(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             c->fx, c->fy, 0.44f, wsp, wc);
            } else {
                Color cc = (c->kind == REWARD_PINK) ? (Color){255, 105, 190} : ((c->kind == REWARD_PURPLE) ? (Color){175, 85, 255} : (Color){245, 205, 55});
                float scale = (c->kind == REWARD_PURPLE) ? 0.34f : ((c->kind == REWARD_PINK) ? 0.30f : 0.26f);
                draw_billboard_masked_sprite(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                             c->fx, c->fy, scale, DOT_SPRITE_8X8, cc);
            }
        }
        for (int ni = 0; ni < g_npc_count; ni++) {
            const NPC *n = &g_npcs[ni];
            if (!n->active) continue;
            const uint8_t *nsp = n->sprite;
            if (!nsp[0] && !nsp[1] && !nsp[2] && !nsp[3] && !nsp[4] && !nsp[5] && !nsp[6] && !nsp[7]) nsp = KEY_SPRITE_8X8;
            float nx = (float)n->x + 0.5f;
            float ny = (float)n->y + 0.5f;
            draw_billboard_masked_sprite(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                         nx, ny, 0.82f, nsp, npc_color_for(n->color_id));
            float ndx = nx - pos_x;
            float ndy = ny - pos_y;
            float nd2 = ndx * ndx + ndy * ndy;
            bool show_text = false;
            if (n->talk_timer > 0.0f) show_text = true;
            else if (n->text_mode == TEXT_MODE_NEAR && nd2 <= 3.35f * 3.35f) show_text = true;
            else if (n->text_mode == TEXT_MODE_ALWAYS && nd2 <= 5.50f * 5.50f) show_text = true;
            if (show_text) {
                draw_world_text_label(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                      nx, ny, 0.82f,
                                      n->completed ? "THANKS" : n->text, (Color){245,245,245}, (Color){15,20,28});
            }
        }
        if (g_has_success) {
            draw_billboard_sprite(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                  g_success_x, g_success_y, 0.70f, (Color){70, 255, 100});
        }
        for (int ei = 0; ei < g_enemy_count; ei++) {
            Enemy *e = &g_enemies[ei];
            if (!e->active) continue;
            Color ec = e->state ? (Color){255, 75, 65} : npc_color_for(e->color_id);
            draw_enemy_sprite_with_hp(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf, e, ec);
            const char *eline = NULL;
            float edx = e->x - pos_x;
            float edy = e->y - pos_y;
            float ed2 = edx * edx + edy * edy;
            bool enemy_text_near = ed2 <= 6.0f * 6.0f;
            if (e->text_count > 0 && (e->text_timer > 0.0f || (e->state && enemy_text_near))) eline = e->text[e->text_index % e->text_count];
            else if (e->state && enemy_text_near) eline = "!!!";
            if (eline && eline[0]) {
                draw_world_text_label(fb, pos_x, pos_y, dir_x, dir_y, plane_x, plane_y, center_y, zbuf,
                                      e->x, e->y, 0.95f, eline, (Color){255,235,210}, (Color){35,8,8});
            }
        }
    }

    Color white = {225, 225, 225};
    fill_rect_raw(fb, TOP_W, TOP_H, TOP_W / 2 - 6, TOP_H / 2, TOP_W / 2 + 7, TOP_H / 2 + 1, white);
    fill_rect_raw(fb, TOP_W, TOP_H, TOP_W / 2, TOP_H / 2 - 6, TOP_W / 2 + 1, TOP_H / 2 + 7, white);

    if (g_level_won && !g_render_angle_override) {
        fill_rect_raw(fb, TOP_W, TOP_H, 102, 96, 298, 126, (Color){4, 35, 10});
        draw_text3x5(fb, TOP_W, TOP_H, 130, 104, "SUCCESS", (Color){120,255,140}, 2);
        if (g_random_play) draw_text3x5(fb, TOP_W, TOP_H, 124, 120, "A NEXT  START MENU", (Color){235,235,235}, 1);
    }

    if (g_render_world_hud) {
        fill_rect_raw(fb, TOP_W, TOP_H, 4, 4, TOP_W - 4, 26, (Color){5, 5, 5});
        draw_text3x5(fb, TOP_W, TOP_H, 8, 8, g_edit_mode ? "EDITOR" : (g_random_play ? "RANDOM" : "PLAY"), g_edit_mode ? (Color){120,240,120} : (Color){120,190,255}, 2);
        draw_text_number(fb, TOP_W, TOP_H, 82, 8, g_dirty ? "SLOT* " : "SLOT ", g_slot, (Color){240, 240, 240}, 2);
        int status_x = 246;
        if (g_edit_mode) draw_text_number(fb, TOP_W, TOP_H, 160, 8, "TILE ", g_selected_tile, map_tile_color(g_selected_tile), 2);
        else {
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

    if (g_entity_edit_mode != EDIT_MODE_NONE) {
        render_entity_editor(fb);
        return;
    }

    if (!g_edit_mode && !g_random_play) {
        fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){6, 7, 12});
        draw_text3x5(fb, BOT_W, BOT_H, 12, 12, "PLAY HUD", (Color){120,190,255}, 2);
        draw_text_number(fb, BOT_W, BOT_H, 12, 40, "KEYS ", g_player_keys, (Color){245,205,55}, 2);
        draw_text_number(fb, BOT_W, BOT_H, 118, 40, "SCORE ", g_player_score, (Color){230,230,160}, 2);
        draw_text3x5(fb, BOT_W, BOT_H, 12, 68, g_current_weapon >= 0 ? weapon_name(g_current_weapon) : "UNARMED", (Color){190,220,255}, 2);
        draw_text3x5(fb, BOT_W, BOT_H, 12, 96, "X ATTACK  Y WEAPON", (Color){210,210,210}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 12, 110, "SELECT TALK  START MENU", (Color){210,210,210}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 12, 220, g_status, (Color){240,240,170}, 1);
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
            fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + (y + 1) * cell, c);

            if (cell >= 10) {
                fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + y * cell + 1, (Color){8,8,8});
                fill_rect_raw(fb, BOT_W, BOT_H, ox + x * cell, oy + y * cell, ox + x * cell + 1, oy + (y + 1) * cell, (Color){8,8,8});
            }
        }
    }

    if (!g_edit_mode) {
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


static const char *entity_reward_name(int kind) {
    static char buf[20];
    if (kind == REWARD_DOT) return "DOT";
    if (kind == REWARD_KEY) return "KEY";
    if (kind == REWARD_PINK) return "PINK";
    if (kind == REWARD_PURPLE) return "PURPLE";
    if (kind >= REWARD_WEAPON_BASE && kind < REWARD_WEAPON_BASE + MAX_WEAPONS) {
        snprintf(buf, sizeof(buf), "%s", weapon_name(kind - REWARD_WEAPON_BASE));
        return buf;
    }
    return "ITEM";
}

static const char *entity_quest_name(int q) {
    switch (q) {
        case QUEST_NONE: return "READY";
        case QUEST_COINS: return "COINS";
        case QUEST_KEY: return "KEYS";
        case QUEST_NPC: return "NPC";
        default: return "?";
    }
}

static void draw_entity_edit_row(u8 *fb, int row, const char *label, const char *value) {
    int y = 42 + row * 15;
    Color bg = (g_entity_edit_cursor == row) ? (Color){48, 66, 102} : (Color){16, 18, 28};
    Color fg = (g_entity_edit_cursor == row) ? (Color){255, 230, 150} : (Color){230, 230, 230};
    fill_rect_raw(fb, BOT_W, BOT_H, 10, y - 2, BOT_W - 10, y + 12, bg);
    draw_text3x5(fb, BOT_W, BOT_H, 16, y + 2, label, fg, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 128, y + 2, value ? value : "", fg, 1);
}

static NPC *render_edit_npc(void) {
    if (g_entity_edit_mode != EDIT_MODE_NPC) return NULL;
    for (int i = 0; i < g_npc_count; i++) {
        if (g_npcs[i].active && g_npcs[i].x == g_entity_edit_x && g_npcs[i].y == g_entity_edit_y) return &g_npcs[i];
    }
    return NULL;
}

static EnemyMeta *render_edit_enemy_meta(void) {
    if (g_entity_edit_mode != EDIT_MODE_ENEMY) return NULL;
    for (int i = 0; i < g_enemy_meta_count; i++) {
        if (g_enemy_metas[i].active && g_enemy_metas[i].x == g_entity_edit_x && g_enemy_metas[i].y == g_entity_edit_y) return &g_enemy_metas[i];
    }
    return NULL;
}


static void draw_sprite_preview_grid(u8 *fb, int x0, int y0, const uint8_t *sp, Color c, int scale, int cx, int cy) {
    if (!sp) return;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            bool on = (sp[y] & (1 << (7 - x))) != 0;
            Color cell = on ? c : (Color){22, 24, 32};
            fill_rect_raw(fb, BOT_W, BOT_H, x0 + x * scale, y0 + y * scale,
                          x0 + (x + 1) * scale - 1, y0 + (y + 1) * scale - 1, cell);
            if (x == cx && y == cy) {
                fill_rect_raw(fb, BOT_W, BOT_H, x0 + x * scale, y0 + y * scale,
                              x0 + (x + 1) * scale - 1, y0 + y * scale + 2, (Color){255,255,255});
                fill_rect_raw(fb, BOT_W, BOT_H, x0 + x * scale, y0 + y * scale,
                              x0 + x * scale + 2, y0 + (y + 1) * scale - 1, (Color){255,255,255});
            }
        }
    }
}

static uint8_t *render_sprite_pixels(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC) {
        NPC *n = render_edit_npc();
        return n ? n->sprite : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY) {
        EnemyMeta *m = render_edit_enemy_meta();
        return m ? m->sprite : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_WEAPON) {
        int wi = clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1);
        return g_weapons[wi].sprite;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return g_default_npc_sprite;
    return NULL;
}

static uint8_t render_sprite_color_id(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC) { NPC *n = render_edit_npc(); return n ? n->color_id : 0; }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY) { EnemyMeta *m = render_edit_enemy_meta(); return m ? m->color_id : 0; }
    if (g_sprite_edit_target == SPRITE_TARGET_WEAPON) return g_weapons[clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1)].color_id;
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return g_default_npc_color;
    return 0;
}

void render_sprite_editor(u8 *fb) {
    if (!fb) return;
    uint8_t *sp = render_sprite_pixels();
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){7, 8, 14});
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, 28, (Color){18, 24, 42});
    draw_text3x5(fb, BOT_W, BOT_H, 8, 7, "SPRITE ART EDITOR", (Color){255,235,170}, 2);
    if (!sp) { draw_text3x5(fb, BOT_W, BOT_H, 20, 50, "NO SPRITE", (Color){255,120,120}, 2); return; }
    Color c = npc_color_for(render_sprite_color_id());
    draw_sprite_preview_grid(fb, 96, 44, sp, c, 14, g_sprite_edit_cursor_x, g_sprite_edit_cursor_y);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 44, "D-PAD MOVE", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 58, "A TOGGLE", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 72, "L/R COLOR", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 86, "Y PRESET", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 100, "X CLEAR", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 226, "B BACK - ART SAVES IN BW3", (Color){190,200,220}, 1);
}

void render_entity_editor(u8 *fb) {
    if (!fb || g_entity_edit_mode == EDIT_MODE_NONE) return;
    if (g_entity_edit_mode == EDIT_MODE_SPRITE) { render_sprite_editor(fb); return; }
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){7, 8, 14});
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, 28, (Color){18, 24, 42});
    draw_text3x5(fb, BOT_W, BOT_H, 8, 7,
                 g_entity_edit_mode == EDIT_MODE_NPC ? "NPC EDITOR" : (g_entity_edit_mode == EDIT_MODE_ENEMY ? "ENEMY EDITOR" : "WEAPON EDITOR"),
                 (Color){255, 235, 170}, 2);
    draw_text3x5(fb, BOT_W, BOT_H, 8, 226, "DUP/DDOWN ROW  DLEFT/DRIGHT VALUE  L/R FAST  A EDIT  B BACK", (Color){190, 200, 220}, 1);

    char buf[64];
    if (g_entity_edit_mode == EDIT_MODE_NPC) {
        NPC *n = render_edit_npc();
        if (!n) { draw_text3x5(fb, BOT_W, BOT_H, 16, 52, "NPC NOT FOUND", (Color){255,120,120}, 1); return; }
        snprintf(buf, sizeof(buf), "%d,%d", n->x, n->y); draw_entity_edit_row(fb, 0, "TEXT", n->text[0] ? n->text : "HELLO");
        snprintf(buf, sizeof(buf), "%s", entity_quest_name(n->quest_type)); draw_entity_edit_row(fb, 1, "QUEST", buf);
        snprintf(buf, sizeof(buf), "%d", n->quest_target); draw_entity_edit_row(fb, 2, "TARGET", buf);
        snprintf(buf, sizeof(buf), "%s", entity_reward_name(n->reward_kind)); draw_entity_edit_row(fb, 3, "REWARD", buf);
        snprintf(buf, sizeof(buf), "%d", n->reward_amount); draw_entity_edit_row(fb, 4, "AMOUNT", buf);
        const char *tm = (n->text_mode == TEXT_MODE_INTERACT) ? "INTERACT" : ((n->text_mode == TEXT_MODE_NEAR) ? "NEAR" : "FAR OK");
        draw_entity_edit_row(fb, 5, "TEXT MODE", tm);
        snprintf(buf, sizeof(buf), "%d", n->color_id & 7); draw_entity_edit_row(fb, 6, "NPC COLOR", buf);
        draw_entity_edit_row(fb, 7, "EDIT ART", "A OPEN");
        snprintf(buf, sizeof(buf), "%d", g_default_npc_color & 7); draw_entity_edit_row(fb, 8, "DEFAULT COLOR", buf);
        draw_entity_edit_row(fb, 9, "DEFAULT ART", "A OPEN");
        draw_entity_edit_row(fb, 10, "USE DEFAULT", "A APPLY");
        draw_entity_edit_row(fb, 11, "EDIT WEAPON", (n->reward_kind >= REWARD_WEAPON_BASE) ? entity_reward_name(n->reward_kind) : "A OPEN");
        draw_entity_edit_row(fb, 12, "DONE", "A CLOSE");
        draw_sprite_preview_grid(fb, 270, 34, n->sprite, npc_color_for(n->color_id), 4, -1, -1);
    } else if (g_entity_edit_mode == EDIT_MODE_ENEMY) {
        EnemyMeta *m = render_edit_enemy_meta();
        if (!m) { draw_text3x5(fb, BOT_W, BOT_H, 16, 52, "ENEMY NOT FOUND", (Color){255,120,120}, 1); return; }
        char joined[96]; joined[0] = '\0';
        for (int i = 0; i < m->text_count && i < ENEMY_TEXT_LINES; i++) {
            if (i) strncat(joined, "|", sizeof(joined) - strlen(joined) - 1);
            strncat(joined, m->text[i], sizeof(joined) - strlen(joined) - 1);
        }
        draw_entity_edit_row(fb, 0, "TEXT LINES", joined[0] ? joined : "HEY|OUCH");
        snprintf(buf, sizeof(buf), "%d", m->hp ? m->hp : 5); draw_entity_edit_row(fb, 1, "HEALTH", buf);
        snprintf(buf, sizeof(buf), "%d", m->attack ? m->attack : 1); draw_entity_edit_row(fb, 2, "ATTACK", buf);
        snprintf(buf, sizeof(buf), "%d", m->color_id & 7); draw_entity_edit_row(fb, 3, "COLOR", buf);
        draw_entity_edit_row(fb, 4, "EDIT ART", "A OPEN");
        draw_entity_edit_row(fb, 5, "DONE", "A CLOSE");
        draw_sprite_preview_grid(fb, 270, 34, m->sprite, npc_color_for(m->color_id), 4, -1, -1);
    } else if (g_entity_edit_mode == EDIT_MODE_WEAPON) {
        int wi = clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1);
        WeaponDef *w = &g_weapons[wi];
        snprintf(buf, sizeof(buf), "%d %s", wi, weapon_name(wi)); draw_entity_edit_row(fb, 0, "WEAPON", buf);
        draw_entity_edit_row(fb, 1, "NAME", w->name);
        snprintf(buf, sizeof(buf), "%d", w->damage); draw_entity_edit_row(fb, 2, "DAMAGE", buf);
        snprintf(buf, sizeof(buf), "%d", w->range); draw_entity_edit_row(fb, 3, "RANGE", buf);
        snprintf(buf, sizeof(buf), "%d", w->cooldown); draw_entity_edit_row(fb, 4, "COOLDOWN", buf);
        snprintf(buf, sizeof(buf), "%d", w->color_id & 7); draw_entity_edit_row(fb, 5, "COLOR", buf);
        draw_entity_edit_row(fb, 6, "EDIT ICON", "A OPEN");
        draw_entity_edit_row(fb, 7, "DONE", "A CLOSE");
        draw_sprite_preview_grid(fb, 270, 34, w->sprite, npc_color_for(w->color_id), 4, -1, -1);
    }
}

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
    int y = 171 + row * 6;
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
        char npcbuf[16];
        snprintf(npcbuf, sizeof(npcbuf), "%d", g_default_npc_color & 7);
        draw_settings_row(fb, 10, "DEFAULT NPC", npcbuf);
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

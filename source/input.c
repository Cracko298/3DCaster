#include "common.h"

#define CSTICK_YAW_SPEED       2.60f
#define TOUCH_YAW_PER_PIXEL    0.0105f

static bool s_touch_look_active = false;
static int s_touch_last_x = 0;
static int s_touch_last_y = 0;

static void wrap_player_angle(Level *lv) {
    while (lv->player_angle < 0.0f) lv->player_angle += TWO_PI_F;
    while (lv->player_angle >= TWO_PI_F) lv->player_angle -= TWO_PI_F;
}

static bool apply_lr_look(Level *lv, float dt, u32 kHeld, bool allow_dpad_look) {
    float old_angle = lv->player_angle;
    float rot = ROT_SPEED * dt;

    if (kHeld & KEY_L) lv->player_angle -= rot;
    if (kHeld & KEY_R) lv->player_angle += rot;

    if (allow_dpad_look) {
        if (kHeld & KEY_DLEFT) lv->player_angle -= rot;
        if (kHeld & KEY_DRIGHT) lv->player_angle += rot;
    }

    wrap_player_angle(lv);
    return fabsf(lv->player_angle - old_angle) > 0.00001f;
}

static bool apply_cstick_look(Level *lv, float dt, u32 kHeld) {
    if (!g_is_new3ds) return false;

    float old_angle = lv->player_angle;
    float lx = 0.0f;

    if (kHeld & KEY_CSTICK_LEFT)  lx -= 1.0f;
    if (kHeld & KEY_CSTICK_RIGHT) lx += 1.0f;

    lv->player_angle += lx * CSTICK_YAW_SPEED * dt;
    wrap_player_angle(lv);

    return fabsf(lv->player_angle - old_angle) > 0.00001f;
}

static bool apply_touch_look_play_only(Level *lv, u32 kHeld) {
    if (!(kHeld & KEY_TOUCH)) {
        s_touch_look_active = false;
        return false;
    }

    touchPosition tp;
    hidTouchRead(&tp);

    if (!s_touch_look_active) {
        s_touch_look_active = true;
        s_touch_last_x = tp.px;
        s_touch_last_y = tp.py;
        return false;
    }

    int dx = (int)tp.px - s_touch_last_x;
    s_touch_last_x = tp.px;
    s_touch_last_y = tp.py;

    dx = clampi32(dx, -32, 32);

    float old_angle = lv->player_angle;
    lv->player_angle += (float)dx * TOUCH_YAW_PER_PIXEL;
    wrap_player_angle(lv);

    return fabsf(lv->player_angle - old_angle) > 0.00001f;
}

static void apply_vertical_physics(Level *lv, float dt, u32 kDown, bool allow_jump) {
    if (allow_jump && (kDown & KEY_A) && lv->on_ground) {
        lv->player_vz = JUMP_SPEED;
        lv->on_ground = false;
    }

    lv->player_vz -= GRAVITY * dt;
    lv->player_z += lv->player_vz * dt;

    float ground = ground_height_at(lv, lv->player_x, lv->player_y, lv->player_z);
    if (lv->player_z <= ground && lv->player_vz <= 0.0f) {
        lv->player_z = ground;
        lv->player_vz = 0.0f;
        lv->on_ground = true;
    } else {
        lv->on_ground = false;
    }
}

static bool apply_cpad_movement(Level *lv, float dt, u32 kHeld, bool allow_sprint, float *out_speed) {
    float old_x = lv->player_x;
    float old_y = lv->player_y;
    if (out_speed) *out_speed = 0.0f;

    circlePosition cp;
    hidCircleRead(&cp);
    float forward = ((float)cp.dy) / 156.0f;
    float strafe = ((float)cp.dx) / 156.0f;
    forward = clampf32(forward, -1.0f, 1.0f);
    strafe = clampf32(strafe, -1.0f, 1.0f);

    float speed = MOVE_SPEED * dt;
    if (allow_sprint && (kHeld & KEY_B)) speed *= SPRINT_MULT;

    float ang = lv->player_angle;
    float dx = cosf(ang);
    float dy = sinf(ang);
    float move_x = (dx * forward - dy * strafe) * speed;
    float move_y = (dy * forward + dx * strafe) * speed;

    if (move_x != 0.0f || move_y != 0.0f) {
        float new_x = lv->player_x + move_x;
        float new_y = lv->player_y + move_y;
        if (can_stand_at(lv, new_x, lv->player_y, lv->player_z)) lv->player_x = new_x;
        if (can_stand_at(lv, lv->player_x, new_y, lv->player_z)) lv->player_y = new_y;
    }

    float actual_dx = lv->player_x - old_x;
    float actual_dy = lv->player_y - old_y;
    float moved_dist = sqrtf(actual_dx * actual_dx + actual_dy * actual_dy);
    if (out_speed && dt > 0.0001f) *out_speed = moved_dist / dt;

    float ground = ground_height_at(lv, lv->player_x, lv->player_y, lv->player_z);
    if (lv->player_z <= ground && lv->player_vz <= 0.0f) {
        lv->player_z = ground;
        lv->player_vz = 0.0f;
        lv->on_ground = true;
    }

    return moved_dist > 0.00001f;
}



static int editor_max_dim(const Level *lv) {
    if (!lv) return EDITOR_ZOOM_MIN_TILES;
    return lv->width > lv->height ? lv->width : lv->height;
}

static void editor_set_center(const Level *lv, int cx, int cy) {
    int vx, vy, vw, vh;
    editor_view_bounds(lv, &vx, &vy, &vw, &vh);
    g_editor_view_x = cx - vw / 2;
    g_editor_view_y = cy - vh / 2;
    editor_clamp_view(lv);
}

void editor_reset_view(void) {
    g_editor_view_x = 0;
    g_editor_view_y = 0;
    g_editor_zoom_tiles = 0;
}

void editor_view_bounds(const Level *lv, int *vx, int *vy, int *vw, int *vh) {
    int w = lv ? lv->width : MIN_MAP_W;
    int h = lv ? lv->height : MIN_MAP_H;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    int view_w = w;
    int view_h = h;

    if (g_editor_zoom_tiles > 0) {
        int z = clampi32(g_editor_zoom_tiles, EDITOR_ZOOM_MIN_TILES, MAX_MAP_W);
        view_w = w < z ? w : z;
        view_h = h < z ? h : z;
    }

    int max_x = w - view_w;
    int max_y = h - view_h;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;

    int x = clampi32(g_editor_view_x, 0, max_x);
    int y = clampi32(g_editor_view_y, 0, max_y);

    if (vx) *vx = x;
    if (vy) *vy = y;
    if (vw) *vw = view_w;
    if (vh) *vh = view_h;
}

void editor_clamp_view(const Level *lv) {
    int vx, vy, vw, vh;
    editor_view_bounds(lv, &vx, &vy, &vw, &vh);
    g_editor_view_x = vx;
    g_editor_view_y = vy;

    if (lv) {
        int max_dim = editor_max_dim(lv);
        if (g_editor_zoom_tiles >= max_dim || max_dim <= EDITOR_ZOOM_MIN_TILES) {
            g_editor_zoom_tiles = 0;
            g_editor_view_x = 0;
            g_editor_view_y = 0;
        }
    }
}

void editor_zoom_fit(const Level *lv) {
    (void)lv;
    editor_reset_view();
    snprintf(g_status, sizeof(g_status), "ZOOM FIT");
}

void editor_zoom_in(const Level *lv) {
    if (!lv) return;

    int max_dim = editor_max_dim(lv);
    if (max_dim <= EDITOR_ZOOM_MIN_TILES) {
        editor_reset_view();
        snprintf(g_status, sizeof(g_status), "ZOOM FIT");
        return;
    }

    int cx, cy;
    if (g_editor_zoom_tiles <= 0) {
        cx = (int)lv->player_x;
        cy = (int)lv->player_y;
    } else {
        int vx, vy, vw, vh;
        editor_view_bounds(lv, &vx, &vy, &vw, &vh);
        cx = vx + vw / 2;
        cy = vy + vh / 2;
    }

    const int levels[] = {192, 128, 96, 64, 48, 32, 24, 16, 12, 8};
    const int count = (int)(sizeof(levels) / sizeof(levels[0]));
    int current = (g_editor_zoom_tiles <= 0) ? max_dim : g_editor_zoom_tiles;
    int next = EDITOR_ZOOM_MIN_TILES;

    for (int i = 0; i < count; i++) {
        if (levels[i] < current) {
            next = levels[i];
            break;
        }
    }

    if (next < EDITOR_ZOOM_MIN_TILES) next = EDITOR_ZOOM_MIN_TILES;
    if (next >= max_dim) {
        editor_reset_view();
        snprintf(g_status, sizeof(g_status), "ZOOM FIT");
        return;
    }

    g_editor_zoom_tiles = next;
    editor_set_center(lv, cx, cy);
    snprintf(g_status, sizeof(g_status), "ZOOM %dX%d", next, next);
}

void editor_zoom_out(const Level *lv) {
    if (!lv || g_editor_zoom_tiles <= 0) {
        editor_reset_view();
        snprintf(g_status, sizeof(g_status), "ZOOM FIT");
        return;
    }

    int vx, vy, vw, vh;
    editor_view_bounds(lv, &vx, &vy, &vw, &vh);
    int cx = vx + vw / 2;
    int cy = vy + vh / 2;
    int max_dim = editor_max_dim(lv);

    const int levels[] = {8, 12, 16, 24, 32, 48, 64, 96, 128, 192};
    const int count = (int)(sizeof(levels) / sizeof(levels[0]));
    int next = max_dim;

    for (int i = 0; i < count; i++) {
        if (levels[i] > g_editor_zoom_tiles) {
            next = levels[i];
            break;
        }
    }

    if (next >= max_dim) {
        editor_reset_view();
        snprintf(g_status, sizeof(g_status), "ZOOM FIT");
        return;
    }

    g_editor_zoom_tiles = next;
    editor_set_center(lv, cx, cy);
    snprintf(g_status, sizeof(g_status), "ZOOM %dX%d", next, next);
}

void editor_pan_view(const Level *lv, int dx, int dy) {
    if (!lv || g_editor_zoom_tiles <= 0) return;
    int old_x = g_editor_view_x;
    int old_y = g_editor_view_y;
    g_editor_view_x += dx;
    g_editor_view_y += dy;
    editor_clamp_view(lv);
    if (g_editor_view_x != old_x || g_editor_view_y != old_y) {
        snprintf(g_status, sizeof(g_status), "VIEW %d %d", g_editor_view_x, g_editor_view_y);
    }
}

static void update_view_bob_from_speed(float dt, float speed, bool on_ground) {
    if (!g_view_bob || !on_ground || speed < 0.03f) {
        g_camera_speed += (0.0f - g_camera_speed) * clampf32(dt * 12.0f, 0.0f, 1.0f);
        g_bob_amount += (0.0f - g_bob_amount) * clampf32(dt * 10.0f, 0.0f, 1.0f);
        return;
    }

    float target_speed = clampf32(speed, 0.0f, MOVE_SPEED * SPRINT_MULT * 1.15f);
    float smooth = clampf32(dt * 14.0f, 0.0f, 1.0f);
    g_camera_speed += (target_speed - g_camera_speed) * smooth;

    float speed01 = clampf32(g_camera_speed / (MOVE_SPEED * SPRINT_MULT), 0.0f, 1.0f);
    float target_amount = speed01 * 2.20f;
    g_bob_amount += (target_amount - g_bob_amount) * smooth;

    float phase_speed = 5.5f + g_camera_speed * 1.45f;
    g_bob_phase += phase_speed * dt;
    while (g_bob_phase >= TWO_PI_F) g_bob_phase -= TWO_PI_F;
}


static uint32_t rng_next(uint32_t *state) {
    uint32_t x = *state ? *state : 0xA341316Cu;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x ? x : 0xC8013EA4u;
    return *state;
}

void reset_runtime_entities(void) {
    memset(g_enemies, 0, sizeof(g_enemies));
    g_enemy_count = 0;
    g_has_success = false;
    g_success_x = 0.0f;
    g_success_y = 0.0f;
    g_level_won = false;
}

void spawn_entities_from_level(const Level *lv) {
    reset_runtime_entities();
    if (!lv || lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;

    for (int y = 1; y < lv->height - 1; y++) {
        for (int x = 1; x < lv->width - 1; x++) {
            uint8_t t = lv->tiles[y * lv->width + x] & MAX_TILE_ID;
            if (t == TILE_AI_SPAWN && g_enemy_count < MAX_ENEMIES) {
                Enemy *e = &g_enemies[g_enemy_count++];
                memset(e, 0, sizeof(*e));
                e->active = true;
                e->x = e->start_x = (float)x + 0.5f;
                e->y = e->start_y = (float)y + 0.5f;
                e->z = e->start_z = ground_height_at(lv, e->x, e->y, 0.0f);
                e->angle = ((float)((x * 37 + y * 101) & 255) / 255.0f) * TWO_PI_F;
                e->speed = 1.35f;
                e->roam_timer = 0.25f + (float)((x + y) & 7) * 0.18f;
                e->hp = 1;
                e->state = 0;
            } else if (t == TILE_SUCCESS && !g_has_success) {
                g_has_success = true;
                g_success_x = (float)x + 0.5f;
                g_success_y = (float)y + 0.5f;
            }
        }
    }
}

static bool ai_line_of_sight(const Level *lv, float x0, float y0, float x1, float y1) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.001f) return true;

    int steps = (int)(dist * 8.0f);
    if (steps < 1) steps = 1;
    if (steps > 160) steps = 160;

    for (int i = 1; i < steps; i++) {
        float t = (float)i / (float)steps;
        int tx = (int)(x0 + dx * t);
        int ty = (int)(y0 + dy * t);
        if (tx < 0 || ty < 0 || tx >= lv->width || ty >= lv->height) return false;
        if (tile_blocks_side(lv->tiles[ty * lv->width + tx], 0.0f)) return false;
    }

    return true;
}

static void reset_player_after_caught(Level *lv) {
    if (!lv) return;
    lv->player_x = g_play_start_x;
    lv->player_y = g_play_start_y;
    lv->player_z = g_play_start_z;
    lv->player_vz = 0.0f;
    lv->player_angle = g_play_start_angle;
    lv->on_ground = true;

    for (int i = 0; i < g_enemy_count; i++) {
        Enemy *e = &g_enemies[i];
        if (!e->active) continue;
        e->x = e->start_x;
        e->y = e->start_y;
        e->z = e->start_z;
        e->angle += 1.57f;
    }

    snprintf(g_status, sizeof(g_status), "CAUGHT - RESET");
}

static void ai_pick_roam_angle(Enemy *e) {
    if (!e) return;
    uint32_t r = rng_next(&g_random_seed);
    e->angle = ((float)(r & 4095) / 4096.0f) * TWO_PI_F;
    r = rng_next(&g_random_seed);
    e->roam_timer = 0.75f + ((float)(r & 1023) / 1023.0f) * 1.75f;
}

static bool ai_can_spot_player(const Level *lv, const Enemy *e, float to_px, float to_py, float dist2) {
    if (!lv || !e) return false;
    const float sight_range = 13.0f;
    if (dist2 > sight_range * sight_range) return false;
    if (!ai_line_of_sight(lv, e->x, e->y, lv->player_x, lv->player_y)) return false;

    if (dist2 < 2.25f) return true;

    float dist = sqrtf(dist2);
    if (dist < 0.001f) return true;
    float look_x = cosf(e->angle);
    float look_y = sinf(e->angle);
    float dot = (look_x * to_px + look_y * to_py) / dist;
    return dot > 0.34f;
}

static void update_enemies(Level *lv, float dt) {
    if (!lv || g_level_won) return;

    for (int i = 0; i < g_enemy_count; i++) {
        Enemy *e = &g_enemies[i];
        if (!e->active) continue;

        float to_px = lv->player_x - e->x;
        float to_py = lv->player_y - e->y;
        float dist2 = to_px * to_px + to_py * to_py;

        if (dist2 < 0.18f) {
            reset_player_after_caught(lv);
            return;
        }

        bool chase = ai_can_spot_player(lv, e, to_px, to_py, dist2);
        float dir_x;
        float dir_y;

        if (chase && dist2 > 0.001f) {
            float inv_dist = 1.0f / sqrtf(dist2);
            dir_x = to_px * inv_dist;
            dir_y = to_py * inv_dist;
            e->angle = atan2f(dir_y, dir_x);
            e->state = 1;
        } else {
            e->state = 0;
            e->roam_timer -= dt;
            if (e->roam_timer <= 0.0f) ai_pick_roam_angle(e);
            dir_x = cosf(e->angle);
            dir_y = sinf(e->angle);
        }

        float step = e->speed * (chase ? 1.28f : 0.48f) * dt;
        float nx = e->x + dir_x * step;
        float ny = e->y + dir_y * step;
        bool moved = false;

        if (can_stand_at(lv, nx, e->y, e->z)) {
            e->x = nx;
            moved = true;
        }
        if (can_stand_at(lv, e->x, ny, e->z)) {
            e->y = ny;
            moved = true;
        }

        if (!moved) {
            if (chase) e->angle += 0.75f;
            ai_pick_roam_angle(e);
        }
    }
}

static void check_success_tile(Level *lv) {
    if (!lv || g_level_won) return;
    int tx = (int)lv->player_x;
    int ty = (int)lv->player_y;
    if (tx < 0 || ty < 0 || tx >= lv->width || ty >= lv->height) return;

    if ((lv->tiles[ty * lv->width + tx] & MAX_TILE_ID) == TILE_SUCCESS) {
        g_level_won = true;
        snprintf(g_status, sizeof(g_status), g_random_play ? "SUCCESS A NEXT START MENU" : "SUCCESS START MENU");
    }
}

static void maze_carve_rect(Level *lv, int x0, int y0, int w, int h, uint8_t tile) {
    if (!lv) return;
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            if (x > 0 && y > 0 && x < lv->width - 1 && y < lv->height - 1) {
                lv->tiles[y * lv->width + x] = tile & MAX_TILE_ID;
            }
        }
    }
}

static void maze_cell_origin(int cx, int cy, int *ox, int *oy) {
    if (ox) *ox = 2 + cx * 4;
    if (oy) *oy = 2 + cy * 4;
}

static void maze_carve_cell(Level *lv, int cx, int cy) {
    int ox, oy;
    maze_cell_origin(cx, cy, &ox, &oy);
    maze_carve_rect(lv, ox, oy, 2, 2, 0);
}

static void maze_carve_connection(Level *lv, int cx, int cy, int nx, int ny) {
    int ox, oy;
    maze_cell_origin(cx, cy, &ox, &oy);

    if (nx > cx) maze_carve_rect(lv, ox + 2, oy, 2, 2, 0);
    else if (nx < cx) maze_carve_rect(lv, ox - 2, oy, 2, 2, 0);
    else if (ny > cy) maze_carve_rect(lv, ox, oy + 2, 2, 2, 0);
    else if (ny < cy) maze_carve_rect(lv, ox, oy - 2, 2, 2, 0);
}

void generate_random_maze(Level *lv, uint32_t seed) {
    if (!lv) return;
    if (seed == 0) seed = (uint32_t)svcGetSystemTick();
    g_random_seed = seed ? seed : 1u;

    memset(lv, 0, sizeof(*lv));
    lv->width = RANDOM_MAZE_W;
    lv->height = RANDOM_MAZE_H;

    int n = lv->width * lv->height;
    for (int i = 0; i < n; i++) lv->tiles[i] = 1;

    const int cells_w = RANDOM_MAZE_CELLS_W;
    const int cells_h = RANDOM_MAZE_CELLS_H;
    const int cell_count = cells_w * cells_h;

    uint8_t *visited = (uint8_t*)calloc((size_t)cell_count, 1);
    int *stack_x = (int*)malloc(sizeof(int) * (size_t)cell_count);
    int *stack_y = (int*)malloc(sizeof(int) * (size_t)cell_count);
    if (!visited || !stack_x || !stack_y) {
        free(visited);
        free(stack_x);
        free(stack_y);
        new_open_level(lv);
        return;
    }

    int top = 0;
    stack_x[top] = 0;
    stack_y[top] = 0;
    top++;
    visited[0] = 1;
    maze_carve_cell(lv, 0, 0);

    while (top > 0) {
        int cx = stack_x[top - 1];
        int cy = stack_y[top - 1];
        int dirs[4] = {0, 1, 2, 3};

        for (int i = 3; i > 0; i--) {
            int j = (int)(rng_next(&g_random_seed) % (uint32_t)(i + 1));
            int tmp = dirs[i];
            dirs[i] = dirs[j];
            dirs[j] = tmp;
        }

        bool carved = false;
        for (int i = 0; i < 4; i++) {
            int dx = 0;
            int dy = 0;
            if (dirs[i] == 0) dx = 1;
            else if (dirs[i] == 1) dx = -1;
            else if (dirs[i] == 2) dy = 1;
            else dy = -1;

            int nx = cx + dx;
            int ny = cy + dy;
            if (nx < 0 || ny < 0 || nx >= cells_w || ny >= cells_h) continue;
            int ni = ny * cells_w + nx;
            if (visited[ni]) continue;

            maze_carve_connection(lv, cx, cy, nx, ny);
            maze_carve_cell(lv, nx, ny);
            visited[ni] = 1;
            if (top < cell_count) {
                stack_x[top] = nx;
                stack_y[top] = ny;
                top++;
            }
            carved = true;
            break;
        }

        if (!carved) top--;
    }

    free(visited);
    free(stack_x);
    free(stack_y);

    int start_ox, start_oy;
    maze_cell_origin(0, 0, &start_ox, &start_oy);
    lv->player_x = (float)start_ox + 1.0f;
    lv->player_y = (float)start_oy + 1.0f;
    lv->player_z = 0.0f;
    lv->player_vz = 0.0f;
    lv->player_angle = 0.0f;
    lv->on_ground = true;

    int exit_ox, exit_oy;
    maze_cell_origin(cells_w - 1, cells_h - 1, &exit_ox, &exit_oy);
    maze_carve_rect(lv, exit_ox, exit_oy, 2, 2, TILE_SUCCESS);

    int placed = 0;
    int target_enemies = 5;
    int attempts = 0;
    while (placed < target_enemies && attempts < 4000) {
        attempts++;
        int cx = (int)(rng_next(&g_random_seed) % (uint32_t)cells_w);
        int cy = (int)(rng_next(&g_random_seed) % (uint32_t)cells_h);
        if (cx < 4 && cy < 4) continue;
        if (cx == cells_w - 1 && cy == cells_h - 1) continue;

        int ox, oy;
        maze_cell_origin(cx, cy, &ox, &oy);
        if (lv->tiles[oy * lv->width + ox] != 0) continue;
        lv->tiles[oy * lv->width + ox] = TILE_AI_SPAWN;
        placed++;
    }

    snprintf(g_level_name, sizeof(g_level_name), "RANDOM %04lX", (unsigned long)(seed & 0xFFFFu));
    force_valid_spawn(lv);
}

static void enter_random_seed_play(void) {
    uint32_t seed = (uint32_t)svcGetSystemTick();
    generate_random_maze(&g_level, seed);
    g_random_play = true;
    g_edit_mode = false;
    g_in_menu = false;
    g_resize_menu = false;
    g_duplicate_menu = false;
    g_delete_armed = false;
    g_dirty = false;
    g_camera_pitch = 0.0f;
    s_touch_look_active = false;
    editor_reset_view();
    g_play_start_x = g_level.player_x;
    g_play_start_y = g_level.player_y;
    g_play_start_z = g_level.player_z;
    g_play_start_angle = g_level.player_angle;
    spawn_entities_from_level(&g_level);
    snprintf(g_status, sizeof(g_status), "RANDOM SEED %04lX", (unsigned long)(seed & 0xFFFFu));
}

void enter_slot(bool edit_mode) {
    if (!load_bwl_slot(&g_level)) {
        new_open_level(&g_level);
        default_slot_name(g_slot, g_level_name, sizeof(g_level_name));
        g_dirty = true;
        snprintf(g_status, sizeof(g_status), "NEW SLOT %d", g_slot);
    }

    g_edit_mode = edit_mode;
    g_in_menu = false;
    g_resize_menu = false;
    g_duplicate_menu = false;
    g_delete_armed = false;
    g_camera_pitch = 0.0f;
    s_touch_look_active = false;
    editor_reset_view();
    g_random_play = false;
    g_play_start_x = g_level.player_x;
    g_play_start_y = g_level.player_y;
    g_play_start_z = g_level.player_z;
    g_play_start_angle = g_level.player_angle;
    if (edit_mode) reset_runtime_entities();
    else spawn_entities_from_level(&g_level);
}

void apply_resize_menu(void) {
    if (load_bwl_slot_index(&g_preview_level, g_slot, false)) {
        resize_level_preserve(&g_preview_level, (uint16_t)g_resize_w, (uint16_t)g_resize_h);
    } else {
        new_sized_level(&g_preview_level, (uint16_t)g_resize_w, (uint16_t)g_resize_h);
    }

    char name[LEVEL_NAME_MAX + 1];
    if (!load_slot_meta(g_slot, name, sizeof(name))) default_slot_name(g_slot, name, sizeof(name));
    if (save_bwl2_slot(&g_preview_level, g_slot, name, true)) {
        refresh_slot_info(g_slot);
        refresh_preview_slot();
    }
    g_resize_menu = false;
}


static void reset_settings_defaults(void) {
    g_fov_degrees = FOV_DEGREES;
    g_level_depth = 1.0f;
    g_view_bob = true;
    g_3d_enabled = false;
    g_dof_enabled = false;
    g_dof_start = 10.0f;
    g_dof_strength = 0.55f;
    g_antialiasing = false;
    g_fast_render = false;
    g_debug_overlay = false;
    g_camera_pitch = 0.0f;
    g_bob_phase = 0.0f;
    g_bob_amount = 0.0f;
    g_camera_speed = 0.0f;
    save_app_settings();
    snprintf(g_status, sizeof(g_status), "SETTINGS DEFAULT SAVED");
}

static void adjust_setting(int dir) {
    if (dir == 0) return;

    switch (g_settings_cursor) {
        case 0:
            g_fov_degrees = clampf32(g_fov_degrees + (float)(dir * 2), FOV_MIN_DEGREES, FOV_MAX_DEGREES);
            snprintf(g_status, sizeof(g_status), "FOV %d", (int)(g_fov_degrees + 0.5f));
            break;
        case 1:
            g_level_depth = clampf32(g_level_depth + (float)dir * 0.10f, 0.50f, 2.00f);
            snprintf(g_status, sizeof(g_status), "DEPTH %d", (int)(g_level_depth * 100.0f + 0.5f));
            break;
        case 2:
            g_view_bob = !g_view_bob;
            if (!g_view_bob) g_bob_phase = 0.0f;
            snprintf(g_status, sizeof(g_status), "BOB %s", g_view_bob ? "ON" : "OFF");
            break;
        case 3:
            g_3d_enabled = !g_3d_enabled;
            snprintf(g_status, sizeof(g_status), "3D %s", g_3d_enabled ? "ON" : "OFF");
            break;
        case 4:
            g_dof_enabled = !g_dof_enabled;
            snprintf(g_status, sizeof(g_status), "DOF %s", g_dof_enabled ? "ON" : "OFF");
            break;
        case 5:
            g_dof_start = clampf32(g_dof_start + (float)dir, 4.0f, 32.0f);
            snprintf(g_status, sizeof(g_status), "DOF DIST %d", (int)(g_dof_start + 0.5f));
            break;
        case 6:
            g_dof_strength = clampf32(g_dof_strength + (float)dir * 0.10f, 0.10f, 1.00f);
            snprintf(g_status, sizeof(g_status), "DOF STR %d", (int)(g_dof_strength * 100.0f + 0.5f));
            break;
        case 7:
            g_antialiasing = !g_antialiasing;
            snprintf(g_status, sizeof(g_status), "AA %s", g_antialiasing ? "ON" : "OFF");
            break;
        case 8:
            g_fast_render = !g_fast_render;
            snprintf(g_status, sizeof(g_status), "FAST RENDER %s", g_fast_render ? "ON" : "OFF");
            break;
        case 9:
            g_debug_overlay = !g_debug_overlay;
            snprintf(g_status, sizeof(g_status), "DEBUG %s", g_debug_overlay ? "ON" : "OFF");
            break;
    }

    save_app_settings();
}

static void handle_settings_menu_input(u32 kDown) {
    if (kDown & KEY_DUP) g_settings_cursor = (g_settings_cursor + SETTINGS_ROW_COUNT - 1) % SETTINGS_ROW_COUNT;
    if (kDown & KEY_DDOWN) g_settings_cursor = (g_settings_cursor + 1) % SETTINGS_ROW_COUNT;

    if (kDown & KEY_DLEFT) adjust_setting(-1);
    if (kDown & KEY_DRIGHT) adjust_setting(1);
    if (kDown & KEY_A) adjust_setting(1);
    if (kDown & KEY_X) reset_settings_defaults();

    if (kDown & KEY_B) {
        g_settings_menu = false;
        snprintf(g_status, sizeof(g_status), "SETTINGS CLOSED");
    }
}

void handle_world_menu_input(u32 kDown) {
    if (g_settings_menu) {
        handle_settings_menu_input(kDown);
        return;
    }

    if (g_resize_menu) {
        if (kDown & KEY_DLEFT) g_resize_w = clampi32(g_resize_w - MAP_SIZE_STEP, MIN_MAP_W, MAX_MAP_W);
        if (kDown & KEY_DRIGHT) g_resize_w = clampi32(g_resize_w + MAP_SIZE_STEP, MIN_MAP_W, MAX_MAP_W);
        if (kDown & KEY_DDOWN) g_resize_h = clampi32(g_resize_h - MAP_SIZE_STEP, MIN_MAP_H, MAX_MAP_H);
        if (kDown & KEY_DUP) g_resize_h = clampi32(g_resize_h + MAP_SIZE_STEP, MIN_MAP_H, MAX_MAP_H);
        if (kDown & KEY_X) { g_resize_w = DEFAULT_MAP_W; g_resize_h = DEFAULT_MAP_H; }
        if (kDown & KEY_B) g_resize_menu = false;
        if (kDown & KEY_A) apply_resize_menu();
        return;
    }

    if (g_duplicate_menu) {
        if (kDown & KEY_DUP) g_dup_target = (g_dup_target + 1) % SLOT_COUNT;
        if (kDown & KEY_DDOWN) g_dup_target = (g_dup_target + SLOT_COUNT - 1) % SLOT_COUNT;
        if (kDown & KEY_B) g_duplicate_menu = false;
        if (kDown & KEY_A) {
            duplicate_slot(g_dup_source, g_dup_target);
            g_duplicate_menu = false;
        }
        return;
    }

    if (kDown & KEY_DUP) {
        g_slot = (g_slot + SLOT_COUNT - 1) % SLOT_COUNT;
        refresh_preview_slot();
        g_delete_armed = false;
    }
    if (kDown & KEY_DDOWN) {
        g_slot = (g_slot + 1) % SLOT_COUNT;
        refresh_preview_slot();
        g_delete_armed = false;
    }
    if (kDown & KEY_DLEFT) {
        g_menu_action = (g_menu_action + MENU_ACTION_COUNT - 1) % MENU_ACTION_COUNT;
        g_delete_armed = false;
    }
    if (kDown & KEY_DRIGHT) {
        g_menu_action = (g_menu_action + 1) % MENU_ACTION_COUNT;
        g_delete_armed = false;
    }
    if (kDown & KEY_B) {
        g_delete_armed = false;
    }

    if (kDown & KEY_A) {
        switch (g_menu_action) {
            case MENU_ACTION_PLAY:
                enter_slot(false);
                break;
            case MENU_ACTION_RANDOM:
                enter_random_seed_play();
                break;
            case MENU_ACTION_EDIT:
                enter_slot(true);
                break;
            case MENU_ACTION_RESIZE:
                g_resize_w = g_slots[g_slot].exists ? g_slots[g_slot].width : DEFAULT_MAP_W;
                g_resize_h = g_slots[g_slot].exists ? g_slots[g_slot].height : DEFAULT_MAP_H;
                g_resize_menu = true;
                break;
            case MENU_ACTION_DUPLICATE:
                g_dup_source = g_slot;
                g_dup_target = (g_slot + 1) % SLOT_COUNT;
                g_duplicate_menu = true;
                break;
            case MENU_ACTION_RENAME:
                prompt_rename_slot(g_slot);
                break;
            case MENU_ACTION_DELETE:
                if (!g_delete_armed) {
                    g_delete_armed = true;
                    snprintf(g_status, sizeof(g_status), "CONFIRM DELETE S%d", g_slot);
                } else {
                    delete_slot(g_slot);
                    g_delete_armed = false;
                }
                break;
            case MENU_ACTION_SETTINGS:
                g_settings_menu = true;
                g_delete_armed = false;
                snprintf(g_status, sizeof(g_status), "SETTINGS");
                break;
        }
    }
}

void update_physics_and_movement(float dt, u32 kDown, u32 kHeld) {
    Level *lv = &g_level;

    if (g_level_won) {
        update_view_bob_from_speed(dt, 0.0f, true);
        if (g_random_play && (kDown & KEY_A)) {
            enter_random_seed_play();
        }
        return;
    }

    apply_vertical_physics(lv, dt, kDown, true);
    apply_lr_look(lv, dt, kHeld, true);
    apply_cstick_look(lv, dt, kHeld);
    apply_touch_look_play_only(lv, kHeld);
    float speed = 0.0f;
    apply_cpad_movement(lv, dt, kHeld, true, &speed);
    update_view_bob_from_speed(dt, speed, lv->on_ground);
    update_enemies(lv, dt);
    check_success_tile(lv);
}

static void update_editor_player(float dt, u32 kHeld) {
    Level *lv = &g_level;

    float old_x = lv->player_x;
    float old_y = lv->player_y;
    float old_z = lv->player_z;
    float old_angle = lv->player_angle;

    apply_vertical_physics(lv, dt, 0, false);
    apply_lr_look(lv, dt, kHeld, false);
    apply_cstick_look(lv, dt, kHeld);
    float speed = 0.0f;
    apply_cpad_movement(lv, dt, kHeld, false, &speed);
    update_view_bob_from_speed(dt, speed, lv->on_ground);

    if (fabsf(lv->player_x - old_x) > 0.00001f ||
        fabsf(lv->player_y - old_y) > 0.00001f ||
        fabsf(lv->player_z - old_z) > 0.00001f ||
        fabsf(lv->player_angle - old_angle) > 0.00001f) {
        g_dirty = true;
    }
}

static bool touch_in_rect(const touchPosition *tp, int x0, int y0, int x1, int y1) {
    return tp && tp->px >= x0 && tp->px < x1 && tp->py >= y0 && tp->py < y1;
}

static bool handle_editor_touch_controls(const Level *lv, const touchPosition *tp, u32 kDown) {
    if (!tp || !(kDown & KEY_TOUCH)) return false;
    if (tp->py < EDITOR_CTRL_Y) return false;

    if (tp->px < 180) return false;

    if (touch_in_rect(tp, 184, 221, 203, 240)) {
        editor_zoom_out(lv);
        return true;
    }
    if (touch_in_rect(tp, 206, 221, 225, 240)) {
        editor_zoom_in(lv);
        return true;
    }
    if (touch_in_rect(tp, 228, 221, 253, 240)) {
        editor_zoom_fit(lv);
        return true;
    }
    if (touch_in_rect(tp, 258, 221, 276, 240)) {
        editor_pan_view(lv, -1, 0);
        return true;
    }
    if (touch_in_rect(tp, 279, 212, 297, 226)) {
        editor_pan_view(lv, 0, -1);
        return true;
    }
    if (touch_in_rect(tp, 279, 226, 297, 240)) {
        editor_pan_view(lv, 0, 1);
        return true;
    }
    if (touch_in_rect(tp, 300, 221, 320, 240)) {
        editor_pan_view(lv, 1, 0);
        return true;
    }

    return tp->py >= EDITOR_CTRL_Y;
}

void editor_touch(u32 kDown, u32 kHeld) {
    if (!(kHeld & KEY_TOUCH)) return;

    touchPosition tp;
    hidTouchRead(&tp);

    Level *lv = &g_level;
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;

    editor_clamp_view(lv);

    if (handle_editor_touch_controls(lv, &tp, kDown)) return;

    if (tp.py >= EDITOR_CTRL_Y && tp.px < 180) {
        for (int t = 0; t <= MAX_TILE_ID; t++) {
            int row = t / 8;
            int col = t & 7;
            int x0 = PALETTE_X + col * PALETTE_STEP;
            int y0 = 214 + row * 13;
            if (tp.px >= x0 - 2 && tp.px <= x0 + PALETTE_W + 2 &&
                tp.py >= y0 - 1 && tp.py <= y0 + 11) {
                g_selected_tile = (uint8_t)t;
                snprintf(g_status, sizeof(g_status), "TILE %d", g_selected_tile);
                return;
            }
        }
        return;
    }

    int cell, ox, oy;
    int vx, vy, vw, vh;
    editor_layout(lv, &cell, &ox, &oy);
    editor_view_bounds(lv, &vx, &vy, &vw, &vh);

    int local_x = ((int)tp.px - ox) / cell;
    int local_y = ((int)tp.py - oy) / cell;
    int x = vx + local_x;
    int y = vy + local_y;
    if (local_x < 0 || local_y < 0 || local_x >= vw || local_y >= vh) return;
    if (x < 0 || y < 0 || x >= lv->width || y >= lv->height) return;

    if (kHeld & KEY_X) {
        if (tile_at(lv, x, y) >= 1 && tile_at(lv, x, y) <= 6) set_tile(lv, x, y, 0);
        lv->player_x = (float)x + 0.5f;
        lv->player_y = (float)y + 0.5f;
        lv->player_z = ground_height_at(lv, lv->player_x, lv->player_y, PLATFORM_TOP);
        lv->player_vz = 0.0f;
        lv->on_ground = true;
        g_dirty = true;
        snprintf(g_status, sizeof(g_status), "SPAWN %d %d", x, y);
    } else if (kHeld & KEY_B) {
        uint8_t old = tile_at(lv, x, y);
        set_tile(lv, x, y, 0);
        if (old != 0) g_dirty = true;
        snprintf(g_status, sizeof(g_status), "ERASE %d %d", x, y);
    } else {
        uint8_t old = tile_at(lv, x, y);
        set_tile(lv, x, y, g_selected_tile);
        if (old != g_selected_tile) g_dirty = true;
        snprintf(g_status, sizeof(g_status), "PAINT %d %d", x, y);
    }
}

void handle_global_input(u32 kDown, u32 kHeld) {
    if (kDown & KEY_START) {
        if (g_dirty) save_bwl2(&g_level);
        refresh_all_slots();
        refresh_preview_slot();
        g_in_menu = true;
        snprintf(g_status, sizeof(g_status), "RETURNED TO MENU");
        return;
    }

    if ((kDown & KEY_SELECT) && g_edit_mode) {
        if (g_dirty) save_bwl2(&g_level);
        refresh_all_slots();
        refresh_preview_slot();
        g_resize_w = g_level.width;
        g_resize_h = g_level.height;
        g_resize_menu = true;
        g_in_menu = true;
        snprintf(g_status, sizeof(g_status), "RESIZE CURRENT");
        return;
    }

    if ((g_edit_mode && (kDown & KEY_A)) || ((kDown & KEY_Y) && (kHeld & KEY_L))) {
        save_bwl2(&g_level);
        return;
    }

    if ((kDown & KEY_Y) && !(kHeld & KEY_L)) {
        load_bwl_slot(&g_level);
        return;
    }

    if ((kDown & KEY_X) && !g_edit_mode) {
        new_open_level(&g_level);
        g_dirty = true;
        snprintf(g_level_name, sizeof(g_level_name), "UNTITLED");
        snprintf(g_status, sizeof(g_status), "NEW LEVEL");
    }
}

void handle_editor_input(float dt, u32 kDown, u32 kHeld) {
    if (kDown & KEY_DUP) editor_zoom_in(&g_level);
    if (kDown & KEY_DDOWN) editor_zoom_out(&g_level);

    if (kDown & KEY_DLEFT) {
        g_selected_tile = (uint8_t)((g_selected_tile + MAX_TILE_ID) % (MAX_TILE_ID + 1));
        snprintf(g_status, sizeof(g_status), "TILE %d", g_selected_tile);
    }
    if (kDown & KEY_DRIGHT) {
        g_selected_tile = (uint8_t)((g_selected_tile + 1) % (MAX_TILE_ID + 1));
        snprintf(g_status, sizeof(g_status), "TILE %d", g_selected_tile);
    }

    update_editor_player(dt, kHeld);
    editor_touch(kDown, kHeld);
}

#include "common.h"

#define CSTICK_YAW_SPEED       2.60f
#define TOUCH_YAW_PER_PIXEL    0.0105f

static bool s_touch_look_active = false;
static int s_touch_last_x = 0;
static int s_touch_last_y = 0;

const char *editor_tool_name(int tool) {
    if (tool == EDITOR_TOOL_SELECT) return "SELECT";
    if (tool == EDITOR_TOOL_ROOM) return "ROOM";
    return "PAINT";
}

static const char *tile_short_name(uint8_t tile) {
    tile &= MAX_TILE_ID;
    switch (tile) {
        case 0: return "EMPTY";
        case PLATFORM_TILE: return "PLATFORM";
        case TILE_DOT: return "DOT 1";
        case TILE_PINK: return "PINK 5";
        case TILE_PURPLE: return "PURPLE 10";
        case TILE_NPC: return "NPC";
        case TILE_AI_SPAWN: return "AI";
        case TILE_SUCCESS: return "SUCCESS";
        case TILE_KEY: return "KEY";
        case TILE_DOOR: return "DOOR";
        default: break;
    }

    static char buf[12];
    snprintf(buf, sizeof(buf), "TILE %d", tile);
    return buf;
}

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


static bool ai_line_of_sight(const Level *lv, float x0, float y0, float x1, float y1);

static uint32_t rng_next(uint32_t *state) {
    uint32_t x = *state ? *state : 0xA341316Cu;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x ? x : 0xC8013EA4u;
    return *state;
}

const char *weapon_name(int weapon) {
    if (weapon < 0 || weapon >= MAX_WEAPONS) return "NONE";
    return g_weapons[weapon].name;
}

void randomize_weapon_stats(uint32_t seed) {
    uint32_t st = seed ? seed : 0x51C0FFEEu;
    const char *names[MAX_WEAPONS] = {"SWORD", "DAGGER", "KNIFE", "MACE", "MALLET", "AXE", "SPEAR", "STAFF"};
    const uint8_t base_damage[MAX_WEAPONS] = {4, 2, 2, 6, 8, 6, 4, 3};
    const uint8_t base_cool[MAX_WEAPONS] = {22, 10, 8, 32, 42, 36, 28, 30};
    for (int i = 0; i < MAX_WEAPONS; i++) {
        snprintf(g_weapons[i].name, sizeof(g_weapons[i].name), "%s", names[i]);
        uint32_t r = rng_next(&st);
        int delta = (int)(r % 5u) - 2;
        int dmg = (int)base_damage[i] + delta;
        if (dmg < 1) dmg = 1;
        if (dmg > 15) dmg = 15;
        g_weapons[i].damage = (uint8_t)dmg;
        g_weapons[i].range = (uint8_t)(1 + (rng_next(&st) % 2u));
        int cd = (int)base_cool[i] + (int)(rng_next(&st) % 9u) - 4;
        if (cd < 5) cd = 5;
        if (cd > 60) cd = 60;
        g_weapons[i].cooldown = (uint8_t)cd;
        g_weapons[i].color_id = (uint8_t)(i & 7);
        copy_default_sprite(g_weapons[i].sprite, SPRITE_TARGET_WEAPON);
    }
}

static const uint8_t DEFAULT_NPC_SPRITE[SPRITE_BYTES]    = {0x3C,0x7E,0xDB,0xFF,0x7E,0x3C,0x24,0x66};
static const uint8_t DEFAULT_ENEMY_SPRITE[SPRITE_BYTES]  = {0x18,0x3C,0x7E,0xDB,0xFF,0x7E,0x66,0xC3};
static const uint8_t DEFAULT_WEAPON_SPRITE[SPRITE_BYTES] = {0x10,0x38,0x10,0x10,0x10,0x54,0x38,0x10};
static const uint16_t DEFAULT_ENEMY_SPRITE_16[ENEMY_SPRITE_ROWS] = {
    0x0180,0x03C0,0x07E0,0x0E70,0x1FF8,0x1BD8,0x1FF8,0x0FF0,
    0x07E0,0x0660,0x0E70,0x1A58,0x324C,0x6246,0x4002,0x0000
};
static const uint16_t DEFAULT_NPC_SPRITE_16[ENEMY_SPRITE_ROWS] = {
    0x0FF0,0x0FF0,0x3FFC,0x3FFC,0xF3CF,0xF3CF,0xFFFF,0xFFFF,
    0x3FFC,0x3FFC,0x0FF0,0x0FF0,0x0C30,0x0C30,0x3C3C,0x3C3C
};
static const uint32_t DEFAULT_BOSS_SPRITE_32[BOSS_SPRITE_ROWS] = {
    0x0003C000u,0x000FF000u,0x003FFC00u,0x007FFE00u,0x00E66700u,0x01FFFFFFu,0x03DBDB80u,0x03FFFFFFu,
    0x03A5A580u,0x03FFFFFFu,0x01FFFF00u,0x00BFFC00u,0x007FF800u,0x00FFFF00u,0x01E7E780u,0x03C3C3C0u,
    0x078181E0u,0x0F0000F0u,0x1C000038u,0x3800001Cu,0x7000000Eu,0x60000006u,0x00000000u,0x00000000u,
    0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u
};

static bool sprite_is_empty(const uint8_t *sp) {
    if (!sp) return true;
    for (int i = 0; i < SPRITE_BYTES; i++) if (sp[i]) return false;
    return true;
}

void copy_default_sprite(uint8_t *dst, int kind) {
    if (!dst) return;
    const uint8_t *src = DEFAULT_NPC_SPRITE;
    if (kind == SPRITE_TARGET_ENEMY) src = DEFAULT_ENEMY_SPRITE;
    else if (kind == SPRITE_TARGET_WEAPON) src = DEFAULT_WEAPON_SPRITE;
    else if (kind == SPRITE_TARGET_DEFAULT_NPC) src = DEFAULT_NPC_SPRITE;
    memcpy(dst, src, SPRITE_BYTES);
}

static void ensure_sprite_or_default(uint8_t *dst, int kind) {
    if (sprite_is_empty(dst)) copy_default_sprite(dst, kind);
}

static bool enemy_sprite16_is_empty(const uint16_t *sp) {
    if (!sp) return true;
    for (int i = 0; i < ENEMY_SPRITE_ROWS; i++) if (sp[i]) return false;
    return true;
}

static bool npc_sprite16_is_empty(const uint16_t *sp) {
    return enemy_sprite16_is_empty(sp);
}

static bool boss_sprite_is_empty(const uint32_t *sp) {
    if (!sp) return true;
    for (int i = 0; i < BOSS_SPRITE_ROWS; i++) if (sp[i]) return false;
    return true;
}

void copy_default_enemy_sprite16(uint16_t *dst) {
    if (!dst) return;
    memcpy(dst, DEFAULT_ENEMY_SPRITE_16, sizeof(DEFAULT_ENEMY_SPRITE_16));
}

void copy_default_npc_sprite16(uint16_t *dst) {
    if (!dst) return;
    memcpy(dst, DEFAULT_NPC_SPRITE_16, sizeof(DEFAULT_NPC_SPRITE_16));
}

void copy_current_default_npc_sprite16(uint16_t *dst) {
    if (!dst) return;
    bool empty = true;
    for (int i = 0; i < ENEMY_SPRITE_ROWS; i++) if (g_default_npc_sprite16[i]) empty = false;
    if (empty) copy_default_npc_sprite16(g_default_npc_sprite16);
    memcpy(dst, g_default_npc_sprite16, sizeof(uint16_t) * ENEMY_SPRITE_ROWS);
}

void copy_default_boss_sprite(uint32_t *dst) {
    if (!dst) return;
    memcpy(dst, DEFAULT_BOSS_SPRITE_32, sizeof(DEFAULT_BOSS_SPRITE_32));
}

static void expand_8x8_to_16x16(uint16_t *dst, const uint8_t *src8) {
    if (!dst) return;
    memset(dst, 0, sizeof(uint16_t) * ENEMY_SPRITE_ROWS);
    if (!src8) { copy_default_enemy_sprite16(dst); return; }
    for (int y = 0; y < ENEMY_SPRITE_H; y++) {
        int sy = y / 2;
        uint16_t out = 0;
        for (int x = 0; x < ENEMY_SPRITE_W; x++) {
            int sx = x / 2;
            if (src8[sy] & (uint8_t)(1u << (7 - sx))) out |= (uint16_t)(1u << (ENEMY_SPRITE_W - 1 - x));
        }
        dst[y] = out;
    }
}

static void ensure_enemy_sprite16_or_default(uint16_t *dst, const uint8_t *fallback8) {
    if (!dst) return;
    if (enemy_sprite16_is_empty(dst)) {
        if (fallback8) expand_8x8_to_16x16(dst, fallback8);
        else copy_default_enemy_sprite16(dst);
    }
}

static void ensure_npc_sprite16_or_default(uint16_t *dst, const uint8_t *fallback8) {
    if (!dst) return;
    if (npc_sprite16_is_empty(dst)) {
        if (fallback8) expand_8x8_to_16x16(dst, fallback8);
        else copy_current_default_npc_sprite16(dst);
    }
}

static void ensure_boss_sprite_or_default(uint32_t *dst) {
    if (boss_sprite_is_empty(dst)) copy_default_boss_sprite(dst);
}

static uint32_t *sprite_edit_boss_pixels(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_BOSS) {
        EnemyMeta *m = enemy_meta_find_at(g_entity_edit_x, g_entity_edit_y);
        return m ? m->boss_sprite : NULL;
    }
    return NULL;
}


static uint16_t *sprite_edit_enemy16_pixels(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC) {
        NPC *n = npc_find_at(g_entity_edit_x, g_entity_edit_y);
        return n ? n->sprite16 : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return g_default_npc_sprite16;
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY) {
        EnemyMeta *m = enemy_meta_find_at(g_entity_edit_x, g_entity_edit_y);
        return m ? m->sprite16 : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_NPC_ANIM) {
        NPCAnim *a = npc_anim_ensure_at(g_entity_edit_x, g_entity_edit_y);
        if (!a) return NULL;
        g_anim_edit_state = clampi32(g_anim_edit_state, 0, 2);
        g_anim_edit_frame = clampi32(g_anim_edit_frame, 0, ANIM_FRAMES - 1);
        return a->frames[g_anim_edit_state][g_anim_edit_frame];
    }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY_ANIM) {
        EnemyAnim *a = enemy_anim_ensure_at(g_entity_edit_x, g_entity_edit_y);
        if (!a) return NULL;
        g_anim_edit_state = clampi32(g_anim_edit_state, 0, 3);
        g_anim_edit_frame = clampi32(g_anim_edit_frame, 0, ANIM_FRAMES - 1);
        return a->frames[g_anim_edit_state][g_anim_edit_frame];
    }
    return NULL;
}

static uint8_t *sprite_edit_pixels(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC) {
        return NULL; /* NPC art is edited as 16x16 now. */
    }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY) {
        EnemyMeta *m = enemy_meta_find_at(g_entity_edit_x, g_entity_edit_y);
        return m ? m->sprite : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_WEAPON) {
        int wi = clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1);
        return g_weapons[wi].sprite;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return NULL; /* Default NPC art is edited as 16x16 now. */
    return NULL;
}

static uint8_t *sprite_edit_color_ptr(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC) {
        NPC *n = npc_find_at(g_entity_edit_x, g_entity_edit_y);
        return n ? &n->color_id : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY) {
        EnemyMeta *m = enemy_meta_find_at(g_entity_edit_x, g_entity_edit_y);
        return m ? &m->color_id : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_WEAPON) {
        int wi = clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1);
        return &g_weapons[wi].color_id;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return &g_default_npc_color;
    if (g_sprite_edit_target == SPRITE_TARGET_NPC_ANIM) {
        NPC *n = npc_find_at(g_entity_edit_x, g_entity_edit_y);
        return n ? &n->color_id : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY_ANIM) {
        EnemyMeta *m = enemy_meta_find_at(g_entity_edit_x, g_entity_edit_y);
        return m ? &m->color_id : NULL;
    }
    return NULL;
}

void open_sprite_editor(int target) {
    g_sprite_edit_return_mode = g_entity_edit_mode;
    g_sprite_edit_target = target;
    g_entity_edit_mode = EDIT_MODE_SPRITE;
    g_sprite_edit_cursor_x = 0;
    g_sprite_edit_cursor_y = 0;
    uint32_t *bsp = sprite_edit_boss_pixels();
    uint16_t *esp = sprite_edit_enemy16_pixels();
    if (target == SPRITE_TARGET_NPC) { NPC *n = npc_find_at(g_entity_edit_x, g_entity_edit_y); if (n) ensure_npc_sprite16_or_default(n->sprite16, n->sprite); }
    if (target == SPRITE_TARGET_NPC_ANIM) {
        NPC *n = npc_find_at(g_entity_edit_x, g_entity_edit_y);
        NPCAnim *a = npc_anim_ensure_at(g_entity_edit_x, g_entity_edit_y);
        if (n && a) {
            g_anim_edit_state = clampi32(g_anim_edit_state, 0, 2);
            g_anim_edit_frame = clampi32(g_anim_edit_frame, 0, ANIM_FRAMES - 1);
            bool empty = true;
            for (int si = 0; si < ENEMY_SPRITE_ROWS; si++) if (a->frames[g_anim_edit_state][g_anim_edit_frame][si]) empty = false;
            if (empty) memcpy(a->frames[g_anim_edit_state][g_anim_edit_frame], n->sprite16, sizeof(uint16_t) * ENEMY_SPRITE_ROWS);
        }
    }
    if (target == SPRITE_TARGET_ENEMY_ANIM) {
        EnemyMeta *m = enemy_meta_find_at(g_entity_edit_x, g_entity_edit_y);
        EnemyAnim *a = enemy_anim_ensure_at(g_entity_edit_x, g_entity_edit_y);
        if (m && a) {
            g_anim_edit_state = clampi32(g_anim_edit_state, 0, 3);
            g_anim_edit_frame = clampi32(g_anim_edit_frame, 0, ANIM_FRAMES - 1);
            bool empty = true;
            for (int si = 0; si < ENEMY_SPRITE_ROWS; si++) if (a->frames[g_anim_edit_state][g_anim_edit_frame][si]) empty = false;
            if (empty) memcpy(a->frames[g_anim_edit_state][g_anim_edit_frame], m->sprite16, sizeof(uint16_t) * ENEMY_SPRITE_ROWS);
        }
    }
    if (target == SPRITE_TARGET_DEFAULT_NPC) ensure_npc_sprite16_or_default(g_default_npc_sprite16, g_default_npc_sprite);
    if (bsp) ensure_boss_sprite_or_default(bsp);
    else if (esp) {
        if (target == SPRITE_TARGET_NPC || target == SPRITE_TARGET_DEFAULT_NPC) ensure_npc_sprite16_or_default(esp, NULL);
        else ensure_enemy_sprite16_or_default(esp, NULL);
    } else {
        uint8_t *sp = sprite_edit_pixels();
        if (sp) ensure_sprite_or_default(sp, target == SPRITE_TARGET_DEFAULT_NPC ? SPRITE_TARGET_NPC : target);
    }
    snprintf(g_status, sizeof(g_status), target == SPRITE_TARGET_BOSS ? "BOSS ART 32X32" : ((target == SPRITE_TARGET_NPC || target == SPRITE_TARGET_DEFAULT_NPC || target == SPRITE_TARGET_NPC_ANIM || target == SPRITE_TARGET_ENEMY_ANIM) ? "ART 16X16" : "SPRITE EDIT"));
}

void handle_sprite_edit_input(u32 kDown) {
    uint8_t *sp = sprite_edit_pixels();
    uint32_t *bsp = sprite_edit_boss_pixels();
    uint16_t *esp = sprite_edit_enemy16_pixels();
    uint8_t *color = sprite_edit_color_ptr();
    bool boss_art = (bsp != NULL);
    bool enemy_art = (esp != NULL);
    if (!sp && !bsp && !esp) { g_entity_edit_mode = g_sprite_edit_return_mode; return; }

    int max_x = boss_art ? (BOSS_SPRITE_W - 1) : (enemy_art ? (ENEMY_SPRITE_W - 1) : 7);
    int max_y = boss_art ? (BOSS_SPRITE_H - 1) : (enemy_art ? (ENEMY_SPRITE_H - 1) : 7);

    if (kDown & KEY_TOUCH) {
        touchPosition tp;
        hidTouchRead(&tp);
        int grid_x = boss_art ? 88 : (enemy_art ? 108 : 96);
        int grid_y = boss_art ? 36 : (enemy_art ? 40 : 44);
        int grid_scale = boss_art ? 4 : (enemy_art ? 7 : 14);
        int px = ((int)tp.px - grid_x) / grid_scale;
        int py = ((int)tp.py - grid_y) / grid_scale;
        if (px >= 0 && py >= 0 && px <= max_x && py <= max_y) {
            g_sprite_edit_cursor_x = px;
            g_sprite_edit_cursor_y = py;
            if (boss_art) {
                uint32_t bit = (uint32_t)(1u << (BOSS_SPRITE_W - 1 - g_sprite_edit_cursor_x));
                bsp[g_sprite_edit_cursor_y] ^= bit;
            } else if (enemy_art) {
                uint16_t bit = (uint16_t)(1u << (ENEMY_SPRITE_W - 1 - g_sprite_edit_cursor_x));
                esp[g_sprite_edit_cursor_y] ^= bit;
            } else {
                uint8_t bit = (uint8_t)(1u << (7 - g_sprite_edit_cursor_x));
                sp[g_sprite_edit_cursor_y] ^= bit;
            }
            if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) g_settings_dirty = true;
            else g_dirty = true;
            snprintf(g_status, sizeof(g_status), "SPRITE PIXEL %d,%d", g_sprite_edit_cursor_x, g_sprite_edit_cursor_y);
            return;
        }
    }

    if (kDown & KEY_B) {
        g_entity_edit_mode = g_sprite_edit_return_mode;
        g_sprite_edit_target = 0;
        snprintf(g_status, sizeof(g_status), "SPRITE SAVED");
        return;
    }
    if (kDown & KEY_DLEFT)  g_sprite_edit_cursor_x = clampi32(g_sprite_edit_cursor_x - 1, 0, max_x);
    if (kDown & KEY_DRIGHT) g_sprite_edit_cursor_x = clampi32(g_sprite_edit_cursor_x + 1, 0, max_x);
    if (kDown & KEY_DUP)    g_sprite_edit_cursor_y = clampi32(g_sprite_edit_cursor_y - 1, 0, max_y);
    if (kDown & KEY_DDOWN)  g_sprite_edit_cursor_y = clampi32(g_sprite_edit_cursor_y + 1, 0, max_y);

    if (kDown & KEY_A) {
        if (boss_art) {
            uint32_t bit = (uint32_t)(1u << (BOSS_SPRITE_W - 1 - g_sprite_edit_cursor_x));
            bsp[g_sprite_edit_cursor_y] ^= bit;
        } else if (enemy_art) {
            uint16_t bit = (uint16_t)(1u << (ENEMY_SPRITE_W - 1 - g_sprite_edit_cursor_x));
            esp[g_sprite_edit_cursor_y] ^= bit;
        } else {
            uint8_t bit = (uint8_t)(1u << (7 - g_sprite_edit_cursor_x));
            sp[g_sprite_edit_cursor_y] ^= bit;
        }
        if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) g_settings_dirty = true;
        else g_dirty = true;
    }
    if (kDown & KEY_X) {
        if (boss_art) memset(bsp, 0, sizeof(uint32_t) * BOSS_SPRITE_ROWS);
        else if (enemy_art) memset(esp, 0, sizeof(uint16_t) * ENEMY_SPRITE_ROWS);
        else memset(sp, 0, SPRITE_BYTES);
        if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) g_settings_dirty = true;
        else g_dirty = true;
        snprintf(g_status, sizeof(g_status), "SPRITE CLEARED");
    }
    if (kDown & KEY_Y) {
        if (boss_art) copy_default_boss_sprite(bsp);
        else if (enemy_art) {
            if (g_sprite_edit_target == SPRITE_TARGET_NPC || g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC || g_sprite_edit_target == SPRITE_TARGET_NPC_ANIM) copy_default_npc_sprite16(esp);
            else copy_default_enemy_sprite16(esp);
        } else {
            int kind = g_sprite_edit_target;
            if (kind == SPRITE_TARGET_DEFAULT_NPC) kind = SPRITE_TARGET_NPC;
            copy_default_sprite(sp, kind);
        }
        if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) g_settings_dirty = true;
        else g_dirty = true;
        snprintf(g_status, sizeof(g_status), "SPRITE PRESET");
    }
    if (color && (kDown & (KEY_L | KEY_R))) {
        int d = (kDown & KEY_L) ? -1 : 1;
        *color = (uint8_t)((*color + (d < 0 ? 7 : 1)) & 7);
        if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) g_settings_dirty = true;
        else g_dirty = true;
        snprintf(g_status, sizeof(g_status), "COLOR %d", *color & 7);
    }
}

NPC *npc_find_at(int x, int y) {
    for (int i = 0; i < g_npc_count; i++) {
        if (g_npcs[i].active && g_npcs[i].x == x && g_npcs[i].y == y) return &g_npcs[i];
    }
    return NULL;
}

NPC *npc_ensure_at(int x, int y) {
    NPC *n = npc_find_at(x, y);
    if (n) return n;
    if (g_npc_count >= MAX_NPCS) return NULL;
    n = &g_npcs[g_npc_count++];
    memset(n, 0, sizeof(*n));
    n->active = true;
    n->x = x;
    n->y = y;
    n->color_id = g_default_npc_color & 7;
    n->text_mode = TEXT_MODE_NEAR;
    n->text_speed = TEXT_SPEED_MEDIUM;
    memcpy(n->sprite, g_default_npc_sprite, SPRITE_BYTES);
    ensure_sprite_or_default(n->sprite, SPRITE_TARGET_NPC);
    copy_current_default_npc_sprite16(n->sprite16);
    ensure_npc_sprite16_or_default(n->sprite16, n->sprite);
    n->talk_timer = 0.0f;
    n->quest_type = QUEST_COINS;
    n->quest_target = (uint16_t)(5 + ((x + y) % 6));
    n->reward_kind = (uint8_t)(REWARD_WEAPON_BASE + ((x + y) % MAX_WEAPONS));
    n->reward_amount = 1;
    snprintf(n->text, sizeof(n->text), "HELP ME AND ILL REWARD YOU");
    return n;
}

EnemyMeta *enemy_meta_find_at(int x, int y) {
    for (int i = 0; i < g_enemy_meta_count; i++) {
        if (g_enemy_metas[i].active && g_enemy_metas[i].x == x && g_enemy_metas[i].y == y) return &g_enemy_metas[i];
    }
    return NULL;
}

EnemyMeta *enemy_meta_ensure_at(int x, int y) {
    EnemyMeta *m = enemy_meta_find_at(x, y);
    if (m) return m;
    if (g_enemy_meta_count >= MAX_ENEMIES) return NULL;
    m = &g_enemy_metas[g_enemy_meta_count++];
    memset(m, 0, sizeof(*m));
    m->active = true;
    m->x = x;
    m->y = y;
    m->hp = (uint8_t)(5 + ((x + y) & 3));
    m->attack = 1;
    m->color_id = (uint8_t)((x + y) & 7);
    copy_default_sprite(m->sprite, SPRITE_TARGET_ENEMY);
    copy_default_enemy_sprite16(m->sprite16);
    copy_default_boss_sprite(m->boss_sprite);
    m->ai_rank = AI_RANK_GRUNT;
    m->spawn_kind = AI_SPAWN_NONE;
    m->spawn_limit = 0;
    m->command_range = 10;
    m->sight_range = 13;
    m->ranged_attack = 0;
    m->melee_range = 7;
    m->attack_cooldown = 7;
    m->spawn_cooldown = 25;
    m->projectile_color = m->color_id & 7;
    m->projectile_style = 0;
    m->projectile_anim = 1;
    m->speed_attr = 100;
    m->size_pct = 100;
    m->text_speed = TEXT_SPEED_MEDIUM;
    m->text_count = 3;
    snprintf(m->text[0], sizeof(m->text[0]), "HEY");
    snprintf(m->text[1], sizeof(m->text[1]), "OUCH");
    snprintf(m->text[2], sizeof(m->text[2]), "GET BACK HERE");
    return m;
}

static const char *reward_kind_name(int reward_kind) {
    static char buf[20];
    if (reward_kind == REWARD_DOT) return "DOT";
    if (reward_kind == REWARD_KEY) return "KEY";
    if (reward_kind == REWARD_PINK) return "PINK";
    if (reward_kind == REWARD_PURPLE) return "PURPLE";
    if (reward_kind == REWARD_HEALTH) return "HEALTH";
    if (reward_kind >= REWARD_WEAPON_BASE && reward_kind < REWARD_WEAPON_BASE + MAX_WEAPONS) {
        snprintf(buf, sizeof(buf), "%s", weapon_name(reward_kind - REWARD_WEAPON_BASE));
        return buf;
    }
    return "ITEM";
}

static void reward_to_code(const NPC *n, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    int amount = n ? (int)n->reward_amount : 1;
    if (amount < 1) amount = 1;
    int k = n ? (int)n->reward_kind : REWARD_DOT;
    if (k == REWARD_DOT) snprintf(out, out_size, "D%d", amount);
    else if (k == REWARD_KEY) snprintf(out, out_size, "K%d", amount);
    else if (k == REWARD_PINK) snprintf(out, out_size, "P%d", amount);
    else if (k == REWARD_PURPLE) snprintf(out, out_size, "U%d", amount);
    else if (k == REWARD_HEALTH) snprintf(out, out_size, "H%d", amount);
    else if (k >= REWARD_WEAPON_BASE && k < REWARD_WEAPON_BASE + MAX_WEAPONS) snprintf(out, out_size, "W%d", k - REWARD_WEAPON_BASE);
    else snprintf(out, out_size, "D1");
}

static bool parse_reward_code(const char *code, uint8_t *kind, uint16_t *amount) {
    if (!code || !kind || !amount) return false;
    while (*code == ' ') code++;
    char c0 = *code;
    if (c0 >= 'a' && c0 <= 'z') c0 = (char)(c0 - 32);

    int n = 0;
    for (const char *p = code; *p; p++) {
        if (*p >= '0' && *p <= '9') n = n * 10 + (*p - '0');
    }
    if (n < 1) n = 1;
    if (n > 99) n = 99;

    if (c0 == 'D' || c0 == 'C') { *kind = REWARD_DOT; *amount = (uint16_t)n; return true; }
    if (c0 == 'K') { *kind = REWARD_KEY; *amount = (uint16_t)n; return true; }
    if (c0 == 'P') { *kind = REWARD_PINK; *amount = (uint16_t)n; return true; }
    if (c0 == 'U') { *kind = REWARD_PURPLE; *amount = (uint16_t)n; return true; }
    if (c0 == 'H') { *kind = REWARD_HEALTH; *amount = (uint16_t)n; return true; }
    if (c0 == 'W' || c0 == 'S' || c0 == 'M') {
        int wi = 0;
        if (strstr(code, "DAG") || strstr(code, "dag")) wi = WEAPON_DAGGER;
        else if (strstr(code, "KNI") || strstr(code, "kni")) wi = WEAPON_KNIFE;
        else if (strstr(code, "MAC") || strstr(code, "mac")) wi = WEAPON_MACE;
        else if (strstr(code, "MAL") || strstr(code, "mal")) wi = WEAPON_MALLET;
        else if (strstr(code, "SWORD") || strstr(code, "sword")) wi = WEAPON_SWORD;
        else {
            wi = n;
            if (wi < 0) wi = 0;
            if (wi >= MAX_WEAPONS) wi = MAX_WEAPONS - 1;
        }
        *kind = (uint8_t)(REWARD_WEAPON_BASE + wi);
        *amount = 1;
        return true;
    }
    return false;
}

static void edit_npc_text_and_reward(NPC *n) {
    if (!n) return;
    char text[NPC_TEXT_MAX];
    snprintf(text, sizeof(text), "%s", n->text[0] ? n->text : "HELLO");
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, NPC_TEXT_MAX - 1);
    swkbdSetHintText(&swkbd, "NPC text above head");
    swkbdSetInitialText(&swkbd, text);
    SwkbdButton button = swkbdInputText(&swkbd, text, sizeof(text));
    if (button != SWKBD_BUTTON_NONE) {
        snprintf(n->text, sizeof(n->text), "%s", text);
    }

    char code[24];
    reward_to_code(n, code, sizeof(code));
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(code) - 1);
    swkbdSetHintText(&swkbd, "Reward D3 K1 P2 U1 W0-W4");
    swkbdSetInitialText(&swkbd, code);
    button = swkbdInputText(&swkbd, code, sizeof(code));
    if (button != SWKBD_BUTTON_NONE) {
        uint8_t kind;
        uint16_t amount;
        if (parse_reward_code(code, &kind, &amount)) {
            n->reward_kind = kind;
            n->reward_amount = amount;
        }
    }
    g_dirty = true;
    snprintf(g_status, sizeof(g_status), "NPC %s X%d", reward_kind_name(n->reward_kind), n->reward_amount);
}

static void split_enemy_text(EnemyMeta *m, const char *src) {
    if (!m || !src) return;
    for (int i = 0; i < ENEMY_TEXT_LINES; i++) m->text[i][0] = '\0';
    m->text_count = 0;
    int line = 0;
    int pos = 0;
    for (const char *p = src; *p && line < ENEMY_TEXT_LINES; p++) {
        if (*p == '|') {
            m->text[line][pos] = '\0';
            if (pos > 0) line++;
            pos = 0;
            continue;
        }
        if (pos < ENEMY_TEXT_MAX - 1) m->text[line][pos++] = *p;
    }
    if (line < ENEMY_TEXT_LINES) {
        m->text[line][pos] = '\0';
        if (pos > 0) line++;
    }
    m->text_count = (uint8_t)line;
    if (m->text_count == 0) {
        m->text_count = 1;
        snprintf(m->text[0], sizeof(m->text[0]), "GRR");
    }
}

static void edit_enemy_text_meta(EnemyMeta *m) {
    if (!m) return;
    char joined[ENEMY_TEXT_LINES * ENEMY_TEXT_MAX];
    joined[0] = '\0';
    for (int i = 0; i < m->text_count && i < ENEMY_TEXT_LINES; i++) {
        if (i > 0) strncat(joined, "|", sizeof(joined) - strlen(joined) - 1);
        strncat(joined, m->text[i], sizeof(joined) - strlen(joined) - 1);
    }
    if (!joined[0]) snprintf(joined, sizeof(joined), "HEY|OUCH|GET BACK");
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(joined) - 1);
    swkbdSetHintText(&swkbd, "Enemy lines separated by |");
    swkbdSetInitialText(&swkbd, joined);
    SwkbdButton button = swkbdInputText(&swkbd, joined, sizeof(joined));
    if (button != SWKBD_BUTTON_NONE) {
        split_enemy_text(m, joined);
        g_dirty = true;
        snprintf(g_status, sizeof(g_status), "AI TEXT %d LINES", m->text_count);
    }
}


static int prompt_int_value(const char *hint, int current, int lo, int hi) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", current);
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(buf) - 1);
    swkbdSetHintText(&swkbd, hint ? hint : "Number");
    swkbdSetInitialText(&swkbd, buf);
    SwkbdButton button = swkbdInputText(&swkbd, buf, sizeof(buf));
    if (button == SWKBD_BUTTON_NONE) return current;

    int sign = 1;
    int v = 0;
    const char *p = buf;
    while (*p == ' ') p++;
    if (*p == '-') { sign = -1; p++; }
    while (*p) {
        if (*p >= '0' && *p <= '9') v = v * 10 + (*p - '0');
        p++;
    }
    v *= sign;
    return clampi32(v, lo, hi);
}

static void prompt_weapon_name(int weapon) {
    if (weapon < 0 || weapon >= MAX_WEAPONS) return;
    char name[sizeof(g_weapons[weapon].name)];
    snprintf(name, sizeof(name), "%s", g_weapons[weapon].name[0] ? g_weapons[weapon].name : "WEAPON");
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(name) - 1);
    swkbdSetHintText(&swkbd, "Weapon name");
    swkbdSetInitialText(&swkbd, name);
    SwkbdButton button = swkbdInputText(&swkbd, name, sizeof(name));
    if (button != SWKBD_BUTTON_NONE) {
        snprintf(g_weapons[weapon].name, sizeof(g_weapons[weapon].name), "%s", name);
        g_dirty = true;
        snprintf(g_status, sizeof(g_status), "WEAPON NAME %s", g_weapons[weapon].name);
    }
}

static NPC *entity_editor_npc(void) {
    if (g_entity_edit_mode != EDIT_MODE_NPC) return NULL;
    return npc_find_at(g_entity_edit_x, g_entity_edit_y);
}

static EnemyMeta *entity_editor_enemy_meta(void) {
    if (g_entity_edit_mode != EDIT_MODE_ENEMY) return NULL;
    return enemy_meta_find_at(g_entity_edit_x, g_entity_edit_y);
}

static int entity_editor_row_count(void) {
    if (g_entity_edit_mode == EDIT_MODE_NPC) return 19;
    if (g_entity_edit_mode == EDIT_MODE_ENEMY) return 28;
    if (g_entity_edit_mode == EDIT_MODE_DOOR) return 9;
    if (g_entity_edit_mode == EDIT_MODE_WEAPON) return 8;
    if (g_entity_edit_mode == EDIT_MODE_SPRITE) return 1;
    return 1;
}

static const char *text_speed_name(uint8_t speed) {
    switch (speed) {
        case TEXT_SPEED_INSTANT: return "INSTANT";
        case TEXT_SPEED_SLOW: return "SLOW";
        case TEXT_SPEED_FAST: return "FAST";
        case TEXT_SPEED_MEDIUM:
        default: return "MEDIUM";
    }
}

static const char *ai_rank_name(uint8_t rank) {
    if (rank == AI_RANK_BOSS) return "BOSS";
    if (rank == AI_RANK_CAPTAIN) return "CAPTAIN";
    return "GRUNT";
}

static const char *ai_spawn_name(uint8_t kind) {
    if (kind == AI_SPAWN_GRUNT) return "GRUNT";
    return "NONE";
}

void close_entity_editor(void) {
    if (g_settings_dirty) {
        save_app_settings();
        g_settings_dirty = false;
    }
    g_entity_edit_mode = EDIT_MODE_NONE;
    g_entity_edit_cursor = 0;
    g_entity_edit_scroll = 0;
    snprintf(g_status, sizeof(g_status), "EDIT CLOSED");
}

void open_weapon_editor(int weapon) {
    g_entity_edit_mode = EDIT_MODE_WEAPON;
    g_entity_edit_cursor = 0;
    g_entity_edit_scroll = 0;
    g_entity_edit_weapon = clampi32(weapon, 0, MAX_WEAPONS - 1);
    snprintf(g_status, sizeof(g_status), "WEAPON EDIT %s", weapon_name(g_entity_edit_weapon));
}

void open_entity_editor_at(int x, int y) {
    uint8_t t = tile_at(&g_level, x, y);
    if (t == TILE_NPC) {
        NPC *n = npc_ensure_at(x, y);
        if (!n) return;
        g_entity_edit_mode = EDIT_MODE_NPC;
        g_entity_edit_cursor = 0;
        g_entity_edit_scroll = 0;
        g_entity_edit_x = x;
        g_entity_edit_y = y;
        snprintf(g_status, sizeof(g_status), "NPC EDIT %d %d", x, y);
    } else if (t == TILE_AI_SPAWN) {
        EnemyMeta *m = enemy_meta_ensure_at(x, y);
        if (!m) return;
        if (m->hp == 0) m->hp = (uint8_t)(5 + ((x + y) & 3));
        if (m->attack == 0) m->attack = 1;
        if (m->command_range == 0) m->command_range = 10;
        if (m->sight_range == 0) m->sight_range = 13;
        if (m->melee_range == 0) m->melee_range = 7;
        if (m->attack_cooldown == 0) m->attack_cooldown = 7;
        if (m->spawn_cooldown == 0) m->spawn_cooldown = 25;
        m->projectile_color &= 7;
        if (m->projectile_style > 2) m->projectile_style = 0;
        ensure_boss_sprite_or_default(m->boss_sprite);
        g_entity_edit_mode = EDIT_MODE_ENEMY;
        g_entity_edit_cursor = 0;
        g_entity_edit_scroll = 0;
        g_entity_edit_x = x;
        g_entity_edit_y = y;
        snprintf(g_status, sizeof(g_status), "ENEMY EDIT %d %d", x, y);
    } else if (t == TILE_DOOR) {
        DoorMeta *dm = door_meta_ensure_at(x, y);
        if (!dm) return;
        g_entity_edit_mode = EDIT_MODE_DOOR;
        g_entity_edit_cursor = 0;
        g_entity_edit_scroll = 0;
        g_entity_edit_x = x;
        g_entity_edit_y = y;
        snprintf(g_status, sizeof(g_status), "DOOR EDIT %d %d", x, y);
    }
}

static void cycle_npc_reward(NPC *n, int dir) {
    if (!n) return;
    int r = n->reward_kind;
    if (dir >= 0) {
        if (r == REWARD_DOT) r = REWARD_KEY;
        else if (r == REWARD_KEY) r = REWARD_PINK;
        else if (r == REWARD_PINK) r = REWARD_PURPLE;
        else if (r == REWARD_PURPLE) r = REWARD_HEALTH;
        else if (r == REWARD_HEALTH) r = REWARD_WEAPON_BASE;
        else if (r >= REWARD_WEAPON_BASE && r < REWARD_WEAPON_BASE + MAX_WEAPONS - 1) r++;
        else r = REWARD_DOT;
    } else {
        if (r == REWARD_DOT) r = REWARD_WEAPON_BASE + MAX_WEAPONS - 1;
        else if (r > REWARD_WEAPON_BASE && r < REWARD_WEAPON_BASE + MAX_WEAPONS) r--;
        else if (r == REWARD_WEAPON_BASE) r = REWARD_HEALTH;
        else if (r == REWARD_HEALTH) r = REWARD_PURPLE;
        else if (r == REWARD_PURPLE) r = REWARD_PINK;
        else if (r == REWARD_PINK) r = REWARD_KEY;
        else if (r == REWARD_KEY) r = REWARD_DOT;
        else r = REWARD_DOT;
    }
    n->reward_kind = (uint8_t)r;
    if (n->reward_amount < 1) n->reward_amount = 1;
}

static void cycle_npc_quest(NPC *n, int dir) {
    if (!n) return;
    int q = n->quest_type;
    q += (dir >= 0) ? 1 : -1;
    if (q < QUEST_NONE) q = QUEST_NPC;
    if (q > QUEST_NPC) q = QUEST_NONE;
    n->quest_type = (uint8_t)q;
    if (n->quest_type == QUEST_NONE) n->quest_target = 0;
    else if (n->quest_target == 0) n->quest_target = 1;
}

void handle_entity_edit_input(u32 kDown) {
    if (g_entity_edit_mode == EDIT_MODE_NONE) return;
    if (g_entity_edit_mode == EDIT_MODE_SPRITE) { handle_sprite_edit_input(kDown); return; }

    int rows = entity_editor_row_count();
    if (kDown & KEY_TOUCH) {
        touchPosition tp;
        hidTouchRead(&tp);
        int visible = (g_entity_edit_mode == EDIT_MODE_WEAPON) ? 8 : 12;
        int row = g_entity_edit_scroll + (((int)tp.py - 40) / 15);
        if (row >= g_entity_edit_scroll && row < g_entity_edit_scroll + visible && row < rows && tp.px >= 8 && tp.px <= BOT_W - 8) {
            g_entity_edit_cursor = row;
            kDown |= KEY_A;
        }
    }
    if (kDown & KEY_DUP) g_entity_edit_cursor = (g_entity_edit_cursor + rows - 1) % rows;
    if (kDown & KEY_DDOWN) g_entity_edit_cursor = (g_entity_edit_cursor + 1) % rows;
    int visible_rows = (g_entity_edit_mode == EDIT_MODE_WEAPON) ? 8 : 12;
    if (g_entity_edit_cursor < g_entity_edit_scroll) g_entity_edit_scroll = g_entity_edit_cursor;
    if (g_entity_edit_cursor >= g_entity_edit_scroll + visible_rows) g_entity_edit_scroll = g_entity_edit_cursor - visible_rows + 1;
    if (g_entity_edit_scroll < 0) g_entity_edit_scroll = 0;
    if (g_entity_edit_scroll > rows - visible_rows) g_entity_edit_scroll = rows > visible_rows ? rows - visible_rows : 0;
    if (kDown & KEY_B) { close_entity_editor(); return; }

    int dir = 0;
    if (kDown & KEY_DLEFT) dir = -1;
    if (kDown & KEY_DRIGHT) dir = 1;
    int big = 0;
    if (kDown & KEY_L) big = -1;
    if (kDown & KEY_R) big = 1;

    if (g_entity_edit_mode == EDIT_MODE_NPC) {
        NPC *n = entity_editor_npc();
        if (!n) { close_entity_editor(); return; }
        switch (g_entity_edit_cursor) {
            case 0: if (kDown & KEY_A) edit_npc_text_and_reward(n); break;
            case 1: if (dir || (kDown & KEY_A)) { cycle_npc_quest(n, dir >= 0 ? 1 : -1); g_dirty = true; } break;
            case 2:
                if (dir || big) { int step = big ? big * 10 : dir; n->quest_target = (uint16_t)clampi32((int)n->quest_target + step, 0, 999); g_dirty = true; }
                if (kDown & KEY_A) { n->quest_target = (uint16_t)prompt_int_value("Quest target", n->quest_target, 0, 999); g_dirty = true; }
                break;
            case 3: if (dir || (kDown & KEY_A)) { cycle_npc_reward(n, dir >= 0 ? 1 : -1); g_dirty = true; } break;
            case 4:
                if (dir || big) { int step = big ? big * 5 : dir; n->reward_amount = (uint16_t)clampi32((int)n->reward_amount + step, 1, 99); g_dirty = true; }
                if (kDown & KEY_A) { n->reward_amount = (uint16_t)prompt_int_value("Reward amount", n->reward_amount, 1, 99); g_dirty = true; }
                break;
            case 5: if (dir || (kDown & KEY_A)) { n->text_mode = (uint8_t)((n->text_mode + (dir < 0 ? 2 : 1)) % 3); g_dirty = true; } break;
            case 6: if (dir || (kDown & KEY_A)) { n->text_speed = (uint8_t)((n->text_speed + (dir < 0 ? 3 : 1)) % 4); g_dirty = true; } break;
            case 7: if (dir || (kDown & KEY_A)) { n->color_id = (uint8_t)((n->color_id + (dir < 0 ? 7 : 1)) & 7); g_dirty = true; } break;
            case 8: if (kDown & KEY_A) open_sprite_editor(SPRITE_TARGET_NPC); break;
            case 9: if (dir || (kDown & KEY_A)) { g_default_npc_color = (uint8_t)((g_default_npc_color + (dir < 0 ? 7 : 1)) & 7); g_settings_dirty = true; } break;
            case 10: if (kDown & KEY_A) open_sprite_editor(SPRITE_TARGET_DEFAULT_NPC); break;
            case 11: if (kDown & KEY_A) { memcpy(n->sprite, g_default_npc_sprite, SPRITE_BYTES); copy_current_default_npc_sprite16(n->sprite16); n->color_id = g_default_npc_color; g_dirty = true; } break;
            case 12:
                if (kDown & KEY_A) {
                    if (n->reward_kind >= REWARD_WEAPON_BASE && n->reward_kind < REWARD_WEAPON_BASE + MAX_WEAPONS) open_weapon_editor(n->reward_kind - REWARD_WEAPON_BASE);
                    else open_weapon_editor(0);
                }
                break;
            case 13: { NPCAnim *a = npc_anim_ensure_at(n->x, n->y); if (a && (dir || (kDown & KEY_A))) { a->enabled = !a->enabled; if (a->enabled && a->speed == ANIM_SPEED_OFF) a->speed = ANIM_SPEED_MEDIUM; g_dirty = true; } } break;
            case 14: { NPCAnim *a = npc_anim_ensure_at(n->x, n->y); if (a && (dir || (kDown & KEY_A))) { a->speed = (uint8_t)((a->speed + (dir < 0 ? 3 : 1)) % 4); a->enabled = a->speed != ANIM_SPEED_OFF; g_dirty = true; } } break;
            case 15: if (dir || (kDown & KEY_A)) { g_anim_edit_state = (g_anim_edit_state + (dir < 0 ? 2 : 1)) % 3; } break;
            case 16: if (dir || (kDown & KEY_A)) { g_anim_edit_frame = (g_anim_edit_frame + (dir < 0 ? ANIM_FRAMES - 1 : 1)) % ANIM_FRAMES; } break;
            case 17: if (kDown & KEY_A) { npc_anim_ensure_at(n->x, n->y); open_sprite_editor(SPRITE_TARGET_NPC_ANIM); } break;
            case 18: if (kDown & KEY_A) close_entity_editor(); break;
        }
        snprintf(g_status, sizeof(g_status), "NPC Q%d %s", n->quest_type, text_speed_name(n->text_speed));
        return;
    }

    if (g_entity_edit_mode == EDIT_MODE_ENEMY) {
        EnemyMeta *m = entity_editor_enemy_meta();
        if (!m) { close_entity_editor(); return; }
        if (m->speed_attr == 0) m->speed_attr = 100;
        if (m->size_pct == 0) m->size_pct = 100;
        if (m->text_speed > TEXT_SPEED_FAST) m->text_speed = TEXT_SPEED_MEDIUM;
        if (!m->melee_range) m->melee_range = (m->ai_rank == AI_RANK_BOSS) ? 10 : 7;
        if (!m->sight_range) m->sight_range = (m->ai_rank == AI_RANK_BOSS) ? 20 : (m->ai_rank == AI_RANK_CAPTAIN ? 16 : 13);
        if (!m->attack_cooldown) m->attack_cooldown = 7;
        if (!m->spawn_cooldown) m->spawn_cooldown = 25;
        m->projectile_color &= 7;
        if (m->projectile_style > 2) m->projectile_style = 0;
        switch (g_entity_edit_cursor) {
            case 0: if (kDown & KEY_A) edit_enemy_text_meta(m); break;
            case 1:
                if (dir || big) { int step = big ? big * 5 : dir; m->hp = (uint8_t)clampi32((int)(m->hp ? m->hp : 5) + step, 1, 255); g_dirty = true; }
                if (kDown & KEY_A) { m->hp = (uint8_t)prompt_int_value("Enemy health", m->hp ? m->hp : 5, 1, 255); g_dirty = true; }
                break;
            case 2:
                if (dir || big) { int step = big ? big * 2 : dir; m->attack = (uint8_t)clampi32((int)(m->attack ? m->attack : 1) + step, 0, 99); g_dirty = true; }
                if (kDown & KEY_A) { m->attack = (uint8_t)prompt_int_value("Enemy attack", m->attack ? m->attack : 1, 0, 99); g_dirty = true; }
                break;
            case 3:
                if (dir || big) { int step = big ? big * 10 : dir * 5; m->speed_attr = (uint8_t)clampi32((int)(m->speed_attr ? m->speed_attr : 100) + step, 25, 220); g_dirty = true; }
                if (kDown & KEY_A) { m->speed_attr = (uint8_t)prompt_int_value("Speed pct", m->speed_attr ? m->speed_attr : 100, 25, 220); g_dirty = true; }
                break;
            case 4:
                if (dir || big) { int max_sz = (m->ai_rank == AI_RANK_BOSS) ? 140 : 115; int step = big ? big * 10 : dir * 5; m->size_pct = (uint8_t)clampi32((int)(m->size_pct ? m->size_pct : 100) + step, 50, max_sz); g_dirty = true; }
                if (kDown & KEY_A) { int max_sz = (m->ai_rank == AI_RANK_BOSS) ? 140 : 115; m->size_pct = (uint8_t)prompt_int_value("Size pct", m->size_pct ? m->size_pct : 100, 50, max_sz); g_dirty = true; }
                break;
            case 5: if (dir || (kDown & KEY_A)) { m->text_speed = (uint8_t)((m->text_speed + (dir < 0 ? 3 : 1)) % 4); g_dirty = true; } break;
            case 6: if (dir || (kDown & KEY_A)) { m->color_id = (uint8_t)((m->color_id + (dir < 0 ? 7 : 1)) & 7); g_dirty = true; } break;
            case 7: if (dir || (kDown & KEY_A)) { m->ai_rank = (uint8_t)((m->ai_rank + (dir < 0 ? 2 : 1)) % 3); if (m->ai_rank != AI_RANK_BOSS && m->size_pct > 115) m->size_pct = 115; if (m->sight_range == 0 || (m->ai_rank == AI_RANK_BOSS && m->sight_range < 16)) m->sight_range = (m->ai_rank == AI_RANK_BOSS) ? 20 : (m->ai_rank == AI_RANK_CAPTAIN ? 16 : 13); g_dirty = true; } break;
            case 8:
                if (dir || (kDown & KEY_A)) { m->ranged_attack = m->ranged_attack ? 0 : 1; g_dirty = true; }
                break;
            case 9:
                if (dir || big) { int step = big ? big * 2 : dir; m->melee_range = (uint8_t)clampi32((int)(m->melee_range ? m->melee_range : 7) + step, 2, 30); g_dirty = true; }
                if (kDown & KEY_A) { m->melee_range = (uint8_t)prompt_int_value("Attack range x10", m->melee_range ? m->melee_range : 7, 2, 30); g_dirty = true; }
                break;
            case 10:
                if (dir || big) { int step = big ? big * 2 : dir; m->attack_cooldown = (uint8_t)clampi32((int)(m->attack_cooldown ? m->attack_cooldown : 7) + step, 2, 60); g_dirty = true; }
                if (kDown & KEY_A) { m->attack_cooldown = (uint8_t)prompt_int_value("Atk cooldown x10", m->attack_cooldown ? m->attack_cooldown : 7, 2, 60); g_dirty = true; }
                break;
            case 11:
                if (dir || big) { int step = big ? big * 2 : dir; m->spawn_limit = (uint8_t)clampi32((int)m->spawn_limit + step, 0, 8); g_dirty = true; }
                if (kDown & KEY_A) { m->spawn_limit = (uint8_t)prompt_int_value("Spawn limit", m->spawn_limit, 0, 8); g_dirty = true; }
                break;
            case 12: if (dir || (kDown & KEY_A)) { m->spawn_kind = m->spawn_kind ? AI_SPAWN_NONE : AI_SPAWN_GRUNT; g_dirty = true; } break;
            case 13:
                if (dir || big) { int step = big ? big * 5 : dir; m->spawn_cooldown = (uint8_t)clampi32((int)(m->spawn_cooldown ? m->spawn_cooldown : 25) + step, 5, 120); g_dirty = true; }
                if (kDown & KEY_A) { m->spawn_cooldown = (uint8_t)prompt_int_value("Spawn cooldown x10", m->spawn_cooldown ? m->spawn_cooldown : 25, 5, 120); g_dirty = true; }
                break;
            case 14:
                if (dir || big) { int step = big ? big * 4 : dir; m->sight_range = (uint8_t)clampi32((int)(m->sight_range ? m->sight_range : 13) + step, 2, 40); g_dirty = true; }
                if (kDown & KEY_A) { m->sight_range = (uint8_t)prompt_int_value("Sight range", m->sight_range ? m->sight_range : 13, 2, 40); g_dirty = true; }
                break;
            case 15:
                if (dir || big) { int step = big ? big * 4 : dir; m->command_range = (uint8_t)clampi32((int)(m->command_range ? m->command_range : 10) + step, 2, 32); g_dirty = true; }
                if (kDown & KEY_A) { m->command_range = (uint8_t)prompt_int_value("Command range", m->command_range ? m->command_range : 10, 2, 32); g_dirty = true; }
                break;
            case 16: if (dir || (kDown & KEY_A)) { m->projectile_color = (uint8_t)((m->projectile_color + (dir < 0 ? 7 : 1)) & 7); g_dirty = true; } break;
            case 17: if (dir || (kDown & KEY_A)) { m->projectile_style = (uint8_t)((m->projectile_style + (dir < 0 ? 2 : 1)) % 3); g_dirty = true; } break;
            case 18: if (dir || (kDown & KEY_A)) { m->projectile_anim = m->projectile_anim ? 0 : 1; g_dirty = true; } break;
            case 19: if (kDown & KEY_A) { ensure_enemy_sprite16_or_default(m->sprite16, m->sprite); open_sprite_editor(SPRITE_TARGET_ENEMY); } break;
            case 20: if (kDown & KEY_A) { m->ai_rank = AI_RANK_BOSS; ensure_boss_sprite_or_default(m->boss_sprite); open_sprite_editor(SPRITE_TARGET_BOSS); g_dirty = true; } break;
            case 21:
                if (dir || big) { int step = big ? big * 5 : dir; g_player_health_max = clampi32(g_player_health_max + step, PLAYER_HEALTH_MIN, PLAYER_HEALTH_MAX); if (g_player_health > g_player_health_max) g_player_health = g_player_health_max; g_dirty = true; }
                if (kDown & KEY_A) { g_player_health_max = prompt_int_value("Player max HP", g_player_health_max, PLAYER_HEALTH_MIN, PLAYER_HEALTH_MAX); if (g_player_health > g_player_health_max) g_player_health = g_player_health_max; g_dirty = true; }
                break;
            case 22: { EnemyAnim *a = enemy_anim_ensure_at(m->x, m->y); if (a && (dir || (kDown & KEY_A))) { a->enabled = !a->enabled; if (a->enabled && a->speed == ANIM_SPEED_OFF) a->speed = ANIM_SPEED_MEDIUM; g_dirty = true; } } break;
            case 23: { EnemyAnim *a = enemy_anim_ensure_at(m->x, m->y); if (a && (dir || (kDown & KEY_A))) { a->speed = (uint8_t)((a->speed + (dir < 0 ? 3 : 1)) % 4); a->enabled = a->speed != ANIM_SPEED_OFF; g_dirty = true; } } break;
            case 24: if (dir || (kDown & KEY_A)) { g_anim_edit_state = (g_anim_edit_state + (dir < 0 ? 3 : 1)) % 4; } break;
            case 25: if (dir || (kDown & KEY_A)) { g_anim_edit_frame = (g_anim_edit_frame + (dir < 0 ? ANIM_FRAMES - 1 : 1)) % ANIM_FRAMES; } break;
            case 26: if (kDown & KEY_A) { enemy_anim_ensure_at(m->x, m->y); open_sprite_editor(SPRITE_TARGET_ENEMY_ANIM); } break;
            case 27: if (kDown & KEY_A) close_entity_editor(); break;
        }
        if (m->ai_rank == AI_RANK_BOSS && m->spawn_kind != AI_SPAWN_NONE && m->spawn_limit == 0) m->spawn_limit = 2;
        snprintf(g_status, sizeof(g_status), "%s HP%d ATK%d", ai_rank_name(m->ai_rank), m->hp, m->attack);
        return;
    }

    if (g_entity_edit_mode == EDIT_MODE_DOOR) {
        DoorMeta *d = door_meta_ensure_at(g_entity_edit_x, g_entity_edit_y);
        if (!d) { close_entity_editor(); return; }
        switch (g_entity_edit_cursor) {
            case 0: if (dir || (kDown & KEY_A)) { d->texture_id = (uint8_t)((d->texture_id + (dir < 0 ? 7 : 1)) & MAX_TEXTURE_ID); g_dirty = true; } break;
            case 1: if (dir || big) { int step = big ? big * 5 : dir; d->group_id = (uint8_t)clampi32((int)d->group_id + step, 0, 31); g_dirty = true; } if (kDown & KEY_A) { d->group_id = (uint8_t)prompt_int_value("Door group", d->group_id, 0, 31); g_dirty = true; } break;
            case 2: if (dir || (kDown & KEY_A)) { d->door_type = (uint8_t)((d->door_type + (dir < 0 ? 3 : 1)) % 4); g_dirty = true; } break;
            case 3: if (dir || (kDown & KEY_A)) { d->speed = (uint8_t)((d->speed + (dir < 0 ? 2 : 1)) % 3); g_dirty = true; } break;
            case 4: if (dir || (kDown & KEY_A)) { d->move_dir = (uint8_t)((d->move_dir + (dir < 0 ? 3 : 1)) % 4); g_dirty = true; } break;
            case 5: if (kDown & KEY_A) { d->switch_pressed = !d->switch_pressed; g_dirty = true; } break;
            case 6: if (kDown & KEY_A) { d->toggled = !d->toggled; g_dirty = true; } break;
            case 7: if (kDown & KEY_A) { set_wall_texture(&g_level, d->x, d->y, d->texture_id); set_floor_texture(&g_level, d->x, d->y, d->texture_id); g_dirty = true; } break;
            case 8: if (kDown & KEY_A) close_entity_editor(); break;
        }
        snprintf(g_status, sizeof(g_status), "DOOR %s G%d %s", door_type_name(d->door_type), d->group_id, door_speed_name(d->speed));
        return;
    }

    if (g_entity_edit_mode == EDIT_MODE_WEAPON) {
        int wi = clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1);
        WeaponDef *w = &g_weapons[wi];
        switch (g_entity_edit_cursor) {
            case 0: if (dir || (kDown & KEY_A)) g_entity_edit_weapon = (g_entity_edit_weapon + (dir < 0 ? MAX_WEAPONS - 1 : 1)) % MAX_WEAPONS; break;
            case 1: if (kDown & KEY_A) prompt_weapon_name(wi); break;
            case 2:
                if (dir || big) { int step = big ? big * 5 : dir; w->damage = (uint8_t)clampi32((int)w->damage + step, 1, 99); g_dirty = true; }
                if (kDown & KEY_A) { w->damage = (uint8_t)prompt_int_value("Weapon damage", w->damage, 1, 99); g_dirty = true; }
                break;
            case 3:
                if (dir || big) { int step = big ? big * 2 : dir; w->range = (uint8_t)clampi32((int)w->range + step, 1, 8); g_dirty = true; }
                if (kDown & KEY_A) { w->range = (uint8_t)prompt_int_value("Weapon range", w->range, 1, 8); g_dirty = true; }
                break;
            case 4:
                if (dir || big) { int step = big ? big * 5 : dir; w->cooldown = (uint8_t)clampi32((int)w->cooldown + step, 1, 99); g_dirty = true; }
                if (kDown & KEY_A) { w->cooldown = (uint8_t)prompt_int_value("Cooldown frames", w->cooldown, 1, 99); g_dirty = true; }
                break;
            case 5: if (dir || (kDown & KEY_A)) { w->color_id = (uint8_t)((w->color_id + (dir < 0 ? 7 : 1)) & 7); g_dirty = true; } break;
            case 6: if (kDown & KEY_A) open_sprite_editor(SPRITE_TARGET_WEAPON); break;
            case 7: if (kDown & KEY_A) close_entity_editor(); break;
        }
        snprintf(g_status, sizeof(g_status), "%s D%d R%d C%d", weapon_name(g_entity_edit_weapon), g_weapons[g_entity_edit_weapon].damage, g_weapons[g_entity_edit_weapon].range, g_weapons[g_entity_edit_weapon].cooldown);
        return;
    }
}

static int collectible_value(int kind) {
    if (kind == REWARD_KEY) return 0;
    if (kind == REWARD_PINK) return 5;
    if (kind == REWARD_PURPLE) return 10;
    return 1;
}

static bool add_pickup_at(float x, float y, int kind, int amount) {
    if (g_collectible_count >= MAX_COLLECTIBLES) return false;
    Collectible *c = &g_collectibles[g_collectible_count++];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->x = (int)x;
    c->y = (int)y;
    c->fx = x;
    c->fy = y;
    c->kind = kind;
    if (amount < 1) amount = 1;
    c->amount = amount;
    g_collectibles_left++;
    if (kind == REWARD_DOT || kind == REWARD_PINK || kind == REWARD_PURPLE) {
        g_coins_total += collectible_value(kind);
    }
    return true;
}

static bool spawn_reward_in_front(Level *lv, int reward_kind, int reward_amount) {
    if (!lv) return false;
    float fx = lv->player_x + cosf(lv->player_angle) * 0.95f;
    float fy = lv->player_y + sinf(lv->player_angle) * 0.95f;
    int tx = (int)fx;
    int ty = (int)fy;
    if (tx <= 0 || ty <= 0 || tx >= lv->width - 1 || ty >= lv->height - 1 || tile_blocks_side(tile_at(lv, tx, ty), 0.0f)) {
        fx = lv->player_x;
        fy = lv->player_y;
    }
    bool ok = false;
    int count = reward_amount < 1 ? 1 : reward_amount;
    for (int i = 0; i < count; i++) {
        float ox = ((i % 3) - 1) * 0.18f;
        float oy = ((i / 3) % 3) * 0.18f;
        ok = add_pickup_at(fx + ox, fy + oy, reward_kind, 1) || ok;
    }
    return ok;
}

static int completed_npc_count(void) {
    int n = 0;
    for (int i = 0; i < g_npc_count; i++) if (g_npcs[i].active && g_npcs[i].completed) n++;
    return n;
}

static int active_npc_count(void) {
    int n = 0;
    for (int i = 0; i < g_npc_count; i++) if (g_npcs[i].active) n++;
    return n;
}

static void refresh_mission_totals(void) {
    g_missions_total = active_npc_count();
    g_missions_done = completed_npc_count();
}

static void refresh_success_percent(void) {
    int parts = 0;
    int sum = 0;

    if (g_enemies_total > 0) {
        sum += clampi32((g_enemies_killed * 100) / g_enemies_total, 0, 100);
        parts++;
    }
    if (g_coins_total > 0) {
        sum += clampi32((g_coins_bank * 100) / g_coins_total, 0, 100);
        parts++;
    }
    if (g_missions_total > 0) {
        sum += clampi32((g_missions_done * 100) / g_missions_total, 0, 100);
        parts++;
    }

    g_success_percent = parts > 0 ? clampi32(sum / parts, 0, 100) : 100;
}

static bool npc_quest_ready(const NPC *n) {
    if (!n || n->completed) return false;
    switch (n->quest_type) {
        case QUEST_NONE: return true;
        case QUEST_COINS: return g_player_score >= (int)n->quest_target;
        case QUEST_KEY: return g_player_keys >= (int)n->quest_target;
        case QUEST_NPC: return completed_npc_count() >= (int)n->quest_target;
        default: return false;
    }
}

static const char *npc_quest_desc(const NPC *n) {
    static char buf[64];
    if (!n) return "NO NPC";
    if (n->quest_type == QUEST_NONE) snprintf(buf, sizeof(buf), "QUEST READY");
    else if (n->quest_type == QUEST_COINS) snprintf(buf, sizeof(buf), "QUEST COINS %d/%d", g_player_score, n->quest_target);
    else if (n->quest_type == QUEST_KEY) snprintf(buf, sizeof(buf), "QUEST KEYS %d/%d", g_player_keys, n->quest_target);
    else snprintf(buf, sizeof(buf), "QUEST NPCS %d/%d", completed_npc_count(), n->quest_target);
    return buf;
}

static void update_npc_talk_timers(float dt) {
    for (int i = 0; i < g_npc_count; i++) {
        NPC *n = &g_npcs[i];
        if (!n->active) continue;
        float nx = (float)n->x + 0.5f;
        float ny = (float)n->y + 0.5f;
        float dx = g_level.player_x - nx;
        float dy = g_level.player_y - ny;
        float radius = (n->text_mode == TEXT_MODE_ALWAYS) ? 16.0f : 10.0f;
        bool close = (dx * dx + dy * dy) <= radius * radius;
        if (!close) {
            n->talk_timer = 0.0f;
            continue;
        }
        n->known = true;
        if (n->talk_timer <= 0.0f) n->talk_timer = 0.01f;
        else if (n->talk_timer < 999.0f) n->talk_timer += dt;
    }
}

static bool toggle_nearby_door(Level *lv, u32 kDown) {
    if (!lv || !(kDown & KEY_SELECT)) return false;
    Door *best = NULL;
    float best_d2 = 2.10f * 2.10f;
    for (int i = 0; i < g_door_count; i++) {
        Door *d = &g_doors[i];
        if (!d->active || d->door_type != DOOR_TYPE_TOGGLE) continue;
        float cx = (float)d->x + 0.5f;
        float cy = (float)d->y + 0.5f;
        float dx = lv->player_x - cx;
        float dy = lv->player_y - cy;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_d2) { best_d2 = d2; best = d; }
    }
    if (!best) return false;
    bool next = !best->toggled;
    int remaining = MAX_DOOR_GROUP_SIZE;
    for (int i = 0; i < g_door_count && remaining > 0; i++) {
        Door *d = &g_doors[i];
        if (!d->active) continue;
        bool same = best->group_id ? (d->group_id == best->group_id) : (d == best);
        if (!same) continue;
        d->toggled = next;
        d->opening = next;
        DoorMeta *dm = door_meta_find_at(d->x, d->y);
        if (dm) dm->toggled = next;
        remaining--;
    }
    snprintf(g_status, sizeof(g_status), next ? "DOOR TOGGLED OPEN" : "DOOR TOGGLED CLOSED");
    return true;
}

static void talk_to_nearby_npc(Level *lv, u32 kDown) {
    if (!lv || !(kDown & KEY_SELECT)) return;
    NPC *best = NULL;
    float best_d2 = 3.25f * 3.25f;
    for (int i = 0; i < g_npc_count; i++) {
        NPC *n = &g_npcs[i];
        if (!n->active) continue;
        float nx = (float)n->x + 0.5f;
        float ny = (float)n->y + 0.5f;
        float dx = lv->player_x - nx;
        float dy = lv->player_y - ny;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_d2) { best_d2 = d2; best = n; }
    }
    if (!best) {
        snprintf(g_status, sizeof(g_status), "NO NPC QUEST NEARBY");
        return;
    }
    best->known = true;
    if (best->completed) {
        snprintf(g_status, sizeof(g_status), "QUEST ALREADY COMPLETE");
        return;
    }
    if (!npc_quest_ready(best)) {
        snprintf(g_status, sizeof(g_status), "%s", npc_quest_desc(best));
        return;
    }
    if (best->quest_type == QUEST_KEY && g_player_keys >= (int)best->quest_target) {
        g_player_keys -= (int)best->quest_target;
    }
    best->completed = true;
    refresh_mission_totals();
    refresh_success_percent();
    spawn_reward_in_front(lv, best->reward_kind, best->reward_amount);
    snprintf(g_status, sizeof(g_status), "QUEST COMPLETE - REWARD DROPPED");
}

static void cycle_owned_weapon(void) {
    int start = g_current_weapon;
    for (int step = 0; step < MAX_WEAPONS; step++) {
        int idx = (start + 1 + step + MAX_WEAPONS) % MAX_WEAPONS;
        if (g_player_weapons[idx]) {
            g_current_weapon = idx;
            snprintf(g_status, sizeof(g_status), "%s DMG%d", weapon_name(idx), g_weapons[idx].damage);
            return;
        }
    }
    g_current_weapon = -1;
    snprintf(g_status, sizeof(g_status), "NO WEAPON");
}

static void attack_with_weapon(Level *lv, u32 kDown) {
    if (!lv || !(kDown & KEY_X) || g_attack_cooldown > 0.0f) return;
    if (g_current_weapon < 0 || g_current_weapon >= MAX_WEAPONS || !g_player_weapons[g_current_weapon]) {
        snprintf(g_status, sizeof(g_status), "NEED WEAPON");
        return;
    }

    WeaponDef *w = &g_weapons[g_current_weapon];
    float fx = cosf(lv->player_angle);
    float fy = sinf(lv->player_angle);
    float range = 0.85f + (float)w->range * 0.42f;
    int best = -1;
    float best_dot = 0.45f;
    for (int i = 0; i < g_enemy_count; i++) {
        Enemy *e = &g_enemies[i];
        if (!e->active || e->dying) continue;
        float dx = e->x - lv->player_x;
        float dy = e->y - lv->player_y;
        float d2 = dx * dx + dy * dy;
        if (d2 > range * range || d2 < 0.0001f) continue;
        float dist = sqrtf(d2);
        float dot = (dx * fx + dy * fy) / dist;
        if (dot > best_dot && ai_line_of_sight(lv, lv->player_x, lv->player_y, e->x, e->y)) {
            best_dot = dot;
            best = i;
        }
    }

    g_attack_cooldown = ((float)w->cooldown) / 60.0f;
    g_slash_type = (g_slash_type + 1 + g_current_weapon) % 3;
    g_slash_total = clampf32(0.18f + ((float)w->cooldown / 220.0f), 0.20f, 0.38f);
    g_slash_timer = g_slash_total;
    g_weapon_bounce_timer = 0.16f + clampf32((float)w->range * 0.025f, 0.0f, 0.12f);
    if (best < 0) {
        snprintf(g_status, sizeof(g_status), "%s MISS", weapon_name(g_current_weapon));
        return;
    }
    Enemy *e = &g_enemies[best];
    e->hp -= (int)w->damage;
    e->state = 1;
    e->hit_timer = 0.28f;
    if (e->text_count > 0) {
        e->text_index = (uint8_t)((e->text_index + 1) % e->text_count);
        e->text_timer = 1.35f;
    }
    if (e->hp <= 0) {
        e->hp = 0;
        e->dying = true;
        e->death_timer = 0.48f;
        g_enemies_killed++;
        if (g_enemies_killed > g_enemies_total && g_enemies_total > 0) g_enemies_killed = g_enemies_total;
        g_player_score += 3;
        snprintf(g_status, sizeof(g_status), "%s KO +3", weapon_name(g_current_weapon));
    } else {
        snprintf(g_status, sizeof(g_status), "%s HIT HP%d", weapon_name(g_current_weapon), e->hp);
    }
}

void reset_runtime_entities(void) {
    g_screen_shake_timer = 0.0f;
    memset(g_enemies, 0, sizeof(g_enemies));
    memset(g_collectibles, 0, sizeof(g_collectibles));
    memset(g_doors, 0, sizeof(g_doors));
    memset(g_projectiles, 0, sizeof(g_projectiles));
    if (!g_loaded_npc_metadata) memset(g_npcs, 0, sizeof(g_npcs));
    g_enemy_count = 0;
    g_collectible_count = 0;
    g_collectibles_left = 0;
    g_door_count = 0;
    if (!g_loaded_npc_metadata) g_npc_count = 0;
    g_player_keys = 0;
    g_player_score = 0;
    g_player_health_max = clampi32(g_player_health_max, PLAYER_HEALTH_MIN, PLAYER_HEALTH_MAX);
    g_player_health = g_player_health_max;
    g_player_dead = false;
    g_player_hurt_timer = 0.0f;
    g_death_killer[0] = '\0';
    g_coins_bank = 0;
    g_coins_total = 0;
    g_enemies_total = 0;
    g_enemies_killed = 0;
    g_missions_total = 0;
    g_missions_done = 0;
    g_success_percent = 0;
    for (int i = 0; i < MAX_WEAPONS; i++) g_player_weapons[i] = false;
    g_current_weapon = -1;
    g_attack_cooldown = 0.0f;
    g_slash_timer = 0.0f;
    g_slash_total = 0.26f;
    g_slash_type = 2;
    g_weapon_bounce_timer = 0.0f;
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
                g_enemies_total++;
                memset(e, 0, sizeof(*e));
                e->active = true;
                e->x = e->start_x = (float)x + 0.5f;
                e->y = e->start_y = (float)y + 0.5f;
                e->z = e->start_z = ground_height_at(lv, e->x, e->y, 0.0f);
                e->angle = ((float)((x * 37 + y * 101) & 255) / 255.0f) * TWO_PI_F;
                e->speed = 1.35f;
                e->roam_timer = 0.25f + (float)((x + y) & 7) * 0.18f;
                e->hit_timer = 0.0f;
                e->death_timer = 0.0f;
                e->dying = false;
                e->parent_index = -1;
                e->ai_rank = AI_RANK_GRUNT;
                e->spawn_kind = AI_SPAWN_NONE;
                e->spawn_limit = 0;
                e->command_range = 10;
                e->sight_range = 13;
                e->ranged_attack = 0;
                e->melee_range = 7;
                e->attack_cooldown = 7;
                e->spawn_cooldown = 25;
                e->projectile_style = 0;
                e->projectile_anim = 1;
                e->spawn_timer = 1.0f;
                e->ranged_timer = 0.0f;
                e->hp = 4 + ((x + y) % 4);
                e->attack = 1;
                EnemyMeta *meta = enemy_meta_find_at(x, y);
                e->color_id = (uint8_t)((x + y) & 7);
                e->projectile_color = e->color_id & 7;
                copy_default_sprite(e->sprite, SPRITE_TARGET_ENEMY);
                copy_default_enemy_sprite16(e->sprite16);
                e->speed_attr = 100;
                e->size_pct = 100;
                e->text_speed = TEXT_SPEED_MEDIUM;
                if (meta) {
                    if (meta->hp > 0) e->hp = meta->hp;
                    if (meta->attack > 0) e->attack = meta->attack;
                    e->color_id = meta->color_id & 7;
                    e->ai_rank = meta->ai_rank;
                    e->spawn_kind = meta->spawn_kind;
                    e->spawn_limit = meta->spawn_limit;
                    e->command_range = meta->command_range ? meta->command_range : 10;
                    e->sight_range = meta->sight_range ? meta->sight_range : ((meta->ai_rank == AI_RANK_BOSS) ? 20 : (meta->ai_rank == AI_RANK_CAPTAIN ? 16 : 13));
                    e->ranged_attack = meta->ranged_attack;
                    e->melee_range = meta->melee_range ? meta->melee_range : ((meta->ai_rank == AI_RANK_BOSS) ? 10 : 7);
                    e->attack_cooldown = meta->attack_cooldown ? meta->attack_cooldown : 7;
                    e->spawn_cooldown = meta->spawn_cooldown ? meta->spawn_cooldown : 25;
                    e->projectile_color = meta->projectile_color & 7;
                    e->projectile_style = meta->projectile_style % 3;
                    e->projectile_anim = meta->projectile_anim ? 1 : 0;
                    e->speed_attr = meta->speed_attr ? meta->speed_attr : 100;
                    e->size_pct = meta->size_pct ? meta->size_pct : (meta->ai_rank == AI_RANK_BOSS ? 118 : 100);
                    if (e->ai_rank != AI_RANK_BOSS && e->size_pct > 115) e->size_pct = 115;
                    e->text_speed = meta->text_speed <= TEXT_SPEED_FAST ? meta->text_speed : TEXT_SPEED_MEDIUM;
                    memcpy(e->sprite, meta->sprite, SPRITE_BYTES);
                    memcpy(e->sprite16, meta->sprite16, sizeof(e->sprite16));
                    memcpy(e->boss_sprite, meta->boss_sprite, sizeof(e->boss_sprite));
                    ensure_sprite_or_default(e->sprite, SPRITE_TARGET_ENEMY);
                    ensure_enemy_sprite16_or_default(e->sprite16, e->sprite);
                    ensure_boss_sprite_or_default(e->boss_sprite);
                    if (e->ai_rank == AI_RANK_BOSS && e->hp < 20) e->hp = 20;
                    if (e->ai_rank == AI_RANK_BOSS && e->spawn_kind != AI_SPAWN_NONE && e->spawn_limit == 0) e->spawn_limit = 2;
                }
                e->hp_max = e->hp;
                e->state = 0;
                e->text_count = 0;
                e->text_index = 0;
                e->text_timer = 0.0f;
                if (meta && meta->text_count > 0) {
                    e->text_count = meta->text_count;
                    if (e->text_count > ENEMY_TEXT_LINES) e->text_count = ENEMY_TEXT_LINES;
                    for (int li = 0; li < e->text_count; li++) {
                        snprintf(e->text[li], sizeof(e->text[li]), "%s", meta->text[li]);
                    }
                } else {
                    e->text_count = 2;
                    snprintf(e->text[0], sizeof(e->text[0]), "HEY");
                    snprintf(e->text[1], sizeof(e->text[1]), "GRR");
                }
            } else if (t == TILE_SUCCESS && !g_has_success) {
                g_has_success = true;
                g_success_x = (float)x + 0.5f;
                g_success_y = (float)y + 0.5f;
            } else if (t == TILE_NPC) {
                npc_ensure_at(x, y);
            } else if ((t == TILE_DOT || t == TILE_PINK || t == TILE_PURPLE || t == TILE_KEY) && g_collectible_count < MAX_COLLECTIBLES) {
                int kind = REWARD_DOT;
                if (t == TILE_KEY) kind = REWARD_KEY;
                else if (t == TILE_PINK) kind = REWARD_PINK;
                else if (t == TILE_PURPLE) kind = REWARD_PURPLE;
                add_pickup_at((float)x + 0.5f, (float)y + 0.5f, kind, 1);
            } else if (t == TILE_DOOR && g_door_count < MAX_DOORS) {
                Door *d = &g_doors[g_door_count++];
                memset(d, 0, sizeof(*d));
                d->active = true;
                d->opening = false;
                d->x = x;
                d->y = y;
                d->open_t = 0.0f;
                DoorMeta *dm = door_meta_find_at(x, y);
                if (!dm) dm = door_meta_ensure_at(x, y);
                if (dm) {
                    d->texture_id = dm->texture_id & MAX_TEXTURE_ID;
                    d->group_id = dm->group_id;
                    d->door_type = dm->door_type;
                    d->speed = dm->speed;
                    d->move_dir = dm->move_dir;
                    d->switch_pressed = dm->switch_pressed;
                    d->toggled = dm->toggled;
                } else {
                    d->door_type = DOOR_TYPE_AUTO;
                    d->speed = DOOR_SPEED_MEDIUM;
                    d->move_dir = DOOR_MOVE_UP;
                }
            }
        }
    }

    refresh_mission_totals();
    refresh_success_percent();

    if (g_random_play && g_collectible_count < MAX_COLLECTIBLES) {
        for (int y = 2; y < lv->height - 2; y++) {
            bool placed = false;
            for (int x = 2; x < lv->width - 2; x++) {
                if ((lv->tiles[y * lv->width + x] & MAX_TILE_ID) == 0) {
                    int w = (int)(rng_next(&g_random_seed) % MAX_WEAPONS);
                    add_pickup_at((float)x + 0.5f, (float)y + 0.5f, REWARD_WEAPON_BASE + w, 1);
                    placed = true;
                    break;
                }
            }
            if (placed) break;
        }
    }
}

static void set_door_group_opening(uint8_t group_id, int only_x, int only_y, bool open, bool consume_key, Level *lv) {
    int changed = 0;
    int remaining = MAX_DOOR_GROUP_SIZE;
    for (int i = 0; i < g_door_count && remaining > 0; i++) {
        Door *d = &g_doors[i];
        if (!d->active) continue;
        bool same = false;
        if (group_id != 0) same = (d->group_id == group_id);
        else same = (d->x == only_x && d->y == only_y);
        if (!same) continue;
        d->opening = open;
        if (open && d->open_t <= 0.0f) d->open_t = 0.01f;
        changed++;
        remaining--;
    }
    if (changed && consume_key && g_player_keys > 0) g_player_keys--;
    (void)lv;
}

static bool door_switch_pressed_near(Level *lv, Door *d) {
    if (!lv || !d) return false;

    int px = (int)floorf(lv->player_x);
    int py = (int)floorf(lv->player_y);
    if (px < 0 || py < 0 || px >= lv->width || py >= lv->height) return false;

    uint8_t player_tile = tile_at(lv, px, py);
    if (player_tile != PLATFORM_TILE) return false;

    int remaining = MAX_DOOR_GROUP_SIZE;
    for (int i = 0; i < g_door_count && remaining > 0; i++) {
        Door *gd = &g_doors[i];
        if (!gd->active) continue;
        bool same = d->group_id ? (gd->group_id == d->group_id) : (gd == d);
        if (!same) continue;
        int md = abs(px - gd->x) + abs(py - gd->y);
        if (md >= 2 && md <= 8) return true;
        remaining--;
    }

    return false;
}

static void update_collectibles_and_doors(Level *lv, float dt) {
    if (!lv || g_level_won) return;

    const float key_r2 = KEY_PICKUP_RADIUS * KEY_PICKUP_RADIUS;
    for (int i = 0; i < g_collectible_count; i++) {
        Collectible *c = &g_collectibles[i];
        if (!c->active) continue;

        float dx = lv->player_x - c->fx;
        float dy = lv->player_y - c->fy;
        float dz = lv->player_z - ground_height_at(lv, c->fx, c->fy, 0.0f);
        if ((dx * dx + dy * dy) <= key_r2 && fabsf(dz) < 0.85f) {
            c->active = false;
            if (g_collectibles_left > 0) g_collectibles_left--;
            if (c->kind == REWARD_KEY) {
                g_player_keys++;
                g_player_score += 10;
                snprintf(g_status, sizeof(g_status), "KEY +1  KEYS %d", g_player_keys);
            } else if (c->kind == REWARD_HEALTH) {
                int heal = c->amount > 0 ? c->amount * HEALTH_PICKUP_AMOUNT : HEALTH_PICKUP_AMOUNT;
                g_player_health += heal;
                if (g_player_health > g_player_health_max) g_player_health = g_player_health_max;
                snprintf(g_status, sizeof(g_status), "HEALTH +%d  HP %d/%d", heal, g_player_health, g_player_health_max);
            } else if (c->kind >= REWARD_WEAPON_BASE && c->kind < REWARD_WEAPON_BASE + MAX_WEAPONS) {
                int w = c->kind - REWARD_WEAPON_BASE;
                g_player_weapons[w] = true;
                if (g_current_weapon < 0) g_current_weapon = w;
                snprintf(g_status, sizeof(g_status), "GOT %s DMG%d", weapon_name(w), g_weapons[w].damage);
            } else {
                int value = collectible_value(c->kind);
                g_coins_bank += value;
                if (g_coins_bank > g_coins_total && g_coins_total > 0) g_coins_bank = g_coins_total;
                g_player_score += value;
                snprintf(g_status, sizeof(g_status), "+%d COINS %d SCORE %d", value, g_coins_bank, g_player_score);
            }
        }
    }

    const float door_r2 = DOOR_TRIGGER_RADIUS * DOOR_TRIGGER_RADIUS;
    for (int i = 0; i < g_door_count; i++) {
        Door *d = &g_doors[i];
        if (!d->active) continue;
        float cx = (float)d->x + 0.5f;
        float cy = (float)d->y + 0.5f;
        float dx = lv->player_x - cx;
        float dy = lv->player_y - cy;
        bool near = (dx * dx + dy * dy) <= door_r2;
        bool wants_open = false;

        if (d->door_type == DOOR_TYPE_AUTO) {
            wants_open = near;
        } else if (d->door_type == DOOR_TYPE_KEY) {
            if (near && d->open_t <= 0.0f) {
                if (g_player_keys > 0) {
                    set_door_group_opening(d->group_id, d->x, d->y, true, true, lv);
                    snprintf(g_status, sizeof(g_status), "KEY DOOR OPEN  KEYS %d", g_player_keys);
                } else {
                    snprintf(g_status, sizeof(g_status), "NEED A KEY");
                }
            }
            wants_open = d->opening;
        } else if (d->door_type == DOOR_TYPE_SWITCH) {
            d->switch_pressed = door_switch_pressed_near(lv, d);
            wants_open = d->switch_pressed;
        } else if (d->door_type == DOOR_TYPE_TOGGLE) {
            wants_open = d->toggled;
        }

        if (d->door_type == DOOR_TYPE_AUTO || d->door_type == DOOR_TYPE_SWITCH || d->door_type == DOOR_TYPE_TOGGLE) {
            if (d->group_id != 0) set_door_group_opening(d->group_id, d->x, d->y, wants_open, false, lv);
            else d->opening = wants_open;
        }

        float seconds = door_speed_seconds(d->speed);
        if (seconds < 0.05f) seconds = DOOR_OPEN_TIME;
        if (d->opening) {
            d->open_t += dt / seconds;
            if (d->open_t > 1.0f) d->open_t = 1.0f;
        } else {
            d->open_t -= dt / seconds;
            if (d->open_t < 0.0f) d->open_t = 0.0f;
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
        uint8_t tt = lv->tiles[ty * lv->width + tx] & MAX_TILE_ID;
        if (tt == TILE_DOOR) { if (door_blocks_at(tx, ty)) return false; }
        else if (tile_blocks_side(tt, 0.0f)) return false;
    }

    return true;
}

static void player_take_damage(Level *lv, int damage, const char *killer) {
    if (!lv || g_player_dead) return;
    if (g_player_hurt_timer > 0.0f) return;

    int dmg = damage > 0 ? damage : 1;
    int cap = (g_player_health_max + 3) / 4;
    if (cap < 1) cap = 1;
    if (dmg > cap) dmg = cap;

    g_player_health -= dmg;
    g_player_hurt_timer = PLAYER_HURT_TIME;
    if (g_screen_shake_enabled) g_screen_shake_timer = 0.30f;
    if (g_player_health <= 0) {
        g_player_health = 0;
        g_player_dead = true;
        snprintf(g_death_killer, sizeof(g_death_killer), "%s", (killer && killer[0]) ? killer : "AN ENEMY");
        snprintf(g_status, sizeof(g_status), "DEFEATED BY %s", g_death_killer);
        return;
    }

    snprintf(g_status, sizeof(g_status), "-%d HP  %d/%d", dmg, g_player_health, g_player_health_max);
}

static void spawn_boss_arrow(const Enemy *boss, const Level *lv) {
    if (!boss || !lv) return;
    int idx = -1;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!g_projectiles[i].active) { idx = i; break; }
    }
    if (idx < 0) return;
    if (idx >= g_projectile_count) g_projectile_count = idx + 1;

    float dx = lv->player_x - boss->x;
    float dy = lv->player_y - boss->y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 0.001f) return;
    dx /= d;
    dy /= d;

    Projectile *pr = &g_projectiles[idx];
    memset(pr, 0, sizeof(*pr));
    pr->active = true;
    pr->x = boss->x + dx * 0.35f;
    pr->y = boss->y + dy * 0.35f;
    pr->z = ground_height_at(lv, boss->x, boss->y, PLATFORM_TOP) + EYE_HEIGHT * 0.90f;
    pr->vx = dx * BOSS_ARROW_SPEED;
    pr->vy = dy * BOSS_ARROW_SPEED;
    pr->life = BOSS_ARROW_LIFE;
    pr->damage = boss->attack > 0 ? boss->attack : 2;
    pr->color_id = boss->projectile_color & 7;
    pr->style = boss->projectile_style % 3;
    pr->anim = boss->projectile_anim ? 1 : 0;
    snprintf(pr->killer, sizeof(pr->killer), "%s SHOT", ai_rank_name(boss->ai_rank));
}

static void update_boss_projectiles(Level *lv, float dt) {
    if (!lv || g_player_dead || g_level_won) return;
    for (int i = 0; i < g_projectile_count; i++) {
        Projectile *pr = &g_projectiles[i];
        if (!pr->active) continue;
        pr->life -= dt;
        if (pr->life <= 0.0f) { pr->active = false; continue; }

        pr->x += pr->vx * dt;
        pr->y += pr->vy * dt;
        int tx = (int)pr->x;
        int ty = (int)pr->y;
        if (tx < 0 || ty < 0 || tx >= lv->width || ty >= lv->height || ((tile_at(lv, tx, ty) == TILE_DOOR) ? door_blocks_at(tx, ty) : tile_blocks_side(tile_at(lv, tx, ty), 0.0f))) {
            pr->active = false;
            continue;
        }

        float dx = lv->player_x - pr->x;
        float dy = lv->player_y - pr->y;
        if ((dx * dx + dy * dy) < 0.22f * 0.22f) {
            pr->active = false;
            player_take_damage(lv, pr->damage, pr->killer);
        }
    }
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
        e->text_timer = 0.8f;
    }

    snprintf(g_status, sizeof(g_status), "RESPAWNED");
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

    float sight_range = (float)(e->sight_range ? e->sight_range :
        (e->ai_rank == AI_RANK_BOSS ? 20 : (e->ai_rank == AI_RANK_CAPTAIN ? 16 : 13)));
    sight_range = clampf32(sight_range, 2.0f, 40.0f);
    if (dist2 > sight_range * sight_range) return false;
    if (!ai_line_of_sight(lv, e->x, e->y, lv->player_x, lv->player_y)) return false;

    float dist = sqrtf(dist2);
    if (dist < 0.001f) return true;

    /* Close and already-alert enemies should not feel blind just because their
       roam angle is facing away for a few frames. */
    if (dist <= sight_range * 0.45f) return true;
    if (e->state) return true;
    if (e->ai_rank >= AI_RANK_CAPTAIN && dist <= sight_range * 0.70f) return true;

    float look_x = cosf(e->angle);
    float look_y = sinf(e->angle);
    float dot = (look_x * to_px + look_y * to_py) / dist;

    /* About a 200-degree cone: still directional, but not frustratingly narrow. */
    return dot > -0.18f;
}

static float enemy_hit_radius(const Enemy *e) {
    if (!e) return 0.18f;
    float size = (float)(e->size_pct ? e->size_pct : 100) / 100.0f;
    float base = (e->ai_rank == AI_RANK_BOSS) ? 0.36f : 0.22f;
    return clampf32(base * size, 0.14f, 0.62f);
}

static bool ai_room_is_contained(uint8_t room) {
    return room == ROOM_TREASURE || room == ROOM_BOSS || room == ROOM_TRAP ||
           room == ROOM_ENEMY || room == ROOM_SAFE || room == ROOM_PUZZLE || room == ROOM_SECRET;
}

static bool ai_room_allows_player(const Level *lv, const Enemy *e) {
    if (!lv || !e) return true;
    uint8_t er = room_at(lv, (int)e->start_x, (int)e->start_y);
    if (er == ROOM_NONE) er = room_at(lv, (int)e->x, (int)e->y);
    uint8_t pr = room_at(lv, (int)lv->player_x, (int)lv->player_y);
    if (pr == ROOM_SAFE) return false;
    if (er == ROOM_NONE) return true;
    if (er == ROOM_CORRIDOR) return pr == ROOM_CORRIDOR;
    if (ai_room_is_contained(er)) return pr == er;
    return pr == er || pr == ROOM_NONE;
}

static bool ai_can_enter_room(const Level *lv, const Enemy *e, float nx, float ny) {
    if (!lv || !e) return true;
    uint8_t er = room_at(lv, (int)e->start_x, (int)e->start_y);
    if (er == ROOM_NONE) er = room_at(lv, (int)e->x, (int)e->y);
    if (er == ROOM_NONE) return true;
    uint8_t nr = room_at(lv, (int)nx, (int)ny);
    if (er == ROOM_CORRIDOR) return nr == ROOM_CORRIDOR;
    return nr == er;
}

static bool ai_position_clear_of_player(const Level *lv, float nx, float ny, const Enemy *self, float desired_stop) {
    if (!lv || !self) return true;
    float dx = nx - lv->player_x;
    float dy = ny - lv->player_y;
    float min_d = enemy_hit_radius(self) + PLAYER_RADIUS + 0.08f;
    if (desired_stop > min_d) min_d = desired_stop * 0.82f;
    return (dx * dx + dy * dy) >= min_d * min_d;
}

static bool ai_position_clear_of_enemies(int self_index, float nx, float ny, const Enemy *self) {
    float r0 = enemy_hit_radius(self);
    for (int j = 0; j < g_enemy_count; j++) {
        if (j == self_index) continue;
        const Enemy *o = &g_enemies[j];
        if (!o->active || o->dying) continue;
        float dx = nx - o->x;
        float dy = ny - o->y;
        float min_d = r0 + enemy_hit_radius(o);
        if ((dx * dx + dy * dy) < min_d * min_d) return false;
    }
    return true;
}

static int ai_count_children_for_parent(int parent_index) {
    int count = 0;
    for (int i = 0; i < g_enemy_count; i++) {
        Enemy *e = &g_enemies[i];
        if (e->active && !e->dying && e->parent_index == parent_index) count++;
    }
    return count;
}

static void ai_command_nearby_minions(Level *lv, int leader_index) {
    if (!lv || leader_index < 0 || leader_index >= g_enemy_count) return;
    Enemy *leader = &g_enemies[leader_index];
    if (!leader->active || leader->dying) return;
    if (leader->ai_rank < AI_RANK_CAPTAIN) return;

    float range = (float)(leader->command_range ? leader->command_range : 10);
    float range2 = range * range;
    for (int i = 0; i < g_enemy_count; i++) {
        if (i == leader_index) continue;
        Enemy *m = &g_enemies[i];
        if (!m->active || m->dying || m->ai_rank >= leader->ai_rank) continue;
        if (room_at(lv, (int)m->start_x, (int)m->start_y) != room_at(lv, (int)leader->start_x, (int)leader->start_y)) continue;
        float dx = m->x - leader->x;
        float dy = m->y - leader->y;
        if (dx * dx + dy * dy > range2) continue;
        if (!ai_line_of_sight(lv, leader->x, leader->y, m->x, m->y)) continue;
        m->state = 1;
        m->text_timer = 0.35f;
    }
}

static bool ai_spawn_minion_for_boss(Level *lv, int parent_index) {
    if (!lv || parent_index < 0 || parent_index >= g_enemy_count) return false;
    if (g_enemy_count >= MAX_ENEMIES) return false;
    Enemy *boss = &g_enemies[parent_index];
    if (!boss->active || boss->dying || boss->ai_rank != AI_RANK_BOSS) return false;
    if (boss->spawn_kind == AI_SPAWN_NONE || boss->spawn_limit == 0) return false;
    if (ai_count_children_for_parent(parent_index) >= boss->spawn_limit) return false;

    for (int tries = 0; tries < 10; tries++) {
        float ang = ((float)(rng_next(&g_random_seed) & 4095) / 4096.0f) * TWO_PI_F;
        float dist = 1.35f + (float)((rng_next(&g_random_seed) & 255) / 255.0f) * 1.20f;
        float nx = boss->x + cosf(ang) * dist;
        float ny = boss->y + sinf(ang) * dist;
        if (!can_stand_at(lv, nx, ny, boss->z)) continue;
        if (!ai_can_enter_room(lv, boss, nx, ny)) continue;
        if (!ai_position_clear_of_enemies(parent_index, nx, ny, boss)) continue;
        if (!ai_position_clear_of_player(lv, nx, ny, boss, 0.0f)) continue;

        Enemy *e = &g_enemies[g_enemy_count++];
        memset(e, 0, sizeof(*e));
        e->active = true;
        e->x = e->start_x = nx;
        e->y = e->start_y = ny;
        e->z = e->start_z = ground_height_at(lv, nx, ny, 0.0f);
        e->angle = ang + PI_F;
        e->speed = 1.45f;
        e->roam_timer = 0.25f;
        e->hp = 3 + (boss->attack / 3);
        if (e->hp > 12) e->hp = 12;
        e->hp_max = e->hp;
        e->attack = boss->attack > 2 ? boss->attack / 2 : 1;
        e->ai_rank = AI_RANK_GRUNT;
        e->spawn_kind = AI_SPAWN_NONE;
        e->spawn_limit = 0;
        e->command_range = 8;
        e->sight_range = boss->sight_range ? boss->sight_range : 13;
        e->ranged_attack = 0;
        e->melee_range = boss->melee_range ? boss->melee_range : 7;
        e->attack_cooldown = boss->attack_cooldown ? boss->attack_cooldown : 7;
        e->spawn_cooldown = 25;
        e->projectile_color = boss->projectile_color & 7;
        e->projectile_style = boss->projectile_style;
        e->projectile_anim = boss->projectile_anim;
        e->parent_index = parent_index;
        e->color_id = (uint8_t)((boss->color_id + 1) & 7);
        copy_default_sprite(e->sprite, SPRITE_TARGET_ENEMY);
        copy_default_enemy_sprite16(e->sprite16);
        int source_count = g_enemy_meta_count > 0 ? g_enemy_meta_count : g_enemy_count;
        if (source_count > 0) {
            int pick = (int)(rng_next(&g_random_seed) % (uint32_t)source_count);
            if (g_enemy_meta_count > 0) {
                EnemyMeta *src = &g_enemy_metas[pick % g_enemy_meta_count];
                memcpy(e->sprite, src->sprite, SPRITE_BYTES);
                memcpy(e->sprite16, src->sprite16, sizeof(e->sprite16));
                e->color_id = src->color_id & 7;
            } else {
                Enemy *src = &g_enemies[pick % g_enemy_count];
                memcpy(e->sprite, src->sprite, SPRITE_BYTES);
                memcpy(e->sprite16, src->sprite16, sizeof(e->sprite16));
                e->color_id = src->color_id & 7;
            }
        }
        ensure_enemy_sprite16_or_default(e->sprite16, e->sprite);
        e->size_pct = boss->size_pct ? (uint8_t)clampi32((int)boss->size_pct - 42, 55, 82) : 70;
        e->speed_attr = boss->speed_attr ? boss->speed_attr : 100;
        e->text_count = 1;
        snprintf(e->text[0], sizeof(e->text[0]), "MINION");
        e->text_timer = 0.9f;
        e->state = 1;
        g_enemies_total++;
        return true;
    }
    return false;
}

static void update_enemies(Level *lv, float dt) {
    if (!lv || g_level_won || g_player_dead) return;
    g_frame_counter++;

    for (int i = 0; i < g_enemy_count; i++) {
        Enemy *e = &g_enemies[i];
        if (!e->active) continue;

        if (e->hit_timer > 0.0f) {
            e->hit_timer -= dt;
            if (e->hit_timer < 0.0f) e->hit_timer = 0.0f;
        }
        if (e->text_timer > 0.0f) {
            e->text_timer -= dt;
            if (e->text_timer < 0.0f) e->text_timer = 0.0f;
        }
        if (e->ranged_timer > 0.0f) {
            e->ranged_timer -= dt;
            if (e->ranged_timer < 0.0f) e->ranged_timer = 0.0f;
        }

        if (e->dying) {
            e->death_timer -= dt;
            if (e->death_timer <= 0.0f) {
                e->active = false;
                e->death_timer = 0.0f;
            }
            continue;
        }

        float to_px = lv->player_x - e->x;
        float to_py = lv->player_y - e->y;
        float dist2 = to_px * to_px + to_py * to_py;

        /* AI hierarchy leaders stay lightly active, but simple far enemies sleep/throttle. */
        if (e->ai_rank == AI_RANK_GRUNT && dist2 > AI_SLEEP_DIST2) {
            if (((g_frame_counter + (uint32_t)i) & 15u) != 0u) continue;
        } else if (e->ai_rank == AI_RANK_GRUNT && dist2 > (24.0f * 24.0f)) {
            if (((g_frame_counter + (uint32_t)i) & 7u) != 0u) continue;
        } else if (dist2 > (30.0f * 30.0f)) {
            if (((g_frame_counter + (uint32_t)i) & 3u) != 0u) continue;
        }

        bool room_ok = ai_room_allows_player(lv, e);
        float hit_r = enemy_hit_radius(e);
        float melee_range = ((float)(e->melee_range ? e->melee_range : 7) * 0.10f) + hit_r + PLAYER_RADIUS;
        if (room_ok && dist2 < melee_range * melee_range && e->ranged_timer <= 0.0f && ai_line_of_sight(lv, e->x, e->y, lv->player_x, lv->player_y)) {
            int atk = e->attack > 0 ? e->attack : 1;
            e->ranged_timer = (float)(e->attack_cooldown ? e->attack_cooldown : 7) * 0.10f;
            e->text_timer = 1.0f;
            player_take_damage(lv, atk, ai_rank_name(e->ai_rank));
            return;
        }

        bool chase = room_ok && (ai_can_spot_player(lv, e, to_px, to_py, dist2) || e->state);
        if (!room_ok) e->state = 0;
        float dir_x;
        float dir_y;
        float dist = dist2 > 0.0001f ? sqrtf(dist2) : 0.0f;
        float stop_range = melee_range * 0.86f;
        float body_space = hit_r + PLAYER_RADIUS + 0.16f;
        if (stop_range < body_space) stop_range = body_space;
        if (e->ranged_attack && stop_range < 3.0f) stop_range = 3.0f;

        if (e->ai_rank >= AI_RANK_CAPTAIN) ai_command_nearby_minions(lv, i);

        if (e->ai_rank == AI_RANK_BOSS && e->spawn_kind != AI_SPAWN_NONE && e->spawn_limit > 0) {
            e->spawn_timer -= dt;
            if (e->spawn_timer <= 0.0f) {
                e->spawn_timer = ((float)(e->spawn_cooldown ? e->spawn_cooldown : 25) * 0.10f) + (float)((rng_next(&g_random_seed) & 255) / 255.0f);
                if (dist2 < (22.0f * 22.0f)) ai_spawn_minion_for_boss(lv, i);
            }
        }

        if (e->ranged_attack && chase && e->ranged_timer <= 0.0f &&
            dist2 > 2.25f && dist2 < (12.0f * 12.0f) &&
            ai_line_of_sight(lv, e->x, e->y, lv->player_x, lv->player_y)) {
            float cd = (float)(e->attack_cooldown ? e->attack_cooldown : 11) * 0.10f;
            if (cd < 0.45f) cd = 0.45f;
            e->ranged_timer = cd + 0.35f;
            e->text_timer = 1.0f;
            spawn_boss_arrow(e, lv);
            snprintf(g_status, sizeof(g_status), "%s SHOT!", ai_rank_name(e->ai_rank));
        }

        if (chase && dist2 > 0.001f) {
            float inv_dist = dist > 0.0001f ? (1.0f / dist) : 0.0f;
            dir_x = to_px * inv_dist;
            dir_y = to_py * inv_dist;
            e->angle = atan2f(dir_y, dir_x);
            if (!e->state && e->text_count > 0) e->text_timer = 1.0f;
            e->state = 1;

            /* Do not keep pushing into the player. If overlapping, back out.
               If within attack range, hold position and attack from there. */
            float overlap_space = hit_r + PLAYER_RADIUS + 0.06f;
            if (dist < overlap_space) {
                dir_x = -dir_x;
                dir_y = -dir_y;
            } else if (dist <= stop_range) {
                dir_x = 0.0f;
                dir_y = 0.0f;
            }
        } else {
            e->state = 0;
            e->roam_timer -= dt;
            if (e->roam_timer <= 0.0f) ai_pick_roam_angle(e);
            dir_x = cosf(e->angle);
            dir_y = sinf(e->angle);
        }

        float rank_speed = (e->ai_rank == AI_RANK_BOSS) ? 0.82f : (e->ai_rank == AI_RANK_CAPTAIN ? 1.08f : 1.0f);
        float attr_speed = clampf32((float)(e->speed_attr ? e->speed_attr : 100) / 100.0f, 0.25f, 2.20f);
        float step = e->speed * attr_speed * rank_speed * (chase ? 1.28f : 0.48f) * dt;
        float nx = e->x + dir_x * step;
        float ny = e->y + dir_y * step;
        bool moved = false;

        if (fabsf(dir_x) > 0.0001f &&
            can_stand_at(lv, nx, e->y, e->z) && ai_can_enter_room(lv, e, nx, e->y) &&
            ai_position_clear_of_player(lv, nx, e->y, e, stop_range) &&
            ai_position_clear_of_enemies(i, nx, e->y, e)) {
            e->x = nx;
            moved = true;
        }
        if (fabsf(dir_y) > 0.0001f &&
            can_stand_at(lv, e->x, ny, e->z) && ai_can_enter_room(lv, e, e->x, ny) &&
            ai_position_clear_of_player(lv, e->x, ny, e, stop_range) &&
            ai_position_clear_of_enemies(i, e->x, ny, e)) {
            e->y = ny;
            moved = true;
        }

        if (!moved && (fabsf(dir_x) > 0.0001f || fabsf(dir_y) > 0.0001f)) {
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
        refresh_mission_totals();
        refresh_success_percent();
        g_level_won = true;
        snprintf(g_status, sizeof(g_status), g_random_play ? "RESULTS %d%% A NEXT" : "RESULTS %d%%", g_success_percent);
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


    int target_items = 30;
    placed = 0;
    attempts = 0;
    while (placed < target_items && attempts < 6000) {
        attempts++;
        int cx = (int)(rng_next(&g_random_seed) % (uint32_t)cells_w);
        int cy = (int)(rng_next(&g_random_seed) % (uint32_t)cells_h);
        if (cx < 3 && cy < 3) continue;
        if (cx == cells_w - 1 && cy == cells_h - 1) continue;
        int ox, oy;
        maze_cell_origin(cx, cy, &ox, &oy);
        if (lv->tiles[oy * lv->width + ox] != 0) continue;
        uint32_t r = rng_next(&g_random_seed) % 10u;
        lv->tiles[oy * lv->width + ox] = (r == 0) ? TILE_PURPLE : ((r < 3) ? TILE_PINK : TILE_DOT);
        placed++;
    }

    placed = 0;
    attempts = 0;
    while (placed < 3 && attempts < 2000) {
        attempts++;
        int cx = (int)(rng_next(&g_random_seed) % (uint32_t)cells_w);
        int cy = (int)(rng_next(&g_random_seed) % (uint32_t)cells_h);
        if (cx < 5 && cy < 5) continue;
        if (cx == cells_w - 1 && cy == cells_h - 1) continue;
        int ox, oy;
        maze_cell_origin(cx, cy, &ox, &oy);
        if (lv->tiles[oy * lv->width + ox] != 0) continue;
        lv->tiles[oy * lv->width + ox] = (placed == 0) ? TILE_KEY : TILE_NPC;
        placed++;
    }

    snprintf(g_level_name, sizeof(g_level_name), "RANDOM %04lX", (unsigned long)(seed & 0xFFFFu));
    force_valid_spawn(lv);
}

static void enter_random_seed_play(void) {
    uint32_t seed = (uint32_t)svcGetSystemTick();
    g_loaded_npc_metadata = false;
    generate_random_maze(&g_level, seed);
    randomize_weapon_stats(seed);
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
        g_loaded_npc_metadata = false;
        new_open_level(&g_level);
        default_slot_name(g_slot, g_level_name, sizeof(g_level_name));
        g_dirty = true;
        snprintf(g_status, sizeof(g_status), "NEW SLOT %d", g_slot);
    }

    if (!g_loaded_npc_metadata) randomize_weapon_stats(checksum_level_tiles(&g_level));

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
    /* Spawn a frozen runtime preview in editor mode so NPCs, enemies, coins,
       doors, and pickups are visible in the 3D/editor views without running
       AI, pickups, combat, or door triggers. */
    spawn_entities_from_level(&g_level);
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
    g_screen_shake_enabled = true;
    g_settings_scroll = 0;
    g_settings_dirty = true;
    snprintf(g_status, sizeof(g_status), "SETTINGS DEFAULTS");
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
        case 10:
            g_screen_shake_enabled = !g_screen_shake_enabled;
            if (!g_screen_shake_enabled) g_screen_shake_timer = 0.0f;
            snprintf(g_status, sizeof(g_status), "SHAKE %s", g_screen_shake_enabled ? "ON" : "OFF");
            break;
        case 11:
            g_default_npc_color = (uint8_t)((g_default_npc_color + (dir > 0 ? 1 : 7)) & 7);
            snprintf(g_status, sizeof(g_status), "DEFAULT NPC COLOR %d", g_default_npc_color);
            break;
        case 12:
            snprintf(g_status, sizeof(g_status), "DEFAULT ART USE NPC EDIT");
            break;
        case 13:
            g_player_health_max = clampi32(g_player_health_max + dir, PLAYER_HEALTH_MIN, PLAYER_HEALTH_MAX);
            if (g_player_health > g_player_health_max) g_player_health = g_player_health_max;
            snprintf(g_status, sizeof(g_status), "PLAYER HP %d", g_player_health_max);
            break;
    }

    g_settings_dirty = true;
}

static void handle_settings_menu_input(u32 kDown) {
    const int visible_rows = 10;
    if (kDown & KEY_DUP) g_settings_cursor = (g_settings_cursor + SETTINGS_ROW_COUNT - 1) % SETTINGS_ROW_COUNT;
    if (kDown & KEY_DDOWN) g_settings_cursor = (g_settings_cursor + 1) % SETTINGS_ROW_COUNT;
    if (g_settings_cursor < g_settings_scroll) g_settings_scroll = g_settings_cursor;
    if (g_settings_cursor >= g_settings_scroll + visible_rows) g_settings_scroll = g_settings_cursor - visible_rows + 1;
    if (g_settings_scroll < 0) g_settings_scroll = 0;
    if (g_settings_scroll > SETTINGS_ROW_COUNT - visible_rows) g_settings_scroll = SETTINGS_ROW_COUNT - visible_rows;

    if (kDown & KEY_DLEFT) adjust_setting(-1);
    if (kDown & KEY_DRIGHT) adjust_setting(1);
    if (kDown & KEY_A) adjust_setting(1);
    if (kDown & KEY_X) reset_settings_defaults();

    if (kDown & KEY_B) {
        if (g_settings_dirty) {
            save_app_settings();
            g_settings_dirty = false;
        }
        g_settings_menu = false;
        snprintf(g_status, sizeof(g_status), "SETTINGS SAVED");
    }
}

void handle_world_menu_input(u32 kDown) {
    if (g_entity_edit_mode != EDIT_MODE_NONE) {
        handle_entity_edit_input(kDown);
        return;
    }
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
            case MENU_ACTION_WEAPONS:
                open_weapon_editor(g_entity_edit_weapon);
                g_delete_armed = false;
                snprintf(g_status, sizeof(g_status), "WEAPON EDIT");
                break;
            case MENU_ACTION_SETTINGS:
                g_settings_menu = true;
                g_settings_scroll = 0;
                g_delete_armed = false;
                snprintf(g_status, sizeof(g_status), "SETTINGS");
                break;
        }
    }
}

void update_physics_and_movement(float dt, u32 kDown, u32 kHeld) {
    Level *lv = &g_level;

    if (g_player_hurt_timer > 0.0f) {
        g_player_hurt_timer -= dt;
        if (g_player_hurt_timer < 0.0f) g_player_hurt_timer = 0.0f;
    }
    if (g_weapon_bounce_timer > 0.0f) {
        g_weapon_bounce_timer -= dt;
        if (g_weapon_bounce_timer < 0.0f) g_weapon_bounce_timer = 0.0f;
    }
    if (g_screen_shake_timer > 0.0f) {
        g_screen_shake_timer -= dt;
        if (g_screen_shake_timer < 0.0f) g_screen_shake_timer = 0.0f;
    }

    if (g_level_won) {
        update_view_bob_from_speed(dt, 0.0f, true);
        if (g_random_play && (kDown & KEY_A)) {
            enter_random_seed_play();
        }
        return;
    }

    if (g_player_dead) {
        update_view_bob_from_speed(dt, 0.0f, true);
        if (kDown & KEY_A) {
            reset_player_after_caught(lv);
            g_player_health = g_player_health_max;
            g_player_dead = false;
            g_player_hurt_timer = PLAYER_HURT_TIME;
            memset(g_projectiles, 0, sizeof(g_projectiles));
            g_projectile_count = 0;
        }
        return;
    }

    if (g_attack_cooldown > 0.0f) {
        g_attack_cooldown -= dt;
        if (g_attack_cooldown < 0.0f) g_attack_cooldown = 0.0f;
    }
    if (g_slash_timer > 0.0f) {
        g_slash_timer -= dt;
        if (g_slash_timer < 0.0f) g_slash_timer = 0.0f;
    }
    update_npc_talk_timers(dt);
    if (kDown & KEY_Y) cycle_owned_weapon();
    attack_with_weapon(lv, kDown);
    if (!toggle_nearby_door(lv, kDown)) talk_to_nearby_npc(lv, kDown);

    apply_vertical_physics(lv, dt, kDown, true);
    apply_lr_look(lv, dt, kHeld, true);
    apply_cstick_look(lv, dt, kHeld);
    apply_touch_look_play_only(lv, kHeld);
    float speed = 0.0f;
    apply_cpad_movement(lv, dt, kHeld, true, &speed);
    update_view_bob_from_speed(dt, speed, lv->on_ground);
    update_collectibles_and_doors(lv, dt);
    update_boss_projectiles(lv, dt);
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

    /* Left panel is the 4x4 tile palette. */
    if (tp->px < 100) return false;

    if (touch_in_rect(tp, 188, 188, 216, 206)) {
        g_editor_tool = (g_editor_tool + 1) % 3;
        snprintf(g_status, sizeof(g_status), "TOOL %s", editor_tool_name(g_editor_tool));
        return true;
    }
    if (touch_in_rect(tp, 188, 210, 216, 232)) {
        g_selected_room = (uint8_t)((g_selected_room + 1) % (MAX_ROOM_ID + 1));
        snprintf(g_status, sizeof(g_status), "ROOM %s", room_class_name(g_selected_room));
        return true;
    }

    if (touch_in_rect(tp, 108, 188, 144, 206)) {
        editor_zoom_out(lv);
        return true;
    }
    if (touch_in_rect(tp, 148, 188, 184, 206)) {
        editor_zoom_in(lv);
        return true;
    }
    if (touch_in_rect(tp, 108, 210, 184, 232)) {
        editor_zoom_fit(lv);
        return true;
    }

    if (touch_in_rect(tp, 218, 214, 248, 236)) {
        editor_pan_view(lv, -1, 0);
        return true;
    }
    if (touch_in_rect(tp, 252, 188, 286, 210)) {
        editor_pan_view(lv, 0, -1);
        return true;
    }
    if (touch_in_rect(tp, 252, 214, 286, 236)) {
        editor_pan_view(lv, 0, 1);
        return true;
    }
    if (touch_in_rect(tp, 290, 214, 320, 236)) {
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

    if (tp.py >= EDITOR_CTRL_Y && tp.px < 100) {
        for (int t = 0; t <= MAX_TILE_ID; t++) {
            int row = t / 4;
            int col = t & 3;
            int x0 = 4 + col * 23;
            int y0 = 188 + row * 12;
            if (tp.px >= x0 - 2 && tp.px <= x0 + 19 + 2 &&
                tp.py >= y0 - 1 && tp.py <= y0 + 10 + 1) {
                g_selected_tile = (uint8_t)t;
                snprintf(g_status, sizeof(g_status), "%s", tile_short_name(g_selected_tile));
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

    uint8_t cur_tile = tile_at(lv, x, y);
    if (g_editor_tool == EDITOR_TOOL_SELECT) {
        if (cur_tile == TILE_AI_SPAWN || cur_tile == TILE_NPC || cur_tile == TILE_DOOR) {
            open_entity_editor_at(x, y);
        } else {
            g_selected_tile = cur_tile;
            snprintf(g_status, sizeof(g_status), "SELECT %d,%d %s R:%s", x, y, tile_short_name(cur_tile), room_class_name(room_at(lv, x, y)));
        }
        return;
    }
    if (g_editor_tool == EDITOR_TOOL_ROOM) {
        uint8_t old_room = room_at(lv, x, y);
        set_room(lv, x, y, g_selected_room);
        if (old_room != g_selected_room) g_dirty = true;
        snprintf(g_status, sizeof(g_status), "ROOM %d,%d %s", x, y, room_class_name(g_selected_room));
        return;
    }
    if ((cur_tile == TILE_AI_SPAWN || cur_tile == TILE_NPC || cur_tile == TILE_DOOR) && (kHeld & KEY_A)) {
        open_entity_editor_at(x, y);
        return;
    }
    if (cur_tile == TILE_NPC && (kHeld & KEY_L)) {
        NPC *n = npc_ensure_at(x, y);
        if (n) {
            n->color_id = (uint8_t)((n->color_id + 1) & 7);
            g_dirty = true;
            snprintf(g_status, sizeof(g_status), "NPC COLOR %d", n->color_id);
        }
        return;
    }
    if (cur_tile == TILE_NPC && (kHeld & KEY_Y)) {
        NPC *n = npc_ensure_at(x, y);
        if (n) {
            n->quest_type = (uint8_t)((n->quest_type + 1) % 4);
            if (n->quest_type == QUEST_NONE) n->quest_target = 0;
            else if (n->quest_type == QUEST_COINS) n->quest_target = 5;
            else if (n->quest_type == QUEST_KEY) n->quest_target = 1;
            else n->quest_target = 1;
            g_dirty = true;
            snprintf(g_status, sizeof(g_status), "%s", npc_quest_desc(n));
        }
        return;
    }
    if (cur_tile == TILE_NPC && (kHeld & KEY_R)) {
        NPC *n = npc_ensure_at(x, y);
        if (n) {
            int r = n->reward_kind;
            if (r == REWARD_DOT) r = REWARD_KEY;
            else if (r == REWARD_KEY) r = REWARD_PINK;
            else if (r == REWARD_PINK) r = REWARD_PURPLE;
            else if (r == REWARD_PURPLE) r = REWARD_HEALTH;
        else if (r == REWARD_HEALTH) r = REWARD_WEAPON_BASE;
            else if (r >= REWARD_WEAPON_BASE && r < REWARD_WEAPON_BASE + MAX_WEAPONS - 1) r++;
            else r = REWARD_DOT;
            n->reward_kind = (uint8_t)r;
            n->reward_amount = 1;
            g_dirty = true;
            snprintf(g_status, sizeof(g_status), "NPC REWARD %s", reward_kind_name(r));
        }
        return;
    }

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
        if (old != 0) {
            g_dirty = true;
            if (old == TILE_AI_SPAWN || old == TILE_NPC || old == TILE_DOOR || old == TILE_DOT || old == TILE_PINK || old == TILE_PURPLE || old == TILE_KEY) {
                spawn_entities_from_level(lv);
            }
        }
        snprintf(g_status, sizeof(g_status), "ERASE %d %d", x, y);
    } else {
        uint8_t old = tile_at(lv, x, y);
        set_tile(lv, x, y, g_selected_tile);
        if (g_selected_tile == TILE_NPC) npc_ensure_at(x, y);
        if (g_selected_tile == TILE_AI_SPAWN) enemy_meta_ensure_at(x, y);
        if (g_selected_tile == TILE_DOOR) door_meta_ensure_at(x, y);
        if (old != g_selected_tile) {
            g_dirty = true;
            if (old == TILE_AI_SPAWN || old == TILE_NPC || old == TILE_DOOR || old == TILE_DOT || old == TILE_PINK || old == TILE_PURPLE || old == TILE_KEY ||
                g_selected_tile == TILE_AI_SPAWN || g_selected_tile == TILE_NPC || g_selected_tile == TILE_DOOR || g_selected_tile == TILE_DOT || g_selected_tile == TILE_PINK || g_selected_tile == TILE_PURPLE || g_selected_tile == TILE_KEY) {
                spawn_entities_from_level(lv);
            }
        }
        snprintf(g_status, sizeof(g_status), "PAINT %d %d", x, y);
    }
}

void handle_global_input(u32 kDown, u32 kHeld) {
    if (kDown & KEY_START) {
        if (g_edit_mode && g_entity_edit_mode != EDIT_MODE_NONE) {
            snprintf(g_status, sizeof(g_status), "B BACK FIRST");
            return;
        }
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

    if ((kDown & KEY_Y) && (kHeld & KEY_L)) {
        snprintf(g_status, sizeof(g_status), "L+Y SAVE DISABLED");
        return;
    }

    if (g_edit_mode && (kDown & KEY_A)) {
        save_bwl2(&g_level);
        return;
    }

    /* Plain Y is used by play mode to cycle owned weapons and by editor
       entity shortcuts. Do not reload the level here, because that resets
       the player spawn/position whenever Y is pressed. */

}

void handle_editor_input(float dt, u32 kDown, u32 kHeld) {
    if (g_entity_edit_mode != EDIT_MODE_NONE) {
        handle_entity_edit_input(kDown);
        return;
    }
    if (kDown & KEY_DUP) editor_zoom_in(&g_level);
    if (kDown & KEY_DDOWN) editor_zoom_out(&g_level);

    if (kDown & KEY_DLEFT) {
        if (g_editor_tool == EDITOR_TOOL_ROOM) {
            g_selected_room = (uint8_t)((g_selected_room + MAX_ROOM_ID) % (MAX_ROOM_ID + 1));
            snprintf(g_status, sizeof(g_status), "ROOM %s", room_class_name(g_selected_room));
        } else {
            g_selected_tile = (uint8_t)((g_selected_tile + MAX_TILE_ID) % (MAX_TILE_ID + 1));
            snprintf(g_status, sizeof(g_status), "%s", tile_short_name(g_selected_tile));
        }
    }
    if (kDown & KEY_DRIGHT) {
        if (g_editor_tool == EDITOR_TOOL_ROOM) {
            g_selected_room = (uint8_t)((g_selected_room + 1) % (MAX_ROOM_ID + 1));
            snprintf(g_status, sizeof(g_status), "ROOM %s", room_class_name(g_selected_room));
        } else {
            g_selected_tile = (uint8_t)((g_selected_tile + 1) % (MAX_TILE_ID + 1));
            snprintf(g_status, sizeof(g_status), "%s", tile_short_name(g_selected_tile));
        }
    }

    update_editor_player(dt, kHeld);
    editor_touch(kDown, kHeld);
}

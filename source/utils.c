#include "common.h"

float clampf32(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

uint16_t clamp_u16_from_float(float v) {
    if (v < 0.0f) return 0;
    if (v > 65535.0f) return 65535;
    return (uint16_t)(v + 0.5f);
}

int clampi32(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

int round_size_step(int v) {
    v = clampi32(v, MIN_MAP_W, MAX_MAP_W);
    v = ((v + (MAP_SIZE_STEP / 2)) / MAP_SIZE_STEP) * MAP_SIZE_STEP;
    return clampi32(v, MIN_MAP_W, MAX_MAP_W);
}

void sanitize_level_name(char *name) {
    if (!name || !name[0]) {
        if (name) snprintf(name, LEVEL_NAME_MAX + 1, "Untitled");
        return;
    }

    int out = 0;
    for (int i = 0; name[i] && out < LEVEL_NAME_MAX; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 32);
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_' || ch == '.') {
            name[out++] = (char)ch;
        }
    }
    while (out > 0 && name[out - 1] == ' ') out--;
    name[out] = '\0';
    if (out == 0) snprintf(name, LEVEL_NAME_MAX + 1, "Untitled");
}

uint16_t qpos(float v) {
    return clamp_u16_from_float(v * POS_SCALE);
}

float uqpos(uint16_t v) {
    return ((float)v) / POS_SCALE;
}

uint16_t qangle(float rad) {
    float a = fmodf(rad, TWO_PI_F);
    if (a < 0.0f) a += TWO_PI_F;
    return (uint16_t)(((a * ANGLE_SCALE) + 0.5f));
}

float uqangle(uint16_t v) {
    return (((float)v) / 65535.0f) * TWO_PI_F;
}

uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

float read_f32_le(const uint8_t *p) {
    uint32_t u = read_u32_le(p);
    float f;
    memcpy(&f, &u, sizeof(float));
    return f;
}

bool write_all(FILE *f, const void *data, size_t size) {
    return fwrite(data, 1, size, f) == size;
}

bool write_u16_le(FILE *f, uint16_t v) {
    uint8_t b[2];
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)(v >> 8);
    return write_all(f, b, 2);
}

int tile_index(const Level *lv, int x, int y) {
    return y * lv->width + x;
}

uint8_t tile_at(const Level *lv, int x, int y) {
    if (x < 0 || y < 0 || x >= lv->width || y >= lv->height) return 1;
    return lv->tiles[tile_index(lv, x, y)];
}

void set_tile(Level *lv, int x, int y, uint8_t v) {
    if (x < 0 || y < 0 || x >= lv->width || y >= lv->height) return;
    lv->tiles[tile_index(lv, x, y)] = v & MAX_TILE_ID;
}

uint8_t room_at(const Level *lv, int x, int y) {
    if (!lv || x < 0 || y < 0 || x >= lv->width || y >= lv->height) return ROOM_NONE;
    int idx = tile_index(lv, x, y);
    if (idx < 0 || idx >= MAX_TILES) return ROOM_NONE;
    return g_room_tiles[idx] <= MAX_ROOM_ID ? g_room_tiles[idx] : ROOM_NONE;
}

void set_room(Level *lv, int x, int y, uint8_t v) {
    if (!lv || x < 0 || y < 0 || x >= lv->width || y >= lv->height) return;
    int idx = tile_index(lv, x, y);
    if (idx < 0 || idx >= MAX_TILES) return;
    g_room_tiles[idx] = (v <= MAX_ROOM_ID) ? v : ROOM_NONE;
}

void clear_room_overlay(void) {
    memset(g_room_tiles, 0, sizeof(g_room_tiles));
}

void clear_texture_overlays(void) {
    memset(g_wall_textures, 0, sizeof(g_wall_textures));
    memset(g_floor_textures, 0, sizeof(g_floor_textures));
}

uint16_t actor_id_for_tile(uint8_t tile, int x, int y) {
    uint32_t base = ((uint32_t)(tile & 0x0F) << 12) ^ ((uint32_t)(y & 0xFF) << 6) ^ (uint32_t)(x & 0x3F);
    base ^= ((uint32_t)(x & 0xC0) << 8) ^ ((uint32_t)(y & 0x100) << 6);
    if (base == 0) base = 1;
    return (uint16_t)(base & 0xFFFFu);
}

void clear_event_flags(void) {
    memset(g_event_flags, 0, sizeof(g_event_flags));
}

bool event_flag_get(uint8_t flag_id) {
    if (flag_id >= MAX_EVENT_FLAGS) return false;
    return g_event_flags[flag_id] != 0;
}

void event_flag_set(uint8_t flag_id, bool value) {
    if (flag_id >= MAX_EVENT_FLAGS) return;
    g_event_flags[flag_id] = value ? 1 : 0;
}

void event_flag_toggle(uint8_t flag_id) {
    if (flag_id >= MAX_EVENT_FLAGS) return;
    g_event_flags[flag_id] ^= 1;
}

void clear_triggers(void) {
    memset(g_triggers, 0, sizeof(g_triggers));
    g_trigger_count = 0;
}

const char *synth_event_name(uint8_t event_id) {
    switch (event_id) {
        case AUDIO_EVENT_ATTACK: return "ATTACK";
        case AUDIO_EVENT_HIT: return "HIT";
        case AUDIO_EVENT_PICKUP: return "PICKUP";
        case AUDIO_EVENT_DOOR: return "DOOR";
        case AUDIO_EVENT_QUEST: return "QUEST";
        case AUDIO_EVENT_NPC: return "NPC";
        case AUDIO_EVENT_ENEMY: return "ENEMY";
        case AUDIO_EVENT_BOSS: return "BOSS";
        default: return "NONE";
    }
}

const char *audio_sound_name(uint8_t sound_id) {
    switch (sound_id) {
        case AUDIO_ID_ATTACK: return "ATTACK";
        case AUDIO_ID_HIT: return "HIT";
        case AUDIO_ID_PICKUP: return "PICKUP";
        case AUDIO_ID_DOOR: return "DOOR";
        case AUDIO_ID_QUEST: return "QUEST";
        case AUDIO_ID_NPC: return "NPC";
        case AUDIO_ID_ENEMY: return "ENEMY";
        case AUDIO_ID_BOSS: return "BOSS";
        case AUDIO_ID_MUSIC: return "MUSIC";
        case AUDIO_ID_NONE:
        default: return "NONE";
    }
}

uint8_t synth_default_sound_for_event(uint8_t event_id) {
    switch (event_id) {
        case AUDIO_EVENT_ATTACK: return AUDIO_ID_ATTACK;
        case AUDIO_EVENT_HIT: return AUDIO_ID_HIT;
        case AUDIO_EVENT_PICKUP: return AUDIO_ID_PICKUP;
        case AUDIO_EVENT_DOOR: return AUDIO_ID_DOOR;
        case AUDIO_EVENT_QUEST: return AUDIO_ID_QUEST;
        case AUDIO_EVENT_NPC: return AUDIO_ID_NPC;
        case AUDIO_EVENT_ENEMY: return AUDIO_ID_ENEMY;
        case AUDIO_EVENT_BOSS: return AUDIO_ID_BOSS;
        default: return AUDIO_ID_NONE;
    }
}

static void synth_add_pattern(uint8_t sound_id, uint8_t kind, uint8_t loop, uint8_t event_id, const SynthNote *notes, int count) {
    if (g_audio_pattern_count >= MAX_SYNTH_PATTERNS || !notes || count <= 0) return;
    SynthPattern *p = &g_audio_patterns[g_audio_pattern_count++];
    memset(p, 0, sizeof(*p));
    p->active = true;
    p->sound_id = sound_id;
    p->kind = kind;
    p->loop = loop ? 1 : 0;
    p->event_id = event_id;
    if (count > MAX_SYNTH_NOTES) count = MAX_SYNTH_NOTES;
    p->note_count = (uint8_t)count;
    for (int i = 0; i < count; i++) p->notes[i] = notes[i];
}

static void synth_add_default(uint8_t sound_id, uint8_t event_id, uint8_t pitch, uint8_t len, uint8_t wave, uint8_t vol) {
    SynthNote n = { pitch, len, wave, vol };
    synth_add_pattern(sound_id, AUDIO_KIND_SFX, 0, event_id, &n, 1);
}

void synth_reset_defaults(void) {
    memset(g_audio_patterns, 0, sizeof(g_audio_patterns));
    g_audio_pattern_count = 0;
    g_audio_enabled = true;
    g_audio_last_event = AUDIO_EVENT_NONE;
    g_audio_event_timer = 0.0f;
    g_level_music_id = AUDIO_ID_MUSIC;
    synth_add_default(AUDIO_ID_ATTACK, AUDIO_EVENT_ATTACK, 64, 5, AUDIO_WAVE_SQUARE, 10);
    synth_add_default(AUDIO_ID_HIT, AUDIO_EVENT_HIT, 38, 7, AUDIO_WAVE_NOISE, 12);
    synth_add_default(AUDIO_ID_PICKUP, AUDIO_EVENT_PICKUP, 76, 8, AUDIO_WAVE_TRIANGLE, 9);
    synth_add_default(AUDIO_ID_DOOR, AUDIO_EVENT_DOOR, 45, 12, AUDIO_WAVE_SAW, 8);
    synth_add_default(AUDIO_ID_QUEST, AUDIO_EVENT_QUEST, 84, 18, AUDIO_WAVE_TRIANGLE, 12);
    synth_add_default(AUDIO_ID_NPC, AUDIO_EVENT_NPC, 58, 9, AUDIO_WAVE_SINE, 6);
    synth_add_default(AUDIO_ID_ENEMY, AUDIO_EVENT_ENEMY, 32, 10, AUDIO_WAVE_PULSE, 9);
    synth_add_default(AUDIO_ID_BOSS, AUDIO_EVENT_BOSS, 26, 22, AUDIO_WAVE_SQUARE, 13);
    SynthNote music[] = {
        {48, 8, AUDIO_WAVE_TRIANGLE, 5}, {55, 8, AUDIO_WAVE_TRIANGLE, 5},
        {60, 8, AUDIO_WAVE_TRIANGLE, 6}, {55, 8, AUDIO_WAVE_TRIANGLE, 5},
        {50, 8, AUDIO_WAVE_TRIANGLE, 5}, {57, 8, AUDIO_WAVE_TRIANGLE, 5},
        {62, 10, AUDIO_WAVE_TRIANGLE, 6}, {57, 6, AUDIO_WAVE_TRIANGLE, 5}
    };
    synth_add_pattern(AUDIO_ID_MUSIC, AUDIO_KIND_MUSIC, 1, AUDIO_EVENT_NONE, music, (int)(sizeof(music) / sizeof(music[0])));
    for (int i = 0; i < MAX_WEAPONS; i++) if (!g_weapons[i].sound_id) g_weapons[i].sound_id = AUDIO_ID_ATTACK;
}

void synth_play_sound(uint8_t sound_id) {
    if (!g_audio_enabled || sound_id == AUDIO_ID_NONE) return;
    g_audio_last_event = AUDIO_EVENT_NONE;
    g_audio_event_timer = 0.18f;
    audio_play_sound(sound_id);
}

void synth_start_music(uint8_t sound_id) {
    if (!g_audio_enabled || sound_id == AUDIO_ID_NONE) return;
    audio_start_music(sound_id);
}

void synth_stop_music(void) {
    audio_stop_music();
}

void synth_play_event(uint8_t event_id) {
    if (!g_audio_enabled || event_id == AUDIO_EVENT_NONE) return;
    g_audio_last_event = event_id;
    g_audio_event_timer = 0.18f;
    audio_play_event(event_id);
}

void synth_update(float dt) {
    if (g_audio_event_timer > 0.0f) {
        g_audio_event_timer -= dt;
        if (g_audio_event_timer <= 0.0f) {
            g_audio_event_timer = 0.0f;
            g_audio_last_event = AUDIO_EVENT_NONE;
        }
    }
    audio_update(dt);
}

void clear_extended_entity_metadata(void) {
    memset(g_door_metas, 0, sizeof(g_door_metas));
    memset(g_npc_anims, 0, sizeof(g_npc_anims));
    memset(g_enemy_anims, 0, sizeof(g_enemy_anims));
    clear_triggers();
    synth_reset_defaults();
    g_door_meta_count = 0;
    g_npc_anim_count = 0;
    g_enemy_anim_count = 0;
}

uint8_t wall_texture_at(const Level *lv, int x, int y) {
    int i = tile_index(lv, x, y);
    if (i < 0 || i >= MAX_TILES) return 0;
    return g_wall_textures[i] & MAX_TEXTURE_ID;
}

uint8_t floor_texture_at(const Level *lv, int x, int y) {
    int i = tile_index(lv, x, y);
    if (i < 0 || i >= MAX_TILES) return 0;
    return g_floor_textures[i] & MAX_TEXTURE_ID;
}

void set_wall_texture(Level *lv, int x, int y, uint8_t v) {
    int i = tile_index(lv, x, y);
    if (i < 0 || i >= MAX_TILES) return;
    g_wall_textures[i] = v & MAX_TEXTURE_ID;
}

void set_floor_texture(Level *lv, int x, int y, uint8_t v) {
    int i = tile_index(lv, x, y);
    if (i < 0 || i >= MAX_TILES) return;
    g_floor_textures[i] = v & MAX_TEXTURE_ID;
}

DoorMeta *door_meta_find_at(int x, int y) {
    for (int i = 0; i < g_door_meta_count; i++) if (g_door_metas[i].active && g_door_metas[i].x == x && g_door_metas[i].y == y) return &g_door_metas[i];
    return NULL;
}

DoorMeta *door_meta_ensure_at(int x, int y) {
    DoorMeta *m = door_meta_find_at(x, y);
    if (m) return m;
    if (g_door_meta_count >= MAX_DOORS) return NULL;
    m = &g_door_metas[g_door_meta_count++];
    memset(m, 0, sizeof(*m));
    m->active = true;
    m->actor_id = actor_id_for_tile(TILE_DOOR, x, y);
    m->x = x;
    m->y = y;
    m->texture_id = 0;
    m->group_id = 0;
    m->door_type = DOOR_TYPE_AUTO;
    m->speed = DOOR_SPEED_MEDIUM;
    m->move_dir = DOOR_MOVE_UP;
    m->sound_id = AUDIO_ID_DOOR;
    return m;
}

uint8_t door_texture_at(int x, int y) {
    DoorMeta *m = door_meta_find_at(x, y);
    if (m) return m->texture_id & MAX_TEXTURE_ID;
    for (int i = 0; i < g_door_count; i++) if (g_doors[i].active && g_doors[i].x == x && g_doors[i].y == y) return g_doors[i].texture_id & MAX_TEXTURE_ID;
    return 0;
}

NPCAnim *npc_anim_find_at(int x, int y) {
    for (int i = 0; i < g_npc_anim_count; i++) if (g_npc_anims[i].active && g_npc_anims[i].x == x && g_npc_anims[i].y == y) return &g_npc_anims[i];
    return NULL;
}

NPCAnim *npc_anim_ensure_at(int x, int y) {
    NPCAnim *a = npc_anim_find_at(x, y);
    if (a) return a;
    if (g_npc_anim_count >= MAX_NPCS) return NULL;
    a = &g_npc_anims[g_npc_anim_count++];
    memset(a, 0, sizeof(*a));
    a->active = true;
    a->x = x;
    a->y = y;
    a->enabled = false;
    a->speed = ANIM_SPEED_OFF;
    return a;
}

EnemyAnim *enemy_anim_find_at(int x, int y) {
    for (int i = 0; i < g_enemy_anim_count; i++) if (g_enemy_anims[i].active && g_enemy_anims[i].x == x && g_enemy_anims[i].y == y) return &g_enemy_anims[i];
    return NULL;
}

EnemyAnim *enemy_anim_ensure_at(int x, int y) {
    EnemyAnim *a = enemy_anim_find_at(x, y);
    if (a) return a;
    if (g_enemy_anim_count >= MAX_ENEMIES) return NULL;
    a = &g_enemy_anims[g_enemy_anim_count++];
    memset(a, 0, sizeof(*a));
    a->active = true;
    a->x = x;
    a->y = y;
    a->enabled = false;
    a->speed = ANIM_SPEED_OFF;
    return a;
}

const char *door_type_name(uint8_t t) {
    if (t == DOOR_TYPE_KEY) return "KEY";
    if (t == DOOR_TYPE_TOGGLE) return "TOGGLE";
    if (t == DOOR_TYPE_SWITCH) return "SWITCH";
    return "AUTO";
}

const char *door_speed_name(uint8_t s) {
    if (s == DOOR_SPEED_SLOW) return "SLOW";
    if (s == DOOR_SPEED_FAST) return "FAST";
    return "MEDIUM";
}

const char *door_move_name(uint8_t d) {
    if (d == DOOR_MOVE_DOWN) return "DOWN";
    if (d == DOOR_MOVE_LEFT) return "LEFT";
    if (d == DOOR_MOVE_RIGHT) return "RIGHT";
    return "UP";
}

const char *anim_speed_name(uint8_t s) {
    if (s == ANIM_SPEED_SLOW) return "SLOW";
    if (s == ANIM_SPEED_MEDIUM) return "MEDIUM";
    if (s == ANIM_SPEED_FAST) return "FAST";
    return "OFF";
}

float door_speed_seconds(uint8_t s) {
    if (s == DOOR_SPEED_SLOW) return 1.35f;
    if (s == DOOR_SPEED_FAST) return 0.34f;
    return 0.72f;
}

static int anim_step_mod(uint8_t s) {
    if (s == ANIM_SPEED_SLOW) return 28;
    if (s == ANIM_SPEED_MEDIUM) return 14;
    if (s == ANIM_SPEED_FAST) return 7;
    return 0;
}

static bool frame_empty16(const uint16_t *f) {
    if (!f) return true;
    for (int i = 0; i < ENEMY_SPRITE_ROWS; i++) if (f[i]) return false;
    return true;
}

const uint16_t *npc_anim_frame_for(const NPC *n) {
    if (!n) return NULL;
    NPCAnim *a = npc_anim_find_at(n->x, n->y);
    if (!a || !a->enabled || a->speed == ANIM_SPEED_OFF) return NULL;
    bool talking = n->talk_timer > 0.0f;
    if (!talking) {
        float nx = (float)n->x + 0.5f;
        float ny = (float)n->y + 0.5f;
        float dx = g_level.player_x - nx;
        float dy = g_level.player_y - ny;
        float radius = (n->text_mode == TEXT_MODE_ALWAYS) ? 16.0f : 10.0f;
        talking = (dx * dx + dy * dy) <= radius * radius;
    }
    int state = g_edit_mode ? (n->completed ? NPC_ANIM_COMPLETE : NPC_ANIM_IDLE) : (n->completed ? NPC_ANIM_COMPLETE : (talking ? NPC_ANIM_TALK : NPC_ANIM_IDLE));
    int mod = anim_step_mod(a->speed);
    int frame = (g_edit_mode || mod == 0) ? 0 : (int)((g_frame_counter / (uint32_t)mod) % ANIM_FRAMES);
    if (frame_empty16(a->frames[state][frame])) return NULL;
    return a->frames[state][frame];
}

const uint16_t *enemy_anim_frame_for(const Enemy *e) {
    if (!e) return NULL;
    EnemyAnim *a = enemy_anim_find_at((int)floorf(e->start_x), (int)floorf(e->start_y));
    if (!a || !a->enabled || a->speed == ANIM_SPEED_OFF) return NULL;
    int state = ENEMY_ANIM_IDLE;
    if (!g_edit_mode) {
        if (e->ranged_attack && e->ranged_timer > 0.1f) state = ENEMY_ANIM_SHOOT;
        else if (e->ranged_timer > 0.1f || e->hit_timer > 0.0f) state = ENEMY_ANIM_ATTACK;
        else if (e->state) state = ENEMY_ANIM_MOVE;
    }
    int mod = anim_step_mod(a->speed);
    int frame = (g_edit_mode || mod == 0) ? 0 : (int)((g_frame_counter / (uint32_t)mod) % ANIM_FRAMES);
    if (frame_empty16(a->frames[state][frame])) return NULL;
    return a->frames[state][frame];
}

bool door_blocks_at(int x, int y) {
    for (int i = 0; i < g_door_count; i++) {
        Door *d = &g_doors[i];
        if (!d->active || d->x != x || d->y != y) continue;
        return d->open_t < 0.92f;
    }
    return true;
}

const char *room_class_name(uint8_t room) {
    switch (room) {
        case ROOM_TREASURE: return "TREASURE";
        case ROOM_BOSS: return "BOSS";
        case ROOM_TRAP: return "TRAP";
        case ROOM_ENEMY: return "ENEMY";
        case ROOM_CORRIDOR: return "CORRIDOR";
        case ROOM_SAFE: return "SAFE";
        case ROOM_PUZZLE: return "PUZZLE";
        case ROOM_SECRET: return "SECRET";
        case ROOM_NONE:
        default: return "NONE";
    }
}

Color room_class_color(uint8_t room) {
    switch (room) {
        case ROOM_TREASURE: return (Color){235, 185, 60};
        case ROOM_BOSS: return (Color){230, 60, 70};
        case ROOM_TRAP: return (Color){230, 110, 45};
        case ROOM_ENEMY: return (Color){175, 75, 230};
        case ROOM_CORRIDOR: return (Color){95, 110, 125};
        case ROOM_SAFE: return (Color){75, 220, 135};
        case ROOM_PUZZLE: return (Color){90, 155, 235};
        case ROOM_SECRET: return (Color){220, 95, 205};
        case ROOM_NONE:
        default: return (Color){0, 0, 0};
    }
}

bool tile_blocks_side(uint8_t tile, float z) {
    if (tile >= 1 && tile <= 6) return true;
    if (tile == PLATFORM_TILE) return z < (PLATFORM_TOP - STEP_HEIGHT);
    if (tile == TILE_DOOR) return true;
    return false;
}

bool tile_blocks_raycast(uint8_t tile) {
    if (tile >= 1 && tile <= 6) return true;
    if (tile == PLATFORM_TILE) return true;
    if (tile == TILE_DOOR) return true;
    return false;
}

float door_open_fraction_at(int x, int y) {
    for (int i = 0; i < g_door_count; i++) {
        const Door *d = &g_doors[i];
        if (!d->active) continue;
        if (d->x == x && d->y == y) return clampf32(d->open_t, 0.0f, 1.0f);
    }
    return 0.0f;
}

bool can_stand_at(const Level *lv, float x, float y, float z) {
    const float r = PLAYER_RADIUS;
    const float px[4] = { x - r, x + r, x - r, x + r };
    const float py[4] = { y - r, y - r, y + r, y + r };

    for (int i = 0; i < 4; i++) {
        int ix = (int)px[i];
        int iy = (int)py[i];
        if (ix < 0 || iy < 0 || ix >= lv->width || iy >= lv->height) return false;
        uint8_t tt = lv->tiles[tile_index(lv, ix, iy)] & MAX_TILE_ID;
        if (tt == TILE_DOOR) { if (door_blocks_at(ix, iy)) return false; }
        else if (tile_blocks_side(tt, z)) return false;
    }
    return true;
}

float ground_height_at(const Level *lv, float x, float y, float z) {
    int ix = (int)x;
    int iy = (int)y;
    if (ix < 0 || iy < 0 || ix >= lv->width || iy >= lv->height) return 0.0f;
    uint8_t tile = lv->tiles[tile_index(lv, ix, iy)];
    if (tile == PLATFORM_TILE && z >= (PLATFORM_TOP - STEP_HEIGHT)) return PLATFORM_TOP;
    return 0.0f;
}

void force_valid_spawn(Level *lv) {
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) {
        new_open_level(lv);
        return;
    }

    int n = lv->width * lv->height;
    for (int i = 0; i < n; i++) lv->tiles[i] &= 0x0F;

    lv->player_x = clampf32(lv->player_x, 1.1f, (float)lv->width - 1.1f);
    lv->player_y = clampf32(lv->player_y, 1.1f, (float)lv->height - 1.1f);
    lv->player_z = clampf32(lv->player_z, 0.0f, 8.0f);

    if (can_stand_at(lv, lv->player_x, lv->player_y, lv->player_z)) return;

    for (int y = 1; y < lv->height - 1; y++) {
        for (int x = 1; x < lv->width - 1; x++) {
            uint8_t t = tile_at(lv, x, y);
            if (t == 0 || t == PLATFORM_TILE) {
                float z = (t == PLATFORM_TILE) ? PLATFORM_TOP : 0.0f;
                if (can_stand_at(lv, (float)x + 0.5f, (float)y + 0.5f, z)) {
                    lv->player_x = (float)x + 0.5f;
                    lv->player_y = (float)y + 0.5f;
                    lv->player_z = z;
                    lv->player_vz = 0.0f;
                    lv->on_ground = true;
                    return;
                }
            }
        }
    }

    new_open_level(lv);
}

void make_border(Level *lv) {
    for (int x = 0; x < lv->width; x++) {
        set_tile(lv, x, 0, 1);
        set_tile(lv, x, lv->height - 1, 1);
    }
    for (int y = 0; y < lv->height; y++) {
        set_tile(lv, 0, y, 1);
        set_tile(lv, lv->width - 1, y, 1);
    }
}

void new_sized_level(Level *lv, uint16_t width, uint16_t height) {
    memset(lv, 0, sizeof(*lv));
    if (lv == &g_level) { clear_room_overlay(); clear_texture_overlays(); clear_extended_entity_metadata(); }
    lv->width = (uint16_t)clampi32(round_size_step(width), MIN_MAP_W, MAX_MAP_W);
    lv->height = (uint16_t)clampi32(round_size_step(height), MIN_MAP_H, MAX_MAP_H);
    lv->player_x = clampf32(5.5f, 1.1f, (float)lv->width - 1.1f);
    lv->player_y = clampf32(5.5f, 1.1f, (float)lv->height - 1.1f);
    lv->player_z = 0.0f;
    lv->player_vz = 0.0f;
    lv->player_angle = 0.0f;
    lv->on_ground = true;
    make_border(lv);

    int w = lv->width;

    if (lv->width > 40 && lv->height > 24) {
        for (int x = 12; x < 36 && x < lv->width - 1; x++) lv->tiles[18 * w + x] = 2;
    }
    if (lv->width > 40 && lv->height > 48) {
        for (int y = 20; y < 44 && y < lv->height - 1; y++) lv->tiles[y * w + 36] = 3;
    }
    if (lv->width > 76 && lv->height > 38) {
        for (int x = 50; x < 72 && x < lv->width - 1; x++) lv->tiles[34 * w + x] = 4;
    }
    if (lv->width > 76 && lv->height > 60) {
        for (int y = 35; y < 56 && y < lv->height - 1; y++) lv->tiles[y * w + 72] = 5;
    }

    if (lv->width > 28 && lv->height > 14) {
        for (int x = 10; x < 25 && x < lv->width - 1; x++) lv->tiles[10 * w + x] = PLATFORM_TILE;
    }
    if (lv->width > 28 && lv->height > 26) {
        for (int y = 10; y < 23 && y < lv->height - 1; y++) lv->tiles[y * w + 24] = PLATFORM_TILE;
    }

    if (lv->width > 32 && lv->height > 60) {
        for (int y = 50; y < 56 && y < lv->height - 1; y++) {
            for (int x = 18; x < 28 && x < lv->width - 1; x++) {
                if (x == 18 || x == 27 || y == 50 || y == 55) {
                    lv->tiles[y * w + x] = PLATFORM_TILE;
                }
            }
        }
    }
}

void new_open_level(Level *lv) {
    new_sized_level(lv, DEFAULT_MAP_W, DEFAULT_MAP_H);
}

void resize_level_preserve(Level *lv, uint16_t width, uint16_t height) {
    if (!lv) return;

    bool preserve_rooms = (lv == &g_level);
    uint8_t old_rooms[MAX_TILES];
    if (preserve_rooms) {
        memcpy(old_rooms, g_room_tiles, sizeof(old_rooms));
        memset(g_room_tiles, 0, sizeof(g_room_tiles));
    }

    Level *tmp = &g_resize_temp;
    memset(tmp, 0, sizeof(*tmp));
    tmp->width = (uint16_t)round_size_step(width);
    tmp->height = (uint16_t)round_size_step(height);

    int copy_w = lv->width < tmp->width ? lv->width : tmp->width;
    int copy_h = lv->height < tmp->height ? lv->height : tmp->height;

    for (int y = 0; y < copy_h; y++) {
        for (int x = 0; x < copy_w; x++) {
            tmp->tiles[y * tmp->width + x] = lv->tiles[y * lv->width + x] & 0x0F;
            if (preserve_rooms) g_room_tiles[y * tmp->width + x] = old_rooms[y * lv->width + x] <= MAX_ROOM_ID ? old_rooms[y * lv->width + x] : ROOM_NONE;
        }
    }

    make_border(tmp);
    tmp->player_x = clampf32(lv->player_x, 1.1f, (float)tmp->width - 1.1f);
    tmp->player_y = clampf32(lv->player_y, 1.1f, (float)tmp->height - 1.1f);
    tmp->player_z = lv->player_z;
    tmp->player_vz = 0.0f;
    tmp->player_angle = lv->player_angle;
    tmp->on_ground = true;
    force_valid_spawn(tmp);

    *lv = *tmp;
}

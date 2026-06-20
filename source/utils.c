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
        if (tile_blocks_side(lv->tiles[tile_index(lv, ix, iy)], z)) return false;
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
    if (lv == &g_level) clear_room_overlay();
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

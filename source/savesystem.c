#include "common.h"

void make_slot_path(char *out, size_t out_size, bool backup) {
    snprintf(out, out_size, backup ? SAVE_PATH_BACKUP : SAVE_PATH_PRIMARY, g_slot);
}

void make_slot_fs_path_for(int slot, char *out, size_t out_size, bool backup) {
    snprintf(out, out_size, backup ? SAVE_FS_BACKUP : SAVE_FS_PRIMARY, slot);
}

void make_slot_fs_path(char *out, size_t out_size, bool backup) {
    make_slot_fs_path_for(g_slot, out, out_size, backup);
}

void make_meta_fs_path_for(int slot, char *out, size_t out_size, bool backup) {
    snprintf(out, out_size, backup ? META_FS_BACKUP : META_FS_PRIMARY, slot);
}

bool mem_put_u8(uint8_t *out, size_t cap, size_t *pos, uint8_t v) {
    if (*pos >= cap) return false;
    out[(*pos)++] = v;
    return true;
}

bool mem_put_u16_le(uint8_t *out, size_t cap, size_t *pos, uint16_t v) {
    return mem_put_u8(out, cap, pos, (uint8_t)(v & 0xFF)) &&
           mem_put_u8(out, cap, pos, (uint8_t)(v >> 8));
}

bool mem_put_u32_le(uint8_t *out, size_t cap, size_t *pos, uint32_t v) {
    return mem_put_u8(out, cap, pos, (uint8_t)(v & 0xFF)) &&
           mem_put_u8(out, cap, pos, (uint8_t)((v >> 8) & 0xFF)) &&
           mem_put_u8(out, cap, pos, (uint8_t)((v >> 16) & 0xFF)) &&
           mem_put_u8(out, cap, pos, (uint8_t)((v >> 24) & 0xFF));
}

bool mem_put_raw_packet(uint8_t *out, size_t cap, size_t *pos, const uint8_t *tiles, int start, int count) {
    if (!mem_put_u8(out, cap, pos, (uint8_t)(count - 1))) return false;

    for (int i = 0; i < count; i += 2) {
        uint8_t a = tiles[start + i] & 0x0F;
        uint8_t b = 0;
        if (i + 1 < count) b = tiles[start + i + 1] & 0x0F;
        if (!mem_put_u8(out, cap, pos, (uint8_t)(a | (b << 4)))) return false;
    }

    return true;
}

bool encode_bwl2_memory(const Level *lv, uint8_t **out_data, size_t *out_size) {
    if (!lv || !out_data || !out_size) return false;
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return false;

    int n = lv->width * lv->height;
    size_t cap = (size_t)n * 2 + 32768;
    uint8_t *out = (uint8_t*)malloc(cap);
    if (!out) return false;

    size_t pos = 0;
    bool ok = true;

    ok = ok && mem_put_u8(out, cap, &pos, 'B');
    ok = ok && mem_put_u8(out, cap, &pos, 'W');
    ok = ok && mem_put_u8(out, cap, &pos, '4');
    ok = ok && mem_put_u16_le(out, cap, &pos, lv->width);
    ok = ok && mem_put_u16_le(out, cap, &pos, lv->height);
    ok = ok && mem_put_u16_le(out, cap, &pos, qpos(lv->player_x));
    ok = ok && mem_put_u16_le(out, cap, &pos, qpos(lv->player_y));
    ok = ok && mem_put_u16_le(out, cap, &pos, qpos(lv->player_z));
    ok = ok && mem_put_u16_le(out, cap, &pos, qangle(lv->player_angle));

    size_t tile_size_pos = pos;
    ok = ok && mem_put_u32_le(out, cap, &pos, 0);
    size_t tile_start = pos;

    int i = 0;
    while (ok && i < n) {
        uint8_t run_val = lv->tiles[i] & 0x0F;
        int run_len = 1;

        while (i + run_len < n && run_len < 128 && ((lv->tiles[i + run_len] & 0x0F) == run_val)) {
            run_len++;
        }

        if (run_len >= 4) {
            ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)(0x80 | (run_len - 1)));
            ok = ok && mem_put_u8(out, cap, &pos, run_val);
            i += run_len;
            continue;
        }

        int raw_start = i;
        int raw_count = 0;

        while (i < n && raw_count < 128) {
            if (raw_count > 0) {
                uint8_t val = lv->tiles[i] & 0x0F;
                int look = 1;
                while (i + look < n && look < 128 && ((lv->tiles[i + look] & 0x0F) == val)) look++;
                if (look >= 4) break;
            }
            i++;
            raw_count++;
        }

        if (raw_count <= 0) ok = false;
        else ok = ok && mem_put_raw_packet(out, cap, &pos, lv->tiles, raw_start, raw_count);
    }

    uint32_t tile_size = (uint32_t)(pos - tile_start);
    out[tile_size_pos + 0] = (uint8_t)(tile_size & 0xFF);
    out[tile_size_pos + 1] = (uint8_t)((tile_size >> 8) & 0xFF);
    out[tile_size_pos + 2] = (uint8_t)((tile_size >> 16) & 0xFF);
    out[tile_size_pos + 3] = (uint8_t)((tile_size >> 24) & 0xFF);

    /* BWL4 room class overlay. Values are per tile and optional for older saves. */
    ok = ok && mem_put_u8(out, cap, &pos, 'R') && mem_put_u8(out, cap, &pos, 'O') && mem_put_u8(out, cap, &pos, 'M') && mem_put_u8(out, cap, &pos, '4');
    ok = ok && mem_put_u32_le(out, cap, &pos, (uint32_t)n);
    for (int ri = 0; ok && ri < n; ri++) ok = ok && mem_put_u8(out, cap, &pos, g_room_tiles[ri] <= MAX_ROOM_ID ? g_room_tiles[ri] : ROOM_NONE);

    /* World/player metadata */
    ok = ok && mem_put_u8(out, cap, &pos, 'W') && mem_put_u8(out, cap, &pos, 'L') && mem_put_u8(out, cap, &pos, 'D') && mem_put_u8(out, cap, &pos, '5');
    ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)clampi32(g_player_health_max, PLAYER_HEALTH_MIN, PLAYER_HEALTH_MAX));

    /* NPC chunk */
    ok = ok && mem_put_u8(out, cap, &pos, 'N') && mem_put_u8(out, cap, &pos, 'P') && mem_put_u8(out, cap, &pos, 'C') && mem_put_u8(out, cap, &pos, '5');
    int npc_count = (lv == &g_level) ? g_npc_count : 0;
    if (npc_count < 0) npc_count = 0;
    if (npc_count > MAX_NPCS) npc_count = MAX_NPCS;
    ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)npc_count);
    for (int ni = 0; ok && ni < npc_count; ni++) {
        const NPC *npc = &g_npcs[ni];
        ok = ok && mem_put_u16_le(out, cap, &pos, (uint16_t)npc->x);
        ok = ok && mem_put_u16_le(out, cap, &pos, (uint16_t)npc->y);
        ok = ok && mem_put_u8(out, cap, &pos, npc->color_id);
        ok = ok && mem_put_u8(out, cap, &pos, npc->text_mode);
        ok = ok && mem_put_u8(out, cap, &pos, npc->text_speed <= TEXT_SPEED_FAST ? npc->text_speed : TEXT_SPEED_MEDIUM);
        for (int si = 0; ok && si < SPRITE_BYTES; si++) ok = ok && mem_put_u8(out, cap, &pos, npc->sprite[si]);
        for (int si = 0; ok && si < ENEMY_SPRITE_ROWS; si++) ok = ok && mem_put_u16_le(out, cap, &pos, npc->sprite16[si]);
        ok = ok && mem_put_u8(out, cap, &pos, npc->quest_type);
        ok = ok && mem_put_u16_le(out, cap, &pos, npc->quest_target);
        ok = ok && mem_put_u8(out, cap, &pos, npc->reward_kind);
        ok = ok && mem_put_u16_le(out, cap, &pos, npc->reward_amount);
        ok = ok && mem_put_u8(out, cap, &pos, npc->completed ? 1 : 0);
        int len = 0;
        while (len < NPC_TEXT_MAX - 1 && npc->text[len]) len++;
        ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)len);
        for (int ti = 0; ok && ti < len; ti++) ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)npc->text[ti]);
    }

    /* Enemy metadata chunk with stats + text + hierarchy/boss data */
    ok = ok && mem_put_u8(out, cap, &pos, 'E') && mem_put_u8(out, cap, &pos, 'N') && mem_put_u8(out, cap, &pos, 'M') && mem_put_u8(out, cap, &pos, '7');
    int em_count = (lv == &g_level) ? g_enemy_meta_count : 0;
    if (em_count < 0) em_count = 0;
    if (em_count > MAX_ENEMIES) em_count = MAX_ENEMIES;
    ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)em_count);
    for (int ei = 0; ok && ei < em_count; ei++) {
        const EnemyMeta *m = &g_enemy_metas[ei];
        ok = ok && mem_put_u16_le(out, cap, &pos, (uint16_t)m->x);
        ok = ok && mem_put_u16_le(out, cap, &pos, (uint16_t)m->y);
        ok = ok && mem_put_u8(out, cap, &pos, m->hp ? m->hp : 5);
        ok = ok && mem_put_u8(out, cap, &pos, m->attack ? m->attack : 1);
        ok = ok && mem_put_u8(out, cap, &pos, m->color_id);
        for (int si = 0; ok && si < SPRITE_BYTES; si++) ok = ok && mem_put_u8(out, cap, &pos, m->sprite[si]);
        for (int si = 0; ok && si < ENEMY_SPRITE_ROWS; si++) ok = ok && mem_put_u16_le(out, cap, &pos, m->sprite16[si]);
        ok = ok && mem_put_u8(out, cap, &pos, m->ai_rank);
        ok = ok && mem_put_u8(out, cap, &pos, m->spawn_kind);
        ok = ok && mem_put_u8(out, cap, &pos, m->spawn_limit);
        ok = ok && mem_put_u8(out, cap, &pos, m->command_range ? m->command_range : 10);
        ok = ok && mem_put_u8(out, cap, &pos, m->sight_range ? m->sight_range : (m->ai_rank == AI_RANK_BOSS ? 20 : (m->ai_rank == AI_RANK_CAPTAIN ? 16 : 13)));
        ok = ok && mem_put_u8(out, cap, &pos, m->ranged_attack);
        ok = ok && mem_put_u8(out, cap, &pos, m->melee_range ? m->melee_range : 7);
        ok = ok && mem_put_u8(out, cap, &pos, m->attack_cooldown ? m->attack_cooldown : 7);
        ok = ok && mem_put_u8(out, cap, &pos, m->spawn_cooldown ? m->spawn_cooldown : 25);
        ok = ok && mem_put_u8(out, cap, &pos, m->projectile_color & 7);
        ok = ok && mem_put_u8(out, cap, &pos, m->projectile_style % 3);
        ok = ok && mem_put_u8(out, cap, &pos, m->projectile_anim ? 1 : 0);
        ok = ok && mem_put_u8(out, cap, &pos, m->speed_attr ? m->speed_attr : 100);
        ok = ok && mem_put_u8(out, cap, &pos, m->size_pct ? m->size_pct : (m->ai_rank == AI_RANK_BOSS ? 118 : 100));
        ok = ok && mem_put_u8(out, cap, &pos, m->text_speed <= TEXT_SPEED_FAST ? m->text_speed : TEXT_SPEED_MEDIUM);
        for (int bi = 0; ok && bi < BOSS_SPRITE_ROWS; bi++) ok = ok && mem_put_u32_le(out, cap, &pos, m->boss_sprite[bi]);
        uint8_t tc = m->text_count;
        if (tc > ENEMY_TEXT_LINES) tc = ENEMY_TEXT_LINES;
        ok = ok && mem_put_u8(out, cap, &pos, tc);
        for (int li = 0; ok && li < tc; li++) {
            int len = 0;
            while (len < ENEMY_TEXT_MAX - 1 && m->text[li][len]) len++;
            ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)len);
            for (int ti = 0; ok && ti < len; ti++) ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)m->text[li][ti]);
        }
    }

    /* Weapon stat/name chunk */
    ok = ok && mem_put_u8(out, cap, &pos, 'W') && mem_put_u8(out, cap, &pos, 'E') && mem_put_u8(out, cap, &pos, 'P') && mem_put_u8(out, cap, &pos, '3');
    ok = ok && mem_put_u8(out, cap, &pos, MAX_WEAPONS);
    for (int wi = 0; ok && wi < MAX_WEAPONS; wi++) {
        int len = 0;
        while (len < (int)sizeof(g_weapons[wi].name) - 1 && g_weapons[wi].name[len]) len++;
        ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)len);
        for (int ti = 0; ok && ti < len; ti++) ok = ok && mem_put_u8(out, cap, &pos, (uint8_t)g_weapons[wi].name[ti]);
        ok = ok && mem_put_u8(out, cap, &pos, g_weapons[wi].damage);
        ok = ok && mem_put_u8(out, cap, &pos, g_weapons[wi].range);
        ok = ok && mem_put_u8(out, cap, &pos, g_weapons[wi].cooldown);
        ok = ok && mem_put_u8(out, cap, &pos, g_weapons[wi].color_id);
        for (int si = 0; ok && si < SPRITE_BYTES; si++) ok = ok && mem_put_u8(out, cap, &pos, g_weapons[wi].sprite[si]);
    }
    ok = ok && mem_put_u8(out, cap, &pos, 'E') && mem_put_u8(out, cap, &pos, 'N') && mem_put_u8(out, cap, &pos, 'D') && mem_put_u8(out, cap, &pos, '!');

    if (!ok) {
        free(out);
        return false;
    }

    *out_data = out;
    *out_size = pos;
    return true;
}

bool fs_write_whole_file(const char *fs_path, const uint8_t *data, size_t size) {
    if (!g_fs_ready || !fs_path || !data || size == 0 || size > 1024 * 256) return false;

    FS_Archive archive;
    Result rc = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(rc)) return false;

    FS_Path path = fsMakePath(PATH_ASCII, fs_path);
    FSUSER_DeleteFile(archive, path);

    Handle file = 0;
    rc = FSUSER_OpenFile(&file, archive, path, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    if (R_FAILED(rc)) {
        FSUSER_CloseArchive(archive);
        return false;
    }

    u32 written = 0;
    rc = FSFILE_Write(file, &written, 0, data, (u32)size, FS_WRITE_FLUSH);
    FSFILE_Close(file);
    FSUSER_CloseArchive(archive);

    return R_SUCCEEDED(rc) && written == (u32)size;
}

bool fs_read_whole_file(const char *fs_path, uint8_t **out_data, size_t *out_size) {
    if (!g_fs_ready || !fs_path || !out_data || !out_size) return false;

    FS_Archive archive;
    Result rc = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(rc)) return false;

    FS_Path path = fsMakePath(PATH_ASCII, fs_path);
    Handle file = 0;
    rc = FSUSER_OpenFile(&file, archive, path, FS_OPEN_READ, 0);
    if (R_FAILED(rc)) {
        FSUSER_CloseArchive(archive);
        return false;
    }

    u64 file_size64 = 0;
    rc = FSFILE_GetSize(file, &file_size64);
    if (R_FAILED(rc) || file_size64 == 0 || file_size64 > 1024 * 256) {
        FSFILE_Close(file);
        FSUSER_CloseArchive(archive);
        return false;
    }

    uint8_t *data = (uint8_t*)malloc((size_t)file_size64);
    if (!data) {
        FSFILE_Close(file);
        FSUSER_CloseArchive(archive);
        return false;
    }

    u32 bytes_read = 0;
    rc = FSFILE_Read(file, &bytes_read, 0, data, (u32)file_size64);
    FSFILE_Close(file);
    FSUSER_CloseArchive(archive);

    if (R_FAILED(rc) || bytes_read != (u32)file_size64) {
        free(data);
        return false;
    }

    *out_data = data;
    *out_size = (size_t)file_size64;
    return true;
}

bool fs_delete_path(const char *fs_path) {
    if (!g_fs_ready || !fs_path) return false;

    FS_Archive archive;
    Result rc = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(rc)) return false;

    rc = FSUSER_DeleteFile(archive, fsMakePath(PATH_ASCII, fs_path));
    FSUSER_CloseArchive(archive);
    return R_SUCCEEDED(rc);
}

void default_slot_name(int slot, char *out, size_t out_size) {
    snprintf(out, out_size, "LEVEL %d", slot);
}

bool save_slot_meta(int slot, const char *name) {
    char clean[LEVEL_NAME_MAX + 1];
    snprintf(clean, sizeof(clean), "%s", (name && name[0]) ? name : "Untitled");
    sanitize_level_name(clean);

    bool ok = false;
    char path[128];
    make_meta_fs_path_for(slot, path, sizeof(path), false);
    ok = fs_write_whole_file(path, (const uint8_t*)clean, strlen(clean));
    make_meta_fs_path_for(slot, path, sizeof(path), true);
    ok = fs_write_whole_file(path, (const uint8_t*)clean, strlen(clean)) || ok;
    return ok;
}

bool load_slot_meta(int slot, char *out, size_t out_size) {
    if (!out || out_size == 0) return false;
    out[0] = '\0';

    char path[128];
    uint8_t *data = NULL;
    size_t size = 0;

    make_meta_fs_path_for(slot, path, sizeof(path), false);
    if (!fs_read_whole_file(path, &data, &size)) {
        make_meta_fs_path_for(slot, path, sizeof(path), true);
        if (!fs_read_whole_file(path, &data, &size)) return false;
    }

    size_t n = size;
    if (n > LEVEL_NAME_MAX) n = LEVEL_NAME_MAX;
    memcpy(out, data, n);
    out[n] = '\0';
    free(data);
    sanitize_level_name(out);
    return out[0] != '\0';
}

bool decode_bwl2_tiles(const uint8_t *data, size_t size, Level *lv) {
    int expected = lv->width * lv->height;
    int out = 0;
    size_t i = 0;

    while (i < size && out < expected) {
        uint8_t ctrl = data[i++];
        int count = (ctrl & 0x7F) + 1;
        if (ctrl & 0x80) {
            if (i >= size) return false;
            uint8_t val = data[i++] & 0x0F;
            for (int k = 0; k < count && out < expected; k++) lv->tiles[out++] = val;
        } else {
            int byte_count = (count + 1) / 2;
            if (i + byte_count > size) return false;
            for (int k = 0; k < count && out < expected; k++) {
                uint8_t packed = data[i + (k / 2)];
                lv->tiles[out++] = (k & 1) ? ((packed >> 4) & 0x0F) : (packed & 0x0F);
            }
            i += byte_count;
        }
    }

    return out == expected && i == size;
}



static void save_expand_8x8_to_16(uint16_t *dst, const uint8_t *src8) {
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

static void save_expand_legacy_boss14_to_32(uint32_t *dst, const uint16_t *src14) {
    if (!dst) return;
    memset(dst, 0, sizeof(uint32_t) * BOSS_SPRITE_ROWS);
    if (!src14) { copy_default_boss_sprite(dst); return; }
    for (int y = 0; y < BOSS_SPRITE_H; y++) {
        int sy = (y * BOSS_LEGACY_SPRITE_H) / BOSS_SPRITE_H;
        uint16_t row = src14[sy];
        uint32_t out = 0;
        for (int x = 0; x < BOSS_SPRITE_W; x++) {
            int sx = (x * BOSS_LEGACY_SPRITE_W) / BOSS_SPRITE_W;
            if (row & (uint16_t)(1u << (BOSS_LEGACY_SPRITE_W - 1 - sx))) out |= (uint32_t)(1u << (BOSS_SPRITE_W - 1 - x));
        }
        dst[y] = out;
    }
}

static void normalize_npc_defaults(NPC *n) {
    if (!n) return;
    bool empty8 = true;
    for (int si = 0; si < SPRITE_BYTES; si++) if (n->sprite[si]) empty8 = false;
    if (empty8) copy_default_sprite(n->sprite, SPRITE_TARGET_NPC);
    bool empty16 = true;
    for (int si = 0; si < ENEMY_SPRITE_ROWS; si++) if (n->sprite16[si]) empty16 = false;
    if (empty16) save_expand_8x8_to_16(n->sprite16, n->sprite);
    if (n->text_mode > TEXT_MODE_ALWAYS) n->text_mode = TEXT_MODE_NEAR;
    if (n->text_speed > TEXT_SPEED_FAST) n->text_speed = TEXT_SPEED_MEDIUM;
}

static void normalize_enemy_meta_defaults(EnemyMeta *m) {
    if (!m) return;
    if (m->hp == 0) m->hp = (m->ai_rank == AI_RANK_BOSS) ? 24 : 5;
    if (m->attack == 0) m->attack = 1;
    bool empty8 = true; for (int si = 0; si < SPRITE_BYTES; si++) if (m->sprite[si]) empty8 = false;
    if (empty8) copy_default_sprite(m->sprite, SPRITE_TARGET_ENEMY);
    bool empty16 = true; for (int si = 0; si < ENEMY_SPRITE_ROWS; si++) if (m->sprite16[si]) empty16 = false;
    if (empty16) save_expand_8x8_to_16(m->sprite16, m->sprite);
    bool emptyboss = true; for (int bi = 0; bi < BOSS_SPRITE_ROWS; bi++) if (m->boss_sprite[bi]) emptyboss = false;
    if (emptyboss) copy_default_boss_sprite(m->boss_sprite);
    if (m->ai_rank > AI_RANK_BOSS) m->ai_rank = AI_RANK_GRUNT;
    if (m->spawn_kind > AI_SPAWN_GRUNT) m->spawn_kind = AI_SPAWN_NONE;
    if (m->spawn_limit > 8) m->spawn_limit = 8;
    if (m->command_range == 0) m->command_range = 10;
    if (m->sight_range == 0) m->sight_range = (m->ai_rank == AI_RANK_BOSS) ? 20 : (m->ai_rank == AI_RANK_CAPTAIN ? 16 : 13);
    if (m->sight_range > 40) m->sight_range = 40;
    if (!m->melee_range) m->melee_range = (m->ai_rank == AI_RANK_BOSS) ? 10 : 7;
    if (!m->attack_cooldown) m->attack_cooldown = 7;
    if (!m->spawn_cooldown) m->spawn_cooldown = 25;
    m->projectile_color &= 7;
    if (m->projectile_style > 2) m->projectile_style = 0;
    if (!m->speed_attr) m->speed_attr = 100;
    if (!m->size_pct) m->size_pct = (m->ai_rank == AI_RANK_BOSS) ? 118 : 100;
    if (m->ai_rank != AI_RANK_BOSS && m->size_pct > 115) m->size_pct = 115;
    if (m->text_speed > TEXT_SPEED_FAST) m->text_speed = TEXT_SPEED_MEDIUM;
}

static void reset_level_metadata_defaults(void) {
    memset(g_npcs, 0, sizeof(g_npcs));
    memset(g_enemy_metas, 0, sizeof(g_enemy_metas));
    clear_room_overlay();
    g_npc_count = 0;
    g_enemy_meta_count = 0;
    g_loaded_npc_metadata = false;
    g_loaded_enemy_metadata = false;
}

static void parse_bw3_chunks(const uint8_t *p, size_t size) {
    reset_level_metadata_defaults();
    size_t pos = 0;
    while (pos + 4 <= size) {
        char a = (char)p[pos + 0];
        char b = (char)p[pos + 1];
        char c = (char)p[pos + 2];
        char d = (char)p[pos + 3];
        pos += 4;
        if (a == 'E' && b == 'N' && c == 'D' && d == '!') break;

        if (a == 'R' && b == 'O' && c == 'M' && d == '4') {
            if (pos + 4 > size) break;
            uint32_t count = read_u32_le(p + pos); pos += 4;
            uint32_t max_count = (uint32_t)(g_load_temp.width * g_load_temp.height);
            if (count > MAX_TILES || pos + count > size) break;
            memset(g_room_tiles, 0, sizeof(g_room_tiles));
            uint32_t copy_count = count < max_count ? count : max_count;
            for (uint32_t ri = 0; ri < count; ri++) {
                uint8_t rv = p[pos++] & 0x0F;
                if (ri < copy_count) g_room_tiles[ri] = rv <= MAX_ROOM_ID ? rv : ROOM_NONE;
            }
        } else if (a == 'W' && b == 'L' && c == 'D' && d == '5') {
            if (pos >= size) break;
            g_player_health_max = clampi32(p[pos++], PLAYER_HEALTH_MIN, PLAYER_HEALTH_MAX);
            if (g_player_health <= 0 || g_player_health > g_player_health_max) g_player_health = g_player_health_max;
        } else if (a == 'N' && b == 'P' && c == 'C' && (d == '5' || d == '4' || d == '3')) {
            if (pos >= size) break;
            int count = p[pos++];
            if (count > MAX_NPCS) count = MAX_NPCS;
            for (int i = 0; i < count && g_npc_count < MAX_NPCS; i++) {
                size_t need = (d == '5') ? 55 : ((d == '4') ? 23 : 22);
                if (pos + need > size) break;
                NPC *n = &g_npcs[g_npc_count++];
                memset(n, 0, sizeof(*n));
                n->active = true;
                n->x = read_u16_le(p + pos); pos += 2;
                n->y = read_u16_le(p + pos); pos += 2;
                n->color_id = p[pos++];
                n->text_mode = p[pos++];
                n->text_speed = (d == '5' || d == '4') ? p[pos++] : TEXT_SPEED_MEDIUM;
                if (n->text_speed > TEXT_SPEED_FAST) n->text_speed = TEXT_SPEED_MEDIUM;
                for (int si = 0; si < SPRITE_BYTES && pos < size; si++) n->sprite[si] = p[pos++];
                if (d == '5') {
                    for (int si = 0; si < ENEMY_SPRITE_ROWS && pos + 1 < size; si++) { n->sprite16[si] = read_u16_le(p + pos); pos += 2; }
                }
                n->quest_type = p[pos++];
                n->quest_target = read_u16_le(p + pos); pos += 2;
                n->reward_kind = p[pos++];
                n->reward_amount = read_u16_le(p + pos); pos += 2;
                n->completed = p[pos++] != 0;
                if (n->text_mode > TEXT_MODE_ALWAYS) n->text_mode = TEXT_MODE_NEAR;
                if (pos >= size) break;
                int len = p[pos++];
                if (len >= NPC_TEXT_MAX) len = NPC_TEXT_MAX - 1;
                if (pos + (size_t)len > size) len = (int)(size - pos);
                memcpy(n->text, p + pos, len);
                n->text[len] = '\0';
                pos += len;
                normalize_npc_defaults(n);
            }
            g_loaded_npc_metadata = true;
        } else if (a == 'N' && b == 'P' && c == 'C' && d == 'S') {
            if (pos >= size) break;
            int count = p[pos++];
            if (count > MAX_NPCS) count = MAX_NPCS;
            for (int i = 0; i < count && g_npc_count < MAX_NPCS && pos + 12 <= size; i++) {
                NPC *n = &g_npcs[g_npc_count++];
                memset(n, 0, sizeof(*n));
                n->active = true;
                n->x = read_u16_le(p + pos); pos += 2;
                n->y = read_u16_le(p + pos); pos += 2;
                n->color_id = p[pos++];
                n->text_mode = TEXT_MODE_NEAR;
                n->text_speed = TEXT_SPEED_MEDIUM;
                n->quest_type = p[pos++];
                n->quest_target = read_u16_le(p + pos); pos += 2;
                n->reward_kind = p[pos++];
                n->reward_amount = read_u16_le(p + pos); pos += 2;
                n->completed = p[pos++] != 0;
                if (pos >= size) break;
                int len = p[pos++];
                if (len >= NPC_TEXT_MAX) len = NPC_TEXT_MAX - 1;
                if (pos + (size_t)len > size) len = (int)(size - pos);
                memcpy(n->text, p + pos, len);
                n->text[len] = '\0';
                pos += len;
                copy_default_sprite(n->sprite, SPRITE_TARGET_NPC);
                save_expand_8x8_to_16(n->sprite16, n->sprite);
            }
            g_loaded_npc_metadata = true;
        } else if (a == 'E' && b == 'N' && c == 'M' && (d == '7' || d == '6' || d == '5')) {
            if (pos >= size) break;
            int count = p[pos++];
            if (count > MAX_ENEMIES) count = MAX_ENEMIES;
            for (int i = 0; i < count && g_enemy_meta_count < MAX_ENEMIES && pos + (d == '7' ? 190 : (d == '6' ? 189 : 183)) <= size; i++) {
                EnemyMeta *m = &g_enemy_metas[g_enemy_meta_count++];
                memset(m, 0, sizeof(*m));
                m->active = true;
                m->x = read_u16_le(p + pos); pos += 2;
                m->y = read_u16_le(p + pos); pos += 2;
                m->hp = p[pos++];
                m->attack = p[pos++];
                m->color_id = p[pos++];
                for (int si = 0; si < SPRITE_BYTES && pos < size; si++) m->sprite[si] = p[pos++];
                for (int si = 0; si < ENEMY_SPRITE_ROWS && pos + 1 < size; si++) { m->sprite16[si] = read_u16_le(p + pos); pos += 2; }
                m->ai_rank = p[pos++];
                m->spawn_kind = p[pos++];
                m->spawn_limit = p[pos++];
                m->command_range = p[pos++];
                if (d == '7') m->sight_range = p[pos++];
                else m->sight_range = 0;
                m->ranged_attack = p[pos++] ? 1 : 0;
                if (d == '7' || d == '6') {
                    m->melee_range = p[pos++];
                    m->attack_cooldown = p[pos++];
                    m->spawn_cooldown = p[pos++];
                    m->projectile_color = p[pos++] & 7;
                    m->projectile_style = p[pos++] % 3;
                    m->projectile_anim = p[pos++] ? 1 : 0;
                } else {
                    m->melee_range = 0;
                    m->attack_cooldown = 0;
                    m->spawn_cooldown = 0;
                    m->projectile_color = m->color_id & 7;
                    m->projectile_style = 0;
                    m->projectile_anim = 1;
                }
                m->speed_attr = p[pos++];
                m->size_pct = p[pos++];
                m->text_speed = p[pos++];
                for (int bi = 0; bi < BOSS_SPRITE_ROWS && pos + 3 < size; bi++) { m->boss_sprite[bi] = read_u32_le(p + pos); pos += 4; }
                normalize_enemy_meta_defaults(m);
                if (pos >= size) break;
                m->text_count = p[pos++];
                if (m->text_count > ENEMY_TEXT_LINES) m->text_count = ENEMY_TEXT_LINES;
                for (int li = 0; li < m->text_count && pos < size; li++) {
                    int len = p[pos++];
                    if (len >= ENEMY_TEXT_MAX) len = ENEMY_TEXT_MAX - 1;
                    if (pos + (size_t)len > size) len = (int)(size - pos);
                    memcpy(m->text[li], p + pos, len);
                    m->text[li][len] = '\0';
                    pos += len;
                }
            }
            g_loaded_enemy_metadata = true;
        } else if (a == 'E' && b == 'N' && c == 'M' && d == '4') {
            if (pos >= size) break;
            int count = p[pos++];
            if (count > MAX_ENEMIES) count = MAX_ENEMIES;
            for (int i = 0; i < count && g_enemy_meta_count < MAX_ENEMIES && pos + 49 <= size; i++) {
                EnemyMeta *m = &g_enemy_metas[g_enemy_meta_count++];
                memset(m, 0, sizeof(*m));
                m->active = true;
                m->x = read_u16_le(p + pos); pos += 2;
                m->y = read_u16_le(p + pos); pos += 2;
                m->hp = p[pos++];
                m->attack = p[pos++];
                m->color_id = p[pos++];
                for (int si = 0; si < SPRITE_BYTES && pos < size; si++) m->sprite[si] = p[pos++];
                m->ai_rank = p[pos++];
                m->spawn_kind = p[pos++];
                m->spawn_limit = p[pos++];
                m->command_range = p[pos++];
                m->sight_range = 0;
                m->ranged_attack = p[pos++] ? 1 : 0;
                uint16_t legacy[BOSS_LEGACY_SPRITE_ROWS]; memset(legacy, 0, sizeof(legacy));
                for (int bi = 0; bi < BOSS_LEGACY_SPRITE_ROWS && pos + 1 < size; bi++) { legacy[bi] = read_u16_le(p + pos); pos += 2; }
                save_expand_legacy_boss14_to_32(m->boss_sprite, legacy);
                normalize_enemy_meta_defaults(m);
                if (pos >= size) break;
                m->text_count = p[pos++];
                if (m->text_count > ENEMY_TEXT_LINES) m->text_count = ENEMY_TEXT_LINES;
                for (int li = 0; li < m->text_count && pos < size; li++) {
                    int len = p[pos++];
                    if (len >= ENEMY_TEXT_MAX) len = ENEMY_TEXT_MAX - 1;
                    if (pos + (size_t)len > size) len = (int)(size - pos);
                    memcpy(m->text[li], p + pos, len);
                    m->text[li][len] = '\0';
                    pos += len;
                }
            }
            g_loaded_enemy_metadata = true;
        } else if (a == 'E' && b == 'N' && c == 'M' && (d == '3' || d == '2' || d == 'S')) {
            if (pos >= size) break;
            int count = p[pos++];
            if (count > MAX_ENEMIES) count = MAX_ENEMIES;
            for (int i = 0; i < count && g_enemy_meta_count < MAX_ENEMIES; i++) {
                EnemyMeta *m = &g_enemy_metas[g_enemy_meta_count++];
                memset(m, 0, sizeof(*m));
                m->active = true;
                if (d == 'S') {
                    if (pos + 5 > size) break;
                    m->x = read_u16_le(p + pos); pos += 2;
                    m->y = read_u16_le(p + pos); pos += 2;
                    m->hp = (uint8_t)(5 + ((m->x + m->y) & 3));
                    m->attack = 1;
                    m->color_id = (uint8_t)((m->x + m->y) & 7);
                    copy_default_sprite(m->sprite, SPRITE_TARGET_ENEMY);
                } else {
                    if ((d == '3' && pos + 16 > size) || (d == '2' && pos + 7 > size)) break;
                    m->x = read_u16_le(p + pos); pos += 2;
                    m->y = read_u16_le(p + pos); pos += 2;
                    m->hp = p[pos++];
                    m->attack = p[pos++];
                    if (d == '3') { m->color_id = p[pos++]; for (int si = 0; si < SPRITE_BYTES && pos < size; si++) m->sprite[si] = p[pos++]; }
                    else { m->color_id = (uint8_t)((m->x + m->y) & 7); copy_default_sprite(m->sprite, SPRITE_TARGET_ENEMY); }
                }
                m->ai_rank = AI_RANK_GRUNT; m->spawn_kind = AI_SPAWN_NONE; m->spawn_limit = 0; m->command_range = 10; m->sight_range = 13; m->ranged_attack = 0;
                m->melee_range = 7; m->attack_cooldown = 7; m->spawn_cooldown = 25; m->projectile_color = m->color_id & 7; m->projectile_style = 0; m->projectile_anim = 1;
                m->speed_attr = 100; m->size_pct = 100; m->text_speed = TEXT_SPEED_MEDIUM;
                save_expand_8x8_to_16(m->sprite16, m->sprite);
                copy_default_boss_sprite(m->boss_sprite);
                normalize_enemy_meta_defaults(m);
                if (pos >= size) break;
                m->text_count = p[pos++];
                if (m->text_count > ENEMY_TEXT_LINES) m->text_count = ENEMY_TEXT_LINES;
                for (int li = 0; li < m->text_count && pos < size; li++) {
                    int len = p[pos++];
                    if (len >= ENEMY_TEXT_MAX) len = ENEMY_TEXT_MAX - 1;
                    if (pos + (size_t)len > size) len = (int)(size - pos);
                    memcpy(m->text[li], p + pos, len); m->text[li][len] = '\0'; pos += len;
                }
            }
            g_loaded_enemy_metadata = true;
        } else if (a == 'W' && b == 'E' && c == 'P' && d == '3') {
            if (pos >= size) break;
            int count = p[pos++];
            if (count > MAX_WEAPONS) count = MAX_WEAPONS;
            for (int i = 0; i < count && pos < size; i++) {
                int len = p[pos++];
                if (len >= (int)sizeof(g_weapons[i].name)) len = (int)sizeof(g_weapons[i].name) - 1;
                if (pos + (size_t)len > size) len = (int)(size - pos);
                memcpy(g_weapons[i].name, p + pos, len); g_weapons[i].name[len] = '\0'; pos += len;
                if (pos + 12 > size) break;
                g_weapons[i].damage = p[pos++]; g_weapons[i].range = p[pos++]; g_weapons[i].cooldown = p[pos++]; g_weapons[i].color_id = p[pos++];
                for (int si = 0; si < SPRITE_BYTES && pos < size; si++) g_weapons[i].sprite[si] = p[pos++];
                bool empty = true; for (int si = 0; si < SPRITE_BYTES; si++) if (g_weapons[i].sprite[si]) empty = false;
                if (empty) copy_default_sprite(g_weapons[i].sprite, SPRITE_TARGET_WEAPON);
            }
        } else if (a == 'W' && b == 'E' && c == 'P' && d == '2') {
            if (pos >= size) break;
            int count = p[pos++]; if (count > MAX_WEAPONS) count = MAX_WEAPONS;
            for (int i = 0; i < count && pos < size; i++) {
                int len = p[pos++]; if (len >= (int)sizeof(g_weapons[i].name)) len = (int)sizeof(g_weapons[i].name)-1;
                if (pos + (size_t)len > size) len = (int)(size-pos);
                memcpy(g_weapons[i].name, p+pos, len); g_weapons[i].name[len]='\0'; pos += len;
                if (pos + 3 > size) break;
                g_weapons[i].damage = p[pos++]; g_weapons[i].range = p[pos++]; g_weapons[i].cooldown = p[pos++]; g_weapons[i].color_id = (uint8_t)(i & 7); copy_default_sprite(g_weapons[i].sprite, SPRITE_TARGET_WEAPON);
            }
        } else if (a == 'W' && b == 'E' && c == 'A' && d == 'P') {
            if (pos >= size) break;
            int count = p[pos++]; if (count > MAX_WEAPONS) count = MAX_WEAPONS;
            for (int i = 0; i < count && pos + 3 <= size; i++) { g_weapons[i].damage = p[pos++]; g_weapons[i].range = p[pos++]; g_weapons[i].cooldown = p[pos++]; g_weapons[i].color_id = (uint8_t)(i & 7); copy_default_sprite(g_weapons[i].sprite, SPRITE_TARGET_WEAPON); }
        } else {
            break;
        }
    }
}

bool parse_bwl_data(const uint8_t *data, size_t len, Level *lv) {
    if (!data || !lv || len < 15) return false;

    bool ok = false;
    Level *tmp = &g_load_temp;

    if (len >= 19 && data[0] == 'B' && data[1] == 'W' && (data[2] == '3' || data[2] == '4')) {
        uint16_t width = read_u16_le(data + 3);
        uint16_t height = read_u16_le(data + 5);
        uint32_t tile_size = read_u32_le(data + 15);
        if (width >= 3 && height >= 3 && width <= MAX_MAP_W && height <= MAX_MAP_H && 19u + tile_size <= len) {
            memset(tmp, 0, sizeof(*tmp));
            tmp->width = width;
            tmp->height = height;
            tmp->player_x = uqpos(read_u16_le(data + 7));
            tmp->player_y = uqpos(read_u16_le(data + 9));
            tmp->player_z = uqpos(read_u16_le(data + 11));
            tmp->player_vz = 0.0f;
            tmp->player_angle = uqangle(read_u16_le(data + 13));
            tmp->on_ground = false;
            ok = decode_bwl2_tiles(data + 19, tile_size, tmp);
            if (ok && lv == &g_level) parse_bw3_chunks(data + 19 + tile_size, len - 19 - tile_size);
        }
    } else if (len >= 15 && data[0] == 'B' && data[1] == 'W' && data[2] == '2') {
        uint16_t width = read_u16_le(data + 3);
        uint16_t height = read_u16_le(data + 5);
        if (width >= 3 && height >= 3 && width <= MAX_MAP_W && height <= MAX_MAP_H) {
            memset(tmp, 0, sizeof(*tmp));
            tmp->width = width;
            tmp->height = height;
            tmp->player_x = uqpos(read_u16_le(data + 7));
            tmp->player_y = uqpos(read_u16_le(data + 9));
            tmp->player_z = uqpos(read_u16_le(data + 11));
            tmp->player_vz = 0.0f;
            tmp->player_angle = uqangle(read_u16_le(data + 13));
            tmp->on_ground = false;
            ok = decode_bwl2_tiles(data + 15, len - 15, tmp);
        }
    } else if (len >= 24 && memcmp(data, "BWL1", 4) == 0) {
        uint16_t width = read_u16_le(data + 4);
        uint16_t height = read_u16_le(data + 6);
        uint32_t count = read_u32_le(data + 20);
        if (width >= 3 && height >= 3 && width <= MAX_MAP_W && height <= MAX_MAP_H &&
            count == (uint32_t)(width * height) && len == (size_t)(24 + count)) {
            memset(tmp, 0, sizeof(*tmp));
            tmp->width = width;
            tmp->height = height;
            tmp->player_x = read_f32_le(data + 8);
            tmp->player_y = read_f32_le(data + 12);
            tmp->player_z = 0.0f;
            tmp->player_vz = 0.0f;
            tmp->player_angle = read_f32_le(data + 16);
            tmp->on_ground = true;
            for (uint32_t i = 0; i < count; i++) tmp->tiles[i] = data[24 + i] & 0x0F;
            ok = true;
        }
    }

    if (ok) {
        if (lv == &g_level && !(len >= 19 && data[0] == 'B' && data[1] == 'W' && (data[2] == '3' || data[2] == '4'))) reset_level_metadata_defaults();
        force_valid_spawn(tmp);
        *lv = *tmp;
    }

    return ok;
}

int count_nonzero_tiles(const Level *lv) {
    if (!lv || lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return 0;
    int n = lv->width * lv->height;
    int count = 0;
    for (int i = 0; i < n; i++) if ((lv->tiles[i] & 0x0F) != 0) count++;
    return count;
}

uint32_t checksum_level_tiles(const Level *lv) {
    if (!lv || lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return 0;
    int n = lv->width * lv->height;
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) {
        h ^= (uint32_t)(lv->tiles[i] & 0x0F);
        h *= 16777619u;
    }
    h ^= qpos(lv->player_x); h *= 16777619u;
    h ^= qpos(lv->player_y); h *= 16777619u;
    h ^= qpos(lv->player_z); h *= 16777619u;
    h ^= qangle(lv->player_angle); h *= 16777619u;
    return h;
}

bool save_bwl2_slot(const Level *lv, int slot, const char *level_name, bool set_status) {
    uint8_t *data = NULL;
    size_t size = 0;
    if (!encode_bwl2_memory(lv, &data, &size)) {
        if (set_status) snprintf(g_status, sizeof(g_status), "ENCODE FAIL SLOT %d", slot);
        return false;
    }

    char primary_path[128];
    char backup_path[128];
    make_slot_fs_path_for(slot, primary_path, sizeof(primary_path), false);
    make_slot_fs_path_for(slot, backup_path, sizeof(backup_path), true);

    bool primary_ok = fs_write_whole_file(primary_path, data, size);
    bool backup_ok = fs_write_whole_file(backup_path, data, size);
    uint32_t sum = checksum_level_tiles(lv) & 0xFFFFu;
    int filled = count_nonzero_tiles(lv);

    free(data);

    if (primary_ok || backup_ok) {
        char clean[LEVEL_NAME_MAX + 1];
        snprintf(clean, sizeof(clean), "%s", (level_name && level_name[0]) ? level_name : "Untitled");
        sanitize_level_name(clean);
        save_slot_meta(slot, clean);

        if (slot == g_slot) {
            snprintf(g_level_name, sizeof(g_level_name), "%s", clean);
            g_dirty = false;
        }

        if (set_status) {
            if (primary_ok && backup_ok) {
                snprintf(g_status, sizeof(g_status), "SAVED S%d T%d C%04lX", slot, filled, (unsigned long)sum);
            } else if (primary_ok) {
                snprintf(g_status, sizeof(g_status), "SAVED /3DS S%d C%04lX", slot, (unsigned long)sum);
            } else {
                snprintf(g_status, sizeof(g_status), "SAVED ROOT S%d C%04lX", slot, (unsigned long)sum);
            }
        }
        return true;
    }

    if (set_status) {
        if (g_fs_ready) snprintf(g_status, sizeof(g_status), "SAVE FAIL SLOT %d", slot);
        else snprintf(g_status, sizeof(g_status), "FS NOT READY");
    }
    return false;
}

bool save_bwl2(const Level *lv) {
    return save_bwl2_slot(lv, g_slot, g_level_name, true);
}

bool load_bwl_from_fs_path(Level *lv, const char *fs_path) {
    uint8_t *data = NULL;
    size_t size = 0;
    if (!fs_read_whole_file(fs_path, &data, &size)) return false;
    bool ok = parse_bwl_data(data, size, lv);
    free(data);
    return ok;
}

bool load_bwl_slot_index(Level *lv, int slot, bool set_status) {
    char fs_path[128];

    make_slot_fs_path_for(slot, fs_path, sizeof(fs_path), false);
    if (load_bwl_from_fs_path(lv, fs_path)) {
        if (set_status) snprintf(g_status, sizeof(g_status), "LOADED /3DS SLOT %d", slot);
        return true;
    }

    make_slot_fs_path_for(slot, fs_path, sizeof(fs_path), true);
    if (load_bwl_from_fs_path(lv, fs_path)) {
        if (set_status) snprintf(g_status, sizeof(g_status), "LOADED ROOT SLOT %d", slot);
        return true;
    }

    if (set_status) {
        if (g_fs_ready) snprintf(g_status, sizeof(g_status), "LOAD FAIL SLOT %d", slot);
        else snprintf(g_status, sizeof(g_status), "FS NOT READY");
    }
    return false;
}

bool load_bwl_slot(Level *lv) {
    bool ok = load_bwl_slot_index(lv, g_slot, true);
    if (ok) {
        g_dirty = false;
        if (!load_slot_meta(g_slot, g_level_name, sizeof(g_level_name))) default_slot_name(g_slot, g_level_name, sizeof(g_level_name));
    }
    return ok;
}

void refresh_slot_info(int slot) {
    SlotInfo *info = &g_slots[slot];
    memset(info, 0, sizeof(*info));
    default_slot_name(slot, info->name, sizeof(info->name));

    Level *tmp = &g_load_temp;
    if (load_bwl_slot_index(tmp, slot, false)) {
        info->exists = true;
        info->width = tmp->width;
        info->height = tmp->height;
        info->filled_tiles = count_nonzero_tiles(tmp);
        info->checksum = checksum_level_tiles(tmp);
        load_slot_meta(slot, info->name, sizeof(info->name));
    }
}

void refresh_all_slots(void) {
    for (int i = 0; i < SLOT_COUNT; i++) refresh_slot_info(i);
}

void refresh_preview_slot(void) {
    if (load_bwl_slot_index(&g_preview_level, g_slot, false)) {
        g_preview_valid = true;
    } else {
        new_sized_level(&g_preview_level, DEFAULT_MAP_W, DEFAULT_MAP_H);
        g_preview_valid = false;
    }
}

void delete_slot(int slot) {
    char path[128];
    make_slot_fs_path_for(slot, path, sizeof(path), false);
    fs_delete_path(path);
    make_slot_fs_path_for(slot, path, sizeof(path), true);
    fs_delete_path(path);
    make_meta_fs_path_for(slot, path, sizeof(path), false);
    fs_delete_path(path);
    make_meta_fs_path_for(slot, path, sizeof(path), true);
    fs_delete_path(path);

    if (slot == g_slot) {
        g_dirty = false;
        default_slot_name(slot, g_level_name, sizeof(g_level_name));
    }
    refresh_slot_info(slot);
    if (slot == g_slot) refresh_preview_slot();
    snprintf(g_status, sizeof(g_status), "DELETED SLOT %d", slot);
}

bool duplicate_slot(int src_slot, int dst_slot) {
    if (src_slot == dst_slot) {
        snprintf(g_status, sizeof(g_status), "PICK OTHER SLOT");
        return false;
    }

    Level *tmp = &g_load_temp;
    if (!load_bwl_slot_index(tmp, src_slot, false)) {
        snprintf(g_status, sizeof(g_status), "NO SAVE IN SLOT %d", src_slot);
        return false;
    }

    char name[LEVEL_NAME_MAX + 1];
    if (!load_slot_meta(src_slot, name, sizeof(name))) default_slot_name(src_slot, name, sizeof(name));
    char copy_name[LEVEL_NAME_MAX + 1];
    snprintf(copy_name, sizeof(copy_name), "%.26s COPY", name);
    sanitize_level_name(copy_name);

    if (!save_bwl2_slot(tmp, dst_slot, copy_name, false)) {
        snprintf(g_status, sizeof(g_status), "DUP FAIL %d TO %d", src_slot, dst_slot);
        return false;
    }

    refresh_slot_info(dst_slot);
    if (dst_slot == g_slot) refresh_preview_slot();
    snprintf(g_status, sizeof(g_status), "DUP %d TO %d", src_slot, dst_slot);
    return true;
}

void prompt_rename_slot(int slot) {
    char name[LEVEL_NAME_MAX + 1];
    if (!load_slot_meta(slot, name, sizeof(name))) {
        if (slot == g_slot && g_level_name[0]) snprintf(name, sizeof(name), "%s", g_level_name);
        else default_slot_name(slot, name, sizeof(name));
    }

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, LEVEL_NAME_MAX);
    swkbdSetHintText(&swkbd, "Level name");
    swkbdSetInitialText(&swkbd, name);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);

    SwkbdButton button = swkbdInputText(&swkbd, name, sizeof(name));
    if (button == SWKBD_BUTTON_NONE) {
        snprintf(g_status, sizeof(g_status), "RENAME CANCEL");
        return;
    }

    sanitize_level_name(name);
    save_slot_meta(slot, name);
    refresh_slot_info(slot);
    if (slot == g_slot) snprintf(g_level_name, sizeof(g_level_name), "%s", name);
    snprintf(g_status, sizeof(g_status), "RENAMED SLOT %d", slot);
}


static void clamp_app_settings(void) {
    g_fov_degrees = clampf32(g_fov_degrees, FOV_MIN_DEGREES, FOV_MAX_DEGREES);
    g_level_depth = clampf32(g_level_depth, 0.50f, 2.00f);
    g_dof_start = clampf32(g_dof_start, 4.0f, 32.0f);
    g_dof_strength = clampf32(g_dof_strength, 0.10f, 1.00f);
}


static void parse_sprite_hex_line(const char *p, uint8_t *out) {
    if (!p || !out) return;
    const char *q = strchr(p, ' ');
    if (!q) return;
    q++;
    for (int i = 0; i < SPRITE_BYTES; i++) {
        unsigned int v = 0;
        if (sscanf(q + i * 2, "%2x", &v) != 1) return;
        out[i] = (uint8_t)(v & 0xFF);
    }
}

static void parse_sprite16_hex_line(const char *p, uint16_t *out) {
    if (!p || !out) return;
    const char *q = strchr(p, ' ');
    if (!q) return;
    q++;
    for (int i = 0; i < ENEMY_SPRITE_ROWS; i++) {
        unsigned int v = 0;
        if (sscanf(q + i * 4, "%4x", &v) != 1) return;
        out[i] = (uint16_t)(v & 0xFFFF);
    }
}

static bool parse_app_settings_text(const char *txt) {
    if (!txt || strncmp(txt, "3DCASTERCFG1", 12) != 0) return false;

    float fov = FOV_DEGREES;
    float depth = 1.0f;
    int bob = 1;
    int stereo = 0;
    int dof = 0;
    float dof_start = 10.0f;
    float dof_strength = 0.55f;
    int aa = 0;
    int fast = 0;
    int debug = 0;
    int shake = 1;
    int defnpc = 0;

    const char *p = txt;
    while (*p) {
        if (sscanf(p, "FOV %f", &fov) == 1) {
            /* handled */
        } else if (sscanf(p, "DEPTH %f", &depth) == 1) {
            /* handled */
        } else if (sscanf(p, "BOB %d", &bob) == 1) {
            /* handled */
        } else if (sscanf(p, "STEREO3D %d", &stereo) == 1) {
            /* handled */
        } else if (sscanf(p, "DOF %d", &dof) == 1) {
            /* handled */
        } else if (sscanf(p, "DOFSTART %f", &dof_start) == 1) {
            /* handled */
        } else if (sscanf(p, "DOFSTRENGTH %f", &dof_strength) == 1) {
            /* handled */
        } else if (sscanf(p, "AA %d", &aa) == 1) {
            /* handled */
        } else if (sscanf(p, "FAST %d", &fast) == 1) {
            /* handled */
        } else if (sscanf(p, "DEBUG %d", &debug) == 1) {
            /* handled */
        } else if (sscanf(p, "SHAKE %d", &shake) == 1) {
            /* handled */
        } else if (sscanf(p, "DEFAULTNPC %d", &defnpc) == 1) {
            /* handled */
        } else if (strncmp(p, "DEFAULTNPCSPRITE16 ", 19) == 0) {
            parse_sprite16_hex_line(p, g_default_npc_sprite16);
        } else if (strncmp(p, "DEFAULTNPCSPRITE ", 17) == 0) {
            parse_sprite_hex_line(p, g_default_npc_sprite);
        }

        const char *next = strchr(p, '\n');
        if (!next) break;
        p = next + 1;
    }

    g_fov_degrees = fov;
    g_level_depth = depth;
    g_view_bob = bob != 0;
    g_3d_enabled = stereo != 0;
    g_dof_enabled = dof != 0;
    g_dof_start = dof_start;
    g_dof_strength = dof_strength;
    g_antialiasing = aa != 0;
    g_fast_render = fast != 0;
    g_debug_overlay = debug != 0;
    g_screen_shake_enabled = shake != 0;
    g_default_npc_color = (uint8_t)(defnpc & 7);
    bool def16_empty = true;
    for (int si = 0; si < ENEMY_SPRITE_ROWS; si++) if (g_default_npc_sprite16[si]) def16_empty = false;
    if (def16_empty) save_expand_8x8_to_16(g_default_npc_sprite16, g_default_npc_sprite);
    clamp_app_settings();

    if (!g_view_bob) {
        g_bob_phase = 0.0f;
        g_bob_amount = 0.0f;
        g_camera_speed = 0.0f;
    }

    return true;
}

bool load_app_settings(void) {
    if (!g_fs_ready) return false;

    uint8_t *data = NULL;
    size_t size = 0;

    if (!fs_read_whole_file(SETTINGS_FS_PRIMARY, &data, &size)) {
        if (!fs_read_whole_file(SETTINGS_FS_BACKUP, &data, &size)) return false;
    }

    char *txt = (char*)malloc(size + 1);
    if (!txt) {
        free(data);
        return false;
    }

    memcpy(txt, data, size);
    txt[size] = '\0';
    free(data);

    bool ok = parse_app_settings_text(txt);
    free(txt);
    return ok;
}

bool save_app_settings(void) {
    if (!g_fs_ready) return false;

    clamp_app_settings();

    char sprite_hex[SPRITE_BYTES * 2 + 1];
    for (int i = 0; i < SPRITE_BYTES; i++) snprintf(sprite_hex + i * 2, 3, "%02X", g_default_npc_sprite[i]);
    char sprite16_hex[ENEMY_SPRITE_ROWS * 4 + 1];
    for (int i = 0; i < ENEMY_SPRITE_ROWS; i++) snprintf(sprite16_hex + i * 4, 5, "%04X", g_default_npc_sprite16[i]);

    char buf[768];
    int n = snprintf(buf, sizeof(buf),
                     "3DCASTERCFG1\nFOV %.1f\nDEPTH %.2f\nBOB %d\nSTEREO3D %d\nDOF %d\nDOFSTART %.1f\nDOFSTRENGTH %.2f\nAA %d\nFAST %d\nDEBUG %d\nSHAKE %d\nDEFAULTNPC %d\nDEFAULTNPCSPRITE %s\nDEFAULTNPCSPRITE16 %s\n",
                     g_fov_degrees,
                     g_level_depth,
                     g_view_bob ? 1 : 0,
                     g_3d_enabled ? 1 : 0,
                     g_dof_enabled ? 1 : 0,
                     g_dof_start,
                     g_dof_strength,
                     g_antialiasing ? 1 : 0,
                     g_fast_render ? 1 : 0,
                     g_debug_overlay ? 1 : 0,
                     g_screen_shake_enabled ? 1 : 0,
                     g_default_npc_color & 7,
                     sprite_hex,
                     sprite16_hex);
    if (n <= 0 || n >= (int)sizeof(buf)) return false;

    bool primary_ok = fs_write_whole_file(SETTINGS_FS_PRIMARY, (const uint8_t*)buf, (size_t)n);
    bool backup_ok = fs_write_whole_file(SETTINGS_FS_BACKUP, (const uint8_t*)buf, (size_t)n);
    return primary_ok || backup_ok;
}

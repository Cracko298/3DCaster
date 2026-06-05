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
    size_t cap = (size_t)n * 2 + 64;
    uint8_t *out = (uint8_t*)malloc(cap);
    if (!out) return false;

    size_t pos = 0;
    bool ok = true;

    ok = ok && mem_put_u8(out, cap, &pos, 'B');
    ok = ok && mem_put_u8(out, cap, &pos, 'W');
    ok = ok && mem_put_u8(out, cap, &pos, '2');
    ok = ok && mem_put_u16_le(out, cap, &pos, lv->width);
    ok = ok && mem_put_u16_le(out, cap, &pos, lv->height);
    ok = ok && mem_put_u16_le(out, cap, &pos, qpos(lv->player_x));
    ok = ok && mem_put_u16_le(out, cap, &pos, qpos(lv->player_y));
    ok = ok && mem_put_u16_le(out, cap, &pos, qpos(lv->player_z));
    ok = ok && mem_put_u16_le(out, cap, &pos, qangle(lv->player_angle));

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

bool parse_bwl_data(const uint8_t *data, size_t len, Level *lv) {
    if (!data || !lv || len < 15) return false;

    bool ok = false;
    Level *tmp = &g_load_temp;

    if (len >= 15 && data[0] == 'B' && data[1] == 'W' && data[2] == '2') {
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
    snprintf(copy_name, sizeof(copy_name), "%s COPY", name);
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

    char buf[320];
    int n = snprintf(buf, sizeof(buf),
                     "3DCASTERCFG1\nFOV %.1f\nDEPTH %.2f\nBOB %d\nSTEREO3D %d\nDOF %d\nDOFSTART %.1f\nDOFSTRENGTH %.2f\nAA %d\nFAST %d\nDEBUG %d\n",
                     g_fov_degrees,
                     g_level_depth,
                     g_view_bob ? 1 : 0,
                     g_3d_enabled ? 1 : 0,
                     g_dof_enabled ? 1 : 0,
                     g_dof_start,
                     g_dof_strength,
                     g_antialiasing ? 1 : 0,
                     g_fast_render ? 1 : 0,
                     g_debug_overlay ? 1 : 0);
    if (n <= 0 || n >= (int)sizeof(buf)) return false;

    bool primary_ok = fs_write_whole_file(SETTINGS_FS_PRIMARY, (const uint8_t*)buf, (size_t)n);
    bool backup_ok = fs_write_whole_file(SETTINGS_FS_BACKUP, (const uint8_t*)buf, (size_t)n);
    return primary_ok || backup_ok;
}

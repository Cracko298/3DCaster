#include "common.h"

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

void handle_world_menu_input(u32 kDown) {
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
        }
    }
}

void update_physics_and_movement(float dt, u32 kDown, u32 kHeld) {
    Level *lv = &g_level;

    if ((kDown & KEY_A) && lv->on_ground) {
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

    float rot = ROT_SPEED * dt;
    if (kHeld & (KEY_DLEFT | KEY_L)) lv->player_angle -= rot;
    if (kHeld & (KEY_DRIGHT | KEY_R)) lv->player_angle += rot;

    circlePosition cp;
    hidCircleRead(&cp);
    float forward = ((float)cp.dy) / 156.0f;
    float strafe = ((float)cp.dx) / 156.0f;
    forward = clampf32(forward, -1.0f, 1.0f);
    strafe = clampf32(strafe, -1.0f, 1.0f);

    float speed = MOVE_SPEED * dt;
    if (kHeld & KEY_B) speed *= SPRINT_MULT;

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

    ground = ground_height_at(lv, lv->player_x, lv->player_y, lv->player_z);
    if (lv->player_z <= ground && lv->player_vz <= 0.0f) {
        lv->player_z = ground;
        lv->player_vz = 0.0f;
        lv->on_ground = true;
    }
}

void editor_touch(u32 kHeld) {
    if (!(kHeld & KEY_TOUCH)) return;

    touchPosition tp;
    hidTouchRead(&tp);

    Level *lv = &g_level;
    if (lv->width < 3 || lv->height < 3 || lv->width > MAX_MAP_W || lv->height > MAX_MAP_H) return;

    if (tp.py >= PALETTE_Y - 2) {
        for (int t = 0; t < 8; t++) {
            int x0 = PALETTE_X + t * PALETTE_STEP;
            if (tp.px >= x0 - 2 && tp.px <= x0 + PALETTE_W + 2 && tp.py >= PALETTE_Y - 2 && tp.py <= PALETTE_Y + PALETTE_H + 2) {
                g_selected_tile = (uint8_t)t;
                snprintf(g_status, sizeof(g_status), "TILE %d", g_selected_tile);
                return;
            }
        }
    }

    int cell, ox, oy;
    editor_layout(lv, &cell, &ox, &oy);
    int x = ((int)tp.px - ox) / cell;
    int y = ((int)tp.py - oy) / cell;
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

void handle_editor_input(u32 kDown, u32 kHeld) {
    if (kDown & KEY_L) {
        g_selected_tile = (uint8_t)((g_selected_tile + 7) % 8);
        snprintf(g_status, sizeof(g_status), "TILE %d", g_selected_tile);
    }
    if (kDown & KEY_R) {
        g_selected_tile = (uint8_t)((g_selected_tile + 1) % 8);
        snprintf(g_status, sizeof(g_status), "TILE %d", g_selected_tile);
    }
    editor_touch(kHeld);
}

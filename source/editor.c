#include "common.h"

static Color editor_npc_color_for(uint8_t id) {
    static const Color colors[8] = {
        {80, 210, 255}, {255, 120, 180}, {120, 255, 120}, {255, 220, 90},
        {170, 120, 255}, {255, 120, 80}, {120, 220, 210}, {235, 235, 235}
    };
    return colors[id & 7];
}

#define npc_color_for editor_npc_color_for

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
    int visible = (g_entity_edit_mode == EDIT_MODE_WEAPON) ? 8 : 12;
    if (row < g_entity_edit_scroll || row >= g_entity_edit_scroll + visible) return;
    int y = 42 + (row - g_entity_edit_scroll) * 15;
    Color bg = (g_entity_edit_cursor == row) ? (Color){48, 66, 102} : (Color){16, 18, 28};
    Color fg = (g_entity_edit_cursor == row) ? (Color){255, 230, 150} : (Color){230, 230, 230};
    fill_rect_raw(fb, BOT_W, BOT_H, 10, y - 2, BOT_W - 10, y + 12, bg);
    draw_text3x5(fb, BOT_W, BOT_H, 16, y + 2, label, fg, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 128, y + 2, value ? value : "", fg, 1);
}

static NPC *render_find_npc_at_edit_pos(void) {
    for (int i = 0; i < g_npc_count; i++) {
        if (g_npcs[i].active && g_npcs[i].x == g_entity_edit_x && g_npcs[i].y == g_entity_edit_y) return &g_npcs[i];
    }
    return NULL;
}

static EnemyMeta *render_find_enemy_meta_at_edit_pos(void) {
    for (int i = 0; i < g_enemy_meta_count; i++) {
        if (g_enemy_metas[i].active && g_enemy_metas[i].x == g_entity_edit_x && g_enemy_metas[i].y == g_entity_edit_y) return &g_enemy_metas[i];
    }
    return NULL;
}

static NPC *render_edit_npc(void) {
    if (g_entity_edit_mode != EDIT_MODE_NPC) return NULL;
    return render_find_npc_at_edit_pos();
}

static EnemyMeta *render_edit_enemy_meta(void) {
    if (g_entity_edit_mode != EDIT_MODE_ENEMY) return NULL;
    return render_find_enemy_meta_at_edit_pos();
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

static void draw_enemy16_sprite_preview_grid(u8 *fb, int x0, int y0, const uint16_t *sp, Color c, int scale, int cx, int cy) {
    if (!sp) return;
    for (int y = 0; y < ENEMY_SPRITE_H; y++) {
        for (int x = 0; x < ENEMY_SPRITE_W; x++) {
            bool on = (sp[y] & (uint16_t)(1u << (ENEMY_SPRITE_W - 1 - x))) != 0;
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

static void draw_boss_sprite_preview_grid(u8 *fb, int x0, int y0, const uint32_t *sp, Color c, int scale, int cx, int cy) {
    if (!sp) return;
    for (int y = 0; y < BOSS_SPRITE_H; y++) {
        for (int x = 0; x < BOSS_SPRITE_W; x++) {
            bool on = (sp[y] & (uint32_t)(1u << (BOSS_SPRITE_W - 1 - x))) != 0;
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

static uint32_t *render_boss_sprite_pixels(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_BOSS) {
        EnemyMeta *m = render_find_enemy_meta_at_edit_pos();
        return m ? m->boss_sprite : NULL;
    }
    return NULL;
}

static uint16_t *render_enemy16_sprite_pixels(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC) {
        NPC *n = render_find_npc_at_edit_pos();
        return n ? n->sprite16 : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return g_default_npc_sprite16;
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY) {
        EnemyMeta *m = render_find_enemy_meta_at_edit_pos();
        return m ? m->sprite16 : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_NPC_ANIM) {
        NPCAnim *a = npc_anim_find_at(g_entity_edit_x, g_entity_edit_y);
        if (!a) return NULL;
        int st = clampi32(g_anim_edit_state, 0, 2);
        int fr = clampi32(g_anim_edit_frame, 0, ANIM_FRAMES - 1);
        return a->frames[st][fr];
    }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY_ANIM) {
        EnemyAnim *a = enemy_anim_find_at(g_entity_edit_x, g_entity_edit_y);
        if (!a) return NULL;
        int st = clampi32(g_anim_edit_state, 0, 3);
        int fr = clampi32(g_anim_edit_frame, 0, ANIM_FRAMES - 1);
        return a->frames[st][fr];
    }
    return NULL;
}

static uint8_t *render_sprite_pixels(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC) {
        return NULL; /* NPC art is edited as 16x16 now. */
    }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY) {
        EnemyMeta *m = render_find_enemy_meta_at_edit_pos();
        return m ? m->sprite : NULL;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_WEAPON) {
        int wi = clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1);
        return g_weapons[wi].sprite;
    }
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return NULL; /* Default NPC art is edited as 16x16 now. */
    return NULL;
}

static uint8_t render_sprite_color_id(void) {
    if (g_sprite_edit_target == SPRITE_TARGET_NPC || g_sprite_edit_target == SPRITE_TARGET_NPC_ANIM) { NPC *n = render_find_npc_at_edit_pos(); return n ? n->color_id : 0; }
    if (g_sprite_edit_target == SPRITE_TARGET_ENEMY || g_sprite_edit_target == SPRITE_TARGET_ENEMY_ANIM || g_sprite_edit_target == SPRITE_TARGET_BOSS) { EnemyMeta *m = render_find_enemy_meta_at_edit_pos(); return m ? m->color_id : 0; }
    if (g_sprite_edit_target == SPRITE_TARGET_WEAPON) return g_weapons[clampi32(g_entity_edit_weapon, 0, MAX_WEAPONS - 1)].color_id;
    if (g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) return g_default_npc_color;
    return 0;
}

void render_sprite_editor(u8 *fb) {
    if (!fb) return;
    uint8_t *sp = render_sprite_pixels();
    uint32_t *bsp = render_boss_sprite_pixels();
    uint16_t *esp16 = render_enemy16_sprite_pixels();
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){7, 8, 14});
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, 28, (Color){18, 24, 42});
    const char *title = "SPRITE ART EDITOR";
    if (bsp) title = "BOSS 32X32 ART";
    else if (esp16) {
        if (g_sprite_edit_target == SPRITE_TARGET_NPC_ANIM) title = "NPC ANIM 16X16";
        else if (g_sprite_edit_target == SPRITE_TARGET_ENEMY_ANIM) title = "ENEMY ANIM 16X16";
        else if (g_sprite_edit_target == SPRITE_TARGET_NPC || g_sprite_edit_target == SPRITE_TARGET_DEFAULT_NPC) title = "NPC 16X16 ART";
        else title = "ENEMY 16X16 ART";
    }
    draw_text3x5(fb, BOT_W, BOT_H, 8, 7, title, (Color){255,235,170}, 2);
    if (!sp && !bsp && !esp16) { draw_text3x5(fb, BOT_W, BOT_H, 20, 50, "NO SPRITE", (Color){255,120,120}, 2); return; }
    Color c = npc_color_for(render_sprite_color_id());
    if (bsp) draw_boss_sprite_preview_grid(fb, 88, 36, bsp, c, 4, g_sprite_edit_cursor_x, g_sprite_edit_cursor_y);
    else if (esp16) draw_enemy16_sprite_preview_grid(fb, 108, 40, esp16, c, 7, g_sprite_edit_cursor_x, g_sprite_edit_cursor_y);
    else draw_sprite_preview_grid(fb, 96, 44, sp, c, 14, g_sprite_edit_cursor_x, g_sprite_edit_cursor_y);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 44, "D-PAD MOVE", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 58, "A TOGGLE", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 72, "L/R COLOR", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 86, "Y PRESET", (Color){210,210,210}, 1);
    draw_text3x5(fb, BOT_W, BOT_H, 22, 100, "X CLEAR", (Color){210,210,210}, 1);
    if (g_sprite_edit_target == SPRITE_TARGET_NPC_ANIM || g_sprite_edit_target == SPRITE_TARGET_ENEMY_ANIM) {
        char info[48];
        snprintf(info, sizeof(info), "STATE %d  FRAME %d", g_anim_edit_state, g_anim_edit_frame);
        draw_text3x5(fb, BOT_W, BOT_H, 22, 114, info, (Color){220,220,170}, 1);
        draw_text3x5(fb, BOT_W, BOT_H, 22, 128, "CHANGE STATE/FRAME IN ENTITY MENU", (Color){190,200,220}, 1);
    } else {
        draw_text3x5(fb, BOT_W, BOT_H, 22, 114, bsp ? "32X32 BOSS" : (esp16 ? "16X16 ART" : "8X8 ART"), (Color){220,220,170}, 1);
    }
    draw_text3x5(fb, BOT_W, BOT_H, 22, 226, "B BACK - ART SAVES IN BW3/ENM5", (Color){190,200,220}, 1);
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

static const char *render_ai_rank_name(uint8_t rank) {
    if (rank == AI_RANK_BOSS) return "BOSS";
    if (rank == AI_RANK_CAPTAIN) return "CAPTAIN";
    return "GRUNT";
}

static const char *render_ai_spawn_name(uint8_t kind) {
    if (kind == AI_SPAWN_GRUNT) return "GRUNT";
    return "NONE";
}

void render_entity_editor(u8 *fb) {
    if (!fb || g_entity_edit_mode == EDIT_MODE_NONE) return;
    if (g_entity_edit_mode == EDIT_MODE_SPRITE) { render_sprite_editor(fb); return; }
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, (Color){7, 8, 14});
    fill_rect_raw(fb, BOT_W, BOT_H, 0, 0, BOT_W, 28, (Color){18, 24, 42});
    draw_text3x5(fb, BOT_W, BOT_H, 8, 7,
                 g_entity_edit_mode == EDIT_MODE_NPC ? "NPC EDITOR" : (g_entity_edit_mode == EDIT_MODE_ENEMY ? "ENEMY EDITOR" : (g_entity_edit_mode == EDIT_MODE_DOOR ? "DOOR EDITOR" : "WEAPON EDITOR")),
                 (Color){255, 235, 170}, 2);
    draw_text3x5(fb, BOT_W, BOT_H, 8, 226, "DUP/DDOWN SCROLL  D<> VALUE  L/R FAST  A EDIT  B BACK", (Color){190, 200, 220}, 1);

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
        draw_entity_edit_row(fb, 6, "TEXT SPEED", text_speed_name(n->text_speed));
        snprintf(buf, sizeof(buf), "%d", n->color_id & 7); draw_entity_edit_row(fb, 7, "NPC COLOR", buf);
        draw_entity_edit_row(fb, 8, "EDIT ART", "A OPEN");
        snprintf(buf, sizeof(buf), "%d", g_default_npc_color & 7); draw_entity_edit_row(fb, 9, "DEFAULT COLOR", buf);
        draw_entity_edit_row(fb, 10, "DEFAULT ART", "A OPEN");
        draw_entity_edit_row(fb, 11, "USE DEFAULT", "A APPLY");
        draw_entity_edit_row(fb, 12, "EDIT WEAPON", (n->reward_kind >= REWARD_WEAPON_BASE) ? entity_reward_name(n->reward_kind) : "A OPEN");
        NPCAnim *na = npc_anim_ensure_at(n->x, n->y);
        draw_entity_edit_row(fb, 13, "ANIM", (na && na->enabled) ? "ON" : "OFF");
        draw_entity_edit_row(fb, 14, "ANIM SPEED", na ? anim_speed_name(na->speed) : "OFF");
        snprintf(buf, sizeof(buf), "%d", g_anim_edit_state); draw_entity_edit_row(fb, 15, "ANIM STATE", buf);
        snprintf(buf, sizeof(buf), "%d", g_anim_edit_frame); draw_entity_edit_row(fb, 16, "ANIM FRAME", buf);
        draw_entity_edit_row(fb, 17, "EDIT ANIM", "A OPEN");
        draw_entity_edit_row(fb, 18, "DONE", "A CLOSE");
        draw_enemy16_sprite_preview_grid(fb, 258, 34, n->sprite16, npc_color_for(n->color_id), 3, -1, -1);
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
        snprintf(buf, sizeof(buf), "%d%%", m->speed_attr ? m->speed_attr : 100); draw_entity_edit_row(fb, 3, "SPEED", buf);
        snprintf(buf, sizeof(buf), "%d%%", m->size_pct ? m->size_pct : 100); draw_entity_edit_row(fb, 4, "SIZE", buf);
        draw_entity_edit_row(fb, 5, "TEXT SPEED", text_speed_name(m->text_speed));
        snprintf(buf, sizeof(buf), "%d", m->color_id & 7); draw_entity_edit_row(fb, 6, "COLOR", buf);
        draw_entity_edit_row(fb, 7, "AI RANK", render_ai_rank_name(m->ai_rank));
        draw_entity_edit_row(fb, 8, "PROJECTILE", m->ranged_attack ? "YES" : "NO");
        snprintf(buf, sizeof(buf), "%.1f", (float)(m->melee_range ? m->melee_range : 7) * 0.10f); draw_entity_edit_row(fb, 9, "ATK RANGE", buf);
        snprintf(buf, sizeof(buf), "%.1fS", (float)(m->attack_cooldown ? m->attack_cooldown : 7) * 0.10f); draw_entity_edit_row(fb, 10, "ATK COOLDN", buf);
        snprintf(buf, sizeof(buf), "%d", m->spawn_limit); draw_entity_edit_row(fb, 11, "SPAWN MAX", buf);
        draw_entity_edit_row(fb, 12, "SPAWN TYPE", render_ai_spawn_name(m->spawn_kind));
        snprintf(buf, sizeof(buf), "%.1fS", (float)(m->spawn_cooldown ? m->spawn_cooldown : 25) * 0.10f); draw_entity_edit_row(fb, 13, "SPAWN CD", buf);
        snprintf(buf, sizeof(buf), "%d", m->sight_range ? m->sight_range : (m->ai_rank == AI_RANK_BOSS ? 20 : (m->ai_rank == AI_RANK_CAPTAIN ? 16 : 13))); draw_entity_edit_row(fb, 14, "SIGHT RNG", buf);
        snprintf(buf, sizeof(buf), "%d", m->command_range ? m->command_range : 10); draw_entity_edit_row(fb, 15, "COMMAND RNG", buf);
        snprintf(buf, sizeof(buf), "%d", m->projectile_color & 7); draw_entity_edit_row(fb, 16, "PROJ COLOR", buf);
        snprintf(buf, sizeof(buf), "%d", m->projectile_style % 3); draw_entity_edit_row(fb, 17, "PROJ STYLE", buf);
        draw_entity_edit_row(fb, 18, "PROJ ANIM", m->projectile_anim ? "YES" : "NO");
        draw_entity_edit_row(fb, 19, "ENEMY 16X16", "A OPEN");
        draw_entity_edit_row(fb, 20, "BOSS 32X32", "A OPEN");
        snprintf(buf, sizeof(buf), "%d", g_player_health_max); draw_entity_edit_row(fb, 21, "PLAYER HP", buf);
        EnemyAnim *ea = enemy_anim_ensure_at(m->x, m->y);
        draw_entity_edit_row(fb, 22, "ANIM", (ea && ea->enabled) ? "ON" : "OFF");
        draw_entity_edit_row(fb, 23, "ANIM SPEED", ea ? anim_speed_name(ea->speed) : "OFF");
        snprintf(buf, sizeof(buf), "%d", g_anim_edit_state); draw_entity_edit_row(fb, 24, "ANIM STATE", buf);
        snprintf(buf, sizeof(buf), "%d", g_anim_edit_frame); draw_entity_edit_row(fb, 25, "ANIM FRAME", buf);
        draw_entity_edit_row(fb, 26, "EDIT ANIM", "A OPEN");
        draw_entity_edit_row(fb, 27, "DONE", "A CLOSE");
        if (m->ai_rank == AI_RANK_BOSS) draw_boss_sprite_preview_grid(fb, 246, 34, m->boss_sprite, npc_color_for(m->color_id), 2, -1, -1);
        else draw_enemy16_sprite_preview_grid(fb, 258, 34, m->sprite16, npc_color_for(m->color_id), 3, -1, -1);
    } else if (g_entity_edit_mode == EDIT_MODE_DOOR) {
        DoorMeta *d = door_meta_ensure_at(g_entity_edit_x, g_entity_edit_y);
        if (!d) { draw_text3x5(fb, BOT_W, BOT_H, 16, 52, "DOOR NOT FOUND", (Color){255,120,120}, 1); return; }
        snprintf(buf, sizeof(buf), "%d", d->texture_id & MAX_TEXTURE_ID); draw_entity_edit_row(fb, 0, "TEXTURE", buf);
        snprintf(buf, sizeof(buf), "%d", d->group_id); draw_entity_edit_row(fb, 1, "GROUP", buf);
        draw_entity_edit_row(fb, 2, "TYPE", door_type_name(d->door_type));
        draw_entity_edit_row(fb, 3, "SPEED", door_speed_name(d->speed));
        draw_entity_edit_row(fb, 4, "MOVE", door_move_name(d->move_dir));
        draw_entity_edit_row(fb, 5, "SWITCH", d->switch_pressed ? "DOWN" : "UP");
        draw_entity_edit_row(fb, 6, "TOGGLED", d->toggled ? "OPEN" : "CLOSED");
        draw_entity_edit_row(fb, 7, "COPY TEX", "TO WALL/FLOOR");
        draw_entity_edit_row(fb, 8, "DONE", "A CLOSE");
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


#include "common.h"

const Color WALL_COLORS[8] = {
    {0,   0,   0},
    {185, 185, 185},
    {190, 70,  65},
    {70,  150, 220},
    {95,  180, 95},
    {215, 185, 80},
    {155, 95,  205},
    {100, 225, 235}
};

Level g_level;
Level g_preview_level;
Level g_load_temp;
Level g_resize_temp;
Enemy g_enemies[MAX_ENEMIES];
Collectible g_collectibles[MAX_COLLECTIBLES];
Door g_doors[MAX_DOORS];
Projectile g_projectiles[MAX_PROJECTILES];
NPC g_npcs[MAX_NPCS];
EnemyMeta g_enemy_metas[MAX_ENEMIES];
uint8_t g_room_tiles[MAX_TILES];
WeaponDef g_weapons[MAX_WEAPONS] = {
    {"SWORD", 3, 1, 20, 1, {0x10,0x18,0x18,0x10,0x10,0x54,0x38,0x10}},
    {"DAGGER", 2, 1, 11, 2, {0x00,0x08,0x18,0x18,0x10,0x10,0x38,0x10}},
    {"KNIFE", 2, 1, 14, 3, {0x04,0x0C,0x1C,0x18,0x10,0x10,0x30,0x20}},
    {"MACE", 5, 1, 34, 5, {0x18,0x3C,0x18,0x18,0x18,0x18,0x18,0x10}},
    {"MALLET", 7, 1, 46, 6, {0x7E,0x7E,0x18,0x18,0x18,0x18,0x18,0x10}},
    {"AXE", 6, 1, 38, 4, {0x38,0x7C,0x58,0x18,0x18,0x18,0x18,0x10}},
    {"SPEAR", 4, 2, 28, 7, {0x10,0x38,0x10,0x10,0x10,0x10,0x10,0x10}},
    {"STAFF", 3, 3, 32, 0, {0x18,0x24,0x18,0x18,0x18,0x18,0x18,0x18}}
};
SlotInfo g_slots[SLOT_COUNT];

bool g_in_menu = true;
bool g_edit_mode = false;
bool g_resize_menu = false;
bool g_duplicate_menu = false;
bool g_delete_armed = false;
bool g_preview_valid = false;
bool g_render_angle_override = false;
bool g_render_world_hud = true;
uint8_t g_selected_tile = 1;
uint8_t g_selected_room = ROOM_ENEMY;
int g_editor_tool = EDITOR_TOOL_PAINT;
int g_slot = 0;
int g_menu_action = MENU_ACTION_PLAY;
int g_dup_source = 0;
int g_dup_target = 1;
int g_resize_w = DEFAULT_MAP_W;
int g_resize_h = DEFAULT_MAP_H;
float g_preview_spin = 0.0f;
float g_render_angle = 0.0f;
char g_status[96] = "BWL3DS";
char g_level_name[LEVEL_NAME_MAX + 1] = "Untitled";
bool g_fs_ready = false;
bool g_dirty = false;
bool g_is_new3ds = false;
float g_camera_pitch = 0.0f;
bool g_settings_menu = false;
int g_settings_cursor = 0;
int g_settings_scroll = 0;
float g_fov_degrees = FOV_DEGREES;
float g_level_depth = 1.0f;
bool g_view_bob = true;
bool g_3d_enabled = false;
bool g_dof_enabled = false;
float g_dof_start = 10.0f;
float g_dof_strength = 0.55f;
bool g_antialiasing = false;
bool g_fast_render = false;
bool g_debug_overlay = false;
float g_bob_phase = 0.0f;
float g_bob_amount = 0.0f;
float g_camera_speed = 0.0f;
int g_editor_view_x = 0;
int g_editor_view_y = 0;
int g_editor_zoom_tiles = 0;
int g_enemy_count = 0;
int g_collectible_count = 0;
int g_collectibles_left = 0;
int g_door_count = 0;
int g_projectile_count = 0;
int g_npc_count = 0;
int g_enemy_meta_count = 0;
int g_player_keys = 0;
int g_player_score = 0;
int g_coins_bank = 0;
int g_coins_total = 0;
int g_enemies_total = 0;
int g_enemies_killed = 0;
int g_missions_total = 0;
int g_missions_done = 0;
int g_success_percent = 0;
bool g_player_weapons[MAX_WEAPONS] = { false, false, false, false, false, false, false, false };
int g_player_health = PLAYER_HEALTH_DEFAULT;
int g_player_health_max = PLAYER_HEALTH_DEFAULT;
bool g_player_dead = false;
float g_player_hurt_timer = 0.0f;
char g_death_killer[32] = "";
int g_current_weapon = -1;
float g_attack_cooldown = 0.0f;
float g_slash_timer = 0.0f;
float g_slash_total = 0.26f;
float g_weapon_bounce_timer = 0.0f;
bool g_screen_shake_enabled = true;
float g_screen_shake_timer = 0.0f;
int g_slash_type = 2;
bool g_has_success = false;
float g_success_x = 0.0f;
float g_success_y = 0.0f;
bool g_random_play = false;
bool g_level_won = false;
float g_play_start_x = 0.0f;
float g_play_start_y = 0.0f;
float g_play_start_z = 0.0f;
float g_play_start_angle = 0.0f;
uint32_t g_random_seed = 1u;
float g_frame_dt = 1.0f / 60.0f;
float g_fps_smooth = 60.0f;
bool g_settings_dirty = false;
bool g_loaded_npc_metadata = false;
bool g_loaded_enemy_metadata = false;
int g_entity_edit_mode = EDIT_MODE_NONE;
int g_entity_edit_cursor = 0;
int g_entity_edit_scroll = 0;
int g_entity_edit_x = 0;
int g_entity_edit_y = 0;
int g_entity_edit_weapon = 0;
uint8_t g_default_npc_color = 0;
uint8_t g_default_npc_sprite[SPRITE_BYTES] = {0x3C,0x7E,0xDB,0xFF,0x7E,0x3C,0x24,0x66};
uint16_t g_default_npc_sprite16[ENEMY_SPRITE_ROWS] = {
    0x0FF0,0x0FF0,0x3FFC,0x3FFC,0xF3CF,0xF3CF,0xFFFF,0xFFFF,
    0x3FFC,0x3FFC,0x0FF0,0x0FF0,0x0C30,0x0C30,0x3C3C,0x3C3C
};
int g_sprite_edit_target = 0;
int g_sprite_edit_cursor_x = 0;
int g_sprite_edit_cursor_y = 0;
int g_sprite_edit_return_mode = EDIT_MODE_NONE;

uint32_t g_frame_counter = 0;

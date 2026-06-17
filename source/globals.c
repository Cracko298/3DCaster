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
int g_player_keys = 0;
int g_player_score = 0;
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

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

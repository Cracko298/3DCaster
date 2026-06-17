#ifndef BWL3DS_COMMON_H
#define BWL3DS_COMMON_H

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

#define RENDER_W 200
#define RENDER_H 120
#define PIXEL_SCALE 2

#define DEFAULT_MAP_W 96
#define DEFAULT_MAP_H 96
#define MAX_MAP_W 192
#define MAX_MAP_H 192
#define MIN_MAP_W 8
#define MIN_MAP_H 8
#define MAP_SIZE_STEP 8
#define MAX_TILES (MAX_MAP_W * MAX_MAP_H)

#define SLOT_COUNT 8
#define LEVEL_NAME_MAX 31
#define SAVE_PATH_PRIMARY  "sdmc:/3ds/bwl_slot%d.bwl"
#define SAVE_PATH_BACKUP   "sdmc:/bwl_slot%d.bwl"
#define SAVE_FS_PRIMARY    "/3ds/bwl_slot%d.bwl"
#define SAVE_FS_BACKUP     "/bwl_slot%d.bwl"
#define META_FS_PRIMARY    "/3ds/bwl_slot%d.meta"
#define META_FS_BACKUP     "/bwl_slot%d.meta"
#define SETTINGS_FS_PRIMARY "/3ds/3dcaster_settings.cfg"
#define SETTINGS_FS_BACKUP  "/3dcaster_settings.cfg"

#define PI_F 3.14159265358979323846f
#define TWO_PI_F 6.28318530717958647692f

#define FOV_DEGREES 66.0f
#define FOV_MIN_DEGREES 50.0f
#define FOV_MAX_DEGREES 110.0f
#define SETTINGS_ROW_COUNT 11
#define MOVE_SPEED 3.3f
#define SPRINT_MULT 1.75f
#define ROT_SPEED 2.35f

#define EYE_HEIGHT 0.55f
#define PLAYER_RADIUS 0.18f
#define GRAVITY 2.9f
#define JUMP_SPEED 1.38f
#define STEP_HEIGHT 0.24f
#define PLATFORM_BOTTOM 0.42f
#define PLATFORM_TOP 0.62f
#define PLATFORM_TILE 7
#define TILE_DOT 8
#define TILE_PINK 9
#define TILE_PURPLE 10
#define TILE_NPC 11
#define TILE_AI_SPAWN 12
#define TILE_SUCCESS 13
#define TILE_KEY 14
#define TILE_DOOR 15
#define MAX_TILE_ID 15
#define MAX_ENEMIES 32
#define MAX_COLLECTIBLES 160
#define MAX_DOORS 128
#define MAX_NPCS 32
#define MAX_WEAPONS 5
#define NPC_TEXT_MAX 48
#define ENEMY_TEXT_MAX 32
#define ENEMY_TEXT_LINES 3
#define SPRITE_BYTES 8
#define TEXT_MODE_INTERACT 0
#define TEXT_MODE_NEAR 1
#define TEXT_MODE_ALWAYS 2
#define EDIT_MODE_NONE 0
#define EDIT_MODE_NPC 1
#define EDIT_MODE_ENEMY 2
#define EDIT_MODE_WEAPON 3
#define EDIT_MODE_SPRITE 4
#define SPRITE_TARGET_NPC 1
#define SPRITE_TARGET_ENEMY 2
#define SPRITE_TARGET_WEAPON 3
#define SPRITE_TARGET_DEFAULT_NPC 4
#define QUEST_NONE 0
#define QUEST_COINS 1
#define QUEST_KEY 2
#define QUEST_NPC 3
#define REWARD_DOT 0
#define REWARD_KEY 1
#define REWARD_PINK 2
#define REWARD_PURPLE 3
#define REWARD_WEAPON_BASE 16
#define WEAPON_SWORD 0
#define WEAPON_DAGGER 1
#define WEAPON_KNIFE 2
#define WEAPON_MACE 3
#define WEAPON_MALLET 4
#define KEY_PICKUP_RADIUS 0.48f
#define DOOR_TRIGGER_RADIUS 1.35f
#define DOOR_OPEN_TIME 0.72f
#define RANDOM_MAZE_CELLS_W 31
#define RANDOM_MAZE_CELLS_H 31
#define RANDOM_MAZE_W (RANDOM_MAZE_CELLS_W * 4 + 2)
#define RANDOM_MAZE_H (RANDOM_MAZE_CELLS_H * 4 + 2)

#define EDITOR_MAP_Y 4
#define EDITOR_MAP_H 176
#define PALETTE_Y 220
#define PALETTE_X 4
#define PALETTE_STEP 22
#define PALETTE_W 18
#define PALETTE_H 16
#define EDITOR_CTRL_Y 184
#define EDITOR_CTRL_H 56
#define EDITOR_ZOOM_MIN_TILES 8


#define POS_SCALE 64.0f
#define ANGLE_SCALE (65535.0f / TWO_PI_F)

#ifndef KEY_CSTICK_RIGHT
#define KEY_CSTICK_RIGHT BIT(24)
#define KEY_CSTICK_LEFT  BIT(25)
#define KEY_CSTICK_UP    BIT(26)
#define KEY_CSTICK_DOWN  BIT(27)
#endif

typedef struct {
    uint8_t r, g, b;
} Color;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t tiles[MAX_TILES];
    float player_x;
    float player_y;
    float player_z;
    float player_vz;
    float player_angle;
    bool on_ground;
} Level;

typedef struct {
    bool active;
    float x, y, z;
    float start_x, start_y, start_z;
    float angle;
    float speed;
    float roam_timer;
    int hp;
    int hp_max;
    int attack;
    int state;
    uint8_t text_count;
    uint8_t text_index;
    uint8_t color_id;
    uint8_t sprite[SPRITE_BYTES];
    float text_timer;
    char text[ENEMY_TEXT_LINES][ENEMY_TEXT_MAX];
} Enemy;

typedef struct {
    bool active;
    int x, y;
    float fx, fy;
    int kind;
} Collectible;

typedef struct {
    bool active;
    bool opening;
    int x, y;
    float open_t;
} Door;


typedef struct {
    bool active;
    int x, y;
    uint8_t color_id;
    uint8_t text_mode;
    uint8_t sprite[SPRITE_BYTES];
    float talk_timer;
    uint8_t quest_type;
    uint16_t quest_target;
    uint8_t reward_kind;
    uint16_t reward_amount;
    bool completed;
    char text[NPC_TEXT_MAX];
} NPC;

typedef struct {
    bool active;
    int x, y;
    uint8_t hp;
    uint8_t attack;
    uint8_t color_id;
    uint8_t sprite[SPRITE_BYTES];
    uint8_t text_count;
    char text[ENEMY_TEXT_LINES][ENEMY_TEXT_MAX];
} EnemyMeta;

typedef struct {
    char name[12];
    uint8_t damage;
    uint8_t range;
    uint8_t cooldown;
    uint8_t color_id;
    uint8_t sprite[SPRITE_BYTES];
} WeaponDef;

typedef struct {
    bool exists;
    char name[LEVEL_NAME_MAX + 1];
    uint16_t width;
    uint16_t height;
    int filled_tiles;
    uint32_t checksum;
} SlotInfo;

typedef enum {
    MENU_ACTION_PLAY = 0,
    MENU_ACTION_RANDOM,
    MENU_ACTION_EDIT,
    MENU_ACTION_RESIZE,
    MENU_ACTION_DUPLICATE,
    MENU_ACTION_RENAME,
    MENU_ACTION_DELETE,
    MENU_ACTION_WEAPONS,
    MENU_ACTION_SETTINGS,
    MENU_ACTION_COUNT
} MenuAction;

extern const Color WALL_COLORS[8];
extern Level g_level;
extern Level g_preview_level;
extern Level g_load_temp;
extern Level g_resize_temp;
extern Enemy g_enemies[MAX_ENEMIES];
extern Collectible g_collectibles[MAX_COLLECTIBLES];
extern Door g_doors[MAX_DOORS];
extern NPC g_npcs[MAX_NPCS];
extern EnemyMeta g_enemy_metas[MAX_ENEMIES];
extern WeaponDef g_weapons[MAX_WEAPONS];
extern SlotInfo g_slots[SLOT_COUNT];
extern bool g_in_menu;
extern bool g_edit_mode;
extern bool g_resize_menu;
extern bool g_duplicate_menu;
extern bool g_delete_armed;
extern bool g_preview_valid;
extern bool g_render_angle_override;
extern bool g_render_world_hud;
extern uint8_t g_selected_tile;
extern int g_slot;
extern int g_menu_action;
extern int g_dup_source;
extern int g_dup_target;
extern int g_resize_w;
extern int g_resize_h;
extern float g_preview_spin;
extern float g_render_angle;
extern char g_status[96];
extern char g_level_name[LEVEL_NAME_MAX + 1];
extern bool g_fs_ready;
extern bool g_dirty;
extern bool g_is_new3ds;
extern float g_camera_pitch;
extern bool g_settings_menu;
extern int g_settings_cursor;
extern float g_fov_degrees;
extern float g_level_depth;
extern bool g_view_bob;
extern bool g_3d_enabled;
extern bool g_dof_enabled;
extern float g_dof_start;
extern float g_dof_strength;
extern bool g_antialiasing;
extern bool g_fast_render;
extern bool g_debug_overlay;
extern float g_bob_phase;
extern float g_bob_amount;
extern float g_camera_speed;
extern int g_editor_view_x;
extern int g_editor_view_y;
extern int g_editor_zoom_tiles;
extern int g_enemy_count;
extern int g_collectible_count;
extern int g_collectibles_left;
extern int g_door_count;
extern int g_npc_count;
extern int g_enemy_meta_count;
extern int g_player_keys;
extern int g_player_score;
extern bool g_player_weapons[MAX_WEAPONS];
extern int g_current_weapon;
extern float g_attack_cooldown;
extern bool g_has_success;
extern float g_success_x;
extern float g_success_y;
extern bool g_random_play;
extern bool g_level_won;
extern float g_play_start_x;
extern float g_play_start_y;
extern float g_play_start_z;
extern float g_play_start_angle;
extern uint32_t g_random_seed;
extern float g_frame_dt;
extern float g_fps_smooth;
extern bool g_settings_dirty;
extern bool g_loaded_npc_metadata;
extern bool g_loaded_enemy_metadata;
extern int g_entity_edit_mode;
extern int g_entity_edit_cursor;
extern int g_entity_edit_x;
extern int g_entity_edit_y;
extern int g_entity_edit_weapon;
extern uint8_t g_default_npc_color;
extern uint8_t g_default_npc_sprite[SPRITE_BYTES];
extern int g_sprite_edit_target;
extern int g_sprite_edit_cursor_x;
extern int g_sprite_edit_cursor_y;
extern int g_sprite_edit_return_mode;


float clampf32(float v, float lo, float hi);
uint16_t clamp_u16_from_float(float v);
int clampi32(int v, int lo, int hi);
int round_size_step(int v);
void sanitize_level_name(char *name);
uint16_t qpos(float v);
float uqpos(uint16_t v);
uint16_t qangle(float rad);
float uqangle(uint16_t v);
uint16_t read_u16_le(const uint8_t *p);
uint32_t read_u32_le(const uint8_t *p);
float read_f32_le(const uint8_t *p);
bool write_all(FILE *f, const void *data, size_t size);
bool write_u16_le(FILE *f, uint16_t v);
int tile_index(const Level *lv, int x, int y);
uint8_t tile_at(const Level *lv, int x, int y);
void set_tile(Level *lv, int x, int y, uint8_t v);
bool tile_blocks_side(uint8_t tile, float z);
bool tile_blocks_raycast(uint8_t tile);
float door_open_fraction_at(int x, int y);
bool can_stand_at(const Level *lv, float x, float y, float z);
float ground_height_at(const Level *lv, float x, float y, float z);
void force_valid_spawn(Level *lv);
void make_border(Level *lv);
void new_sized_level(Level *lv, uint16_t width, uint16_t height);
void new_open_level(Level *lv);
void resize_level_preserve(Level *lv, uint16_t width, uint16_t height);
void put_pixel_raw(u8 *fb, int screen_w, int screen_h, int x, int y, Color c);
void fill_rect_raw(u8 *fb, int screen_w, int screen_h, int x0, int y0, int x1, int y1, Color c);
Color shade_color(Color base, float shade);
void draw_digit(u8 *fb, int sw, int sh, int x, int y, int d, Color c, int scale);
void draw_number(u8 *fb, int sw, int sh, int x, int y, int value, Color c, int scale);
uint8_t glyph3x5_row(char ch, int row);
void draw_char3x5(u8 *fb, int sw, int sh, int x, int y, char ch, Color c, int scale);
void draw_text3x5(u8 *fb, int sw, int sh, int x, int y, const char *text, Color c, int scale);
void draw_text_number(u8 *fb, int sw, int sh, int x, int y, const char *label, int value, Color c, int scale);
void make_slot_path(char *out, size_t out_size, bool backup);
void make_slot_fs_path_for(int slot, char *out, size_t out_size, bool backup);
void make_slot_fs_path(char *out, size_t out_size, bool backup);
void make_meta_fs_path_for(int slot, char *out, size_t out_size, bool backup);
bool mem_put_u8(uint8_t *out, size_t cap, size_t *pos, uint8_t v);
bool mem_put_u16_le(uint8_t *out, size_t cap, size_t *pos, uint16_t v);
bool mem_put_u32_le(uint8_t *out, size_t cap, size_t *pos, uint32_t v);
bool mem_put_raw_packet(uint8_t *out, size_t cap, size_t *pos, const uint8_t *tiles, int start, int count);
bool encode_bwl2_memory(const Level *lv, uint8_t **out_data, size_t *out_size);
bool fs_write_whole_file(const char *fs_path, const uint8_t *data, size_t size);
bool fs_read_whole_file(const char *fs_path, uint8_t **out_data, size_t *out_size);
bool fs_delete_path(const char *fs_path);
void default_slot_name(int slot, char *out, size_t out_size);
bool save_slot_meta(int slot, const char *name);
bool load_slot_meta(int slot, char *out, size_t out_size);
bool decode_bwl2_tiles(const uint8_t *data, size_t size, Level *lv);
bool parse_bwl_data(const uint8_t *data, size_t len, Level *lv);
int count_nonzero_tiles(const Level *lv);
uint32_t checksum_level_tiles(const Level *lv);
bool save_bwl2_slot(const Level *lv, int slot, const char *level_name, bool set_status);
bool save_bwl2(const Level *lv);
bool load_bwl_from_fs_path(Level *lv, const char *fs_path);
bool load_bwl_slot_index(Level *lv, int slot, bool set_status);
bool load_bwl_slot(Level *lv);
void refresh_slot_info(int slot);
void refresh_all_slots(void);
void refresh_preview_slot(void);
void delete_slot(int slot);
bool duplicate_slot(int src_slot, int dst_slot);
void prompt_rename_slot(int slot);
bool load_app_settings(void);
bool save_app_settings(void);
void draw_sky_floor(u8 *fb);
void render_raycast(const Level *lv);
void editor_reset_view(void);
void editor_clamp_view(const Level *lv);
void editor_view_bounds(const Level *lv, int *vx, int *vy, int *vw, int *vh);
void editor_zoom_in(const Level *lv);
void editor_zoom_out(const Level *lv);
void editor_zoom_fit(const Level *lv);
void editor_pan_view(const Level *lv, int dx, int dy);
void editor_layout(const Level *lv, int *cell, int *ox, int *oy);
Color map_tile_color(uint8_t tile);
void draw_tile_palette(u8 *fb);
void render_bottom_map(void);
void render_entity_editor(u8 *fb);
const char *menu_action_name(int action);
void render_world_menu(void);
void enter_slot(bool edit_mode);
void apply_resize_menu(void);
void handle_world_menu_input(u32 kDown);
void open_weapon_editor(int weapon);
void open_entity_editor_at(int x, int y);
void close_entity_editor(void);
void handle_entity_edit_input(u32 kDown);
void generate_random_maze(Level *lv, uint32_t seed);
void spawn_entities_from_level(const Level *lv);
void reset_runtime_entities(void);
NPC *npc_find_at(int x, int y);
NPC *npc_ensure_at(int x, int y);
EnemyMeta *enemy_meta_find_at(int x, int y);
EnemyMeta *enemy_meta_ensure_at(int x, int y);
void randomize_weapon_stats(uint32_t seed);
const char *weapon_name(int weapon);
void copy_default_sprite(uint8_t *dst, int kind);
void open_sprite_editor(int target);
void handle_sprite_edit_input(u32 kDown);
void render_sprite_editor(u8 *fb);
void update_physics_and_movement(float dt, u32 kDown, u32 kHeld);
void editor_touch(u32 kDown, u32 kHeld);
void handle_global_input(u32 kDown, u32 kHeld);
void handle_editor_input(float dt, u32 kDown, u32 kHeld);

#endif

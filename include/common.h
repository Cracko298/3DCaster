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
#define STATE_FS_PRIMARY   "/3ds/bwl_state%d.bws"
#define STATE_FS_BACKUP    "/bwl_state%d.bws"
#define SETTINGS_FS_PRIMARY "/3ds/3dcaster_settings.cfg"
#define SETTINGS_FS_BACKUP  "/3dcaster_settings.cfg"

#define PI_F 3.14159265358979323846f
#define TWO_PI_F 6.28318530717958647692f

#define FOV_DEGREES 66.0f
#define FOV_MIN_DEGREES 50.0f
#define FOV_MAX_DEGREES 110.0f
#define SETTINGS_ROW_COUNT 14
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
#define MAX_ROOM_ID 8
#define ROOM_NONE 0
#define ROOM_TREASURE 1
#define ROOM_BOSS 2
#define ROOM_TRAP 3
#define ROOM_ENEMY 4
#define ROOM_CORRIDOR 5
#define ROOM_SAFE 6
#define ROOM_PUZZLE 7
#define ROOM_SECRET 8
#define EDITOR_TOOL_PAINT 0
#define EDITOR_TOOL_SELECT 1
#define EDITOR_TOOL_ROOM 2
#define MAX_ENEMIES 32
#define MAX_COLLECTIBLES 160
#define MAX_DOORS 128
#define MAX_PROJECTILES 24
#define MAX_NPCS 32
#define MAX_WEAPONS 8
#define NPC_TEXT_MAX 96
#define ENEMY_TEXT_MAX 64
#define ENEMY_TEXT_LINES 3
#define SPRITE_BYTES 8
#define ENEMY_SPRITE_W 16
#define ENEMY_SPRITE_H 16
#define ENEMY_SPRITE_ROWS 16
#define BOSS_SPRITE_W 32
#define BOSS_SPRITE_H 32
#define BOSS_SPRITE_ROWS 32
#define BOSS_LEGACY_SPRITE_W 14
#define BOSS_LEGACY_SPRITE_H 14
#define BOSS_LEGACY_SPRITE_ROWS 14
#define AI_RANK_GRUNT 0
#define AI_RANK_CAPTAIN 1
#define AI_RANK_BOSS 2
#define AI_SPAWN_NONE 0
#define AI_SPAWN_GRUNT 1
#define TEXT_SPEED_INSTANT 0
#define TEXT_SPEED_SLOW 1
#define TEXT_SPEED_MEDIUM 2
#define TEXT_SPEED_FAST 3
#define MAX_RENDERED_SPRITES 32
#define ITEM_RENDER_DIST2 (22.0f * 22.0f)
#define NPC_RENDER_DIST2 (28.0f * 28.0f)
#define ENEMY_RENDER_DIST2 (32.0f * 32.0f)
#define AI_SLEEP_DIST2 (36.0f * 36.0f)
#define TEXT_MODE_INTERACT 0
#define TEXT_MODE_NEAR 1
#define TEXT_MODE_ALWAYS 2
#define EDIT_MODE_NONE 0
#define EDIT_MODE_NPC 1
#define EDIT_MODE_ENEMY 2
#define EDIT_MODE_WEAPON 3
#define EDIT_MODE_SPRITE 4
#define EDIT_MODE_DOOR 5
#define SPRITE_TARGET_NPC 1
#define SPRITE_TARGET_ENEMY 2
#define SPRITE_TARGET_WEAPON 3
#define SPRITE_TARGET_DEFAULT_NPC 4
#define SPRITE_TARGET_BOSS 5
#define SPRITE_TARGET_NPC_ANIM 6
#define SPRITE_TARGET_ENEMY_ANIM 7
#define QUEST_NONE 0
#define QUEST_COINS 1
#define QUEST_KEY 2
#define QUEST_NPC 3
#define REWARD_DOT 0
#define REWARD_KEY 1
#define REWARD_PINK 2
#define REWARD_PURPLE 3
#define REWARD_HEALTH 4
#define REWARD_WEAPON_BASE 16
#define PLAYER_HEALTH_DEFAULT 20
#define PLAYER_HEALTH_MIN 1
#define PLAYER_HEALTH_MAX 99
#define HEALTH_PICKUP_AMOUNT 5
#define ENEMY_HP_BAR_DIST2 (9.0f * 9.0f)
#define PLAYER_HURT_TIME 0.75f
#define BOSS_ARROW_SPEED 5.25f
#define BOSS_ARROW_LIFE 2.20f
#define WEAPON_SWORD 0
#define WEAPON_DAGGER 1
#define WEAPON_KNIFE 2
#define WEAPON_MACE 3
#define WEAPON_MALLET 4
#define KEY_PICKUP_RADIUS 0.48f
#define DOOR_TRIGGER_RADIUS 1.35f
#define DOOR_OPEN_TIME 0.72f
#define MAX_TEXTURE_ID 7
#define DOOR_TYPE_AUTO 0
#define DOOR_TYPE_KEY 1
#define DOOR_TYPE_TOGGLE 2
#define DOOR_TYPE_SWITCH 3
#define DOOR_SPEED_SLOW 0
#define DOOR_SPEED_MEDIUM 1
#define DOOR_SPEED_FAST 2
#define DOOR_MOVE_UP 0
#define DOOR_MOVE_DOWN 1
#define DOOR_MOVE_LEFT 2
#define DOOR_MOVE_RIGHT 3
#define MAX_DOOR_GROUP_SIZE 3
#define ANIM_SPEED_OFF 0
#define ANIM_SPEED_SLOW 1
#define ANIM_SPEED_MEDIUM 2
#define ANIM_SPEED_FAST 3
#define NPC_ANIM_IDLE 0
#define NPC_ANIM_TALK 1
#define NPC_ANIM_COMPLETE 2
#define ENEMY_ANIM_IDLE 0
#define ENEMY_ANIM_MOVE 1
#define ENEMY_ANIM_ATTACK 2
#define ENEMY_ANIM_SHOOT 3
#define ANIM_FRAMES 3
#define MAX_EVENT_FLAGS 64
#define MAX_TRIGGERS 32
#define MAX_SYNTH_PATTERNS 16
#define MAX_SYNTH_NOTES 32
#define AUDIO_EVENT_NONE 0
#define AUDIO_EVENT_ATTACK 1
#define AUDIO_EVENT_HIT 2
#define AUDIO_EVENT_PICKUP 3
#define AUDIO_EVENT_DOOR 4
#define AUDIO_EVENT_QUEST 5
#define AUDIO_EVENT_NPC 6
#define AUDIO_EVENT_ENEMY 7
#define AUDIO_EVENT_BOSS 8
#define TRIGGER_ACTION_SET_FLAG 0
#define TRIGGER_ACTION_CLEAR_FLAG 1
#define TRIGGER_ACTION_TOGGLE_FLAG 2
#define TRIGGER_ACTION_ADD_KEY 3
#define TRIGGER_ACTION_HEAL 4

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
    uint16_t actor_id;
    float x, y, z;
    float start_x, start_y, start_z;
    float angle;
    float speed;
    float roam_timer;
    float hit_timer;
    float death_timer;
    int hp;
    int hp_max;
    int attack;
    int state;
    bool dying;
    uint8_t text_count;
    uint8_t text_index;
    uint8_t color_id;
    uint8_t sprite[SPRITE_BYTES];
    uint16_t sprite16[ENEMY_SPRITE_ROWS];
    float text_timer;
    float anim_timer;
    uint8_t anim_frame;
    uint8_t ai_rank;
    uint8_t spawn_kind;
    uint8_t spawn_limit;
    uint8_t command_range;
    uint8_t sight_range;
    uint8_t ranged_attack;
    uint8_t melee_range;
    uint8_t attack_cooldown;
    uint8_t spawn_cooldown;
    uint8_t projectile_color;
    uint8_t projectile_style;
    uint8_t projectile_anim;
    uint8_t speed_attr;
    uint8_t size_pct;
    uint8_t text_speed;
    float spawn_timer;
    float ranged_timer;
    int parent_index;
    uint32_t boss_sprite[BOSS_SPRITE_ROWS];
    char text[ENEMY_TEXT_LINES][ENEMY_TEXT_MAX];
} Enemy;

typedef struct {
    bool active;
    int x, y;
    float fx, fy;
    int kind;
    int amount;
} Collectible;

typedef struct {
    bool active;
    uint16_t actor_id;
    bool opening;
    int x, y;
    float open_t;
    uint8_t texture_id;
    uint8_t group_id;
    uint8_t door_type;
    uint8_t speed;
    uint8_t move_dir;
    bool switch_pressed;
    bool toggled;
} Door;

typedef struct {
    bool active;
    uint16_t actor_id;
    int x, y;
    uint8_t texture_id;
    uint8_t group_id;
    uint8_t door_type;
    uint8_t speed;
    uint8_t move_dir;
    bool switch_pressed;
    bool toggled;
} DoorMeta;

typedef struct {
    bool active;
    int x, y;
    bool enabled;
    uint8_t speed;
    uint8_t edit_state;
    uint8_t edit_frame;
    uint16_t frames[3][ANIM_FRAMES][ENEMY_SPRITE_ROWS];
} NPCAnim;

typedef struct {
    bool active;
    int x, y;
    bool enabled;
    uint8_t speed;
    uint8_t edit_state;
    uint8_t edit_frame;
    uint16_t frames[4][ANIM_FRAMES][ENEMY_SPRITE_ROWS];
} EnemyAnim;

typedef struct {
    bool active;
    uint16_t actor_id;
    float x, y, z;
    float vx, vy;
    float life;
    int damage;
    uint8_t color_id;
    uint8_t style;
    uint8_t anim;
    char killer[24];
} Projectile;


typedef struct {
    bool active;
    uint16_t actor_id;
    int x, y;
    uint8_t color_id;
    uint8_t text_mode;
    uint8_t text_speed;
    uint8_t sprite[SPRITE_BYTES];       /* legacy 8x8 NPC art for older saves */
    uint16_t sprite16[ENEMY_SPRITE_ROWS]; /* primary 16x16 NPC art */
    float talk_timer;
    uint8_t quest_type;
    uint16_t quest_target;
    uint8_t reward_kind;
    uint16_t reward_amount;
    bool completed;
    bool known;
    char text[NPC_TEXT_MAX];
} NPC;

typedef struct {
    bool active;
    uint16_t actor_id;
    int x, y;
    uint8_t hp;
    uint8_t attack;
    uint8_t color_id;
    uint8_t sprite[SPRITE_BYTES];
    uint16_t sprite16[ENEMY_SPRITE_ROWS];
    uint8_t ai_rank;
    uint8_t spawn_kind;
    uint8_t spawn_limit;
    uint8_t command_range;
    uint8_t sight_range;
    uint8_t ranged_attack;
    uint8_t melee_range;
    uint8_t attack_cooldown;
    uint8_t spawn_cooldown;
    uint8_t projectile_color;
    uint8_t projectile_style;
    uint8_t projectile_anim;
    uint8_t speed_attr;
    uint8_t size_pct;
    uint8_t text_speed;
    uint32_t boss_sprite[BOSS_SPRITE_ROWS];
    uint8_t text_count;
    char text[ENEMY_TEXT_LINES][ENEMY_TEXT_MAX];
} EnemyMeta;

typedef struct {
    bool active;
    uint16_t actor_id;
    int x, y;
    uint8_t flag_id;
    uint8_t action;
    uint8_t amount;
} TriggerMeta;

typedef struct {
    uint8_t pitch;
    uint8_t length;
    uint8_t wave;
    uint8_t volume;
} SynthNote;

typedef struct {
    bool active;
    uint8_t event_id;
    uint8_t note_count;
    SynthNote notes[MAX_SYNTH_NOTES];
} SynthPattern;

typedef struct {
    char name[16];
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
extern Projectile g_projectiles[MAX_PROJECTILES];
extern NPC g_npcs[MAX_NPCS];
extern EnemyMeta g_enemy_metas[MAX_ENEMIES];
extern uint8_t g_room_tiles[MAX_TILES];
extern uint8_t g_wall_textures[MAX_TILES];
extern uint8_t g_floor_textures[MAX_TILES];
extern DoorMeta g_door_metas[MAX_DOORS];
extern NPCAnim g_npc_anims[MAX_NPCS];
extern EnemyAnim g_enemy_anims[MAX_ENEMIES];
extern TriggerMeta g_triggers[MAX_TRIGGERS];
extern int g_trigger_count;
extern uint8_t g_event_flags[MAX_EVENT_FLAGS];
extern SynthPattern g_audio_patterns[MAX_SYNTH_PATTERNS];
extern int g_audio_pattern_count;
extern bool g_audio_enabled;
extern uint8_t g_audio_last_event;
extern float g_audio_event_timer;
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
extern uint8_t g_selected_room;
extern int g_editor_tool;
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
extern int g_settings_scroll;
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
extern int g_projectile_count;
extern int g_npc_count;
extern int g_enemy_meta_count;
extern int g_door_meta_count;
extern int g_npc_anim_count;
extern int g_enemy_anim_count;
extern int g_anim_edit_state;
extern int g_anim_edit_frame;
extern int g_player_keys;
extern int g_player_score;
extern int g_coins_bank;
extern int g_coins_total;
extern int g_enemies_total;
extern int g_enemies_killed;
extern int g_missions_total;
extern int g_missions_done;
extern int g_success_percent;
extern bool g_player_weapons[MAX_WEAPONS];
extern int g_player_health;
extern int g_player_health_max;
extern bool g_player_dead;
extern float g_player_hurt_timer;
extern char g_death_killer[32];
extern int g_current_weapon;
extern float g_attack_cooldown;
extern float g_slash_timer;
extern float g_slash_total;
extern float g_weapon_bounce_timer;
extern bool g_screen_shake_enabled;
extern float g_screen_shake_timer;
extern int g_slash_type;
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
extern int g_entity_edit_scroll;
extern int g_entity_edit_x;
extern int g_entity_edit_y;
extern int g_entity_edit_weapon;
extern uint8_t g_default_npc_color;
extern uint8_t g_default_npc_sprite[SPRITE_BYTES];
extern uint16_t g_default_npc_sprite16[ENEMY_SPRITE_ROWS];
extern int g_sprite_edit_target;
extern int g_sprite_edit_cursor_x;
extern int g_sprite_edit_cursor_y;
extern int g_sprite_edit_return_mode;
extern uint32_t g_frame_counter;


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
uint8_t room_at(const Level *lv, int x, int y);
void set_room(Level *lv, int x, int y, uint8_t v);
void clear_room_overlay(void);
void clear_texture_overlays(void);
void clear_extended_entity_metadata(void);
uint8_t wall_texture_at(const Level *lv, int x, int y);
uint8_t floor_texture_at(const Level *lv, int x, int y);
uint8_t door_texture_at(int x, int y);
void set_wall_texture(Level *lv, int x, int y, uint8_t v);
void set_floor_texture(Level *lv, int x, int y, uint8_t v);
DoorMeta *door_meta_find_at(int x, int y);
DoorMeta *door_meta_ensure_at(int x, int y);
NPCAnim *npc_anim_find_at(int x, int y);
NPCAnim *npc_anim_ensure_at(int x, int y);
EnemyAnim *enemy_anim_find_at(int x, int y);
EnemyAnim *enemy_anim_ensure_at(int x, int y);
const char *door_type_name(uint8_t t);
const char *door_speed_name(uint8_t s);
const char *door_move_name(uint8_t d);
const char *anim_speed_name(uint8_t s);
float door_speed_seconds(uint8_t s);
const uint16_t *npc_anim_frame_for(const NPC *n);
const uint16_t *enemy_anim_frame_for(const Enemy *e);
uint16_t actor_id_for_tile(uint8_t tile, int x, int y);
void clear_event_flags(void);
bool event_flag_get(uint8_t flag_id);
void event_flag_set(uint8_t flag_id, bool value);
void event_flag_toggle(uint8_t flag_id);
void clear_triggers(void);
void synth_reset_defaults(void);
void synth_play_event(uint8_t event_id);
void synth_update(float dt);
const char *synth_event_name(uint8_t event_id);
const char *room_class_name(uint8_t room);
Color room_class_color(uint8_t room);
bool tile_blocks_side(uint8_t tile, float z);
bool tile_blocks_raycast(uint8_t tile);
float door_open_fraction_at(int x, int y);
bool door_blocks_at(int x, int y);
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
void make_state_fs_path_for(int slot, char *out, size_t out_size, bool backup);
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
bool load_slot_embedded_name(int slot, char *out, size_t out_size);
bool save_world_state_slot(int slot);
bool load_world_state_slot(int slot);
void delete_world_state_slot(int slot);
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
const char *editor_tool_name(int tool);
void copy_default_sprite(uint8_t *dst, int kind);
void copy_default_enemy_sprite16(uint16_t *dst);
void copy_default_boss_sprite(uint32_t *dst);
void open_sprite_editor(int target);
void handle_sprite_edit_input(u32 kDown);
void render_sprite_editor(u8 *fb);
void update_physics_and_movement(float dt, u32 kDown, u32 kHeld);
void editor_touch(u32 kDown, u32 kHeld);
void handle_global_input(u32 kDown, u32 kHeld);
void handle_editor_input(float dt, u32 kDown, u32 kHeld);

#endif

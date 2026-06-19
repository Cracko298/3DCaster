# 3DCaster BWL Format Documentation

This document describes the current 3DCaster `.bwl` level format used by the newer engine patches with player health, 16×16 NPC/enemy art, 32×32 boss art, named weapons, enemy stats, AI hierarchy, text speed settings, and world metadata.

The current save format is best thought of as:

```text
BW3 header
compressed tile map
metadata chunks
END!
```

Older formats are still loadable for backwards compatibility:

```text
BWL1
BW2
BW3
```

---

## 1. Basic Rules

### Byte order

All multi-byte integer values are **little-endian**.

```text
u16 = little-endian 16-bit unsigned integer
u32 = little-endian 32-bit unsigned integer
f32 = little-endian IEEE 754 float
u8  = unsigned 8-bit byte
```

### Strings

Strings are stored as:

```text
u8 length
byte[length] text
```

They are **not null-terminated** in the file.

The engine null-terminates them after loading.

### Coordinate system

Most placed objects use tile coordinates:

```text
x: u16 tile X
y: u16 tile Y
```

The player spawn uses quantized world coordinates:

```text
stored_value = float_value * 64.0
float_value = stored_value / 64.0
```

Angles are quantized to a 16-bit circle:

```text
stored_angle = angle_radians / (2π) * 65535
angle_radians = stored_angle / 65535 * 2π
```

---

## 2. Current Limits

These are the important current limits used by the engine:

```c
MAX_MAP_W              192
MAX_MAP_H              192
MAX_TILES              192 * 192

MAX_NPCS               32
MAX_ENEMIES            32
MAX_WEAPONS            8
MAX_COLLECTIBLES       160
MAX_DOORS              128
MAX_PROJECTILES        24

NPC_TEXT_MAX           96
ENEMY_TEXT_MAX         64
ENEMY_TEXT_LINES       3

SPRITE_BYTES           8       // legacy 8x8 bitmask
ENEMY_SPRITE_ROWS      16      // 16x16 art rows
BOSS_SPRITE_ROWS       32      // 32x32 art rows

PLAYER_HEALTH_MIN      1
PLAYER_HEALTH_MAX      99
```

---

## 3. Tile IDs

Each map tile is stored as a 4-bit value from `0` to `15`.

| ID | Name | Meaning |
|---:|---|---|
| `0` | Empty | Walkable floor |
| `1` | Wall 1 | Solid wall |
| `2` | Wall 2 | Solid wall |
| `3` | Wall 3 | Solid wall |
| `4` | Wall 4 | Solid wall |
| `5` | Wall 5 | Solid wall |
| `6` | Wall 6 | Solid wall |
| `7` | Platform | Raised platform/ledge |
| `8` | Dot/Coin | Small coin pickup |
| `9` | Pink collectible | Higher-value collectible |
| `10` | Purple collectible | Higher-value collectible |
| `11` | NPC | NPC marker |
| `12` | AI Spawn | Enemy/AI spawn marker |
| `13` | Success | Floor success/results trigger |
| `14` | Key | Key pickup |
| `15` | Door | Door tile |

Only the lower nibble is saved:

```c
tile & 0x0F
```

---

## 4. BW3 Header

Current files begin with the 3-byte magic:

```text
'B' 'W' '3'
```

### Layout

| Offset | Type | Name | Description |
|---:|---|---|---|
| `0x00` | char[3] | magic | `"BW3"` |
| `0x03` | u16 | width | Map width |
| `0x05` | u16 | height | Map height |
| `0x07` | u16 | player_x | Quantized player X |
| `0x09` | u16 | player_y | Quantized player Y |
| `0x0B` | u16 | player_z | Quantized player Z |
| `0x0D` | u16 | player_angle | Quantized player angle |
| `0x0F` | u32 | tile_stream_size | Size of compressed tile stream |
| `0x13` | bytes | tile_stream | RLE/nibble tile stream |
| `0x13 + tile_stream_size` | bytes | chunks | Metadata chunks |

The fixed BW3 header is `19` bytes long.

---

## 5. BW3 Tile Compression

BW3 uses the same tile packet compression as BW2, except BW3 stores the compressed tile stream size before the stream.

The decoder expands until it has:

```text
width * height
```

tiles.

### Packet control byte

Each packet starts with one control byte:

```text
ctrl
```

The count is:

```c
count = (ctrl & 0x7F) + 1;
```

So each packet represents `1` to `128` tiles.

### RLE packet

If the high bit is set:

```c
ctrl & 0x80
```

Then the next byte is repeated `count` times.

```text
u8 ctrl       // bit 7 set
u8 tile_value // lower nibble used
```

Example:

```text
83 01
```

Means:

```text
count = (0x83 & 0x7F) + 1 = 4
repeat tile 1 four times
```

The encoder normally uses RLE only when a run is at least 4 tiles long.

### Raw packet

If the high bit is clear:

```c
!(ctrl & 0x80)
```

Then the next `ceil(count / 2)` bytes contain packed 4-bit tile values.

```text
u8 ctrl
u8 packed_tiles[ceil(count / 2)]
```

Tiles are packed low nibble first:

```text
byte = tile0 | (tile1 << 4)
```

If there is an odd number of tiles, the high nibble of the last byte is unused.

---

## 6. BW3 Metadata Chunks

After the tile stream, BW3 stores a sequence of metadata chunks.

Current writer order:

```text
WLD5
NPC5
ENM5
WEP3
END!
```

Important: current chunks do **not** include a generic chunk size.  
That means unknown chunks are **not safely skippable** by older parsers. The loader expects known chunk tags and stops when it reaches:

```text
END!
```

---

# 7. `WLD5` World/Player Metadata Chunk

`WLD5` stores world/player-level metadata.

### Layout

```text
char[4] tag = "WLD5"
u8 player_health_max
```

### Fields

| Type | Name | Description |
|---|---|---|
| u8 | player_health_max | Player max health, clamped to `1..99` |

---

# 8. `NPC5` NPC Chunk

`NPC5` stores the current NPC format, including 16×16 NPC art and text speed.

### Layout

```text
char[4] tag = "NPC5"
u8 count

repeat count times:
    u16 x
    u16 y
    u8  color_id
    u8  text_mode
    u8  text_speed
    u8  sprite8[8]
    u16 sprite16[16]
    u8  quest_type
    u16 quest_target
    u8  reward_kind
    u16 reward_amount
    u8  completed
    u8  text_len
    u8  text[text_len]
```

### Per-NPC fixed section size

Before the variable text, each NPC entry has a fixed payload of:

```text
55 bytes
```

Then:

```text
1 byte text_len
text_len bytes of text
```

### Fields

| Type | Name | Description |
|---|---|---|
| u16 | x | NPC tile X |
| u16 | y | NPC tile Y |
| u8 | color_id | Palette/color index |
| u8 | text_mode | Text visibility mode |
| u8 | text_speed | Typewriter speed |
| u8[8] | sprite8 | Legacy 8×8 bitmask |
| u16[16] | sprite16 | Current 16×16 NPC art |
| u8 | quest_type | Quest requirement type |
| u16 | quest_target | Requirement target/count/NPC index |
| u8 | reward_kind | Reward type |
| u16 | reward_amount | Reward amount/weapon index/value |
| u8 | completed | `0` false, nonzero true |
| u8 | text_len | Text byte length |
| u8[] | text | NPC dialogue text |

### NPC text modes

```c
TEXT_MODE_INTERACT = 0
TEXT_MODE_NEAR     = 1
TEXT_MODE_ALWAYS   = 2
```

### Text speeds

```c
TEXT_SPEED_INSTANT = 0
TEXT_SPEED_SLOW    = 1
TEXT_SPEED_MEDIUM  = 2
TEXT_SPEED_FAST    = 3
```

### Quest types

```c
QUEST_NONE  = 0
QUEST_COINS = 1
QUEST_KEY   = 2
QUEST_NPC   = 3
```

### Reward kinds

```c
REWARD_DOT         = 0
REWARD_KEY         = 1
REWARD_PINK        = 2
REWARD_PURPLE      = 3
REWARD_HEALTH      = 4
REWARD_WEAPON_BASE = 16
```

Weapon rewards are encoded as:

```text
reward_kind = REWARD_WEAPON_BASE + weapon_index
```

For example:

```text
16 = weapon 0
17 = weapon 1
18 = weapon 2
...
```

---

# 9. `ENM5` Enemy Metadata Chunk

`ENM5` stores the current enemy/boss format.

It includes:

- normal enemy stats
- 16×16 enemy art
- AI hierarchy fields
- enemy speed and size attributes
- ranged/boss behavior fields
- 32×32 boss art
- enemy text lines
- enemy text speed

### Layout

```text
char[4] tag = "ENM5"
u8 count

repeat count times:
    u16 x
    u16 y
    u8  hp
    u8  attack
    u8  color_id
    u8  sprite8[8]
    u16 sprite16[16]
    u8  ai_rank
    u8  spawn_kind
    u8  spawn_limit
    u8  command_range
    u8  ranged_attack
    u8  speed_attr
    u8  size_pct
    u8  text_speed
    u32 boss_sprite[32]
    u8  text_count
    repeat text_count times:
        u8 text_len
        u8 text[text_len]
```

### Per-enemy fixed section size

Before text lines, each enemy entry has a fixed payload of:

```text
180 bytes
```

Then:

```text
u8 text_count
for each text line:
    u8 text_len
    text_len bytes of text
```

### Fields

| Type | Name | Description |
|---|---|---|
| u16 | x | Enemy tile X |
| u16 | y | Enemy tile Y |
| u8 | hp | Enemy max/current HP at spawn |
| u8 | attack | Enemy contact/projectile damage base |
| u8 | color_id | Palette/color index |
| u8[8] | sprite8 | Legacy 8×8 bitmask |
| u16[16] | sprite16 | Current 16×16 enemy art |
| u8 | ai_rank | AI hierarchy/rank |
| u8 | spawn_kind | What this AI can spawn |
| u8 | spawn_limit | Max spawned helpers |
| u8 | command_range | Command/spawn/influence range |
| u8 | ranged_attack | Nonzero enables ranged behavior; only bosses should use it |
| u8 | speed_attr | Speed percent, usually `100` |
| u8 | size_pct | Render size percent, usually `100` |
| u8 | text_speed | Typewriter speed |
| u32[32] | boss_sprite | 32×32 boss art, one row per u32 |
| u8 | text_count | Number of text lines, max `3` |
| u8/string | text lines | Length-prefixed enemy dialogue/combat text |

### AI ranks

```c
AI_RANK_GRUNT   = 0
AI_RANK_CAPTAIN = 1
AI_RANK_BOSS    = 2
```

### Spawn kinds

```c
AI_SPAWN_NONE  = 0
AI_SPAWN_GRUNT = 1
```

### Ranged attacks

Current gameplay intent:

```text
Normal enemies should not use ranged_attack.
Bosses can use ranged_attack to fire visible arrow projectiles.
```

The file format still stores `ranged_attack` as a byte for every enemy because the same struct is used for all ranks.

### 16×16 enemy art

Each `sprite16` row is a `u16` bitmask.

A common convention is:

```text
bit 15 = leftmost pixel
bit 0  = rightmost pixel
```

### 32×32 boss art

Each `boss_sprite` row is a `u32` bitmask.

A common convention is:

```text
bit 31 = leftmost pixel
bit 0  = rightmost pixel
```

---

# 10. `WEP3` Weapon Chunk

`WEP3` stores named weapons, stats, color, and 8×8 weapon icon art.

### Layout

```text
char[4] tag = "WEP3"
u8 count

repeat count times:
    u8  name_len
    u8  name[name_len]
    u8  damage
    u8  range
    u8  cooldown
    u8  color_id
    u8  sprite8[8]
```

### Fields

| Type | Name | Description |
|---|---|---|
| u8 | name_len | Weapon name length |
| u8[] | name | Weapon name text |
| u8 | damage | Weapon damage |
| u8 | range | Weapon range |
| u8 | cooldown | Weapon cooldown |
| u8 | color_id | Palette/color index |
| u8[8] | sprite8 | 8×8 weapon icon mask |

Current weapon count is:

```c
MAX_WEAPONS = 8
```

Weapon name storage in the current C struct is:

```c
char name[16]
```

So names should be kept to 15 visible characters for safest compatibility.

---

# 11. `END!` End Chunk

The metadata stream ends with:

```text
char[4] tag = "END!"
```

There is no payload.

---

# 12. Legacy Formats

## 12.1 `BWL1`

Old uncompressed map format.

### Layout

```text
char[4] magic = "BWL1"
u16 width
u16 height
f32 player_x
f32 player_y
f32 player_angle
u32 tile_count
u8  tiles[tile_count]
```

### Notes

- `tile_count` must equal `width * height`.
- Tiles are raw bytes but only the lower nibble is used.
- Player Z does not exist in BWL1 and loads as `0.0`.
- No metadata chunks exist.

---

## 12.2 `BW2`

Compressed tile-only format.

### Layout

```text
char[3] magic = "BW2"
u16 width
u16 height
u16 player_x
u16 player_y
u16 player_z
u16 player_angle
compressed tile stream until EOF
```

### Header size

```text
15 bytes
```

### Notes

- Uses the same RLE/nibble tile compression as BW3.
- There is no `tile_stream_size`.
- There are no metadata chunks.
- The compressed stream consumes the rest of the file.

---

# 13. Legacy BW3 Metadata Chunks

The current loader accepts several older chunk tags and upgrades missing data to defaults.

## 13.1 `NPC4`

Similar to `NPC5`, but without 16×16 NPC art.

```text
char[4] tag = "NPC4"
u8 count

repeat:
    u16 x
    u16 y
    u8  color_id
    u8  text_mode
    u8  text_speed
    u8  sprite8[8]
    u8  quest_type
    u16 quest_target
    u8  reward_kind
    u16 reward_amount
    u8  completed
    u8  text_len
    u8  text[text_len]
```

On load, the engine expands the 8×8 sprite into 16×16 art.

## 13.2 `NPC3`

Similar to `NPC4`, but without `text_speed`.

```text
char[4] tag = "NPC3"
u8 count

repeat:
    u16 x
    u16 y
    u8  color_id
    u8  text_mode
    u8  sprite8[8]
    u8  quest_type
    u16 quest_target
    u8  reward_kind
    u16 reward_amount
    u8  completed
    u8  text_len
    u8  text[text_len]
```

On load:

```text
text_speed = TEXT_SPEED_MEDIUM
sprite8 is expanded to sprite16
```

## 13.3 `NPCS`

Very early/simple NPC chunk.

```text
char[4] tag = "NPCS"
u8 count

repeat:
    u16 x
    u16 y
    u8  color_id
    u8  quest_type
    u16 quest_target
    u8  reward_kind
    u16 reward_amount
    u8  completed
    u8  text_len
    u8  text[text_len]
```

On load, default NPC art and default text settings are applied.

---

## 13.4 `ENM4`

Enemy hierarchy/boss chunk before 16×16 enemy art and 32×32 boss art.

```text
char[4] tag = "ENM4"
u8 count

repeat:
    u16 x
    u16 y
    u8  hp
    u8  attack
    u8  color_id
    u8  sprite8[8]
    u8  ai_rank
    u8  spawn_kind
    u8  spawn_limit
    u8  command_range
    u8  ranged_attack
    u16 boss_sprite14[14]
    u8  text_count
    repeat text_count times:
        u8 text_len
        u8 text[text_len]
```

On load:

```text
sprite8 expands to 16x16
boss_sprite14 expands to 32x32
speed_attr = 100
size_pct = 100 or boss default
text_speed = medium
```

## 13.5 `ENM3`

Enemy stat/text chunk with 8×8 sprite art.

```text
char[4] tag = "ENM3"
u8 count

repeat:
    u16 x
    u16 y
    u8  hp
    u8  attack
    u8  color_id
    u8  sprite8[8]
    u8  text_count
    repeat text_count times:
        u8 text_len
        u8 text[text_len]
```

On load, defaults are applied for hierarchy, speed, size, ranged attack, and boss art.

## 13.6 `ENM2`

Older enemy stats/text chunk without sprite or color.

```text
char[4] tag = "ENM2"
u8 count

repeat:
    u16 x
    u16 y
    u8  hp
    u8  attack
    u8  text_count
    repeat text_count times:
        u8 text_len
        u8 text[text_len]
```

On load, color and sprite are generated/defaulted.

## 13.7 `ENMS`

Very early/simple enemy spawn chunk.

```text
char[4] tag = "ENMS"
u8 count

repeat:
    u16 x
    u16 y
    u8  text_count
    repeat text_count times:
        u8 text_len
        u8 text[text_len]
```

On load, HP/attack/color/art defaults are generated.

---

## 13.8 `WEP2`

Named weapon stat chunk without icon/color storage.

```text
char[4] tag = "WEP2"
u8 count

repeat:
    u8 name_len
    u8 name[name_len]
    u8 damage
    u8 range
    u8 cooldown
```

On load, color and icon are defaulted.

## 13.9 `WEAP`

Old weapon stat chunk without names/icons.

```text
char[4] tag = "WEAP"
u8 count

repeat:
    u8 damage
    u8 range
    u8 cooldown
```

On load, names, colors, and icons are defaulted.

---

# 14. External Sidecar Files

The `.bwl` file does not store the slot display name separately in the main map stream.

Slot names are stored in sidecar `.meta` files:

```text
sdmc:/3ds/bwl_slot%d.meta
sdmc:/bwl_slot%d.meta
```

The meta file is plain text containing the sanitized level name.

Settings are also external:

```text
sdmc:/3ds/3dcaster_settings.cfg
sdmc:/3dcaster_settings.cfg
```

Settings contain default NPC color/art, graphics options, and editor/game settings. They are not part of the BWL map format.

---

# 15. Parser Pseudocode

```c
read magic

if magic == "BWL1":
    read BWL1 header
    read raw tiles
    apply metadata defaults

else if magic == "BW2":
    read BW2 header
    decode compressed tiles until EOF
    apply metadata defaults

else if magic == "BW3":
    read BW3 header
    decode compressed tiles using tile_stream_size
    while next tag != "END!":
        if tag == "WLD5": parse world metadata
        else if tag == "NPC5": parse current NPC data
        else if tag == "ENM5": parse current enemy data
        else if tag == "WEP3": parse current weapon data
        else if tag is known legacy chunk: parse and upgrade
        else: stop/fail safely
```

---

# 16. Recommended Future-Proofing

Current BW3 chunks are compact but not very future-proof because chunks do not include lengths.

For a future `BW4`, a safer chunk format would be:

```text
char[4] tag
u32 chunk_size
u8  payload[chunk_size]
```

That would let older versions skip unknown chunks safely.

Recommended future layout:

```text
BW4 header
tile stream
chunks:
    WLD6 size payload
    NPC6 size payload
    ENM6 size payload
    WEP4 size payload
    END! 0
```

This would make adding things like shops, cutscenes, inventory, scripts, or dialogue trees much safer.

---

# 17. Quick Reference

## Current writer output

```text
BW3
tile_stream
WLD5
NPC5
ENM5
WEP3
END!
```

## Current main chunks

| Chunk | Purpose |
|---|---|
| `WLD5` | Player/world metadata |
| `NPC5` | NPCs, quests, rewards, text, 16×16 NPC art |
| `ENM5` | Enemies, bosses, AI hierarchy, stats, text, 16×16/32×32 art |
| `WEP3` | Named weapons, stats, icons |
| `END!` | End of chunk stream |

## Current sprite art sizes

| Entity | Size | Stored As |
|---|---:|---|
| Weapon icon | 8×8 | `u8[8]` |
| Legacy NPC/enemy | 8×8 | `u8[8]` |
| Current NPC | 16×16 | `u16[16]` |
| Current enemy | 16×16 | `u16[16]` |
| Current boss | 32×32 | `u32[32]` |

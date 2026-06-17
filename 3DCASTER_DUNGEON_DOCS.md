# 3DCaster NPC, Enemy, Quest, and Weapon Guide

This guide covers the current NPC, enemy, quest, item reward, and weapon systems in the latest 3DCaster build.

## Tile IDs

The editor uses hexadecimal-style tile IDs from `0` to `F`.

| Tile | Name | Purpose |
|---:|---|---|
| `0` | Empty / Floor | Walkable floor space. |
| `1` | Wall 1 | Solid wall. Usually the default maze wall. |
| `2` | Wall 2 | Solid red wall variant. |
| `3` | Wall 3 | Solid blue wall variant. |
| `4` | Wall 4 | Solid green wall variant. |
| `5` | Wall 5 | Solid yellow/gold wall variant. |
| `6` | Wall 6 | Solid purple wall variant. |
| `7` | Platform | Raised platform / ledge. |
| `8` | Dot / Coin | Collectible worth `1`. |
| `9` | Pink Collectible | Collectible worth `5`. |
| `A` / `10` | Purple Collectible | Collectible worth `10`. |
| `B` / `11` | NPC | NPC marker with text, quests, colors, and rewards. |
| `C` / `12` | Enemy / AI Spawn | Spawns an enemy in Play mode. |
| `D` / `13` | Success Platform | Goal / exit tile. |
| `E` / `14` | Key | Key collectible. |
| `F` / `15` | Door | Door tile. |

## NPC Editing

Place tile `B` in the editor to create an NPC.

### NPC Editor Controls

| Input | Action |
|---|---|
| `A + touch NPC` | Edit NPC overhead/dialog text and exact reward code. |
| `Y + touch NPC` | Cycle NPC quest type. |
| `L + touch NPC` | Cycle NPC color. |
| `R + touch NPC` | Quickly cycle NPC reward type. |

## NPC Text

Use:

```text
A + touch NPC
```

The first keyboard prompt edits the text shown above the NPC's head.

Example NPC text:

```text
BRING ME 5 COINS
```

This text is saved into the `BW3` level metadata.

## NPC Reward Codes

After editing NPC text, the second keyboard prompt edits the reward code.

| Reward Code | Reward |
|---|---|
| `D1` | Spawn 1 normal dot / coin. |
| `D3` | Spawn 3 normal dots / coins. |
| `K1` | Spawn 1 key. |
| `P1` | Spawn 1 pink collectible. |
| `P2` | Spawn 2 pink collectibles. |
| `U1` | Spawn 1 purple collectible. |
| `W0` | Give / spawn sword. |
| `W1` | Give / spawn dagger. |
| `W2` | Give / spawn knife. |
| `W3` | Give / spawn mace. |
| `W4` | Give / spawn mallet. |

Examples:

```text
K1
```

Gives or spawns one key.

```text
P5
```

Gives or spawns five pink collectibles.

```text
W4
```

Gives or spawns a mallet.

## NPC Quest Types

Use:

```text
Y + touch NPC
```

This cycles the NPC's quest type.

| Quest Type | Meaning |
|---|---|
| `READY` / no requirement | NPC can give reward immediately. |
| `COINS` | Player must collect enough coin/score value. |
| `KEYS` | Player must have enough keys. |
| `NPCS` | Player must find/help another NPC. |

In Play mode, interact with a nearby NPC using:

```text
SELECT
```

If the quest is not complete, the status text can show progress such as:

```text
QUEST COINS 2/5
QUEST KEYS 0/1
QUEST NPCS 1/2
```

### Current Quest Target Limitation

The save format supports quest target values, but the editor does not yet have a full exact-target editor.

Current common targets are approximately:

| Quest | Current Target |
|---|---:|
| Coins | `5` |
| Keys | `1` |
| NPCs | `1` |

A future entity editor screen should expose exact quest targets directly.

## Enemy Editing

Place tile `C` in the editor to create an enemy / AI spawn.

### Enemy Text Editing

Use:

```text
A + touch enemy spawn tile
```

Enter enemy text lines separated with `|`.

Example:

```text
HEY YOU|OUCH|GET BACK HERE
```

The enemy can store multiple lines. In Play mode:

- Enemy spots or chases the player: text appears above the enemy.
- Player hits enemy: enemy cycles to the next saved text line.

Example cycle:

```text
HEY YOU
```

Then after one hit:

```text
OUCH
```

Then after another hit:

```text
GET BACK HERE
```

## Enemy Health

Enemies currently have health and show an HP bar above them.

Current behavior:

```text
Enemy HP = automatically generated, usually around 4 to 7 HP
```

Enemy HP exists in gameplay, but exact per-enemy HP is not yet editable through the editor UI.

Suggested future control:

```text
R + touch enemy = edit HP
```

or a full entity editor screen with:

```text
Enemy HP
Enemy text lines
Enemy damage
Enemy behavior
Enemy color/type
```

## Weapons

The player does not start with every weapon. Weapons must be obtained from pickups or NPC rewards.

### Weapon IDs

| Weapon Code | Weapon |
|---|---|
| `W0` | Sword |
| `W1` | Dagger |
| `W2` | Knife |
| `W3` | Mace |
| `W4` | Mallet |

### Play Controls

| Input | Action |
|---|---|
| `X` | Attack. |
| `Y` | Cycle owned weapons. |

### Weapon Stats

Weapon stats are randomized per level/seed and saved into the `BW3` metadata.

General balance:

| Weapon | General Role |
|---|---|
| Sword | Medium damage. |
| Dagger | Lower damage, faster. |
| Knife | Low damage, very fast. |
| Mace | Higher damage. |
| Mallet | Very high damage, slower. |

Weapon damage exists in gameplay, but exact damage editing is not yet exposed in the editor UI.

## Door Behavior

Tile `F` is a door.

Doors are rendered as raycast wall panels. They do not billboard or follow the camera like sprite-style items.

Current door rendering includes panel/frame detail so doors look different from normal walls.

## Collectible Values

| Tile | Collectible | Value |
|---:|---|---:|
| `8` | Dot / Coin | `1` |
| `9` | Pink Collectible | `5` |
| `A` | Purple Collectible | `10` |
| `E` | Key | Key count item |

## What Can Be Edited Right Now?

| Feature | Editor Supported? | How |
|---|---:|---|
| NPC text | Yes | `A + touch NPC` |
| NPC reward code | Yes | `A + touch NPC` |
| NPC quick reward cycle | Yes | `R + touch NPC` |
| NPC quest type | Yes | `Y + touch NPC` |
| NPC color | Yes | `L + touch NPC` |
| Enemy text | Yes | `A + touch C tile` |
| Enemy HP | Partially | Exists, but auto-generated. |
| Weapon damage | Partially | Exists, randomized and saved. |
| Exact quest target | Partially | Save format supports it, editor UI does not yet expose it cleanly. |

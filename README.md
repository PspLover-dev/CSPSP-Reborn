# CS 2D PSP

Top-down Counter-Strike style homebrew for PlayStation Portable.

This project is a PSP remake inspired by **[CSPSP](https://github.com/kevinbchen/cspsp)** by [kevinbchen](https://github.com/kevinbchen) — the original CS 2D homebrew. In-game players, guns, tiles, muzzle flashes, and grenade behaviour follow that game. Maps include layouts converted from the original CSPSP pack (Dust2, Office, Iceworld, and others).

## Play on a PSP / PPSSPP

A ready-to-run package is **`CSPSP-PSP.zip`**.

1. Extract the zip. You should get a `CSPSP` folder.
2. Copy it to `ms0:/PSP/GAME/CSPSP/` (Memory Stick) or, in PPSSPP, to `PSP/GAME/CSPSP/`.
3. The folder must contain `EBOOT.PBP`, `Gfx/`, and `maps/`.
4. Launch **CS 2D PSP** from the XMB / PPSSPP game list.

Custom Firmware (or PPSSPP) is required. Adhoc multiplayer needs WLAN enabled on real hardware.

## Game modes

Solo and Adhoc multiplayer share the same five modes. After the map, pick a mode, then Easy / Medium / Hard. Modes with teams also let you pick your team’s skin (8 looks from the original `players.png`; each team gets a unique skin).

### Normal

Classic team deathmatch. Bots fill the remaining slots. You fight with allies against the other teams. Players respawn after death. Team count depends on map size (Left/Right on the mode screen, Select in a host lobby):

| Map size | Grid | Players | Teams |
|---|---|---|---|
| Small | 50×50 | 10 | 2 |
| Medium | 100×100 | 18 | 2–3 |
| Big | 200×200 | 26 | 2–4 |
| Extra / large | 500×500 | 34 | 2–6 |

### Zombie Survival

Humans (your side + allied bots) versus waves of zombies. Zombies spawn around the map edges, off-screen, and never on your spawn. Survive as long as you can. No extra team picker — everyone human shares one skin.

### Seul contre tous

You versus every bot. No teams. In multiplayer, humans fight the bots together. The round ends when you die, or when no enemy is left. Each fighter gets a unique skin (duplicates only if there are more than 8 people).

### Last Survivor

Free-for-all. No teams, no respawn. Last player or bot standing wins.

### Protect the Base

Each team has a base tile with a long health bar. Destroy every enemy base; the last base standing wins. You respawn at your own base (not if it is already down). HE grenades deal heavy damage to bases. Same team counts as Normal.

## Controls

| Input | Action |
|---|---|
| Analog | Move and face |
| R / Cross | Fire (camera shakes) |
| Square | Reload |
| Triangle | Next weapon |
| L | Previous weapon |
| Start | Pause |
| Circle | Back / erase in the editor |
| D-Pad | Menus, team count, skin grid |

## Multiplayer

One PSP hosts an Adhoc party (name up to 8 characters). Others **Join scan** or **Enter server**. Max occupancy follows the map (up to 34). Host picks map, mode, and (when the mode has teams) how many teams.

## Map editor

Paint tiles on small / medium / big / extra grids. Analog pans on large maps. Start saves under `maps/saved_*.csp`. Saved maps are kept when the 100 stock maps are regenerated.

## Weapons

The original CSPSP arsenal: knife, pistols, shotguns, SMGs, rifles, snipers, M249, plus **HE**, **flashbang**, and **smoke** (fuse and `particles.png` GFX match the original). Dead players drop guns as loot.

## Build from source

[pspdev](https://github.com/pspdev/pspdev) is expected at `/Users/Camille/pspdev` (or set `PSPDEV`).

```bash
export PSPDEV=/Users/Camille/pspdev
export PATH="$PSPDEV/bin:$PATH"
make
```

That writes `build/EBOOT.PBP` and copies `Gfx/` + `maps/` next to it. To rebuild the 100 stock maps:

```bash
make maps
```

## Credits

- **Original game, sprites, guns, tiles, grenades, and several map layouts** — [kevinbchen/cspsp](https://github.com/kevinbchen/cspsp) (CSPSP). This homebrew would not exist without that project.
- **Menus and zombie sprites** — [Kenney.nl Top-down Shooter](https://kenney.nl/assets/top-down-shooter) (CC0).
- **Font** — [Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) (OFL).

See `Gfx/CREDITS.txt`.

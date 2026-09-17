#!/usr/bin/env python3
"""Generate 100 maps inspired by original CSPSP layouts: corridors, open yards, lanes, arenas."""

from __future__ import annotations

import math
import os
import random
from collections import deque

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MAPS = os.path.join(ROOT, "maps")

SIZES = {
    "small": (50, 50),
    "medium": (100, 100),
    "big": (200, 200),
    "extra": (500, 500),
}

THEMES = [
    "dust",
    "office",
    "inferno",
    "nuke",
    "aztec",
    "italy",
    "warehouse",
    "mill",
    "cache",
    "vertigo",
]

# Layout styles inspired by original CSPSP maps (dust2 lanes, office halls, fy arenas, etc.)
STYLES = ["corridors", "open", "lanes", "compound", "arena", "tunnels", "sites", "sprawl"]

STYLE_NAMES = {
    "corridors": ["Office", "Labyrinth", "Halls", "Complex"],
    "open": ["Dust", "Yard", "Plaza", "Plateau"],
    "lanes": ["Assault", "Dust", "Alley", "Long"],
    "compound": ["Militia", "Compound", "Village", "Italy"],
    "arena": ["Iceworld", "Pool", "Pit", "Cage"],
    "tunnels": ["Nuke", "Silo", "Bunker", "Vault"],
    "sites": ["Inferno", "Cache", "Station", "Harbor"],
    "sprawl": ["Warehouse", "Mill", "Train", "Docks"],
}


def inb(g, x, y) -> bool:
    return 0 <= y < len(g) and 0 <= x < len(g[0])


def setf(g, x, y, ch=".") -> None:
    if inb(g, x, y) and 0 < x < len(g[0]) - 1 and 0 < y < len(g) - 1:
        if g[y][x] in ("#", "."):
            g[y][x] = ch


def force_floor(g, x, y) -> None:
    if inb(g, x, y) and 0 < x < len(g[0]) - 1 and 0 < y < len(g) - 1:
        if g[y][x] not in ("T", "O", "A", "X"):
            g[y][x] = "."


def hall_width(w: int) -> int:
    if w >= 400:
        return 7
    if w >= 200:
        return 5
    if w >= 100:
        return 3
    return 2


def stamp(g, x, y, rad: int, ch=".") -> None:
    for oy in range(-rad, rad + 1):
        for ox in range(-rad, rad + 1):
            setf(g, x + ox, y + oy, ch)


def stamp_floor(g, x, y, rad: int) -> None:
    r2 = rad * rad
    for oy in range(-rad, rad + 1):
        for ox in range(-rad, rad + 1):
            if ox * ox + oy * oy <= r2:
                force_floor(g, x + ox, y + oy)


def rect(g, x, y, w, h, ch=".") -> None:
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            setf(g, xx, yy, ch)


def corridor(g, x0, y0, x1, y1, width: int, rng: random.Random) -> None:
    x, y = x0, y0
    rad = max(0, width // 2)
    horiz = rng.random() < 0.5

    def step_x():
        nonlocal x
        while x != x1:
            stamp(g, x, y, rad)
            x += 1 if x1 > x else -1
        stamp(g, x, y, rad)

    def step_y():
        nonlocal y
        while y != y1:
            stamp(g, x, y, rad)
            y += 1 if y1 > y else -1
        stamp(g, x, y, rad)

    if horiz:
        step_x()
        step_y()
    else:
        step_y()
        step_x()


def scatter_cover(g, rng: random.Random, theme: str, density: float) -> None:
    h, w = len(g), len(g[0])
    n = int(w * h * density)
    palette = ["C", "C", "B", "S"]
    if theme in ("dust", "aztec", "italy", "mill", "inferno"):
        palette.append("R")
    for _ in range(n):
        x = rng.randint(2, w - 3)
        y = rng.randint(2, h - 3)
        if g[y][x] == ".":
            g[y][x] = rng.choice(palette)


def place_mark(g, x, y, ch: str) -> None:
    if inb(g, x, y) and g[y][x] == ".":
        g[y][x] = ch


def spawn_cluster(g, cx, cy, mark: str, count: int, rng: random.Random) -> None:
    placed = 0
    for r in range(1, 18):
        for _ in range(count * 4):
            x = cx + rng.randint(-r, r)
            y = cy + rng.randint(-r, r)
            if inb(g, x, y) and g[y][x] == ".":
                g[y][x] = mark
                placed += 1
                if placed >= count:
                    return


def flood_floor(g, x, y) -> set[tuple[int, int]]:
    h, w = len(g), len(g[0])
    seen = set()
    q = deque([(x, y)])
    while q:
        px, py = q.popleft()
        if not (0 <= px < w and 0 <= py < h) or (px, py) in seen:
            continue
        if g[py][px] == "#":
            continue
        seen.add((px, py))
        q.extend(((px + 1, py), (px - 1, py), (px, py + 1), (px, py - 1)))
    return seen


def team_pad_centers(w: int, h: int, n: int = 6) -> list[tuple[int, int]]:
    mid_x, mid_y = w * 0.5, h * 0.5
    rx = max(5.0, w * 0.36)
    ry = max(5.0, h * 0.36)
    pts = []
    for i in range(n):
        ang = (math.pi * 2.0 * i) / n - math.pi * 0.5
        x = int(round(mid_x + math.cos(ang) * rx))
        y = int(round(mid_y + math.sin(ang) * ry))
        x = max(4, min(w - 5, x))
        y = max(4, min(h - 5, y))
        pts.append((x, y))
    return pts


def carve_team_pads(g, rng: random.Random) -> list[tuple[int, int]]:
    """Open plazas where Protect-the-Base homes sit, linked by wide lanes (dust2-style)."""
    h, w = len(g), len(g[0])
    n = 6 if w >= 200 else 4
    rad = 8 if w >= 400 else (6 if w >= 200 else 4)
    lane = hall_width(w)
    pts = team_pad_centers(w, h, n)
    mid = (w // 2, h // 2)
    stamp_floor(g, mid[0], mid[1], rad)
    for x, y in pts:
        stamp_floor(g, x, y, rad)
        corridor(g, x, y, mid[0], mid[1], lane, rng)
        stamp_floor(g, x, y, rad)
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        corridor(g, a[0], a[1], b[0], b[1], lane, rng)
        stamp_floor(g, a[0], a[1], max(3, rad - 2))
    return pts


def polish_map(g, rng: random.Random, reconnect: bool = True) -> None:
    h, w = len(g), len(g[0])
    if w >= 200:
        pts = carve_team_pads(g, rng)
        if pts:
            place_mark(g, pts[0][0], pts[0][1], "A")
            if len(pts) > 1:
                place_mark(g, pts[1][0], pts[1][1], "X")
    elif reconnect:
        connect_all(g, rng, hall_width(w))
    for x in range(w):
        g[0][x] = g[h - 1][x] = "#"
    for y in range(h):
        g[y][0] = g[y][w - 1] = "#"


def connect_all(g, rng: random.Random, width: int) -> None:
    h, w = len(g), len(g[0])
    seeds = [(x, y) for y in range(h) for x in range(w) if g[y][x] != "#"]
    if not seeds:
        return
    start = seeds[0]
    reached = flood_floor(g, *start)
    leftover = [p for p in seeds if p not in reached]
    rng.shuffle(leftover)
    cap = 80 if w >= 400 else (200 if w >= 200 else 400)
    for px, py in leftover[:cap]:
        if (px, py) in reached:
            continue
        tx, ty = rng.choice(tuple(reached) if len(reached) < 8000 else [start, seeds[len(seeds) // 2]])
        corridor(g, px, py, tx, ty, width, rng)
        reached = flood_floor(g, *start)


def gen_corridors(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """Tight office-like halls (cs_office / original indoor maps)."""
    h, w = len(g), len(g[0])
    hall = hall_width(w)
    cols = 4 if w < 80 else (6 if w < 180 else (8 if w < 400 else 10))
    rows = 4 if h < 80 else (6 if h < 180 else 10)
    rooms = []
    cw, ch = (w - 4) // cols, (h - 4) // rows
    for gy in range(rows):
        for gx in range(cols):
            rw = rng.randint(max(4, cw // 3), max(5, cw - 2))
            rh = rng.randint(max(4, ch // 3), max(5, ch - 2))
            rx = 2 + gx * cw + rng.randint(0, max(0, cw - rw))
            ry = 2 + gy * ch + rng.randint(0, max(0, ch - rh))
            rect(g, rx, ry, rw, rh)
            rooms.append((rx + rw // 2, ry + rh // 2))
            if rng.random() < 0.35:
                # extra dead-end alcove
                corridor(g, rooms[-1][0], rooms[-1][1], rooms[-1][0] + rng.choice([-8, 8]), rooms[-1][1], hall, rng)
    for i in range(len(rooms) - 1):
        corridor(g, *rooms[i], *rooms[i + 1], hall, rng)
    for _ in range(len(rooms) // 3):
        a, b = rng.sample(rooms, 2)
        corridor(g, *a, *b, hall, rng)
    scatter_cover(g, rng, theme, 0.012)
    return rooms[0][0], rooms[0][1], rooms[-1][0], rooms[-1][1]


def gen_open(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """Dust-like open ground with wall clusters."""
    h, w = len(g), len(g[0])
    rect(g, 1, 1, w - 2, h - 2)
    blocks = 8 if w < 80 else (14 if w < 180 else (22 if w < 400 else 28))
    for _ in range(blocks):
        bw = rng.randint(3, 8 if w < 80 else 16)
        bh = rng.randint(3, 8 if h < 80 else 16)
        bx = rng.randint(4, w - bw - 4)
        by = rng.randint(4, h - bh - 4)
        for yy in range(by, by + bh):
            for xx in range(bx, bx + bw):
                if inb(g, xx, yy):
                    g[yy][xx] = "#"
        # punch a hole so it doesn't block the whole map
        if rng.random() < 0.6:
            rect(g, bx + 1, by + bh // 2, bw - 2, 1)
    scatter_cover(g, rng, theme, 0.02)
    return 4, 4, w - 5, h - 5


def gen_lanes(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """Long CS lanes like de_dust / de_dust2."""
    h, w = len(g), len(g[0])
    if w >= 200:
        rect(g, 1, 1, w - 2, h - 2)
        band = 6 if w >= 400 else 4
        for y in (h // 4, h // 2, 3 * h // 4):
            for x in range(2, w - 2):
                if abs((x % 28) - 14) > 5:
                    if inb(g, x, y):
                        g[y][x] = "#"
                    for oy in range(1, band):
                        if rng.random() < 0.7 and inb(g, x, y + oy - band // 2):
                            g[y + oy - band // 2][x] = "#"
        for x in (w // 4, w // 2, 3 * w // 4):
            gap = h // 6
            for y in range(2, h - 2):
                if abs((y % (gap * 2)) - gap) > gap // 3:
                    if inb(g, x, y):
                        g[y][x] = "#"
        scatter_cover(g, rng, theme, 0.012)
        return 5, h // 2, w - 6, h // 2
    lane_w = hall_width(w)
    n = 3 if w < 120 else 4
    ys = [h * (i + 1) // (n + 1) for i in range(n)]
    for y in ys:
        rect(g, 2, y - lane_w // 2, w - 4, lane_w)
    xs = [w // 4, w // 2, 3 * w // 4]
    for x in xs:
        rect(g, x - lane_w // 2, min(ys), lane_w, max(ys) - min(ys) + lane_w)
    for y in ys:
        for x in (6, w - 10):
            if rng.random() < 0.8:
                rect(g, x, y - 4, 6, 8)
    scatter_cover(g, rng, theme, 0.015)
    return 5, ys[0], w - 6, ys[-1]


def gen_compound(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """Buildings in a yard (militia / italy)."""
    h, w = len(g), len(g[0])
    rect(g, 1, 1, w - 2, h - 2)
    n = 5 if w < 80 else (9 if w < 180 else 16)
    buildings = []
    for _ in range(n):
        bw = rng.randint(8, 14 if w < 80 else 22)
        bh = rng.randint(8, 14 if h < 80 else 22)
        bx = rng.randint(3, max(4, w - bw - 3))
        by = rng.randint(3, max(4, h - bh - 3))
        # walls
        for yy in range(by, by + bh):
            for xx in range(bx, bx + bw):
                if xx == bx or yy == by or xx == bx + bw - 1 or yy == by + bh - 1:
                    if inb(g, xx, yy):
                        g[yy][xx] = "#"
        # doors
        for _ in range(rng.randint(1, 3)):
            if rng.random() < 0.5:
                setf(g, bx + rng.randint(1, bw - 2), by if rng.random() < 0.5 else by + bh - 1, "D")
                stamp(g, bx + bw // 2, by if rng.random() < 0.5 else by + bh - 1, 0, ".")
            else:
                stamp(g, bx if rng.random() < 0.5 else bx + bw - 1, by + bh // 2, 0, ".")
        buildings.append((bx + bw // 2, by + bh // 2))
    scatter_cover(g, rng, theme, 0.018)
    return 4, 4, w - 5, h - 5


def gen_arena(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """fy_ iceworld / dodgeball style."""
    h, w = len(g), len(g[0])
    pad = 3
    rect(g, pad, pad, w - pad * 2, h - pad * 2)
    # inner ring or pillars
    if rng.random() < 0.5:
        for i in range(4, w - 4, max(6, w // 8)):
            for j in range(4, h - 4, max(6, h // 8)):
                if rng.random() < 0.55:
                    g[j][i] = "#"
                    if rng.random() < 0.4:
                        stamp(g, i, j, 1, "#")
    else:
        rw, rh = w // 3, h // 3
        for yy in range(h // 2 - rh // 2, h // 2 + rh // 2):
            for xx in range(w // 2 - rw // 2, w // 2 + rw // 2):
                if xx in (w // 2 - rw // 2, w // 2 + rw // 2 - 1) or yy in (
                    h // 2 - rh // 2,
                    h // 2 + rh // 2 - 1,
                ):
                    if inb(g, xx, yy):
                        g[yy][xx] = "#"
        stamp(g, w // 2 - rw // 2, h // 2, 1, ".")
        stamp(g, w // 2 + rw // 2 - 1, h // 2, 1, ".")
    scatter_cover(g, rng, theme, 0.025)
    return pad + 2, h // 2, w - pad - 3, h // 2


def gen_tunnels(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """Nuke / bunker tunnels with occasional rooms."""
    h, w = len(g), len(g[0])
    width = hall_width(w)
    x, y = 3, h // 2
    rooms = [(x, y)]
    stamp(g, x, y, width)
    steps = w * 2 if w < 200 else w
    for _ in range(steps):
        dir = rng.choice([(1, 0), (-1, 0), (0, 1), (0, -1), (1, 0), (1, 0)])
        nx, ny = x + dir[0] * rng.randint(2, 6), y + dir[1] * rng.randint(2, 6)
        nx = max(2, min(w - 3, nx))
        ny = max(2, min(h - 3, ny))
        corridor(g, x, y, nx, ny, width, rng)
        x, y = nx, ny
        if rng.random() < 0.08:
            rect(g, x - 3, y - 3, 7, 7)
            rooms.append((x, y))
    scatter_cover(g, rng, theme, 0.01)
    return rooms[0][0], rooms[0][1], x, y


def gen_sites(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """Classic two-site layout: T spawn, mid, A/B, CT spawn."""
    h, w = len(g), len(g[0])
    hall = hall_width(w)
    if w >= 200:
        rect(g, 1, 1, w - 2, h - 2)
        t = (8, h // 2)
        ct = (w - 9, h // 2)
        site_a = (w // 2, 8)
        site_b = (w // 2, h - 9)
        mid = (w // 2, h // 2)
        for cx, cy, rw, rh in (
            (t[0], t[1], 14, 16),
            (ct[0], ct[1], 14, 16),
            (site_a[0], site_a[1], 16, 14),
            (site_b[0], site_b[1], 16, 14),
            (mid[0], mid[1], 14, 14),
        ):
            # wall ring around each site, doors punched
            for yy in range(cy - rh // 2, cy + rh // 2):
                for xx in range(cx - rw // 2, cx + rw // 2):
                    if xx in (cx - rw // 2, cx + rw // 2 - 1) or yy in (cy - rh // 2, cy + rh // 2 - 1):
                        if inb(g, xx, yy):
                            g[yy][xx] = "#"
            stamp_floor(g, cx, cy - rh // 2, 2)
            stamp_floor(g, cx, cy + rh // 2 - 1, 2)
        place_mark(g, site_a[0], site_a[1], "A")
        place_mark(g, site_b[0], site_b[1], "X")
        scatter_cover(g, rng, theme, 0.012)
        return t[0], t[1], ct[0], ct[1]
    t = (4, h // 2)
    ct = (w - 5, h // 2)
    site_a = (w // 2, 5)
    site_b = (w // 2, h - 6)
    mid = (w // 2, h // 2)
    for cx, cy, rw, rh in (
        (t[0], t[1], 8, 10),
        (ct[0], ct[1], 8, 10),
        (site_a[0], site_a[1], 10, 8),
        (site_b[0], site_b[1], 10, 8),
        (mid[0], mid[1], 8, 8),
    ):
        rect(g, cx - rw // 2, cy - rh // 2, rw, rh)
    corridor(g, *t, *mid, hall, rng)
    corridor(g, *ct, *mid, hall, rng)
    corridor(g, *mid, *site_a, hall, rng)
    corridor(g, *mid, *site_b, hall, rng)
    corridor(g, *t, *site_a, hall, rng)
    corridor(g, *ct, *site_b, hall, rng)
    if rng.random() < 0.7:
        corridor(g, *t, *site_b, hall, rng)
        corridor(g, *ct, *site_a, hall, rng)
    place_mark(g, site_a[0], site_a[1], "A")
    place_mark(g, site_b[0], site_b[1], "X")
    scatter_cover(g, rng, theme, 0.014)
    return t[0], t[1], ct[0], ct[1]


def gen_sprawl(g, rng: random.Random, theme: str) -> tuple[int, int, int, int]:
    """Wide rooms and broad halls (warehouse / mill)."""
    h, w = len(g), len(g[0])
    hall = max(4, hall_width(w))
    cols = 3 if w < 80 else (4 if w < 180 else (5 if w < 400 else 6))
    rows = 3 if h < 80 else (4 if h < 180 else 6)
    rooms = []
    cw, ch = (w - 4) // cols, (h - 4) // rows
    for gy in range(rows):
        for gx in range(cols):
            rw = rng.randint(max(8, cw - 6), max(9, cw - 1))
            rh = rng.randint(max(8, ch - 6), max(9, ch - 1))
            rx = 2 + gx * cw + rng.randint(0, max(0, cw - rw))
            ry = 2 + gy * ch + rng.randint(0, max(0, ch - rh))
            rect(g, rx, ry, rw, rh)
            rooms.append((rx + rw // 2, ry + rh // 2, rx, ry, rw, rh))
    for i, a in enumerate(rooms):
        if i + 1 < len(rooms):
            b = rooms[i + 1]
            corridor(g, a[0], a[1], b[0], b[1], hall, rng)
    for gy in range(rows):
        for gx in range(cols - 1):
            i = gy * cols + gx
            j = i + 1
            if j < len(rooms):
                corridor(g, rooms[i][0], rooms[i][1], rooms[j][0], rooms[j][1], hall, rng)
    scatter_cover(g, rng, theme, 0.016)
    return rooms[0][0], rooms[0][1], rooms[-1][0], rooms[-1][1]


GENERATORS = {
    "corridors": gen_corridors,
    "open": gen_open,
    "lanes": gen_lanes,
    "compound": gen_compound,
    "arena": gen_arena,
    "tunnels": gen_tunnels,
    "sites": gen_sites,
    "sprawl": gen_sprawl,
}

ORIG_MAPS = [
    ("de_dust2", "dust", "Dust2"),
    ("Office2D", "office", "Office"),
    ("iceworld", "cache", "Iceworld"),
    ("fy_dodgeball", "vertigo", "Dodgeball"),
    ("fy_nade", "inferno", "Nade"),
    ("fy_poolday", "aztec", "Poolday"),
    ("CastleVonbrown", "italy", "Castle"),
    ("Circle", "warehouse", "Circle"),
    ("Guano", "mill", "Guano"),
    ("Silent_Forest", "aztec", "Silent Forest"),
    ("Small_Forest", "aztec", "Small Forest"),
    ("Small_Train_Yard", "warehouse", "Train Yard"),
    ("lasertag", "nuke", "Lasertag"),
    ("lasertag2", "nuke", "Lasertag 2"),
    ("louismap", "office", "Louis"),
    ("river", "italy", "River"),
    ("winter", "cache", "Winter"),
]

ORIG_URL = "https://raw.githubusercontent.com/kevinbchen/cspsp/master/jge/Projects/cspsp/bin/maps/{name}/map.txt"


def parse_block(text: str, key: str) -> str:
    import re

    m = re.search(rf"{key}\s*\{{(.*?)\}}", text, re.S | re.I)
    return m.group(1) if m else ""


def parse_polys(text: str, key: str) -> list[list[tuple[float, float]]]:
    import re

    body = parse_block(text, key)
    polys: list[list[tuple[float, float]]] = []
    for line in body.splitlines():
        pts = [(float(a), float(b)) for a, b in re.findall(r"\(\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\)", line)]
        if len(pts) >= 3:
            polys.append(pts)
    return polys


def parse_tiles(text: str) -> list[list[int]]:
    body = parse_block(text, "tiles")
    rows: list[list[int]] = []
    for line in body.splitlines():
        line = line.strip().rstrip(",")
        if not line:
            continue
        vals = [int(x) for x in line.split(",") if x.strip() != ""]
        if vals:
            rows.append(vals)
    return rows


def parse_spawns(text: str) -> tuple[list[tuple[float, float]], list[tuple[float, float]]]:
    import re

    body = parse_block(text, "spawns")
    t, ct = [], []
    for team, x, y in re.findall(r"\(\s*(\d+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\)", body):
        p = (float(x) / 32.0, float(y) / 32.0)
        (t if team == "0" else ct).append(p)
    return t, ct


def pip(x: float, y: float, poly: list[tuple[float, float]]) -> bool:
    n = len(poly)
    inside = False
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y):
            xint = (xj - xi) * (y - yi) / ((yj - yi) if yj != yi else 1e-9) + xi
            if x < xint:
                inside = not inside
        j = i
    return inside


def rasterize_orig(text: str) -> tuple[list[list[str]], list[tuple[float, float]], list[tuple[float, float]]]:
    tiles = parse_tiles(text)
    if not tiles or not tiles[0]:
        return [], [], []
    h, w = len(tiles), max(len(r) for r in tiles)
    for row in tiles:
        while len(row) < w:
            row.append(row[-1] if row else 1)
    polys = parse_polys(text, "collision")
    people = parse_polys(text, "collisionPeople")
    g = [["#" for _ in range(w)] for _ in range(h)]
    if polys:
        outer, inners = polys[0], polys[1:] + people
        for y in range(h):
            for x in range(w):
                cx, cy = x * 32 + 16, y * 32 + 16
                if pip(cx, cy, outer) and not any(pip(cx, cy, p) for p in inners):
                    g[y][x] = "."
        floors = sum(1 for row in g for ch in row if ch == ".")
        if floors < w * h * 0.05:
            # winding may be inverted — treat outside-of-inners as floor inside bbox
            g = [["." for _ in range(w)] for _ in range(h)]
            for poly in polys + people:
                for y in range(h):
                    for x in range(w):
                        if pip(x * 32 + 16, y * 32 + 16, poly):
                            # edges of collision polys are walls
                            pass
            # fill using tile ids instead
            border = [tiles[0][x] for x in range(w)] + [tiles[h - 1][x] for x in range(w)]
            border += [tiles[y][0] for y in range(h)] + [tiles[y][w - 1] for y in range(h)]
            wall_id = max(set(border), key=border.count)
            for y in range(h):
                for x in range(w):
                    g[y][x] = "#" if tiles[y][x] == wall_id else "."
    else:
        border = [tiles[0][x] for x in range(w)] + [tiles[h - 1][x] for x in range(w)]
        border += [tiles[y][0] for y in range(h)] + [tiles[y][w - 1] for y in range(h)]
        wall_id = max(set(border), key=border.count)
        for y in range(h):
            for x in range(w):
                g[y][x] = "#" if tiles[y][x] == wall_id else "."
    for x in range(w):
        g[0][x] = g[h - 1][x] = "#"
    for y in range(h):
        g[y][0] = g[y][w - 1] = "#"
    return g, *parse_spawns(text)


def fit_grid(g: list[list[str]], tw: int, th: int) -> list[list[str]]:
    oh, ow = len(g), len(g[0])
    out = [["#" for _ in range(tw)] for _ in range(th)]
    for y in range(th):
        for x in range(tw):
            sx = min(ow - 1, int(x * ow / tw))
            sy = min(oh - 1, int(y * oh / th))
            out[y][x] = g[sy][sx]
    for x in range(tw):
        out[0][x] = out[th - 1][x] = "#"
    for y in range(th):
        out[y][0] = out[y][tw - 1] = "#"
    return out


def fit_pts(pts: list[tuple[float, float]], ow: int, oh: int, tw: int, th: int) -> list[tuple[int, int]]:
    out = []
    for x, y in pts:
        nx = x * tw / max(1, ow)
        ny = y * th / max(1, oh)
        out.append((int(round(nx)), int(round(ny))))
    return out


def flip_grid(g: list[list[str]], mode: str) -> list[list[str]]:
    if mode == "h":
        return [row[::-1] for row in g]
    if mode == "v":
        return g[::-1]
    if mode == "hv":
        return [row[::-1] for row in g][::-1]
    return g


def flip_pts(pts: list[tuple[int, int]], w: int, h: int, mode: str) -> list[tuple[int, int]]:
    out = []
    for x, y in pts:
        if "h" in mode:
            x = w - 1 - x
        if "v" in mode:
            y = h - 1 - y
        out.append((x, y))
    return out


def dump_csp(g, theme: str, size_name: str, title: str, tpts, ctpts, rng: random.Random) -> str:
    h, w = len(g), len(g[0])
    polish_map(g, rng, reconnect=False)
    nsp = 4 if size_name == "small" else (6 if size_name == "medium" else (8 if size_name == "big" else 10))
    floors = [(x, y) for y in range(h) for x in range(w) if g[y][x] == "."]
    for mark, pts in (("T", tpts), ("O", ctpts)):
        placed = 0
        for x, y in pts:
            if 0 <= x < w and 0 <= y < h and g[y][x] != "#":
                g[y][x] = mark
                placed += 1
        if placed < nsp and floors:
            cx, cy = (pts[0] if pts else floors[0])
            spawn_cluster(g, int(cx), int(cy), mark, nsp - placed, rng)
    if not any(ch == "A" for row in g for ch in row):
        place_mark(g, w // 3, h // 3, "A")
        place_mark(g, 2 * w // 3, 2 * h // 3, "X")
    body = "\n".join("".join(row) for row in g)
    return (
        f"CSPSP 1\nname {title}\ntheme {theme}\nsize {size_name}\nw {w}\nh {h}\n{body}\n"
    )


def fetch_orig(name: str) -> str:
    import urllib.request

    cache = os.path.join(ROOT, "tools", "orig_maps")
    os.makedirs(cache, exist_ok=True)
    path = os.path.join(cache, name + ".txt")
    if os.path.isfile(path) and os.path.getsize(path) > 100:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    url = ORIG_URL.format(name=name)
    req = urllib.request.Request(url, headers={"User-Agent": "CSPSP-mapgen"})
    with urllib.request.urlopen(req, timeout=20) as r:
        data = r.read().decode("utf-8", "replace")
    with open(path, "w", encoding="utf-8") as f:
        f.write(data)
    return data


def generate(seed: int, size_name: str, theme: str, style: str, title: str) -> str:
    rng = random.Random(seed)
    w, h = SIZES[size_name]
    g = [["#" for _ in range(w)] for _ in range(h)]
    gen = GENERATORS[style]
    tx, ty, cx, cy = gen(g, rng, theme)
    polish_map(g, rng)

    spawns = 4 if size_name == "small" else (6 if size_name == "medium" else (8 if size_name == "big" else 10))
    spawn_cluster(g, tx, ty, "T", spawns, rng)
    spawn_cluster(g, cx, cy, "O", spawns, rng)

    has_a = any(ch == "A" for row in g for ch in row)
    if not has_a:
        place_mark(g, w // 3, h // 3, "A")
        place_mark(g, 2 * w // 3, 2 * h // 3, "X")

    if theme in ("aztec", "italy") and rng.random() < 0.4:
        for _ in range(w // 8):
            x, y = rng.randint(2, w - 3), rng.randint(2, h - 3)
            if g[y][x] == ".":
                g[y][x] = "~"

    body = "\n".join("".join(row) for row in g)
    return (
        f"CSPSP 1\nname {title}\ntheme {theme}\nsize {size_name}\nw {w}\nh {h}\n{body}\n"
    )


def main() -> None:
    os.makedirs(MAPS, exist_ok=True)
    for name in os.listdir(MAPS):
        if name.endswith(".csp") and not name.startswith("saved"):
            os.remove(os.path.join(MAPS, name))

    orig_data: dict[str, tuple] = {}
    for folder, theme, title in ORIG_MAPS:
        try:
            text = fetch_orig(folder)
            g, tpts, ctpts = rasterize_orig(text)
            if g:
                orig_data[folder] = (g, tpts, ctpts, theme, title)
                print("Loaded original", folder, f"{len(g[0])}x{len(g)}")
        except Exception as e:
            print("Skip original", folder, e)

    index = []
    n = 0

    def emit(g, theme, size_name, title, tpts, ctpts, tag: str) -> None:
        nonlocal n
        n += 1
        rng = random.Random(4000 + n * 17)
        data = dump_csp([row[:] for row in g], theme, size_name, title, tpts, ctpts, rng)
        fname = f"{n:03d}_{tag}_{size_name}.csp"
        with open(os.path.join(MAPS, fname), "w", encoding="utf-8") as f:
            f.write(data)
        index.append(f"{fname}\t{size_name}\t{theme}\t{title}")

    # Original layouts at small + medium, plus mirrors
    for folder, (g, tpts, ctpts, theme, title) in orig_data.items():
        oh, ow = len(g), len(g[0])
        for size_name, suffix in (("small", ""), ("medium", " II")):
            tw, th = SIZES[size_name]
            gg = fit_grid(g, tw, th)
            tp = fit_pts(tpts, ow, oh, tw, th)
            cp = fit_pts(ctpts, ow, oh, tw, th)
            emit(gg, theme, size_name, title + suffix, tp, cp, folder.lower())
        # flipped small
        tw, th = SIZES["small"]
        gg = flip_grid(fit_grid(g, tw, th), "h")
        tp = flip_pts(fit_pts(tpts, ow, oh, tw, th), tw, th, "h")
        cp = flip_pts(fit_pts(ctpts, ow, oh, tw, th), tw, th, "h")
        emit(gg, theme, "small", title + " Mirror", tp, cp, folder.lower() + "_m")

    # Open original layouts scaled to big (dust2 lanes, fy arenas) — no thin corridors
    open_orig = {
        "de_dust2",
        "iceworld",
        "fy_dodgeball",
        "fy_poolday",
        "Circle",
        "Small_Train_Yard",
    }
    for folder, (g, tpts, ctpts, theme, title) in orig_data.items():
        if folder not in open_orig:
            continue
        oh, ow = len(g), len(g[0])
        tw, th = SIZES["big"]
        gg = fit_grid(g, tw, th)
        tp = fit_pts(tpts, ow, oh, tw, th)
        cp = fit_pts(ctpts, ow, oh, tw, th)
        emit(gg, theme, "big", title + " Large", tp, cp, folder.lower() + "_l")

    # Remaining slots: mostly big/extra open layouts (originals already fill small/medium)
    rest = 100 - n
    n_extra = max(8, rest // 2)
    n_big = rest - n_extra
    seq = ["big"] * n_big + ["extra"] * n_extra
    open_styles = ["open", "lanes", "compound", "arena", "sprawl"]
    for i, size_name in enumerate(seq):
        n += 1
        theme = THEMES[(n * 3) % len(THEMES)]
        style = STYLES[(n * 5 + i) % len(STYLES)]
        if size_name == "extra":
            style = open_styles[(n + i) % len(open_styles)]
        elif size_name == "big":
            if style in ("corridors", "tunnels"):
                style = open_styles[(n + i) % len(open_styles)]
        if theme == "dust" and size_name != "extra" and n % 3 == 0:
            style = "lanes" if n % 2 else "open"
        label = "Extra" if size_name == "extra" else size_name.title()
        base = STYLE_NAMES[style][n % len(STYLE_NAMES[style])]
        title = f"{base} {label} {i + 1:02d}"
        data = generate(2000 + n * 31 + i * 7, size_name, theme, style, title)
        fname = f"{n:03d}_{theme}_{style}_{size_name}.csp"
        with open(os.path.join(MAPS, fname), "w", encoding="utf-8") as f:
            f.write(data)
        index.append(f"{fname}\t{size_name}\t{theme}\t{title}")

    with open(os.path.join(MAPS, "index.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(index) + "\n")
    print("Wrote", n, "maps to", MAPS, f"({len(orig_data)} originals converted)")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Render README screenshots from dungeon_cli JSON/ASCII output."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "screenshots"
CLI = ROOT / "build" / "dungeon_cli"

BG = (18, 20, 26)
PANEL = (28, 32, 42)
PANEL_BORDER = (58, 64, 78)
TEXT = (230, 232, 238)
MUTED = (150, 156, 170)
ACCENT = (80, 180, 255)
LINE = (90, 100, 120)

COLORS = {
    "player": (80, 180, 255),
    "start": (90, 200, 90),
    "boss": (210, 70, 70),
    "door_locked": (160, 90, 40),
    "door_open": (210, 170, 70),
    "key": (230, 210, 80),
    "super_chest": (180, 90, 220),
    "chest": (150, 110, 200),
    "elite": (200, 110, 60),
    "monster": (180, 80, 80),
    "main": (70, 90, 120),
    "branch": (55, 70, 90),
}


def font(size: int, mono: bool = False, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "DejaVuSansMono" if mono else "DejaVuSans"
    if bold:
        name += "-Bold"
    path = f"/usr/share/fonts/truetype/dejavu/{name}.ttf"
    return ImageFont.truetype(path, size)


def run_cli(*args: str) -> str:
    exe = CLI if CLI.exists() else Path("dungeon_cli")
    return subprocess.check_output([str(exe), *args], text=True)


def room_label(room: dict, last_main: int) -> str:
    if room["id"] == 1:
        return "S"
    if room["enemy"] == "boss" or room["id"] == last_main:
        return "B"
    if room["event"] == "door":
        return "D"
    if room["keyOpens"] > 0:
        return "K"
    if room["event"] == "super_chest":
        return "C"
    if room["event"] in ("middle_chest", "small_chest"):
        return "c"
    if room["enemy"] == "elite":
        return "E"
    if room["enemy"] == "normal":
        return "m"
    return "M" if room["branch"] == 0 else "o"


def room_color(room: dict, player: int, keys: set[int], last_main: int) -> tuple[int, int, int]:
    if room["id"] == player:
        return COLORS["player"]
    if room["id"] == 1:
        return COLORS["start"]
    if room["enemy"] == "boss" or room["id"] == last_main:
        return COLORS["boss"]
    if room["event"] == "door":
        return COLORS["door_open"] if room["id"] in keys else COLORS["door_locked"]
    if room["keyOpens"] > 0:
        return COLORS["key"]
    if room["event"] == "super_chest":
        return COLORS["super_chest"]
    if room["event"] in ("middle_chest", "small_chest"):
        return COLORS["chest"]
    if room["enemy"] == "elite":
        return COLORS["elite"]
    if room["enemy"] == "normal":
        return COLORS["monster"]
    return COLORS["main"] if room["branch"] == 0 else COLORS["branch"]


def draw_panel(draw: ImageDraw.ImageDraw, dungeon: dict, player: int, steps: int, status: str) -> None:
    draw.rounded_rectangle((16, 16, 348, 784), radius=10, fill=PANEL, outline=PANEL_BORDER, width=1)
    title = font(18, bold=True)
    body = font(15)
    small = font(13)
    draw.text((32, 32), "Generator", font=title, fill=TEXT)
    lines = [
        f"Seed            {dungeon['seed']}",
        f"Main rooms      10-12",
        f"Total rooms     20-25",
        f"Branches        3-4",
        f"Doors           2",
        "",
        "[ Generate ]     [ New seed ]",
        "",
        status,
        "",
        f"Player room     {player}",
        f"Steps           {steps}",
        f"Rooms           {len(dungeon['rooms'])}",
        "",
        "S start    B boss    D door    K key",
        "WASD move    R regenerate    N next",
    ]
    y = 72
    for line in lines:
        color = ACCENT if line.startswith("[") else (MUTED if not line else TEXT)
        draw.text((32, y), line, font=small if len(line) > 28 else body, fill=color)
        y += 28 if line else 16


def render_play(dungeon: dict, player: int, steps: int, status: str, path: Path) -> None:
    img = Image.new("RGB", (1280, 800), BG)
    draw = ImageDraw.Draw(img)
    draw_panel(draw, dungeon, player, steps, status)

    rooms = {r["id"]: r for r in dungeon["rooms"]}
    last_main = dungeon["lastMainId"]
    xs = [r["x"] for r in dungeon["rooms"]]
    ys = [r["y"] for r in dungeon["rooms"]]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    gw = max(1, max_x - min_x + 1)
    gh = max(1, max_y - min_y + 1)

    map_x0, map_y0, map_w, map_h = 380, 40, 860, 720
    cell = min(map_w / gw, map_h / gh) * 0.82
    ox = map_x0 + (map_w - cell * gw) * 0.5
    oy = map_y0 + (map_h - cell * gh) * 0.5

    def screen(x: int, y: int) -> tuple[float, float]:
        return ox + (x - min_x + 0.5) * cell, oy + (max_y - y + 0.5) * cell

    keys = set()
    # Keys already held: those in rooms on a simple walk are not computed;
    # treat keys in the current room and any room id <= player on the visit
    # as not automatically held. Only the current room's key is collected.
    if rooms[player]["keyOpens"] > 0:
        keys.add(rooms[player]["keyOpens"])

    for room in dungeon["rooms"]:
        ax, ay = screen(room["x"], room["y"])
        for nb in room["neighbors"]:
            if nb < room["id"]:
                continue
            other = rooms[nb]
            bx, by = screen(other["x"], other["y"])
            draw.line((ax, ay, bx, by), fill=LINE, width=3)

    label_font = font(16, bold=True)
    s = cell * 0.38
    for room in dungeon["rooms"]:
        cx, cy = screen(room["x"], room["y"])
        color = room_color(room, player, keys, last_main)
        box = (cx - s, cy - s, cx + s, cy + s)
        draw.rounded_rectangle(box, radius=8, fill=color, outline=(230, 230, 230), width=2)
        label = room_label(room, last_main)
        tw = draw.textlength(label, font=label_font)
        draw.text((cx - tw / 2, cy - 10), label, font=label_font, fill=(245, 245, 245))

    caption = font(13)
    draw.text((380, 770), "S start   B boss   D door   K key   C/c chest   E elite   m monster", font=caption, fill=MUTED)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, "PNG", optimize=True)


def render_ascii(text: str, path: Path) -> None:
    mono = font(18, mono=True)
    lines = text.rstrip().splitlines()
    pad = 28
    line_h = 24
    width = int(max(mono.getlength(line) for line in lines) + pad * 2)
    height = pad * 2 + line_h * len(lines) + 36
    img = Image.new("RGB", (width, height), (12, 14, 18))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle((8, 8, width - 8, height - 8), radius=8, fill=(22, 24, 30), outline=(50, 54, 64))
    draw.text((24, 18), "dungeon_cli --seed 12345", font=font(14, bold=True), fill=ACCENT)
    y = 48
    for line in lines:
        draw.text((24, y), line, font=mono, fill=(210, 216, 224))
        y += line_h
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, "PNG", optimize=True)


def main() -> int:
    if not CLI.exists():
        print("build dungeon_cli first", file=sys.stderr)
        return 1
    OUT.mkdir(parents=True, exist_ok=True)

    dungeon = json.loads(run_cli("--seed", "12345", "--json"))
    render_play(
        dungeon,
        player=1,
        steps=0,
        status="Walk from S to B. WASD / click a neighbor.",
        path=OUT / "play-seed-12345.png",
    )
    render_play(
        dungeon,
        player=7,
        steps=6,
        status="Picked up a key. Door D can be opened.",
        path=OUT / "play-with-key.png",
    )

    dungeon7 = json.loads(run_cli("--seed", "7", "--json"))
    render_play(
        dungeon7,
        player=1,
        steps=0,
        status="Seed 7  rooms=22  solvable=yes",
        path=OUT / "play-seed-7.png",
    )

    ascii_text = run_cli("--seed", "12345")
    render_ascii(ascii_text, OUT / "cli-seed-12345.png")
    print(f"wrote screenshots to {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

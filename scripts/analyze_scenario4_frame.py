#!/usr/bin/env python3
"""Analyze LVGL_VERIFY frame dump (uint32 w,h + RGBA rows, LVGL top-left origin)."""

from __future__ import annotations

import struct
import sys
from pathlib import Path


def load_rgba(path: Path) -> tuple[int, int, list[bytes]]:
    data = path.read_bytes()
    w, h = struct.unpack_from("<II", data, 0)
    pixels = []
    off = 8
    row = w * 4
    for _ in range(h):
        pixels.append(data[off : off + row])
        off += row
    return w, h, pixels


def is_bluish(r: int, g: int, b: int, a: int) -> bool:
    return a >= 128 and r > 170 and g > 90 and b < 120


def bbox(mask: list[list[bool]]) -> tuple[int, int, int, int] | None:
    h = len(mask)
    w = len(mask[0]) if h else 0
    min_x, min_y, max_x, max_y = w, h, -1, -1
    for y in range(h):
        for x in range(w):
            if mask[y][x]:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if max_x < 0:
        return None
    return min_x, min_y, max_x, max_y


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <frame_lvgl.rgba>", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    w, h, rows = load_rgba(path)
    blue_mask = [[False] * w for _ in range(h)]

    blue_n = visible_n = 0
    for y, row in enumerate(rows):
        for x in range(w):
            r, g, b, a = row[x * 4 : x * 4 + 4]
            if a >= 32:
                visible_n += 1
            if is_bluish(r, g, b, a):
                blue_mask[y][x] = True
                blue_n += 1

    cx, cy = w // 2, h // 2
    cr, cg, cb, ca = rows[cy][cx * 4 : cx * 4 + 4]
    blue_bb = bbox(blue_mask)

    print(f"frame: {w}x{h} path={path}")
    print(f"visible_pixels={visible_n} bluish={blue_n}")
    print(f"center ({cx},{cy}) rgba=({cr},{cg},{cb},{ca})")

    if blue_bb:
        x0, y0, x1, y1 = blue_bb
        print(
            f"button_bbox: x=[{x0},{x1}] y=[{y0},{y1}] "
            f"size={x1 - x0 + 1}x{y1 - y0 + 1} center=({(x0 + x1) // 2},{(y0 + y1) // 2})"
        )
    else:
        print("button_bbox: NONE (no blue pixels)")
        return 1

    if blue_n < 4:
        print("FAIL: insufficient blue button coverage")
        return 1

    print("PASS: 3D button visible in framebuffer")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

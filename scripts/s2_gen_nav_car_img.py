#!/usr/bin/env python3
"""Regenerate nav car sprite from the source PNG (checkerboard baked into alpha)."""

from __future__ import annotations

import argparse
import os
from collections import deque

from PIL import Image

DEFAULT_SRC = os.path.join(os.path.dirname(__file__), "..", "assets", "nav_car_top_blue.png")
FALLBACK_SRC = os.path.join(os.path.dirname(__file__), "..", "assets", "nav_car_top.png")
OUT_PNG = os.path.join(os.path.dirname(__file__), "..", "assets", "nav_car_top.png")
OUT_C = os.path.join(os.path.dirname(__file__), "..", "src", "demos", "s2_nav_car_img.c")
TARGET_W = 280


def neutral(r: int, g: int, b: int) -> bool:
    return abs(r - g) <= 14 and abs(g - b) <= 14 and abs(r - b) <= 14


def is_checker(r: int, g: int, b: int, a: int) -> bool:
    if a < 8:
        return True
    if not neutral(r, g, b):
        return False
    avg = (r + g + b) / 3.0
    return (205 <= avg <= 222) or (234 <= avg <= 239) or avg >= 248


def is_span_fill(r: int, g: int, b: int, a: int) -> bool:
    """Pixels we may paint inside the per-row car envelope."""
    if a < 8:
        return False
    if not neutral(r, g, b):
        return True
    avg = (r + g + b) / 3.0
    if avg < 155:
        return True
    # Body white + light checker cells baked into the hood/roof (238+).
    return avg >= 238


def build_car_mask(px, w: int, h: int) -> list[list[bool]]:
    bg = [[False] * w for _ in range(h)]
    q: deque[tuple[int, int]] = deque()
    for x in range(w):
        for y in (0, h - 1):
            if is_checker(*px[x, y]):
                bg[y][x] = True
                q.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if not bg[y][x] and is_checker(*px[x, y]):
                bg[y][x] = True
                q.append((x, y))
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and not bg[ny][nx] and is_checker(*px[nx, ny]):
                bg[ny][nx] = True
                q.append((nx, ny))

    car = [[False] * w for _ in range(h)]
    q = deque()
    for y in range(h):
        for x in range(w):
            if bg[y][x]:
                continue
            r, g, b, a = px[x, y]
            if a < 8:
                continue
            if not neutral(r, g, b) or (r + g + b) / 3.0 < 155:
                car[y][x] = True
                q.append((x, y))

    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if not (0 <= nx < w and 0 <= ny < h) or car[ny][nx]:
                continue
            r, g, b, a = px[nx, ny]
            if a < 8 or is_checker(r, g, b, a):
                continue
            if not neutral(r, g, b):
                car[ny][nx] = True
                q.append((nx, ny))
                continue
            avg = (r + g + b) / 3.0
            if avg < 155 or avg >= 225:
                car[ny][nx] = True
                q.append((nx, ny))

    for _ in range(40):
        changed = False
        for y in range(1, h - 1):
            for x in range(1, w - 1):
                if car[y][x]:
                    continue
                n = sum(car[y + dy][x + dx] for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if n >= 3:
                    car[y][x] = True
                    changed = True
        if not changed:
            break

    left = [w] * h
    right = [-1] * h
    for y in range(h):
        xs = [x for x in range(w) if car[y][x]]
        if xs:
            left[y] = min(xs)
            right[y] = max(xs)

    win = 120
    for y in range(h):
        l, r = w, -1
        for yy in range(max(0, y - 40), min(h, y + win)):
            if left[yy] <= right[yy]:
                l = min(l, left[yy])
                r = max(r, right[yy])
        if l > r:
            continue
        for x in range(l, r + 1):
            if car[y][x]:
                continue
            if is_span_fill(*px[x, y]):
                car[y][x] = True

    return car


def keep_largest_component(car: list[list[bool]], w: int, h: int) -> list[list[bool]]:
    seen = [[False] * w for _ in range(h)]
    best: list[tuple[int, int]] = []
    for y in range(h):
        for x in range(w):
            if not car[y][x] or seen[y][x]:
                continue
            q: deque[tuple[int, int]] = deque([(x, y)])
            seen[y][x] = True
            comp = [(x, y)]
            while q:
                cx, cy = q.popleft()
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = cx + dx, cy + dy
                    if 0 <= nx < w and 0 <= ny < h and car[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = True
                        q.append((nx, ny))
                        comp.append((nx, ny))
            if len(comp) > len(best):
                best = comp

    keep = [[False] * w for _ in range(h)]
    for x, y in best:
        keep[y][x] = True
    return keep


def downsample_alpha_max(alpha: Image.Image, ow: int, oh: int) -> Image.Image:
    """Keep a destination pixel opaque if any source pixel in its cell is opaque."""
    w, h = alpha.size
    src = alpha.load()
    out = Image.new("L", (ow, oh))
    dst = out.load()
    for dy in range(oh):
        y0 = dy * h // oh
        y1 = max(y0 + 1, (dy + 1) * h // oh)
        for dx in range(ow):
            x0 = dx * w // ow
            x1 = max(x0 + 1, (dx + 1) * w // ow)
            opaque = False
            for y in range(y0, y1):
                for x in range(x0, x1):
                    if src[x, y] > 128:
                        opaque = True
                        break
                if opaque:
                    break
            dst[dx, dy] = 255 if opaque else 0
    return out


def fill_interior_alpha(out: Image.Image, passes: int = 12) -> None:
    apx = out.split()[3].load()
    opx = out.load()
    aw, ah = out.size
    for _ in range(passes):
        changed = False
        for y in range(1, ah - 1):
            for x in range(1, aw - 1):
                if apx[x, y] > 128:
                    continue
                n = sum(1 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if apx[x + dx, y + dy] > 200)
                if n >= 3:
                    apx[x, y] = 255
                    r, g, b, _a = opx[x, y]
                    if r + g + b < 640:
                        opx[x, y] = (255, 255, 255, 255)
                    else:
                        opx[x, y] = (r, g, b, 255)
                    changed = True
        if not changed:
            break
    out.putalpha(out.split()[3])


def write_c_array(path: str, data: bytes, w: int, h: int) -> None:
    lines = [
        "/** Auto-generated from assets/nav_car_top.png — top-down nav car sprite. */",
        '#include "lvgl/lvgl.h"',
        "",
        "static const uint8_t s2_nav_car_img_map[] = {",
    ]
    row = "    "
    for i, b in enumerate(data):
        row += f"0x{b:02x},"
        if (i + 1) % 16 == 0:
            lines.append(row)
            row = "    "
    if row.strip():
        lines.append(row.rstrip(","))
    lines += [
        "};",
        "",
        "const lv_image_dsc_t s2_nav_car_img = {",
        "    .header = {",
        "        .magic = LV_IMAGE_HEADER_MAGIC,",
        "        .cf = LV_COLOR_FORMAT_ARGB8888,",
        "        .flags = 0,",
        f"        .w = {w},",
        f"        .h = {h},",
        f"        .stride = {w * 4},",
        "        .reserved_2 = 0,",
        "    },",
        "    .data_size = sizeof(s2_nav_car_img_map),",
        "    .data = s2_nav_car_img_map,",
        "    .reserved = NULL,",
        "};",
        "",
    ]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", default=DEFAULT_SRC if os.path.isfile(DEFAULT_SRC) else FALLBACK_SRC)
    parser.add_argument("--out-png", default=OUT_PNG)
    parser.add_argument("--out-c", default=OUT_C)
    parser.add_argument("--width", type=int, default=TARGET_W)
    args = parser.parse_args()

    im = Image.open(args.src).convert("RGBA")
    w, h = im.size
    px = im.load()

    car = build_car_mask(px, w, h)
    car = keep_largest_component(car, w, h)

    for _ in range(24):
        changed = False
        for y in range(1, h - 1):
            for x in range(1, w - 1):
                if car[y][x]:
                    continue
                n = sum(car[y + dy][x + dx] for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if n >= 3:
                    car[y][x] = True
                    changed = True
        if not changed:
            break

    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    opx = out.load()
    white_opaque = 0
    for y in range(h):
        for x in range(w):
            if not car[y][x]:
                continue
            r, g, b, a = px[x, y]
            if neutral(r, g, b) and (r + g + b) / 3.0 >= 225:
                opx[x, y] = (255, 255, 255, 255)
                white_opaque += 1
            else:
                opx[x, y] = (r, g, b, 255)

    bbox = out.getbbox()
    if bbox:
        out = out.crop(bbox)
    ow = args.width
    oh = max(1, round(out.height * (ow / out.width)))

    # Resize alpha with nearest-neighbor; RGB on white so LANCZOS does not pull in black fringe.
    alpha = downsample_alpha_max(out.split()[3], ow, oh)
    flat = Image.new("RGB", out.size, (255, 255, 255))
    flat.paste(out, mask=out.split()[3])
    rgb = flat.resize((ow, oh), Image.LANCZOS)
    out = Image.merge("RGBA", (*rgb.split(), alpha))
    fill_interior_alpha(out)

    os.makedirs(os.path.dirname(os.path.abspath(args.out_png)), exist_ok=True)
    out.save(args.out_png)

    data = bytearray()
    for y in range(oh):
        for x in range(ow):
            r, g, b, a = opx[x, y]
            data.extend([b, g, r, a])

    write_c_array(args.out_c, bytes(data), ow, oh)
    print(f"Wrote {args.out_png} and {args.out_c}: {ow}x{oh}, white_opaque={white_opaque}")


if __name__ == "__main__":
    main()

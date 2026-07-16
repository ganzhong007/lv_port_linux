#!/usr/bin/env python3
"""Downgrade wayland-scanner 1.20 client headers for wayland-client 1.17."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def patch_header(text: str) -> str:
    text = re.sub(
        r"wl_proxy_marshal_flags\(\(struct wl_proxy \*\) ([^,]+),\s*"
        r"([^,]+), NULL, wl_proxy_get_version\(\(struct wl_proxy \*\) \1\), WL_MARSHAL_FLAG_DESTROY\);",
        r"wl_proxy_destroy((struct wl_proxy *) \1);",
        text,
        flags=re.MULTILINE,
    )

    text = re.sub(
        r"id = wl_proxy_marshal_flags\(\(struct wl_proxy \*\) ([^,]+),\s*"
        r"([^,]+), (&[\w_]+), wl_proxy_get_version\(\(struct wl_proxy \*\) \1\), 0, NULL\);",
        r"id = wl_proxy_marshal_constructor((struct wl_proxy *) \1, \2, \3, NULL);",
        text,
        flags=re.MULTILINE,
    )

    text = re.sub(
        r"id = wl_proxy_marshal_flags\(\(struct wl_proxy \*\) ([^,]+),\s*"
        r"([^,]+), (&[\w_]+), wl_proxy_get_version\(\(struct wl_proxy \*\) \1\), 0, NULL,\s*"
        r"([^)]+)\);",
        r"id = wl_proxy_marshal_constructor((struct wl_proxy *) \1, \2, \3, NULL, \4);",
        text,
        flags=re.MULTILINE,
    )

    text = re.sub(
        r"wl_proxy_marshal_flags\(\(struct wl_proxy \*\) ([^,]+),\s*"
        r"([^,]+), NULL, wl_proxy_get_version\(\(struct wl_proxy \*\) \1\), 0(?:, NULL)?(?:,\s*([^)]+))?\);",
        lambda m: (
            f"wl_proxy_marshal((struct wl_proxy *) {m.group(1)}, {m.group(2)}"
            + (f", {m.group(3)}" if m.group(3) else "")
            + ");"
        ),
        text,
        flags=re.MULTILINE,
    )

    return text


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <header>", file=sys.stderr)
        return 1

    path = Path(sys.argv[1])
    original = path.read_text()
    patched = patch_header(original)
    path.write_text(patched)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

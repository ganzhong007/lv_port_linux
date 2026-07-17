#!/usr/bin/env python3
"""Aggregate an LVGL systrace (Android tracing_mark_write B/E) into a
per-function bottleneck report: call count, total (wall) time and self time.

Usage:
    python3 scripts/analyze_trace.py benchmark_logs/3dscene_trace.systrace [top_n]
"""
import re
import sys
from collections import defaultdict

LINE = re.compile(
    r"-\d+\s+\[\d+\]\s+(\d+)\.(\d+):\s+tracing_mark_write:\s+([BE])\|\d+\|(.+)$"
)


def main():
    if len(sys.argv) < 2:
        print("usage: analyze_trace.py <file.systrace> [top_n]")
        sys.exit(1)
    path = sys.argv[1]
    top_n = int(sys.argv[2]) if len(sys.argv) > 2 else 20

    total = defaultdict(int)   # ns, wall time (incl. children)
    self_t = defaultdict(int)  # ns, excl. children
    count = defaultdict(int)
    stack = []                 # (name, start_ns, child_ns_accum)

    first_ns = last_ns = None
    frames = 0

    with open(path, "r", errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if not m:
                continue
            sec, nsec, kind, name = m.groups()
            t = int(sec) * 1_000_000_000 + int(nsec)
            if first_ns is None:
                first_ns = t
            last_ns = t
            if name == "lv_display_refr_timer" and kind == "B":
                frames += 1
            if kind == "B":
                stack.append([name, t, 0])
            else:  # E
                if not stack:
                    continue
                sname, start, child = stack.pop()
                dur = t - start
                total[sname] += dur
                self_t[sname] += dur - child
                count[sname] += 1
                if stack:
                    stack[-1][2] += dur

    span_ns = (last_ns - first_ns) if first_ns is not None else 0
    span_ms = span_ns / 1e6

    print(f"trace span     : {span_ms:.1f} ms")
    print(f"refr frames    : {frames}"
          + (f"  (~{frames * 1000.0 / span_ms:.1f} refr/s)" if span_ms else ""))
    print()

    def fmt(ns):
        return f"{ns/1e6:9.2f} ms"

    rows = sorted(total.items(), key=lambda kv: kv[1], reverse=True)[:top_n]
    hdr = f"{'function':<34}{'calls':>8}{'total':>13}{'self':>13}{'avg':>12}"
    print(hdr)
    print("-" * len(hdr))
    for name, tot in rows:
        c = count[name]
        avg = tot / c if c else 0
        disp = name if len(name) <= 33 else name[:30] + "..."
        print(f"{disp:<34}{c:>8}{fmt(tot):>13}{fmt(self_t[name]):>13}"
              f"{avg/1e6:>10.3f}ms")

    print()
    print("Top by SELF time (excl. children) — the real hot spots:")
    rows_self = sorted(self_t.items(), key=lambda kv: kv[1], reverse=True)[:top_n]
    print(hdr)
    print("-" * len(hdr))
    for name, st in rows_self:
        c = count[name]
        disp = name if len(name) <= 33 else name[:30] + "..."
        print(f"{disp:<34}{c:>8}{fmt(total[name]):>13}{fmt(st):>13}"
              f"{(total[name]/c)/1e6 if c else 0:>10.3f}ms")


if __name__ == "__main__":
    main()

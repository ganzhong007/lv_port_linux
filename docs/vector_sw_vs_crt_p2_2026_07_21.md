# VECTOR SW vs CRT P2 对比（2026-07-21）

相对 P1：

| 指标 | P1 | P2 |
|------|----|----|
| 全屏 exact% | 64.60 | 64.57 |
| 全屏 MAE | 10.06 | **9.96** |
| dash_line 区 MAE | — | 12.08 |
| delta 101+ | 25809 | **25243** |

P2 变更：虚线描边、round/square/butt cap、miter/bevel/round join、轻量 fringe AA。  
观感上绿色虚线曲线应分段可见（相对 P0/P1 的实心粗线）。

产物：`benchmark_logs/vector_sw_vs_crt_p2_2026_07_21/`

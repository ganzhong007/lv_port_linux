# VECTOR SW vs CRT P1 对比（2026-07-21）

相对 P0（`vector_sw_vs_crt_2026_07_21`）：

| 指标 | P0 | P1 |
|------|----|----|
| 全屏 exact% | 63.84 | **64.60** |
| 全屏 MAE | 13.89 | **10.06** |
| grad 区 MAE | 83.99 | **42.57** |
| center MAE | 32.13 | **10.25** |
| delta 101+ | 38530 | **25809** |

视觉：CRT 已出现红→黄→绿线性渐变与 pattern 头像 clip；虚线描边 / 部分径向细节仍弱（P2）。

产物：`benchmark_logs/vector_sw_vs_crt_p1_2026_07_21/`

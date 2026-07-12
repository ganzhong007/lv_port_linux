# Wayland SHM vs EGL Stress 测试报告（WSLg）

> **测试日期**：2026-07-11  
> **环境**：WSL2 + WSLg（`WAYLAND_DISPLAY=wayland-0`）  
> **Demo**：`lv_demo_stress`（`LVGL_APP_DEMO=stress`）

## 测试条件

| 参数 | 值 |
|------|-----|
| 分辨率 | **1600×960**（默认 800×480 的 2 倍，面积 4 倍） |
| 绘制量 | **`LV_DEMO_STRESS_DRAW_MULT = 10`**（额外 180 个 button+label） |
| 时长 | 各 **45 秒** |
| 刷新周期 | `LV_DEF_REFR_PERIOD = 1`（stress 专用配置） |
| SHM 配置 | `configs/wayland.defaults` → `build-stress-shm` |
| EGL 配置 | `configs/wayland-egl.defaults` → `build-stress-egl` |

## 复现命令

```bash
./scripts/benchmark_stress_shm_vs_egl.sh 45 1600 960 10
```

EGL 运行依赖（本机实测需要）：

```bash
sudo apt install libgles2-mesa-dev libegl1-mesa-dev
export LD_LIBRARY_PATH=/usr/lib/wsl/lib:$LD_LIBRARY_PATH
```

## 统计方法

- 数据来源：stderr 中 `sysmon:` 行
- 去掉前 **3** 个启动样本
- **稳定 FPS**：仅统计 **1–120 FPS** 样本（过滤 `LV_DEF_REFR_PERIOD=1` 下 >120 的计数 artifact）
- 同时记录 CPU (total)、render、flush 平均值

## 结果对比

| 指标 | **wayland-shm** | **wayland-egl** |
|------|-----------------|-----------------|
| **稳定 FPS（avg）** | **59.5** | **27.4** |
| **稳定 FPS（median）** | 59 | 23 |
| **稳定 FPS（min / max）** | 57 / 86 | 8 / 120 |
| **稳定样本数** | 100 / 145 | 109 / 138 |
| **原始 fps_avg（含离群）** | 157.0 | 102.7 |
| **CPU (total)** | 2.6% | 51.1% |
| **render** | 0.8 ms | 11.6 ms |
| **flush** | 14.4 ms | 29.3 ms |

## 结论

1. **SHM 明显优于 EGL**：2× 分辨率 + 10× widget 负载下，SHM 仍稳定 ~**60 FPS**（WSLg compositor 上限）；EGL 约 **27 FPS**。
2. **SHM 瓶颈在 flush**（~14 ms），CPU 绘制很轻（render ~0.8 ms）。
3. **EGL 瓶颈在 render + flush**（合计 ~41 ms/帧），CPU 占用高（~51%），NanoVG/GPU 提交更重。
4. `LV_DEF_REFR_PERIOD=1` 时原始 `fps_avg` 会虚高（SHM 157 / EGL 103），系 sysmon 统计窗口 artifact，**不能当真**；实际仍受 compositor 限制。

## 原始日志

本地运行产物（未纳入 git）：

- `benchmark_logs/shm.log`
- `benchmark_logs/egl.log`

查看最近 FPS 行：

```bash
grep 'sysmon:' benchmark_logs/shm.log | tail -10
grep 'sysmon:' benchmark_logs/egl.log | tail -10
```

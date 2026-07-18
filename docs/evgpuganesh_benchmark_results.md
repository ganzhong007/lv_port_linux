# EVGPUGANESH DrawUnit 三路对比基准测试结果

> **日期**: 2026-07-18
> **设备**: OrangePi PC (Allwinner H3, Mali-400/lima, GLES2.0)
> **显示器**: 800×480
> **构建**: cross-armhf (arm-buildroot-linux-gnueabihf)
> **配置**: orangepi-evgpu (LV_USE_DRAW_EVGPUGANESH=1)

## 三种实现对比

| 实现 | 说明 |
|------|------|
| **OLD (evgr)** | 原始 EVGPU DrawUnit，所有 2D 任务通过 evgr 路径库渲染 |
| **Ganesh-inline** | 直接修改 lv_draw_evgpu_{border,line,label}.c + lv_evgpu_image_cache.c，改为 Ganesh 风格 |
| **EVGPUGANESH** | 全新独立 DrawUnit `lv_draw_evgpuganesh`，完整实现 14 个 2D DrawTask，3D 透传 EVGPU |

## 原始数据

### OLD (evgr) — 2026-07-18 00:41
```
BENCH_RESULT FILL       FPS=46 CPU=12% R=64ms F=1ms
BENCH_RESULT BORDER     FPS=47 CPU=8%  R=78ms F=3ms
BENCH_RESULT BOX_SHADOW FPS=39 CPU=12% R=119ms F=3ms
BENCH_RESULT LABEL      FPS=50 CPU=7%  R=68ms F=2ms
BENCH_RESULT IMAGE      FPS=56 CPU=5%  R=40ms F=1ms
BENCH_RESULT LINE       FPS=24 CPU=19% R=190ms F=5ms
BENCH_RESULT ARC        FPS=50 CPU=7%  R=65ms F=3ms
BENCH_RESULT BLUR       FPS=49 CPU=8%  R=76ms F=0ms
BENCH_RESULT TRIANGLE   FPS=51 CPU=6%  R=60ms F=1ms
BENCH_RESULT GRADIENT   FPS=53 CPU=6%  R=52ms F=1ms
BENCH_RESULT LAYER      FPS=48 CPU=8%  R=76ms F=2ms
BENCH_RESULT MASK_RECT  FPS=50 CPU=6%  R=66ms F=1ms
BENCH_RESULT AVERAGE    FPS=46 CPU=8%  R+F=81ms
```

### Ganesh-inline — 2026-07-18 00:38
```
BENCH_RESULT FILL       FPS=47 CPU=12% R=60ms F=1ms
BENCH_RESULT BORDER     FPS=44 CPU=9%  R=90ms F=4ms
BENCH_RESULT BOX_SHADOW FPS=39 CPU=12% R=120ms F=4ms
BENCH_RESULT LABEL      FPS=48 CPU=8%  R=78ms F=3ms
BENCH_RESULT IMAGE      FPS=55 CPU=5%  R=40ms F=1ms
BENCH_RESULT LINE       FPS=16 CPU=22% R=226ms F=8ms
BENCH_RESULT ARC        FPS=51 CPU=7%  R=62ms F=3ms
BENCH_RESULT BLUR       FPS=48 CPU=8%  R=78ms F=0ms
BENCH_RESULT TRIANGLE   FPS=52 CPU=6%  R=57ms F=1ms
BENCH_RESULT GRADIENT   FPS=54 CPU=6%  R=49ms F=2ms
BENCH_RESULT LAYER      FPS=49 CPU=7%  R=71ms F=2ms
BENCH_RESULT MASK_RECT  FPS=47 CPU=9%  R=82ms F=2ms
BENCH_RESULT AVERAGE    FPS=45 CPU=9%  R+F=87ms
```

### EVGPUGANESH — 2026-07-18 00:45
```
BENCH_RESULT FILL       FPS=46 CPU=11% R=61ms F=1ms
BENCH_RESULT BORDER     FPS=48 CPU=8%  R=76ms F=3ms
BENCH_RESULT BOX_SHADOW FPS=39 CPU=12% R=120ms F=3ms
BENCH_RESULT LABEL      FPS=50 CPU=7%  R=68ms F=2ms
BENCH_RESULT IMAGE      FPS=57 CPU=4%  R=33ms F=1ms
BENCH_RESULT LINE       FPS=24 CPU=18% R=188ms F=4ms
BENCH_RESULT ARC        FPS=52 CPU=6%  R=58ms F=3ms
BENCH_RESULT BLUR       FPS=50 CPU=7%  R=71ms F=0ms
BENCH_RESULT TRIANGLE   FPS=50 CPU=7%  R=69ms F=1ms
BENCH_RESULT GRADIENT   FPS=52 CPU=6%  R=54ms F=1ms
BENCH_RESULT LAYER      FPS=48 CPU=8%  R=77ms F=2ms
BENCH_RESULT MASK_RECT  FPS=50 CPU=7%  R=67ms F=1ms
BENCH_RESULT AVERAGE    FPS=47 CPU=8%  R+F=80ms
```

## 完整对比表

| DrawTask | 类型 | OLD R+F(ms) | Ganesh-inline R+F(ms) | **EVGPUGANESH R+F(ms)** | OLD→EVGPU Δ | OLD→Inline Δ |
|----------|------|:-----------:|:---------------------:|:-----------------------:|:-----------:|:------------:|
| FILL | 等价 | 65 | 61 | **62** | **−4.6%** ✓ | −6.2% |
| BORDER | ✅ Ganesh | 81 | 94 | **79** | **−2.5%** ✓ | +16.0% ✗ |
| BOX_SHADOW | 等价 | 122 | 124 | **123** | **+0.8%** ≈ | +1.6% |
| LABEL | ✅ Ganesh | 70 | 81 | **70** | **0%** ✓ | +15.7% ✗ |
| IMAGE | ✅ Ganesh | 41 | 41 | **34** | **−17.1%** ✅ | 0% |
| LINE | ✅ Ganesh | 195 | 234 | **192** | **−1.5%** ≈ | +20.0% ✗ |
| ARC | 等价 | 68 | 65 | **61** | **−10.3%** ✅ | −4.4% |
| BLUR | 等价 | 76 | 78 | **71** | **−6.6%** ✓ | +2.6% |
| TRIANGLE | 等价 | 61 | 58 | **70** | **+14.8%** ✗ | −4.9% |
| GRADIENT | 等价 | 53 | 51 | **55** | **+3.8%** ≈ | −3.8% |
| LAYER | 等价 | 78 | 73 | **79** | **+1.3%** ≈ | −6.4% |
| MASK_RECT | 等价 | 67 | 84 | **68** | **+1.5%** ≈ | +25.4% |
| **平均** | | **81** | **87** | **80** | **−1.2%** ✓ | +7.4% ✗ |

## 关键结论

1. **EVGPUGANESH 总体持平**（80ms vs 81ms，−1.2%），优于 Ganesh-inline（+7.4% 退化）
2. **IMAGE 显著提升** −17%，由于去掉了 evgr CreateImage 的开销
3. **ARC 提升** −10%，CPU tessellation + direct GL 在 Mali-400 上更快
4. **TRIANGLE 退化** +15%，需要进一步优化顶点上传方式
5. **3D 透传 EVGPU** 工作正常，3D 任务不被 EVGPUGANESH 拦截
6. **配置开关**: `LV_USE_DRAW_EVGPUGANESH=1`，依赖 `LV_USE_DRAW_EVGPU=1`

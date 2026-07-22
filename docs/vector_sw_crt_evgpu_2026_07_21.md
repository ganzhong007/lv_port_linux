# VECTOR 三方对比：SW / CRT / EVGPU（2026-07-21）

同 demo、800×480、板端 dump。

| | SW | CRT | EVGPU |
|--|----|-----|-------|
| 后端 | ThorVG（`LVGL_BUF_DUMP`） | C_R_T tess→GLES2 | EVGR |
| 产物 | `vec_sw.*` | `vec_crt.*` | `vec_evgpu.*` |

目录：`benchmark_logs/vector_sw_crt_evgpu_2026_07_21/`

---

## 1. 成对像素（exact% / MAE）

| ROI | CRT vs SW | EVGPU vs SW | CRT vs EVGPU |
|-----|-----------|-------------|--------------|
| **full** | 63.6 / **11.41** | 63.0 / **7.96** | **86.6** / 11.65 |
| red_tri | 31.2 / 13.1 | 32.6 / 10.0 | 94.7 / 4.6 |
| dash | 29.0 / 6.4 | 29.4 / 4.9 | 97.2 / 2.1 |
| pattern | 10.1 / 27.5 | 10.4 / **2.1** | 79.8 / 26.6 |
| fill_grad | 65.5 / **8.1** | 62.4 / 17.6 | 66.2 / 20.8 |
| stroke_grad | 78.3 / 22.1 | 78.4 / 22.3 | 84.2 / 9.4 |
| center_mid | 39.1 / 19.2 | 32.7 / 30.4 | 56.4 / 23.5 |

读法：

- **全屏 MAE：EVGPU 最接近 SW**（7.96），主要吃 pattern 区红利（对 SW MAE 仅 2.1）。
- **CRT↔EVGPU 全屏 exact 86.6%**：solid / dash 高度一致；差在渐变与 pattern 边缘。
- **fill 渐变：CRT 更近 SW**（MAE 8.1 vs EVGPU 17.6）。
- **stroke 渐变 ROI**：SW 该区几乎无笔画（见下），数字被背景稀释，需看定性。

---

## 2. 按能力看「谁画对了」

| 能力 | SW (ThorVG) | CRT | EVGPU |
|------|-------------|-----|-------|
| solid 几何 | ✓ | ✓（与 EVGPU 极近） | ✓ |
| 绿虚线 dash | ✓ | ✓ | ✓ |
| fill 线性渐变（红→绿楔形） | ✓ 色带正确 | ✓ 色带正确 | ✗ **实色橄榄** `(128,128,0)` ≈ 红绿均值 |
| fill 径向（紫矩形内） | ✓ 红蓝环 | 有，色偏不同 | 有，红晕+蓝底 |
| pattern / 头像 clip | ✓ 杏仁形 clip | ✓ 形状/边缘与 SW 差大 | ✓ **与 SW 几乎贴** |
| stroke 线性渐变（右下红→蓝） | ✗ **ROI 无笔画**（非 bg 像素=0） | ✓ 粗红→蓝渐变 | ✗ **实色紫** `(128,64,128)` ≈ 红蓝均值 |

### 抽样（stroke y≈400）

| x | SW | CRT | EVGPU |
|---|----|-----|-------|
| 590–710 | 近背景 | 红→蓝渐变推进 | 全程 `(128,64,128)` |

→ 本 demo 新增的 stroke 渐变：**只有 CRT 按意图画出来**；EVGPU 塌成均值色；SW/ThorVG 该路径未落像素。

### 抽样（fill）

- SW / CRT：`(450,320)` ≈ `(125,129,0)` 同类黄绿。
- EVGPU：大块 `(128,128,0)` 实色，缺红→绿过渡。

---

## 3. 结论（具体情况）

1. **跟 SW 金图比「整体谁近」：** EVGPU 全屏 MAE 略优（pattern 对齐强）；CRT 在 **fill 线性渐变** 上更贴 SW。
2. **CRT vs EVGPU：** solid/dash 几乎一家；**渐变是分水岭**——CRT 有真 1D ramp，EVGPU 在本路径上 fill/stroke 渐变都像 stop 均值实色。
3. **三家都不一致的点：** pattern clip 形状（CRT≠SW）、径向细节、AA/三角化边界；stroke 渐变则是「CRT 独有可见实现」。
4. **不要用单一 full MAE 下结论：** EVGPU「更近 SW」不等于「渐变更好」；CRT「stroke MAE 看起来还行」是因为 SW 该区空白。

absdiff：`absdiff_crt_vs_sw.png`、`absdiff_evgpu_vs_sw.png`、`absdiff_crt_vs_evgpu.png`。

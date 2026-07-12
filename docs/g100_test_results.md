# DrawUnitG100 / G8 验证结果（Checkpoint 勾选表）

> 每个 **Checkpoint（CP）** 通过后在本表追加一行并随主仓 commit 推送。  
> 验证脚本：`scripts/verify_g100.sh CP-XX`  
> 用例定义：[drawunit_g100_test_cases.md](./drawunit_g100_test_cases.md) · 计划：[drawunit_g100_design.md §8.8](./drawunit_g100_design.md#88-checkpoint-步步为营验证与提交)

---

## 使用说明

| 列 | 含义 |
|----|------|
| **CP** | Checkpoint 编号 |
| **日期** | 通过日期（UTC+8） |
| **子模块 SHA** | `lvgl` 短 SHA |
| **主仓 SHA** | `lv_port_linux` 短 SHA |
| **Tag** | 可选 annotated tag |
| **验证** | 执行的 `verify_g100.sh` 参数 |
| **门禁** | 必过用例 ID |
| **结果** | PASS / FAIL / 部分 |
| **备注** | FPS、日志路径、板级环境 |

**SOP：** 子模块 commit+push → 主仓 bump 指针 + 本表一行 + push → 重要 CP 打 tag。

---

## Checkpoint 结果

| CP | 阶段 | 日期 | lvgl SHA | 主仓 SHA | Tag | 验证命令 | 门禁用例 | 结果 | 备注 |
|:--:|:--:|:--:|:--:|:--:|:--|:--|:--|:--:|:--|
| CP-00 | G0 Bootstrap | 2026-07-12 | dee769a | 5420c9e | — | `verify_g100.sh CP-00` | G0-01～08, PF-01 | PASS | stress ~175 FPS, 800×480 |
| CP-01a | G1 shader | 2026-07-12 | f92e2ef | e1519e0 | — | `verify_g100.sh CP-01a` | 编译 + AP shader | PASS | `G100 native shader ready (program=18)`; simple_button ~900 FPS |
| CP-01b | G1 grad | 2026-07-12 | d12fa25 | 5d98660 | — | `verify_g100.sh CP-01b` | GR-12, AP-01～03 | PASS | native grad program=21; render ~920 FPS; no VECTOR grad warn |
| CP-01c | G1 fill/image | 2026-07-12 | f6cb2ea | 60552c1 | — | `verify_g100.sh CP-01c` | D2-01, GR-03～06 | PASS | native solid/tex; fix GL stencil interleave (simple_button colors OK) |
| CP-02 | G2 label | 2026-07-12 | a08447e | 042ecd1 | — | `verify_g100.sh CP-02` | TX-02～04, AP-06 | PASS | FNV-1a label hash; stress 5s smoke |
| CP-03a | G3 vector | 2026-07-12 | 143bf08 | 452e38f | — | `verify_g100.sh CP-03a` | VC-01～03 | PASS | vector dash stroke + cap/join; render/simple_button OK |
| CP-03b | G3 PATTERN | 2026-07-12 | c18f16c | 39fd7a6 | — | `verify_g100.sh CP-03b` | VC-04 | PASS | avatar pattern + grad + dash; vector_graphic demo |
| CP-04a | G4 blur | 2026-07-12 | 27e2f0c | 62ccab8 | — | `verify_g100.sh CP-04a` | BL-03～04 | PASS | Dual Kawase >256; stress 5s smoke |
| CP-04b | G4 FBO 池 | 2026-07-12 | 08588bb | 5e1e457 | | `verify_g100.sh CP-04b` | BL-05～06 | PASS | FBO pool + box_shadow blur pipeline; render 5s |
| CP-05 | G5 完整 2D | 2026-07-12 | cf86c33 | f293adc | `g100-mvp-2d` | `verify_g100.sh CP-05` | D2-09～14, SW-01～02 | PASS | stress 60s; render line/arc/layer/grad OK |
| CP-06 | G6 3D BLIT | 2026-07-12 | 5b93565 | 41a10e5 | `g100-mvp-3d-blit` | `verify_g100.sh CP-06` | D3-01～05 | PASS | gltf demo; lvgl_logo.glb; 3D+2D panel OK |
| CP-07a | G7 perf | 2026-07-12 | 5b93565 | cb9c3f7 | | `verify_g100.sh CP-07a` | PF-01～03 | PASS | benchmark FPS=144 CPU=8%; benchmark_g100.sh |
| CP-07b | G7 libs/g100 | 2026-07-12 | 96ff709 | | | `verify_g100.sh CP-07b` | 无回归 | PASS | LV_USE_G100_LIB facade; path A/B verify |
| CP-08 | G8.0 viewport | | | | | `verify_g100.sh CP-08` | D3-06, D3-16～17 | | |
| CP-09 | G8.1 line/cb | | | | | `verify_g100.sh CP-09` | D3-07～08, AP-08 | | |
| CP-10 | G8.2 mesh | | | | | `verify_g100.sh CP-10` | D3-11, D3-18 | | |
| CP-11 | G8.3 scene | | | | | `verify_g100.sh CP-11` | D3-19, D3-01 | | tag: `g100-mvp-3d-vp` |
| CP-12 | G8.4 light | | | | | `verify_g100.sh CP-12` | D3-13 | | |
| CP-13 | G8.5 pick/load | | | | | `verify_g100.sh CP-13` | D3-14 | | |
| CP-14 | G8.6 theme | | | | | `verify_g100.sh CP-14` | D3-15 | | tag: `g100-mvp-3d-full` |

---

## 里程碑 Tag 记录

| Tag | CP | 日期 | 主仓 SHA | 说明 |
|-----|:--:|:--:|:--:|------|
| `g100-cp-00` | CP-00 | | | Bootstrap（可选补打） |
| `g100-mvp-2d` | CP-05 | 2026-07-12 | f293adc | 2D GPU MVP |
| `g100-mvp-3d-blit` | CP-06 | 2026-07-12 | 41a10e5 | 3D 纹理合成 |
| `g100-mvp-3d-vp` | CP-11 | | | 3D Draw Task 主路径 |
| `g100-mvp-3d-full` | CP-14 | | | 3D 栈完整 |

---

## 环境基线（WSLg 参考）

| 项 | 值 |
|----|-----|
| 配置 | `CONFIG=wayland-g100` |
| 分辨率 | 800×480 |
| stress | `LVGL_APP_DEMO=stress` `LV_DEMO_STRESS_DRAW_MULT=1` |
| 构建目录 | `build-g100-stress`（CP-00 起默认） |

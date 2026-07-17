# Orange Pi (Mali-400 / lima) EVGPU + Wayland-EGL 黑屏排查与修复

> 目标平台:Orange Pi PC(Allwinner H3,Mali-400 MP2,lima 驱动),weston(drm-backend,GL renderer),
> LVGL 配置 `configs/orangepi-evgpu.defaults`(EVGPU + wayland-egl,`LV_EVGR_BACKEND_GLES2`,`LV_COLOR_DEPTH 32`)。
> 现象:`lvglsim` 在板子上以 60 FPS 运行、trace 正常,但 HDMI 屏幕**看不到窗口 / 全黑**。

---

## 1. 结论速览(TL;DR)

黑屏是**三层因素叠加**,并非最初怀疑的"surface 未被 weston 映射":

| # | 因素 | 性质 | 处理 |
|---|---|---|---|
| 0 | 板子 `/tmp`(500M tmpfs)被旧 profiler 日志塞满 100% | **诊断干扰**(截图/日志 0 字节、scp 失败) | 清理 `/tmp` |
| 1 | 窗口 buffer alpha=0 且 surface `[not opaque]` → 整窗透明 | bug | 设置 opaque region |
| 2 | EGL config 硬编码要求 `samples==4`(4x MSAA),lima 的 MSAA resolve 未正确交给合成器 dmabuf | **真正根因** | config 优先选 `samples==0` |

修复后 `3dscene` 在板子上正确渲染(窗口模式与全屏均可)。两处代码修复均在 `lvgl` 子仓的 wayland-EGL 后端,对上游 LVGL 亦为通用 bug fix。

---

## 2. 排查过程与关键证据

### 2.1 排除"surface 未映射"(清理 /tmp 之后)
`/tmp` 满导致早期 `weston-screenshooter` 生成 0 字节、app stdout 0 字节,误判为"没有 surface"。清理后用 `weston-debug` 拿到真相:

- **proto 追踪**:xdg 握手完全正确
  ```
  get_xdg_surface → get_toplevel → set_title("LVGL Simulator")
  → wl_surface.commit(无 buffer) → xdg_surface.configure → ack_configure
  → wl_surface.attach(wl_buffer) → damage → commit      # buffer 在 ack 之后 attach,合规
  ```
- **scene-graph**:窗口确实已映射且在最顶层
  ```
  Layer 5  View 0 (role xdg_toplevel, 'LVGL Simulator'):
      position: (269,79) -> (1069,559)     # 800x480,在屏幕内
      [not opaque]                          # ← 线索1:非不透明
      dmabuf buffer  format: ARGB8888       # ← 真实 GPU buffer 已挂上
  ```

### 2.2 定位"透明" vs "内容黑"
设置 opaque region 后窗口变成**实心矩形**(不再透明),但仍是**黑色**——说明还有第二个问题:内容本身没呈现。

### 2.3 决定性实验:强制红清 + glReadPixels 回读
在 `egl_flush_cb` 里,`eglSwapBuffers` 前对 FBO 0 强制 `glClear` 成红,并回读:
```
DIAG after red-clear: fbo=0 rbo=0 px0=(255,0,0,255) glerr=0
```
- FBO 0 绑定正确、回读到**纯红**、无 GL 错误 → **渲染链路完全正确**;
- 但窗口仍黑 → 问题 100% 在 **`eglSwapBuffers` → 合成器 dmabuf** 这一段:交给 weston 的 buffer 不含 FBO0 内容。

### 2.4 命中根因:MSAA config
`select_config_cb` 给 EVGPU/nanovg 硬编码 `samples==4`。把它改成 `samples==0` 后:
```
DIAG chosen config 12: rgba=8888 depth=24 stencil=8 samples=0
```
窗口立刻显示**红色**(见 `docs/images/opi_redclear_verify.png`)→ 证实 lima 上 4x MSAA surface 的 resolve 未交给共享 dmabuf。移除红清后,真实 3D 场景正常渲染。

---

## 3. 修复内容

### 修复 A — opaque region(解决"透明→看不见")
文件:`lvgl/src/drivers/wayland/lv_wayland_window.c`

新增 helper,并在窗口创建、以及分辨率/尺寸变化(全屏、resize)时调用:
```c
/* Mark the whole window body opaque so the compositor ignores the buffer's
 * alpha channel. EGL/EVGPU buffers can carry alpha=0, which otherwise makes
 * the window fully transparent (appears black/invisible). Must be refreshed
 * whenever the surface is resized. */
static void set_window_opaque_region(lv_wl_window_t * window, int32_t w, int32_t h)
{
    struct wl_region * opaque = wl_compositor_create_region(lv_wl_ctx.wl_compositor);
    if(!opaque) {
        return;
    }
    wl_region_add(opaque, 0, 0, w, h);
    wl_surface_set_opaque_region(window->body, opaque);
    wl_region_destroy(opaque);
}
```
- 创建路径:`lv_wayland_xdg_configure_surface` 之后 → `set_window_opaque_region(...)` + `wl_surface_commit`
- resize 路径:`res_changed_event` 内 `resize_display` 之后再次调用

原理:通过 `wl_surface_set_opaque_region` 告知 weston 整窗不透明,合成器对该区域**不做 alpha 混合、直接取 RGB**,alpha=0 不再导致透明(并可省下层合成)。

### 修复 B — EGL config 优先无 MSAA(解决"内容全黑",真正根因)
文件:`lvgl/src/drivers/wayland/lv_wayland_backend_egl.c`,`wl_egl_select_config_cb`

改为**两遍**选择:先找 `samples==0` 的 stencil-capable 配置,找不到再回退 `samples==4`:
```c
/* Two passes: prefer a non-multisampled config first. On some GPUs
 * (e.g. Mali-400/lima) the MSAA resolve of a wl_egl_window's buffer is
 * not handed to the compositor correctly and the window shows up black,
 * even though the rendering itself is fine. A single-sample config avoids
 * that broken resolve path. Fall back to a 4x MSAA config if that is the
 * only stencil-capable option available. */
for(int pass = 0; pass < 2; ++pass) {
    const int wanted_samples = (pass == 0) ? 0 : 4;
    for(size_t i = 0; i < config_count; ++i) {
        ...
        const bool is_nanovg_compatible = (configs[i].renderable_type & EGL_OPENGL_ES2_BIT) != 0 &&
                                          configs[i].stencil == 8 && configs[i].samples == wanted_samples;
        ...
        if(is_window && resolution_matches && config_cf == target_cf && is_compatible_with_draw_unit) {
            return i;
        }
    }
    if(!LV_USE_DRAW_NANOVG && !LV_USE_DRAW_EVGPU) break;
}
```
说明:EVGPU 的 evgr 是矢量渲染器,自带覆盖/stencil 抗锯齿,不依赖 MSAA;因此 `samples==0` 在功能上等价,且规避 lima 的 resolve 缺陷。桌面等支持 MSAA 的平台仍可回退到 4x。

---

## 4. 修复后的完整通路

```
[构建] 交叉工具链(Bootlin gcc9.3/glibc2.31 + opi-sysroot)
        cmake/toolchain-orangepi-armhf.cmake, configs/orangepi-evgpu.defaults, -DLVGL_APP_DEMO=3dscene
        → build-cross-armhf/bin/lvglsim → scp 到板子 /home/orangepi/lvglsim_cross

[初始化] lv_wayland_window_create:
   create_surface(body) → xdg_surface/toplevel/set_title
   → init_display(EGL): select_config[修复B, 优先 samples==0] → wl_egl_window → EGLSurface → context → makeCurrent
   → xdg configure/ack 握手
   → set_window_opaque_region(body)[修复A] + commit

[每帧] LVGL 刷新(16ms) → EVGPU draw:
   主屏层 evgrluBindFramebuffer(NULL) = 默认 FBO 0
   2D(GLES2 #version 100) + 3D_MESH(立方体+网格) 直接画进 FBO 0
   → egl_flush_cb: eglSwapBuffers(单采样, resolve 到共享 dmabuf ARGB8888)
                   → wl_surface_frame/damage/commit(body)
   → frame_done → lv_display_flush_ready

[合成] weston(drm-backend, GL renderer/lima):
   导入 body 的 dmabuf(EGL_EXT_image_dma_buf_import)
   → body 已 opaque → 忽略 alpha 取 RGB
   → GL 合成 → DRM/KMS → HDMI-A-1 1920x1080@60
```

---

## 5. 验证结果

| 场景 | 结果 | 截图 |
|---|---|---|
| 修复前(全屏) | 全黑(仅鼠标) | `docs/images/opi_blackscreen_before.png` |
| 红清验证(samples==0) | 窗口显示纯红,证实呈现链路 | `docs/images/opi_redclear_verify.png` |
| 3dscene 窗口模式 | 橙色立方体 + 透视网格 | `docs/images/opi_3dscene_windowed.png` |
| 3dscene 全屏(走 resize 路径) | 整屏正常渲染 | `docs/images/opi_3dscene_fullscreen.png` |

---

## 6. 复现 / 排查备忘

- 板子 `/tmp` 是 500M tmpfs,profiler 输出极大,务必先 `df -h /tmp` 并清理旧 `run*.out`,否则截图/日志会静默变 0 字节。
- weston 以 root 运行,socket 在 `/run/user/0`;客户端与 `weston-screenshooter`/`weston-debug` 需 `sudo` 且设 `XDG_RUNTIME_DIR=/run/user/0 WAYLAND_DISPLAY=wayland-0`。
- 关键诊断工具:`weston-debug proto`(协议握手)、`weston-debug scene-graph`(是否映射/是否 opaque/buffer 类型/位置)。
- `weston-screenshooter` 抓不到某窗口≠物理黑屏(需排除直扫描/dmabuf 抓取限制);但本例中 opaque+红清双重验证已确认为真·呈现问题。
- 交叉编译增量 ~2s,是快速二分定位(插桩→推板→截图)的关键。

---

## 7. 受影响文件一览

- `lvgl/src/drivers/wayland/lv_wayland_window.c` — 修复 A(opaque region)
- `lvgl/src/drivers/wayland/lv_wayland_backend_egl.c` — 修复 B(config 优先 samples==0)
- `configs/orangepi-evgpu.defaults` — 板子精简配置(EVGPU + wayland-egl,GLES2)
- `cmake/toolchain-orangepi-armhf.cmake` — armv7l 交叉工具链

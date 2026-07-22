# LVGL 3D Widget Gap Analysis (2026-07-19)

## Executive summary

LVGL 9.x has **4 official 3D widgets** (`3dtexture`, `3dmesh`, `3dlight`, `3dviewport`) and a complete underlying draw pipeline (`lv_draw_3d_*` in `lvgl/src/draw/`). For a single rendered 3D object in a UI, this is sufficient. For a real 3D application (game, configurator, data viz), several **high-leverage widgets are missing**.

This document ranks missing widgets by ROI (return on investment = how many apps need it / how hard to implement), and proposes concrete API headers for the top two.

## What already exists

### Widgets (in `lvgl/src/widgets/`)

| Widget | Purpose | Files |
|---|---|---|
| `lv_3dtexture` | Display a backend texture as a 2D quad | `widgets/3dtexture/` |
| `lv_3dmesh` | Render a 3D triangle mesh (with Phong, depth, cull, pick) | `widgets/3dmesh/` |
| `lv_3dlight` | Directional + point lights | `widgets/3dlight/` |
| `lv_3dviewport` | Render target owning camera, clear, render-callback | `widgets/3dviewport/` |

### Draw pipeline (in `lvgl/src/draw/`)

`lv_draw_3d.c` (dispatch), `lv_draw_3d_pass.c` (render pass), `lv_draw_3d_camera.c` (view/proj MVP), `lv_draw_3d_light.c`, `lv_draw_3d_mesh.c`, `lv_draw_3d_line.c`, `lv_draw_3d_clear.c`, `lv_draw_3d_callback.c`, `lv_draw_3d_scene.c`, `lv_3d_pick.c` (ray pick).

### Underlying types (in `lvgl/include/lvgl/draw/`)

- `lv_3d_camera_t` (yaw, pitch, distance, target, fov_y, near_z, far_z) — already a complete data type
- `lv_3dpoint_t` (3 floats), `lv_3dray_t`, `lv_3d_pick_hit_t`
- 7 `LV_DRAW_TASK_TYPE_3D_*` enum values (see `lvgl/include/lvgl/draw/lv_draw.h`)

### Config switches

- `LV_USE_3DTEXTURE` — 3dtexture widget + draw path
- `LV_USE_3DMESH` — 3dmesh widget
- `LV_USE_3DLIGHT` — 3dlight widget
- `LV_USE_3DVIEWPORT` — 3dviewport widget
- `LV_USE_3D_DRAW_TASKS` — 6 3D draw tasks (VIEWPORT/CLEAR/LINE/CALLBACK/MESH/SCENE)
- `LV_USE_DRAW_3D` — 3D draw module
- `LV_USE_VECTOR_GRAPHIC` — vector (not 3D but related)
- `LV_USE_GPU` — master switch

### Demos

- `lvgl/demos/3dscene/lv_demo_3dscene.c`
- `lvgl/demos/3dview/lv_demo_3dview.c`
- `lvgl/demos/3dviewport/lv_demo_3dviewport.c`

### Loaders

- `lvgl/src/libs/gltf/` — glTF data + animation parser
- `lv_3dmesh_load_obj(path)` — Wavefront .obj loader (in mesh widget)

## Gap analysis

### Tier 1 — Critical (used by almost every 3D app)

| Missing widget | Why it matters | Difficulty |
|---|---|---|
| **`lv_3dcamera`** (independent) | Currently camera is buried inside `lv_3dviewport`; you can't share one camera between two viewports (split-screen, picture-in-picture, minimap) or set up a camera that multiple meshes follow. Every CAD viewer, game, and configurator needs this. | Low — `lv_3d_camera_t` already exists in `lv_draw_3d_camera.h`; just wrap it in a widget. |
| **PBR material** on `lv_3dmesh` | Today mesh only has `set_color(rgba)` (flat). Real assets need albedo + normal + metallic + roughness + AO + emission maps. | Medium-High — needs shader path changes, not just API. |
| **More light types** | `lv_3dlight` only has `set_directional` + `set_point`. Need: spot (cone, falloff), ambient, hemisphere, area. | Low — additive blending into light uniform buffer. |
| **Skybox** | Every outdoor / game / architectural scene needs a 6-face background. Currently you have to fake it as a giant inverted cube with custom UVs. | Low — same as `lv_3dmesh` but with depth-test off + back-face cull off. |

### Tier 2 — Important

| Missing widget | Why it matters | Difficulty |
|---|---|---|
| **`lv_3dscene`** as a widget | Today you call `lv_3dmesh_submit_tree()` manually in a viewport render callback. A scene-graph container would auto-traverse, apply parent transforms, do frustum culling, sort transparent-vs-opaque. | High — needs new traversal code. |
| **3D keyframe animator** | Right now animations are static (no skeleton / no path). glTF assets commonly have animations; you need a player. | High — integrates with `lvgl/src/libs/gltf/`. |
| **Post-processing (bloom, tone mapping, FXAA)** | Visual quality of any modern 3D UI. Not strictly required for utility apps. | Medium — add as `lv_3dviewport` extension. |
| **AABB / OBB / Frustum helpers** | Performance — cull off-screen objects. Picking. | Low — pure math, no GPU. |

### Tier 3 — Nice to have

| Missing widget | Why it matters | Difficulty |
|---|---|---|
| 3D trigger volume | Game-like interaction zones. | Low. |
| Billboard particle | Visual flair (smoke, fire, snow). | Medium. |
| 3D Physics engine | Out of scope for a GUI lib, but **lightweight** AABB collision detector is reasonable. | High. |
| VR / stereo camera | Niche (training / CAD). | Medium. |

## Tier 1 widget specs (full API headers written)

Full API headers for the two highest-ROI widgets are already in the LVGL include tree as of this commit:

- `lvgl/include/lvgl/widgets/lv_3dcamera.h` — independent 3D camera widget
- `lvgl/include/lvgl/widgets/lv_3dskybox.h` — 6-face cubemap / equirectangular / solid sky

These are **header-only proposals** — they declare the intended API but have no `.c` implementation yet. Review the headers, then we implement the `.c` files in a follow-up commit.

## Recommendation (commit ordering for the next 2-3 weeks)

1. Implement `lv_3dcamera.c` against the existing `lv_3d_camera_t` struct (one file, ~120 lines).
2. Implement `lv_3dskybox.c` against the existing `lv_3dmesh` infrastructure (auto-skip depth + back-face cull). One file, ~150 lines.
3. Add spot/ambient/hemisphere variants to `lv_3dlight`. Modify `lv_3dlight.c` and the `lv_draw_3d_light.c` uniform layout. ~80 lines of changes.
4. (Later) PBR material on `lv_3dmesh` — needs shader work; not a one-day change.
5. (Later) `lv_3dscene` as widget — needs a scene-graph traversal layer.

## Files

- `docs/lv_3d_widgets_api_2026_07_19.md` — full API reference for the 4 existing widgets
- `docs/lv_3d_widget_gap_2026_07_19.md` — this file
- `lvgl/include/lvgl/widgets/lv_3dcamera.h` — proposed header
- `lvgl/include/lvgl/widgets/lv_3dskybox.h` — proposed header

## How to extend

Each `lv_3d*_create()` follows the same LVGL widget-class pattern as `lv_3dmesh_create()`:

```c
LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_3dxxx_class;

const lv_obj_class_t lv_3dxxx_class = {
    .constructor_cb = lv_3dxxx_constructor,
    .setter_cb      = lv_3dxxx_setter,
    .event_cb       = NULL,
    .base_class     = &lv_obj_class,
    .instance_size  = sizeof(lv_3dxxx_t),
    .name           = "lv_3dxxx",
};
```

Add a `<widget>/lv_<widget>.c` under `lvgl/src/widgets/`, list it in the
parent `CMakeLists.txt` (the widgets are auto-discovered), declare the public
class in `lvgl/include/lvgl/widgets/`, and add a `LV_USE_<WIDGET>` switch in
`lvgl/include/lvgl/config/lv_conf_internal.h` and the Kconfig template
`lvgl/lv_conf_template.h`.

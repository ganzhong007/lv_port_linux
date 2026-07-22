# LVGL 3D Widgets API Reference (2026-07-19)

**Scope**: All 4 currently-shipped 3D widgets in `lvgl/src/widgets/`. Source files are
the public headers in `lvgl/include/lvgl/widgets/`. Every API listed here is the
current signature extracted from the `.h` files; nothing inferred.

**Conventions**:
- `lv_obj_t *` first arg is the widget instance returned by `_create()`.
- Coordinates use `lv_3dpoint_t` (3 floats, see `lv_draw_3d_camera.h`).
- Matrices are 4×4 column-major float[16] (e.g. `mvp_out[LV_3D_CAMERA_MVP_SIZE]`).
- `lv_3dtexture_id_t` is the backend texture handle (e.g. GL `unsigned int`).
- Each widget's main file `#includes "../config/lv_conf_internal.h"` and is
  guarded by `#if LV_USE_3Dxxx`.

---

## 1. `lv_3dtexture` — display a 3D-rendered texture as a 2D quad

| Function | Signature | Description |
|---|---|---|
| `lv_3dtexture_create` | `lv_obj_t * lv_3dtexture_create(lv_obj_t * parent)` | Create a 3D-texture widget. Object size should be set manually to match the source texture dimensions. |
| `lv_3dtexture_set_src` | `void lv_3dtexture_set_src(lv_obj_t * obj, lv_3dtexture_id_t id)` | Set the source texture handle. `id` comes from the 3D backend (e.g. `glGenTextures`). |
| `lv_3dtexture_set_flip` | `void lv_3dtexture_set_flip(lv_obj_t * obj, bool h_flip, bool v_flip)` | Flip the texture horizontally/vertically. |

**File**: `lvgl/include/lvgl/widgets/lv_3dtexture.h` (86 lines)
**Source**: `lvgl/src/widgets/3dtexture/lv_3dtexture.c`
**Class**: `lv_3dtexture_class`
**Config**: `LV_USE_3DTEXTURE`

---

## 2. `lv_3dmesh` — render a 3D triangle mesh

| Function | Signature | Description |
|---|---|---|
| `lv_3dmesh_create` | `lv_obj_t * lv_3dmesh_create(lv_obj_t * parent)` | Create a 3D-mesh widget. The mesh is a unit cube by default; use `set_box()` to scale, or `load_obj()` to load a model file. |
| `lv_3dmesh_set_color` | `void lv_3dmesh_set_color(lv_obj_t * obj, lv_color_t color, lv_opa_t opa)` | Set base color and opacity (single solid material, no PBR). |
| `lv_3dmesh_set_transform` | `void lv_3dmesh_set_transform(lv_obj_t * obj, float tx, float ty, float tz, float rot_x, float rot_y, float rot_z, float sx, float sy, float sz)` | Set world transform: translate / rotate-XYZ (degrees) / scale. |
| `lv_3dmesh_set_box` | `void lv_3dmesh_set_box(lv_obj_t * obj, float sx, float sy, float sz)` | Set the unit-cube size (default 1×1×1). |
| `lv_3dmesh_set_depth_test` | `void lv_3dmesh_set_depth(lv_obj_t * obj, bool enable)` | Toggle depth-buffer test (Z occlusion). |
| `lv_3dmesh_set_cull_face` | `void lv_3dmesh_set_cull_face(lv_obj_t * obj, bool enable)` | Toggle back-face culling. |
| `lv_3dmesh_set_phong` | `void lv_3dmesh_set_phong(lv_obj_t * obj, bool enable)` | Toggle Phong shading. |
| `lv_3dmesh_set_shininess` | `void lv_3dmesh_set_shininess(lv_obj_t * obj, float shininess)` | Phong specular exponent (higher = tighter highlight). |
| `lv_3dmesh_set_pickable` | `void lv_3dmesh_set_pickable(lv_obj_t * obj, bool pickable)` | Enable ray-mesh intersection for this mesh (used by `pick_at_tree`). |
| `lv_3dmesh_load_obj` | `lv_result_t lv_3dmesh_load_obj(lv_obj_t * obj, const char * path)` | Load a Wavefront `.obj` file from path (filesystem or LVGL asset). Returns `LV_RESULT_OK` on success. |
| `lv_3dmesh_pick_at_tree` | `bool lv_3dmesh_pick_at_tree(lv_obj_t * root, const lv_3dray_t * ray, lv_3d_pick_hit_t * hit)` | Ray-cast from `ray` into the mesh tree under `root`. Fills `hit` and returns true on hit. |
| `lv_3dmesh_submit_tree` | `void lv_3dmesh_submit_tree(lv_obj_t * root, lv_layer_t * pass_layer)` | Walk the widget tree under `root` and submit all 3D meshes to the 3D pass layer (called by viewport render cb). |

**File**: `lvgl/include/lvgl/widgets/lv_3dmesh.h` (74 lines)
**Source**: `lvgl/src/widgets/3dmesh/lv_3dmesh.c`, `lv_3dmesh_obj.c`
**Class**: `lv_3dmesh_class`
**Config**: `LV_USE_3DMESH`

---

## 3. `lv_3dlight` — 3D scene light source

| Function | Signature | Description |
|---|---|---|
| `lv_3dlight_create` | `lv_obj_t * lv_3dlight_create(lv_obj_t * parent)` | Create a 3D light widget. A light is invisible itself; it just contributes to lighting calculations when meshes are rendered. |
| `lv_3dlight_set_directional` | `void lv_3dlight_set_directional(lv_obj_t * obj, float dx, float dy, float dz, lv_color_t color, lv_opa_t opa, float intensity)` | Set a directional (parallel) light, defined by the direction vector (dx,dy,dz). Direction is the direction the light SHINES, e.g. `(1,0,0)` shines toward +X. |
| `lv_3dlight_set_point` | `void lv_3dlight_set_point(lv_obj_t * obj, float x, float y, float z, lv_color_t color, lv_opa_t opa, float intensity, float range)` | Set a point light at world position (x,y,z) with falloff `range`. |
| `lv_3dlight_submit_tree` | `void lv_3dlight_submit_tree(lv_obj_t * root, lv_layer_t * pass_layer)` | Walk the tree under `root` and submit all lights to the 3D pass layer. |

**File**: `lvgl/include/lvgl/widgets/lv_3dlight.h` (57 lines)
**Source**: `lvgl/src/widgets/3dlight/lv_3dlight.c`
**Class**: `lv_3dlight_class`
**Config**: `LV_USE_3DLIGHT`

---

## 4. `lv_3dviewport` — a renderable 3D scene viewport

| Function | Signature | Description |
|---|---|---|
| `lv_3dviewport_create` | `lv_obj_t * lv_3dviewport_create(lv_obj_t * parent)` | Create a 3D viewport widget. The viewport owns a default `lv_3d_camera_t` and a default depth-clear. |
| `lv_3dviewport_set_clear_color` | `void lv_3dviewport_set_clear_color(lv_obj_t * obj, lv_color_t color, lv_opa_t opa)` | Set the background clear color. |
| `lv_3dviewport_set_clear_depth` | `void lv_3dviewport_set_clear_depth(lv_obj_t * obj, bool clear_depth)` | Toggle depth-buffer clear every frame. |
| `lv_3dviewport_set_grid_visible` | `void lv_3dviewport_set_grid_visible(lv_obj_t * obj, bool visible)` | Show/hide a debug ground-grid in the scene. |
| `lv_3dviewport_set_render_cb` | `void lv_3dviewport_set_render_cb(lv_obj_t * obj, lv_draw_3d_cb_t cb, void * user_data)` | Set the user render callback. Inside this cb you typically call `lv_3dmesh_submit_tree()`, `lv_3dlight_submit_tree()`, etc. to push 3D content into the pass layer. |
| `lv_3dviewport_get_camera` | `lv_3d_camera_t * lv_3dviewport_get_camera(lv_obj_t * obj)` | Get the raw `lv_3d_camera_t` (yaw, pitch, distance, target, fov_y, near_z, far_z). Mutate fields directly or use `lv_3d_camera_*` helpers in `lv_draw_3d_camera.h`. |
| `lv_3dviewport_set_orbit` | `void lv_3dviewport_set_orbit(lv_obj_t * obj, float yaw, float pitch, float distance)` | Set orbit camera: spherical coords around the target. |
| `lv_3dviewport_get_ray_from_point` | `lv_3dray_t lv_3dviewport_get_ray_from_point(lv_obj_t * obj, int32_t x, int32_t y)` | Compute a world-space ray from screen pixel (x,y) (origin + direction). |
| `lv_3dviewport_pick_at` | `bool lv_3dviewport_pick_at(lv_obj_t * obj, int32_t x, int32_t y, lv_3d_pick_hit_t * hit)` | Pick (ray cast) at screen (x,y); fills `hit` if any pickable mesh was hit. |

**File**: `lvgl/include/lvgl/widgets/lv_3dviewport.h` (67 lines)
**Source**: `lvgl/src/widgets/3dviewport/lv_3dviewport.c`
**Class**: `lv_3dviewport_class`
**Config**: `LV_USE_3DVIEWPORT`

---

## Underlying data types (in `lvgl/include/lvgl/draw/`)

| Type | Header | Fields |
|---|---|---|
| `lv_3dpoint_t` | `lv_draw_3d_camera.h` | 3 floats (x,y,z) |
| `lv_3dray_t` | `lv_draw_3d_pick.h` | `origin` + `direction` (both `lv_3dpoint_t`) |
| `lv_3d_pick_hit_t` | `lv_draw_3d_pick.h` | `point` + `distance` (float) |
| `lv_3d_camera_t` | `lv_draw_3d_camera.h` | `yaw`, `pitch`, `distance`, `target`, `fov_y`, `near_z`, `far_z` (all floats) |
| `lv_3dtexture_id_t` | (varies) | `unsigned int` (GL texture handle) |
| `LV_3D_CAMERA_MVP_SIZE` | `lv_draw_3d_camera.h` | `16` (4×4 matrix) |

---

## Quick reference by use case

| Use case | Widgets needed |
|---|---|
| Display a 3D-rendered image in a 2D scene | `lv_3dtexture` |
| Render a single 3D object (cube, sphere, .obj) | `lv_3dmesh` (+ optional `lv_3dlight`) |
| Render a full 3D scene with camera and lighting | `lv_3dviewport` + `lv_3dmesh` + `lv_3dlight` |
| Pick 3D objects with the mouse | `lv_3dviewport_pick_at` + `lv_3dmesh_set_pickable` + `lv_3dmesh_pick_at_tree` |

## Class registration summary

| Widget | Class variable | File |
|---|---|---|
| 3dtexture | `lv_3dtexture_class` | `lvgl/src/widgets/3dtexture/lv_3dtexture.c` |
| 3dmesh | `lv_3dmesh_class` | `lvgl/src/widgets/3dmesh/lv_3dmesh.c` |
| 3dlight | `lv_3dlight_class` | `lvgl/src/widgets/3dlight/lv_3dlight.c` |
| 3dviewport | `lv_3dviewport_class` | `lvgl/src/widgets/3dviewport/lv_3dviewport.c` |

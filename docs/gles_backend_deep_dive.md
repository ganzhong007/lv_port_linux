# ThorVG / Rive / Cairo — GLES2 Rendering Deep Dive

Three in-depth source-code analyses of GPU-accelerated 2D rendering engines,
with focus on patterns applicable to **GLES 2.0** (Mali-400) and the evgpuganesh
draw unit in LVGL.

---

## 1. ThorVG GL — Enterprise-Grade Stage-Buffer Architecture

**Source:** `thorvg/src/renderer/gpu_engine/gl/` (~30 files)  
**GLES Target:** 3.0+ only — **no GLES2 path**  
**Code style:** C++ with RAII, smart pointers, template Array<>

### File Map

| File | Role |
|------|------|
| `tvgGlGpuBuffer.h/.cpp` | `GlGpuBuffer` (single GL buffer) + `GlStageBuffer` (VAO + 3 buffers) |
| `tvgGlSolidBatch.h/.cpp` | Pure-color draw-call merger |
| `tvgGlStencilCoverBatch.h/.cpp` | Stencil-then-cover batching with overlap detection |
| `tvgGlRenderPass.h/.cpp` | Per-pass FBO + task list |
| `tvgGlRenderTask.h/.cpp` | Per-draw-command: program + vertex layout + bindings |
| `tvgGlShader.h/.cpp` + `tvgGlShaderSrc.h/.cpp` | GLSL 300 es source strings (~65KB) |
| `tvgGlProgram.h/.cpp` | Program link + static `mCurrentProgram` cache |
| `tvgGlGeometry.cpp` | Tessellation: shape → triangles/stroke/image mesh |
| `tvgGlTextureMgr.h/.cpp` | Texture cache keyed by `RenderSurface*` |
| `tvgGlRenderTarget.h/.cpp` | FBO with MSAA + resolve |
| `tvgGlRenderer.h/.cpp` | Orchestrator: prepare/render/flush |

### Stage Buffer — Most Valuable Pattern for evgpuganesh

Three separate GPU buffers behind one VAO:

| Buffer | GL Target | Contents |
|--------|-----------|----------|
| `mGpuBuffer` | `GL_ARRAY_BUFFER` | Positions, transforms, gradient blocks |
| `mGpuAuxBuffer` | `GL_ARRAY_BUFFER` | Per-vertex colors (RGBA), stencil cover quads |
| `mGpuIndexBuffer` | `GL_ELEMENT_ARRAY_BUFFER` | Triangle indices |

**CPU accumulation** — double-buffered staging arrays:

```cpp
Array<uint8_t> mStageBuffer;  // grows via push()/reserve()
Array<uint8_t> mAuxBuffer;
Array<uint8_t> mIndexBuffer;

bool flushToGPU() {
    mGpuBuffer.updateBufferData(GL_ARRAY_BUFFER, mStageBuffer);
    mGpuAuxBuffer.updateBufferData(GL_ARRAY_BUFFER, mAuxBuffer);
    mGpuIndexBuffer.updateBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
    // clear all CPU buffers
    mStageBuffer.clear();
    mAuxBuffer.clear();
    mIndexBuffer.clear();
    return true;
}
```

`push()` copies data to staging, returns byte offset. `reserve()` returns
write-pointer + offset without copy. UBO data uses `alignOffset()` to meet
`GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT`.

**For GLES2:** Replace VAO with direct `glBindBuffer` per draw; replace UBO
writes with per-uniform `glUniform*` calls. The CPU-side staging pattern is
entirely portable.

### SolidBatch — Pure-Color Composite

Three-phase state machine:

1. **`appendable()`** — checks pass/task/program/viewBounds continuity
2. **`promote()`** — upgrades a single-color draw to per-vertex color when a
   second shape with same color arrives: adds 2nd vertex layout (index=1,
   size=4, type=GL_UNSIGNED_BYTE, normalized=GL_TRUE) pointing at `mGpuAuxBuffer`
3. **`append()`** — extends promoted task: reserves space, builds positions +
   colors + remapped indices, updates draw range, merges viewports

Batch resets on: pass change, program change, or `appendable()` false.

**For GLES2:** The algorithm is pure CPU bookkeeping, directly portable.
The per-vertex color via `glVertexAttribPointer` with `GL_UNSIGNED_BYTE`
normalized is efficient and GLES2-compatible.

### StencilCoverBatch — Merge by Sorted Bounds

- Overlap detection via bounds sorted by X or Y
- Caps: 512 regions, 262K vertices, 1M indices (native); half for WebAssembly
- `merge(stencilTask)` validates contiguous memory in stage buffer
- `mergeCover(coverTask)` validates contiguous vertex data in aux buffer
- Cover merge checks `solidCoverLayout()` — exactly position + color attributes

### Shader Compilation

```cpp
// tvgGlShader.cpp
#if defined(THORVG_GL_TARGET_GLES)
    shaderPack[0] = "#version 300 es\n";
#else
    shaderPack[0] = "#version 330 core\n";
#endif
```

For GLES2: change to `#version 100\n`, remove `highp` from fragment (or guard
with `GL_FRAGMENT_PRECISION_HIGH`), replace `in/out` with `attribute/varying`.

### State Caching

| Cache | Scope | Mechanism |
|-------|-------|-----------|
| Current program | Static | `static uint32_t mCurrentProgram` — skip `glUseProgram` on repeat |
| UBO alignment | Static | Queried once |
| Texture | Per-instance | `TextureMgr::find()` keyed by `RenderSurface*` |
| Batch continuity | Per-batch | Pass+task pointer comparisons |
| Render target | Pool | Reuse FBOs by (width,height) |

### GLES2 Porting Checklist

What breaks on Mali-400 and how to fix:

| ThorVG Feature | GLES2 Fix |
|----------------|-----------|
| `#version 300 es` | `#version 100` |
| `glGenVertexArrays` | Replace with direct `glBindBuffer` each draw |
| `glUniformBlockBinding` | Individual `glUniformMatrix4fv` / `glUniform4fv` |
| `glBlitFramebuffer` | Skip (no MSAA) or shader-based copy |
| `glRenderbufferStorageMultisample` | `glRenderbufferStorage` |
| `glInvalidateFramebuffer` | Remove call |
| `GL_RGBA8` | `GL_RGBA` |
| `highp` in FS | Guard with `#ifdef GL_FRAGMENT_PRECISION_HIGH` |
| UBO alignment | Track manually, pack to std140-like layout |

**Extensions to check on Mali-400:** `GL_OES_framebuffer_object`,
`GL_OES_rgb8_rgba8`, `GL_OES_depth24`, `GL_OES_packed_depth_stencil`,
`GL_EXT_discard_framebuffer`.

---

## 2. Rive — GPU-Only Tessellation + PLS (GLES 3.0+)

**Source:** `rive/renderer/` (~20 files in `src/gl/` + `src/shaders/`)  
**GLES Target:** 3.0 minimum (desktop: 4.2+) — **no GLES2 path**  
**Code style:** Modern C++17 with hybrid GL/gen5 render graph

### GPU Tessellation — Rive's Core Innovation

```
CPU: path → cubic Bezier → TessVertexSpan (control points + segment count)
        ↓
[Vertex Buffer: TessVertexSpan structs]
        ↓
Pass 1: Tessellate
  VS: per-instanced quad → compute curve position + tangent
  FS: Wang's formula → write to m_tessVertexTexture
        ↓
[Texture: coverage quads per instance, rendered to TESSDATA4 texels]
        ↓
Pass 2: Draw Path
  VS: texelFetch from texture → rebuild path through patch geometry
  FS: evaluate coverage, blend color
```

**Wang's formula** (in `tessellate.glsl`):
```glsl
float2 d0 = mat * (-2.*p1 + p2 + p0);
float2 d2 = mat * (-2.*p2 + p3 + p1);
float m = max(dot(d0, d0), dot(d1, d1));
float n = max(ceil(sqrt(.75 * 4. * sqrt(m))), 1.);
```

### Draw Call Architecture — Heavy Instancing

```
Path patches:  glDrawElementsInstanced(indices=patch, instances=spans)
Triangles:     glDrawArraysInstanced(GL_TRIANGLE_STRIP, instances=tris)
Image rects:   glDrawElementsInstanced(indices=rect, instances=images)
```

With `InstanceChunker` + `GLFlushInjector` to work around mobile GPU driver
bugs (Mali/PowerVR/Adreno <600 limit: 8191 instances per flush).

### PLS (Pixel Local Storage) — 3 Backends

| Backend | Requirement | Scope |
|---------|-------------|-------|
| `PLSImplEXTNative` | `EXT_shader_pixel_local_storage` + FBFETCH | Android only |
| `PLSImplWebGL` | `ANGLE_shader_pixel_local_storage` | WebGL/ANGLE |
| `PLSImplRWTexture` | `ARB_shader_image_load_store` | Desktop GL 4.2+ |

PLS planes: Color (RGBA8), Clip (R32UI), TempColor (RGBA8), Coverage (R32UI).

### Super Shader via Define Composition

```cpp
std::vector<const char*> defines;
defines.push_back(GLSL_DRAW_PATH);
defines.push_back(GLSL_ENABLE_CLIPPING);
defines.push_back(GLSL_PLS_IMPL_EXT_NATIVE);
```

Variants across: insertion mode × draw type × shader features (clipping,
clipRect, advancedBlend, feather, evenOdd, dither). Results in ~100+ unique
programs.

### GLES2 Feasibility: Not Directly Portable

| Rive Feature | GLES2 Equivalent |
|--------------|------------------|
| `texelFetch` (integer texture) | Cannot emulate — use CPU tessellation |
| `glVertexAttribDivisor` / instancing | Unroll loop or replicate vertices |
| `GL_UNIFORM_BUFFER` | Separate `glUniform*` calls |
| Image load/store / atomics | Stencil-then-cover (classic approach) |
| PLS | Not available — multi-pass stencil |
| `#version 300 es` in/out | `#version 100` attribute/varying |

**What IS portable:**
- `GLState` class with dirty-bit caching (directly reusable)
- `InstanceChunker` / `GLFlushInjector` pattern (flush-before-overflow)
- Define-composition shader system (replace GLSL definitions with `#ifdef`)
- Feather atlas backend fallback chain (enumerated priority list)

---

## 3. Cairo GL — CPU Spans + GPU Fill (GLES2 Native)

**Source:** `cairo/src/cairo-gl-*.c` (15 files, ~14K lines)  
**GLES Target:** 2.0 native — desktop GL also supported  
**Code style:** C89, flat structs, manual dispatch tables

### File Map

| File | Role |
|------|------|
| `cairo-gl-composite.c` | Core draw: vertex emission + GL state setup |
| `cairo-gl-device.c` | Device init, extension detection, VBO management |
| `cairo-gl-spans-compositor.c` | **Default compositor**: CPU spans → GL triangles |
| `cairo-gl-msaa-compositor.c` | MSAA stencil-then-cover compositor |
| `cairo-gl-traps-compositor.c` | Old trapezoid compositor |
| `cairo-gl-operand.c` | Source/mask operand abstraction |
| `cairo-gl-shaders.c` | Runtime shader generation by string concatenation |
| `cairo-gl-gradient.c` | Gradient → 1D texture baking on CPU |
| `cairo-gl-surface.c` | Surface/texture creation, format table |
| `cairo-gl-glyphs.c` | Glyph caching via rtree atlas |

### Span Compositor — The Default Path

```
cairo_fill()/stroke()
       ↓
CPU scan conversion (pixman)
       ↓
half-open spans: {x1, x2, coverage}
       ↓
emit(): write 6 vertices per span (2 triangles) to client VBO
       ↓
glDrawArrays(GL_TRIANGLES, count)
```

Four span emitter variants:
- `bounded_opaque_spans` — coverage=1.0, no mask
- `bounded_spans` — with alpha scaling
- `unbounded_spans` — fill transparent spans outside visible area
- `clipped_spans` — unbounded + preserve clip

**Box optimization** — aligned integer boxes use `fill_boxes` directly
(2 triangles per box), bypassing scan conversion entirely.

### VBO Strategy — Simplest Possible

```c
ctx->vb = malloc(ctx->vbo_size);  // default 1MB, client memory
// used as pointer in glVertexAttribPointer
```

**No `glMapBuffer`, no GPU VBO at all** — data lives in malloc'd memory,
GL reads via client pointer. Flush when full (`offset > vbo_size`) or state
changes.

Vertex layout for spans:
```
[position: 2f][src_texcoord: 0/2f][mask_texcoord: 0/2f][coverage: 1f]
```

### Gradient Baking — CPU Pre-render

```c
// cairo-gl-gradient.c
sample_width = _cairo_gl_gradient_sample_width(stops, max_texture_size);
// pixman renders gradient to 1×N BGRA buffer
pixman_image_create_linear_gradient(...);
pixman_image_composite32(PIXMAN_OPERATOR_SRC, ...);
// upload to GL texture
glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, sample_width, 1, ...);
```

Cached by `cairo_cache_t` (hash table, max 4096 entries, keyed by stop
array hash). Fragment shader computes 1D texcoord from position:

```glsl
// LINEAR_GRADIENT: 1D coord = dot(pos, gradient_vector)
// RADIAL_GLADIENT_A0: t = 0.5*C/B
// RADIAL_GRADIENT_NONE/EXT: t = (B ± sqrt(B² - a*C)) / a
```

### Shader Generation — Runtime Concatenation

```c
// cairo-gl-shaders.c
// Per (src_type, mask_type) pair:
snprintf(fs_source, ...,
    "uniform sampler2D source_sampler;\n"
    "uniform sampler2D mask_sampler;\n"
    "%s"  // per-operand get_*() functions
    "%s"  // border_fade() if needed
    "%s"  // get_source() / get_mask()
    "void main() { gl_FragColor = get_source() * get_mask().a; }"
);

// Cached: 64-entry hash by (vertex, src, mask, dest, use_coverage, ...)
```

**4 operand types:** CONSTANT, TEXTURE, LINEAR_GRADIENT, RADIAL_GRADIENT

### GLES2 vs Desktop Detection

```c
// cairo-gl-device.c
if (strstr(extensions, "GL_ES_VERSION_2_0"))
    flavor = CAIRO_GL_FLAVOR_ES;
else
    flavor = CAIRO_GL_FLAVOR_DESKTOP;
```

Feature detection per flavor:

| Feature | Desktop GL | GLES2 |
|---------|-----------|-------|
| NPOT textures | `GL_ARB_texture_non_power_of_two` | `GL_OES_texture_npot` / `GL_IMG_texture_npot` |
| BGRA format | Always | `EXT_texture_format_BGRA8888` |
| MapBuffer | Core | `GL_OES_mapbuffer` |
| Packed depth-stencil | `GL_EXT_packed_depth_stencil` | `GL_OES_packed_depth_stencil` |
| MSAA | `GL_EXT_framebuffer_multisample` | `EXT_multisampled_render_to_texture` |
| Read format BGRA | Always | `EXT_read_format_bgra` + little-endian |

### GLES2 Workarounds

- **NPOT repeat emulation:** shader uses `fract()` to wrap coords when
  `GL_OES_texture_npot` is absent
- **CLAMP_TO_BORDER emulation:** `border_fade()` function fades to transparent
  at texture edges
- **No `GL_UNPACK_ROW_LENGTH`:** manual row extraction on CPU
- **No `GL_UNSIGNED_INT_8_8_8_8_REV`:** use `GL_UNSIGNED_BYTE` only
- **Depth-stencil separation:** attach depth and stencil as separate
  renderbuffers (`GL_DEPTH_ATTACHMENT` + `GL_STENCIL_ATTACHMENT`)

### Performance Profile

**Strengths:**
- Span compositor converts large continuous regions into few rectangles
- Gradient texture cache avoids repeated upload
- Glyph atlas via rtree (sparse packing in 1024×1024 texture)
- Client-memory VBO avoids MapBuffer overhead
- Shader cache (64 entries) avoids recompilation

**Weaknesses:**
- CPU scan conversion is bottleneck for complex paths
- Every stroke/fill is an independent draw — no cross-operation merging
- Static 1MB VBO triggers flushes on complex scenes
- Gradient upload requires `glTexImage2D` per new gradient (even if cached)
- All draws use `glDrawArrays` (never indexed) — wastes vertex shader work

---

## 4. Cross-Comparison

| Aspect | ThorVG | Rive | Cairo GL | evgpuganesh (current) |
|--------|--------|------|----------|----------------------|
| **GLES support** | 3.0+ | 3.0+ | **2.0 native** | 2.0 |
| **VBO strategy** | Stage buffer → 1 upload | Instanced buffers | **Client-memory pointer** | 1 VBO per draw |
| **Batching** | SolidBatch + StencilCoverBatch | Instancing (up to 8191) | Per-span emit | None |
| **Path fill** | Stencil-then-cover | PLS (no stencil) | **CPU spans → triangles** | No paths (LVGL upper) |
| **Gradients** | UBO + shader compute | Gradient texture lookup | **CPU-baked 1D texture** | Per-vertex color |
| **State cache** | Program, UBO align | **GLState dirty-bit** | None | None |
| **State management** | Per-pass task list | PLS planes | Immediate-mode | Per-call |
| **Shader system** | Array of programs + blend variants | **Define composition** | **Runtime concat** | Fixed 4 programs |
| **Code complexity** | ~30 files, C++ | ~50 files, C++17 | ~15 files, C | 2 files (gl + h) |
| **GLES2 porting cost** | **Medium** (VAO+UBO→GLES2) | **Very high** (architectural) | **Already native** | N/A |

---

## 5. Actionable Patterns for evgpuganesh (Mali-400 GLES2)

### P0 — Immediate, Low Risk

1. **Client-memory vertex buffer** (Cairo GL)
   - Replace per-draw `glBufferData` with `malloc`'d arena + `glVertexAttribPointer` with client pointer
   - Flush once per frame instead of per draw call
   - **Expected impact:** Eliminates the 32 VBO uploads per frame → major perf win

2. **Dirty-bit state caching** (Rive's `GLState`)
   - Track `currentProgram`, `currentBlendFunc`, `currentScissor` as integers
   - Only call `glUseProgram`/`glBlendFunc`/`glEnable(GL_SCISSOR_TEST)` when value changes
   - **Expected impact:** Removes 10-20 redundant GL calls per frame

### P1 — Low-Medium Effort

3. **Solid-color draw batching** (ThorVG SolidBatch)
   - Collect consecutive draws with same color into one `glDrawArrays` call
   - Append per-vertex color when second color appears
   - **Implementation:** ~100 lines in the flush loop
   - **Expected impact:** 2-5× fewer draw calls for UI-heavy frames

4. **Gradient texture caching** (Cairo GL + ThorVG)
   - Bake gradients to 1D textures on first use, cache by stop-array hash
   - Fragment shader: compute 1D texcoord from position
   - **Implementation:** Add gradient texture cache + 1 new shader variant
   - **Expected impact:** 8× faster BOX_SHADOW (currently worst: 328ms vs 140ms)

5. **Single-frame VBO with append** (ThorVG Stage Buffer)
   - Collect all vertex data in CPU buffer during scene traversal
   - One `glBufferData(GL_STREAM_DRAW)` at frame end
   - **Implementation:** Replace current per-primitive VBO uploads

### P2 — Medium Effort

6. **Shader define-composition** (Rive style, adapted)
   - Instead of 4 fixed programs, compile at runtime from define combos
   - Handles: solid / gradient / image / mask without code duplication
   - **GLES2 version:** `#version 100` + `#define` fragments concatenated

7. **Render pass abstraction** (ThorVG)
   - Group draws by blend mode / scissor rect / source texture
   - Emit state changes only at pass boundaries
   - Natural extension of batching above

### Not Worth Pursuing on Mali-400

| Pattern | Why Not |
|---------|---------|
| GPU tessellation (Rive) | Needs GLES 3.0+ (texelFetch, instancing) |
| PLS (Rive) | Needs GLES 3.0+ extension |
| Stencil-then-cover (ThorVG/NanoVG) | Adds 2 pass per path — LVGL already provides tessellated geometry |
| UBO (ThorVG) | GLES2 has no uniform blocks |
| VAO (ThorVG) | GLES2 has no VAO (but can use `EXT_OES_vertex_array_object` optionally) |

---

## 6. Quick Reference: GLES2 Extension Table for Mali-400

| Extension | Purpose | On Mali-400 (lima) |
|-----------|---------|-------------------|
| `GL_OES_framebuffer_object` | FBO support | **Yes** |
| `GL_OES_rgb8_rgba8` | RGBA8 renderbuffer format | **Yes** |
| `GL_OES_depth24` | 24-bit depth buffer | **Yes** |
| `GL_OES_packed_depth_stencil` | Combined depth+stencil | **Yes** |
| `GL_EXT_discard_framebuffer` | Optimize FB discard | **Yes** |
| `GL_OES_texture_npot` | NPOT texture with repeat | **Yes** (lima) |
| `GL_OES_mapbuffer` | Map GL buffer on CPU | **Yes** |
| `GL_OES_vertex_array_object` | VAO extension | **Yes** (lima) |
| `GL_EXT_texture_format_BGRA8888` | BGRA texture upload | **Yes** |
| `EXT_shader_pixel_local_storage` | PLS | **No** |
| `GL_OES_standard_derivatives` | dFdx/dFdy in FS | **Yes** |

---

*Generated 2026-07-18 from live source-code analysis of ThorVG, Rive, and Cairo.*

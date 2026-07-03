# Prebuilt AArch64 Wayland static libs (see scripts/build_a53_static_wayland.sh).

if(NOT A53_DEPS_PREFIX)
    message(FATAL_ERROR "A53_DEPS_PREFIX not set (run scripts/build_a53_static_wayland.sh)")
endif()

set(_a53_inc "${A53_DEPS_PREFIX}/include")
set(_a53_lib "${A53_DEPS_PREFIX}/lib")

foreach(_a53_wl_lib IN ITEMS
    libwayland-egl.a
    libwayland-cursor.a
    libwayland-client.a
    libxkbcommon.a
    libxml2.a
    libz.a
    libffi.a
)
    if(NOT EXISTS "${_a53_lib}/${_a53_wl_lib}")
        message(FATAL_ERROR "Missing ${_a53_lib}/${_a53_wl_lib} — run scripts/build_a53_static_wayland.sh")
    endif()
endforeach()

if(EXISTS "${_a53_lib}/libpng16.a")
    set(_a53_png_lib "${_a53_lib}/libpng16.a")
elseif(EXISTS "${_a53_lib}/libpng.a")
    set(_a53_png_lib "${_a53_lib}/libpng.a")
else()
    message(FATAL_ERROR "Missing libpng static lib in ${_a53_lib}")
endif()

set(A53_STATIC_WL_INC "${_a53_inc}" PARENT_SCOPE)
set(A53_STATIC_WL_LIBS
    "${_a53_lib}/libwayland-egl.a"
    "${_a53_lib}/libwayland-cursor.a"
    "${_a53_lib}/libwayland-client.a"
    "${_a53_lib}/libxkbcommon.a"
    "${_a53_lib}/libxml2.a"
    "${_a53_png_lib}"
    "${_a53_lib}/libz.a"
    "${_a53_lib}/libffi.a"
    pthread
    m
    rt
    dl
    PARENT_SCOPE
)

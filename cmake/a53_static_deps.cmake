# Prebuilt AArch64 GLFW/GLEW static libs (see scripts/build_a53_static.sh).

if(NOT A53_DEPS_PREFIX)
    message(FATAL_ERROR "A53_DEPS_PREFIX not set (run scripts/build_a53_static.sh)")
endif()

set(_a53_inc "${A53_DEPS_PREFIX}/include")
set(_a53_lib "${A53_DEPS_PREFIX}/lib")

if(NOT EXISTS "${_a53_lib}/libglfw3.a")
    message(FATAL_ERROR "Missing ${_a53_lib}/libglfw3.a — run scripts/build_a53_static.sh")
endif()
if(NOT EXISTS "${_a53_lib}/libGLEW.a")
    message(FATAL_ERROR "Missing ${_a53_lib}/libGLEW.a — run scripts/build_a53_static.sh")
endif()

set(A53_STATIC_GL_INC "${_a53_inc}" PARENT_SCOPE)
set(A53_STATIC_GL_LIBS
    "${_a53_lib}/libglfw3.a"
    "${_a53_lib}/libGLEW.a"
    X11
    xcb
    Xau
    Xdmcp
    pthread
    m
    dl
    rt
    PARENT_SCOPE
)

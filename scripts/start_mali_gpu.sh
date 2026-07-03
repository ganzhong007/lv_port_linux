#!/bin/sh
# Load Mali kernel module and set GLES userspace libs (PetaLinux / ZynqMP).
# Copy to board alongside lvglsim, or install under /usr/local/bin/.

MALI_KO=""
for p in \
    /lib/modules/$(uname -r)/extra/mali.ko \
    /lib/modules/$(uname -r)/kernel/drivers/gpu/arm/mali.ko \
    /opt/petalinux/*/sysroots/*/lib/modules/*/extra/mali.ko; do
    if [ -f "$p" ]; then
        MALI_KO="$p"
        break
    fi
done

if ! lsmod 2>/dev/null | grep -q '^mali '; then
    if [ -n "$MALI_KO" ]; then
        echo "loading $MALI_KO"
        insmod "$MALI_KO" || modprobe mali 2>/dev/null || true
    else
        modprobe mali 2>/dev/null || true
    fi
fi

export LD_LIBRARY_PATH="/usr/lib:${LD_LIBRARY_PATH:-}"

if [ -e /dev/dri/card0 ]; then
    echo "Mali/DRM devices:"
    ls -l /dev/dri/card* 2>/dev/null || true
    for c in /sys/class/drm/card*-*; do
        [ -d "$c" ] || continue
        st=$(cat "$c/status" 2>/dev/null) || st="?"
        echo "  $(basename "$c"): $st"
    done
else
    echo "warn: /dev/dri/card0 not found — check device tree / kernel config"
fi

# Let lvglsim own the CRTC (fbcon otherwise blocks page flip / SetCrtc).
for vt in /sys/class/vtconsole/vtcon*; do
    if [ -f "$vt/name" ] && grep -qi framebuffer "$vt/name" 2>/dev/null; then
        echo 0 > "$vt/bind" 2>/dev/null && echo "unbound $vt" || true
    fi
done

if [ -f /usr/lib/libMali.so ] || [ -f /usr/lib/libMali.so.9 ]; then
    echo "userspace: libMali found"
else
    echo "warn: libMali.so not in /usr/lib"
fi

echo "Run DP (1920x1080): LVGL_SCENARIO=1 ./lvglsim -b drm -W 1920 -H 1080"
echo "Override card: LV_LINUX_DRM_CARD=/dev/dri/cardN ./lvglsim -b drm ..."

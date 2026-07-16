#!/usr/bin/env bash
# Orange Pi: ATK-style native DRM Weston (stop LightDM, start weston-drm.service).
set -eo pipefail
BOARD="${BOARD:-192.168.10.140}"
BOARD_USER="${BOARD_USER:-orangepi}"
BOARD_PASSWORD="${BOARD_PASSWORD:-Even-123}"
REMOTE_BIN="${REMOTE_BIN:-/home/orangepi/wayland_run/lvglsim}"
LVGL_ARGS="${LVGL_ARGS:--m -b wayland}"
export BOARD BOARD_USER BOARD_PASSWORD REMOTE_BIN LVGL_ARGS
python3 - <<'PY'
import os, paramiko, textwrap
board=os.environ["BOARD"]; user=os.environ["BOARD_USER"]; password=os.environ["BOARD_PASSWORD"]
remote_bin=os.environ["REMOTE_BIN"]; lvgl_args=os.environ["LVGL_ARGS"]
c=paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(board,22,user,password,timeout=20,allow_agent=False,look_for_keys=False,
          disabled_algorithms=dict(pubkeys=["rsa-sha2-512","rsa-sha2-256"]))
def run(cmd, sudo=False, timeout=180):
    if sudo: cmd=f"echo '{password}' | sudo -S bash -lc {repr(cmd)}"
    _,o,e=c.exec_command(cmd,timeout=timeout)
    out=o.read().decode(); err=e.read().decode(); rc=o.channel.recv_exit_status()
    print(f"=== rc={rc} ===\n{out}", end='')
    if err.strip() and "[sudo]" not in err: print("ERR:", err[:800])
    return rc,out,err

unit=textwrap.dedent("""\
[Unit]
Description=Weston DRM compositor (native, ATK-style)
After=systemd-user-sessions.service
Conflicts=lightdm.service

[Service]
Type=simple
User=root
Environment=XDG_RUNTIME_DIR=/run/user/0
Environment=XDG_SESSION_TYPE=wayland
WorkingDirectory=/root
ExecStartPre=/bin/mkdir -p /run/user/0 /root/.config/weston
ExecStart=/usr/bin/weston --backend=drm-backend.so --tty=3 --idle-time=0 --log=/tmp/weston-drm.log
Restart=no

[Install]
WantedBy=multi-user.target
""")
ini=textwrap.dedent("""\
[core]
backend=drm-backend.so
shell=desktop-shell.so
[shell]
locking=false
panel-position=top
[output]
name=HDMI-A-1
mode=current
""")
sftp=c.open_sftp()
with sftp.file("/tmp/weston-drm.service","w") as f: f.write(unit)
with sftp.file("/tmp/weston.ini","w") as f: f.write(ini)
sftp.close()
run("killall weston lvglsim 2>/dev/null; true; systemctl stop lightdm; "
    "mkdir -p /root/.config/weston; cp /tmp/weston.ini /root/.config/weston/weston.ini; "
    "cp /tmp/weston-drm.service /etc/systemd/system/weston-drm.service; "
    "systemctl daemon-reload; systemctl restart weston-drm; sleep 3; "
    "systemctl is-active weston-drm; chmod 755 /run/user/0; chmod 777 /run/user/0/wayland-* 2>/dev/null; true",
    sudo=True)
run("grep -iE 'using /dev|GL renderer|GL vendor|Output HDMI|backend' /tmp/weston-drm.log | head -20")
run(f"SOCK=$(ls /run/user/0/wayland-* 2>/dev/null | grep -v lock | head -1 | xargs -n1 basename); "
    f"echo SOCK=$SOCK; export XDG_RUNTIME_DIR=/run/user/0; export WAYLAND_DISPLAY=$SOCK; "
    f"killall lvglsim 2>/dev/null; true; "
    f"cd /home/orangepi/wayland_run; nohup {remote_bin} {lvgl_args} >/tmp/lvglsim.log 2>&1 & sleep 3; "
    f"ps aux | grep '[l]vglsim'; echo ---; cat /tmp/lvglsim.log", sudo=True)
c.close()
print("Orange Pi now uses native DRM Weston (ATK-style).")
print("Restore XFCE: ./scripts/orangepi-weston-restore-x11.sh")
PY

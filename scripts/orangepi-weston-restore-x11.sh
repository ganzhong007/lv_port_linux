#!/usr/bin/env bash
# Restore Orange Pi XFCE/Xorg (stop native DRM Weston, start LightDM).
set -eo pipefail
BOARD="${BOARD:-192.168.10.140}"
BOARD_USER="${BOARD_USER:-orangepi}"
BOARD_PASSWORD="${BOARD_PASSWORD:-Even-123}"
export BOARD BOARD_USER BOARD_PASSWORD
python3 - <<'PY'
import os, paramiko
board=os.environ["BOARD"]; user=os.environ["BOARD_USER"]; password=os.environ["BOARD_PASSWORD"]
c=paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(board,22,user,password,timeout=20,allow_agent=False,look_for_keys=False,
          disabled_algorithms=dict(pubkeys=["rsa-sha2-512","rsa-sha2-256"]))
def run(cmd, sudo=False):
    if sudo: cmd=f"echo '{password}' | sudo -S bash -lc {repr(cmd)}"
    _,o,e=c.exec_command(cmd,timeout=90)
    print(o.read().decode())
    err=e.read().decode()
    if err.strip() and '[sudo]' not in err: print(err)
run("killall lvglsim 2>/dev/null; true; "
    "systemctl disable --now weston-drm 2>/dev/null; killall weston 2>/dev/null; true; "
    "systemctl unmask lightdm; systemctl enable lightdm; "
    "systemctl start lightdm; sleep 2; "
    "systemctl is-active lightdm; pgrep -a Xorg | head -2", sudo=True)
c.close()
print("Restored LightDM/Xorg")
PY

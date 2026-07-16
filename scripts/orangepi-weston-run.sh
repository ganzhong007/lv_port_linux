#!/usr/bin/env bash
set -eo pipefail

BOARD="${BOARD:-192.168.10.140}"
BOARD_USER="${BOARD_USER:-orangepi}"
BOARD_PASSWORD="${BOARD_PASSWORD:-Even-123}"

export BOARD BOARD_USER BOARD_PASSWORD

python3 - <<'PY'
import os, paramiko, time

board = os.environ["BOARD"]
user = os.environ["BOARD_USER"]
password = os.environ["BOARD_PASSWORD"]

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(board, 22, user, password, timeout=20, allow_agent=False, look_for_keys=False,
          disabled_algorithms=dict(pubkeys=["rsa-sha2-512", "rsa-sha2-256"]))

def run(cmd, wait=0):
    if "sudo" not in cmd and cmd.startswith("killall") or cmd.startswith("weston") or cmd.startswith("export"):
        full = cmd
    elif cmd.startswith("sudo "):
        full = f"echo '{password}' | sudo -S bash -lc {repr(cmd[5:])}"
    else:
        full = cmd
    i,o,e = c.exec_command(full, timeout=120)
    if wait:
        time.sleep(wait)
    out=o.read().decode(); err=e.read().decode(); rc=o.channel.recv_exit_status()
    return rc, out, err

# Start weston on X11 backend (keep XFCE, nested compositor)
cmds = [
    "killall weston 2>/dev/null; killall lvglsim 2>/dev/null; true",
    "export DISPLAY=:0; export XDG_RUNTIME_DIR=/run/user/1000; weston --backend=x11-backend.so --socket=wayland-0 >/tmp/weston.log 2>&1 & sleep 2; ps aux | grep '[w]eston'",
    "test -S /run/user/1000/wayland-0 && echo wayland_socket_ok || (cat /tmp/weston.log; exit 1)",
    "export XDG_RUNTIME_DIR=/run/user/1000; export WAYLAND_DISPLAY=wayland-0; cd /home/orangepi/wayland_run; ./lvglsim -m -b wayland >/tmp/lvglsim.log 2>&1 & sleep 3; ps aux | grep '[l]vglsim'",
    "echo '--- lvglsim.log ---'; cat /tmp/lvglsim.log; echo '--- weston.log ---'; tail -20 /tmp/weston.log",
]
for cmd in cmds:
    rc, out, err = run(cmd, 1)
    print(f"$ {cmd[:70]}")
    print(out)
    if err.strip(): print("ERR:", err)
    if rc != 0:
        print(f"FAILED rc={rc}")
        break
c.close()
PY

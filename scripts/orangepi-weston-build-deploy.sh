#!/usr/bin/env bash
set -eo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-orangepi"
BOARD="${BOARD:-192.168.10.140}"
BOARD_USER="${BOARD_USER:-orangepi}"
BOARD_PASSWORD="${BOARD_PASSWORD:-Even-123}"
REMOTE_DIR="${REMOTE_DIR:-/home/orangepi/lv_port_linux}"

export ROOT BUILD_DIR BOARD BOARD_USER BOARD_PASSWORD REMOTE_DIR

python3 - <<'PY'
import os, tarfile, tempfile, paramiko
from pathlib import Path

root = Path(os.environ["ROOT"])
board = os.environ["BOARD"]
user = os.environ["BOARD_USER"]
password = os.environ["BOARD_PASSWORD"]
remote = os.environ["REMOTE_DIR"]

def connect():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(board, 22, user, password, timeout=20, allow_agent=False, look_for_keys=False,
              disabled_algorithms=dict(pubkeys=["rsa-sha2-512", "rsa-sha2-256"]))
    return c

def run(client, cmd, sudo=False):
    if sudo:
        cmd = f"echo '{password}' | sudo -S bash -lc {repr(cmd)}"
    stdin, stdout, stderr = client.exec_command(cmd, timeout=600)
    out = stdout.read().decode()
    err = stderr.read().decode()
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        raise RuntimeError(f"cmd failed ({rc}): {cmd}\n{err}\n{out}")
    return out

client = connect()

install_cmd = (
    "DEBIAN_FRONTEND=noninteractive apt-get update && "
    "DEBIAN_FRONTEND=noninteractive apt-get install -y "
    "weston libwayland-dev libxkbcommon-dev wayland-protocols "
    "cmake pkg-config build-essential python3 git"
)
print("Installing board dependencies...")
run(client, install_cmd, sudo=True)

# pack source (exclude build dirs)
exclude = {"build-a53weston", "build-orangepi", ".git"}
with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as tf:
    tarpath = tf.name
with tarfile.open(tarpath, "w:gz") as tar:
    for item in root.iterdir():
        if item.name in exclude:
            continue
        tar.add(item, arcname=item.name)
sftp = client.open_sftp()
try:
    run(client, f"rm -rf {remote} && mkdir -p {remote}")
    sftp.put(tarpath, f"/tmp/lv_port_linux_src.tar.gz")
finally:
    sftp.close()
os.unlink(tarpath)

run(client, f"tar xzf /tmp/lv_port_linux_src.tar.gz -C {remote}")

build_cmd = (
    f"cd {remote} && "
    "cmake -B build-orangepi -S . -DCONFIG=orangepi-weston -DCMAKE_BUILD_TYPE=Release && "
    "cmake --build build-orangepi -j$(nproc)"
)
print("Building on Orange Pi (armv7l native)...")
run(client, build_cmd)

run(client, f"mkdir -p /home/orangepi/wayland_run")
run(client, f"cp {remote}/build-orangepi/bin/lvglsim /home/orangepi/wayland_run/lvglsim && chmod +x /home/orangepi/wayland_run/lvglsim", sudo=False)
print("Deployed to /home/orangepi/wayland_run/lvglsim")
client.close()
PY

echo "Done."

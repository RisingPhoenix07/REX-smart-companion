#!/usr/bin/env python3
import json
import os
import shutil
import socket
import subprocess
import time
import urllib.parse
import urllib.request
import psutil

REX_HOSTNAME = "rex.local"
REX_FALLBACK_IP = "192.168.1.210"  # <-- Put the IP shown on REX's SYS page here (e.g. "192.168.1.105")
POLL_INTERVAL = 0.65
TELEMETRY_INTERVAL = 2.0


def resolve_rex_base_url():
    if REX_FALLBACK_IP:
        return f"http://{REX_FALLBACK_IP}"
    try:
        ip = socket.gethostbyname(REX_HOSTNAME)
        return f"http://{ip}"
    except Exception as e:
        print(f"[REX BRIDGE] mDNS lookup failed for {REX_HOSTNAME}: {e}")
        return f"http://{REX_HOSTNAME}"


def find_zen_command():
    for bin_name in ("zen-browser", "zen"):
        path = shutil.which(bin_name)
        if path:
            return [path]

    home = os.path.expanduser("~")
    candidates = [
        f"{home}/.local/share/zen/zen",
        f"{home}/.local/bin/zen",
        f"{home}/Applications/zen",
        "/opt/zen/zen",
        "/opt/zen-browser/zen",
    ]
    for c in candidates:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return [c]

    if shutil.which("flatpak"):
        res = subprocess.run(["flatpak", "list", "--app"], capture_output=True, text=True)
        if "app.zen_browser.zen" in res.stdout:
            return ["flatpak", "run", "app.zen_browser.zen"]

    if shutil.which("gtk-launch"):
        for desktop in ("app.zen_browser.zen", "zen-browser", "zen"):
            for ddir in (
                f"{home}/.local/share/applications",
                "/usr/share/applications",
                "/var/lib/flatpak/exports/share/applications",
            ):
                if os.path.exists(f"{ddir}/{desktop}.desktop"):
                    return ["gtk-launch", desktop]

    return None


def tile_wayland_window(direction):
    if shutil.which("wtype"):
        key = "Left" if direction == "left" else "Right"
        subprocess.run(["wtype", "-M", "logo", "-P", key, "-p", key, "-m", "logo"], check=False)
    elif shutil.which("ydotool"):
        code = "105" if direction == "left" else "106"
        subprocess.run(["ydotool", "key", "125:1", f"{code}:1", f"{code}:0", "125:0"], check=False)


def launch_parallel(app1_cmd, app2_cmd):
    if app1_cmd:
        subprocess.Popen(app1_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(2.2)
        tile_wayland_window("left")

    if app2_cmd:
        subprocess.Popen(app2_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.8)
        tile_wayland_window("right")


def get_cpu_temp():
    try:
        temps = psutil.sensors_temperatures()
        for name in ("coretemp", "k10temp", "acpitz", "cpu_thermal"):
            if name in temps and temps[name]:
                return int(temps[name][0].current)
    except Exception:
        pass
    return 0


def push_telemetry(base_url, active_mode):
    cpu = int(psutil.cpu_percent(interval=None))
    ram = int(psutil.virtual_memory().percent)
    temp = get_cpu_temp()
    data = urllib.parse.urlencode({
        "cpu": cpu,
        "ram": ram,
        "temp": temp,
        "mode": active_mode.upper(),
    }).encode("utf-8")
    try:
        req = urllib.request.Request(f"{base_url}/api/telemetry", data=data, method="POST")
        with urllib.request.urlopen(req, timeout=1.5) as resp:
            print(f"[REX BRIDGE] Telemetry -> {resp.status} (CPU {cpu}%, RAM {ram}%, {temp}C)")
    except Exception as e:
        print(f"[REX BRIDGE] Telemetry POST failed: {e}")


def main():
    psutil.cpu_percent(interval=None)
    zen_cmd = find_zen_command()
    base_url = resolve_rex_base_url()
    print(f"[REX BRIDGE] Target: {base_url} | Zen: {zen_cmd}")

    last_seq = None
    active_mode = "idle"
    last_telemetry = 0.0
    last_err_log = 0.0

    while True:
        try:
            with urllib.request.urlopen(f"{base_url}/api/mode", timeout=1.5) as resp:
                raw = resp.read().decode("utf-8")
                payload = json.loads(raw)
                seq = payload.get("seq", 0)
                mode = payload.get("mode", "idle")

                if last_seq is None:
                    last_seq = seq
                    print(f"[REX BRIDGE] Connected! Initial mode={mode}, seq={seq}")
                elif seq != last_seq:
                    last_seq = seq
                    active_mode = mode
                    print(f"[REX BRIDGE] Mode activated: {mode}")

                    if mode == "project":
                        launch_parallel(zen_cmd, ["code", "--new-window"])
                    elif mode == "study":
                        launch_parallel(zen_cmd, ["nautilus", "--new-window"])
                    elif mode == "monitor":
                        last_telemetry = time.time()
                        push_telemetry(base_url, active_mode)

        except Exception as e:
            if time.time() - last_err_log >= 5.0:
                last_err_log = time.time()
                print(f"[REX BRIDGE] Poll error ({base_url}/api/mode): {e}")
                base_url = resolve_rex_base_url()

        if active_mode == "monitor" and (time.time() - last_telemetry >= TELEMETRY_INTERVAL):
            last_telemetry = time.time()
            push_telemetry(base_url, active_mode)

        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
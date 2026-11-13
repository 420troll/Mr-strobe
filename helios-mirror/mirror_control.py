"""TROLLfx Helios Mirror - small Windows controller for mirror.ini.
Requires Python 3 standard library only. Run from the DLL's directory.
"""
from pathlib import Path
import os
import tkinter as tk
from tkinter import ttk, messagebox

PATH = Path(__file__).resolve().parent / "mirror.ini"
DEFAULTS = dict(armed="0", mirror_enabled="1", flip_x="0",
                flip_y="0", size="1.0", offset_x="0", offset_y="0")


def load():
    values = DEFAULTS.copy()
    if PATH.exists():
        for line in PATH.read_text(encoding="utf-8").splitlines():
            if "=" in line and not line.lstrip().startswith(("#", ";")):
                key, value = line.split("=", 1)
                if key.strip() in values:
                    values[key.strip()] = value.strip()
    return values


def save(values):
    # Replace atomically so a reading process never sees partial settings.
    tmp = PATH.with_suffix(".tmp")
    tmp.write_text("# TROLLfx Helios Mirror\n" +
                   "".join(f"{k}={v}\n" for k, v in values.items()),
                   encoding="utf-8")
    os.replace(tmp, PATH)


root = tk.Tk()
root.title("TROLLfx Helios Mirror")
root.geometry("430x410")
root.resizable(False, False)
values = load()
fields = {}
frame = ttk.Frame(root, padding=18)
frame.pack(fill="both", expand=True)
ttk.Label(frame, text="TROLLfx HELIOS MIRROR",
          font=("Segoe UI", 17, "bold")).pack(anchor="w", pady=(0, 12))
ttk.Label(frame, text="USB Helios 0 → Helios 1 (Maxwell DLL proxy)",
          font=("Segoe UI", 9)).pack(anchor="w")
for key, label in [("mirror_enabled", "Mirror output to second DAC"),
                   ("flip_x", "Flip X on DAC 2"),
                   ("flip_y", "Flip Y on DAC 2")]:
    fields[key] = tk.IntVar(value=int(values[key]))
    ttk.Checkbutton(frame, text=label, variable=fields[key]).pack(anchor="w", pady=3)
for key, label in [("size", "DAC 2 size (0.0–1.0)"),
                   ("offset_x", "DAC 2 X offset (-2048..2048)"),
                   ("offset_y", "DAC 2 Y offset (-2048..2048)")]:
    row = ttk.Frame(frame)
    row.pack(fill="x", pady=5)
    ttk.Label(row, text=label, width=30).pack(side="left")
    fields[key] = tk.StringVar(value=values[key])
    ttk.Entry(row, textvariable=fields[key], width=10).pack(side="right")


def write_settings(armed=None):
    try:
        config = {key: str(var.get()) for key, var in fields.items()}
        size = float(config["size"])
        offsets = [int(config[k]) for k in ("offset_x", "offset_y")]
        if not (0 <= size <= 1 and all(-2048 <= i <= 2048 for i in offsets)):
            raise ValueError("Size/offset out of range")
        config["armed"] = str(int(values["armed"] if armed is None else armed))
        save(config)
        values.update(config)
        status.set("ARMED - output enabled" if int(config["armed"]) else "BLACKOUT - RGB disabled")
    except ValueError as exc:
        messagebox.showerror("Invalid settings", str(exc))


buttons = ttk.Frame(frame)
buttons.pack(fill="x", pady=14)
ttk.Button(buttons, text="SAVE SETTINGS", command=write_settings).pack(side="left")
ttk.Button(buttons, text="ARM OUTPUT",
           command=lambda: write_settings(1)).pack(side="left", padx=10)
ttk.Button(buttons, text="BLACKOUT",
           command=lambda: write_settings(0)).pack(side="right")
status = tk.StringVar(value="ARMED" if values["armed"] == "1" else "BLACKOUT - RGB disabled")
ttk.Label(frame, textvariable=status, font=("Segoe UI", 10, "bold")).pack(anchor="w")
ttk.Label(frame, text="Requires the proxy DLL loaded by Maxwell.\n"
          "Not tested against Maxwell yet; keep hardware interlocks active.",
          foreground="#666").pack(anchor="w", pady=10)
root.mainloop()

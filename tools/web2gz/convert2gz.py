import os
import sys
import gzip
import mimetypes
from pathlib import Path

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(ROOT_DIR, "../../lib/gz_web")

MIME_MAP = {
    ".html": "text/html",
    ".css": "text/css",
    ".js": "application/javascript",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".ico": "image/x-icon",
    ".woff2": "font/woff2",
}

os.makedirs(OUT_DIR, exist_ok=True)


def sanitize(name):
    return name.replace(".", "_").replace("-", "_").replace(" ", "_")


def convert_file(path):
    path = Path(path)
    ext = path.suffix.lower()
    mime = MIME_MAP.get(
        ext, mimetypes.guess_type(path.name)[0] or "application/octet-stream"
    )

    with open(path, "rb") as f:
        raw = f.read()

    gz = gzip.compress(raw, compresslevel=9)
    var = sanitize(path.name)
    out_h = Path(OUT_DIR) / f"{var}.h"
    hex_data = ", ".join(f"0x{b:02X}" for b in gz)

    with open(out_h, "w", encoding="utf-8") as h:
        h.write("#pragma once\n\n")
        h.write(f"static const unsigned char {var}[] PROGMEM = {{\n")

        for i in range(0, len(gz), 16):
            chunk = gz[i : i + 16]
            h.write("  " + ", ".join(f"0x{b:02X}" for b in chunk))

            if i + 16 < len(gz):
                h.write(",")

            h.write("\n")

        h.write("};\n\n")
        h.write(f"static const size_t {var}_len = {len(gz)};\n")
        h.write(f'static const char* {var}_mime = "{mime}";\n')
    print(f"[OK] {path.name} -> {out_h}")


def walk(folder):
    for root, _, files in os.walk(folder):
        for file in files:
            convert_file(os.path.join(root, file))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("[INFO] No drag input detected, using default folder...\n")
        DEFAULT_PATH = os.path.join(ROOT_DIR, "../../data/web")
        if not os.path.exists(DEFAULT_PATH):
            print(f"[ERROR] Default path does not exist: {DEFAULT_PATH}")
            sys.exit(1)
        walk(DEFAULT_PATH)
    else:
        for p in sys.argv[1:]:
            if os.path.isdir(p):
                walk(p)
            else:
                convert_file(p)

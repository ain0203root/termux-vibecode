#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import pathlib
import sys
from urllib.request import Request, urlopen

URL = "https://github.com/termux/termux-app/releases/download/v0.118.3/termux-app_v0.118.3+apt-android-7-github-debug_x86_64.apk"
EXPECTED_MIN_BYTES = 10_000_000


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: download-termux.py OUTPUT", file=sys.stderr)
        return 2
    output = pathlib.Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    req = Request(URL, headers={"User-Agent": "termux-vibecode-ci"})
    with urlopen(req, timeout=180) as response, output.open("wb") as handle:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            handle.write(chunk)
    size = output.stat().st_size
    if size < EXPECTED_MIN_BYTES:
        raise SystemExit(f"downloaded APK is suspiciously small: {size} bytes")
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    print(f"termux_apk={output}")
    print(f"termux_apk_bytes={size}")
    print(f"termux_apk_sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

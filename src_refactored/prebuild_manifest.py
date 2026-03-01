#!/usr/bin/env python3
"""
prebuild_version.py — version.txt is authoritative
- Reads MAJOR.MINOR.PATCH
- Auto-increments PATCH
- Writes back to version.txt
- Generates megaguard.json + C++ header
"""

import json
import sys
from pathlib import Path
from datetime import datetime

VERSION_FILENAME = "version.txt"
JSON_FILENAME = "megaguard.json"


def parse_version(text: str):
    parts = text.strip().split(".")
    if len(parts) != 3:
        raise ValueError("version.txt must be MAJOR.MINOR.PATCH")
    return int(parts[0]), int(parts[1]), int(parts[2])


def main():
    root = Path(sys.argv[1]) if len(sys.argv) >= 2 else Path(__file__).parent
    version_path = root / VERSION_FILENAME
    json_path = root / JSON_FILENAME

    # Read version.txt
    if not version_path.exists():
        version_path.write_text("1.0.0", encoding="utf-8")

    major, minor, patch = parse_version(version_path.read_text())

    prev_major = major
    prev_minor = minor

    # Check previous json for major/minor change
    if json_path.exists():
        try:
            old = json.loads(json_path.read_text())
            if old["major"] != major or old["minor"] != minor:
                patch = 0
            else:
                patch += 1
        except Exception:
            patch += 1
    else:
        patch += 1

    # Write updated version.txt
    version_path.write_text(f"{major}.{minor}.{patch}", encoding="utf-8")

    datebuild = datetime.now().strftime("%Y%m%d.%H%M%S")
    full_version = f"{major}.{minor}.{patch}+{datebuild}"

    # Write megaguard.json
    json_data = {
        "major": major,
        "minor": minor,
        "patch": patch,
        "datebuild": datebuild,
        "version": f"{major}.{minor}.{patch}",
        "full_version": full_version
    }

    json_path.write_text(json.dumps(json_data, indent=2), encoding="utf-8")

    # Write C++ header
    header_content = f"""// Auto-generated — DO NOT EDIT
#pragma once

#define MG_VERSION_MAJOR {major}
#define MG_VERSION_MINOR {minor}
#define MG_VERSION_PATCH {patch}
#define MG_VERSION_DATEBUILD "{datebuild}"
#define MG_VERSION_FULL "{full_version}"
"""

    header_path = root / "core" / "version_generated.hpp"
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(header_content, encoding="utf-8")

    print(f"[prebuild] MegaGuard v{full_version}")


if __name__ == "__main__":
    main()

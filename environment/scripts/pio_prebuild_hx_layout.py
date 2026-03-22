# HexaOS prebuild integration for pioarduino.
# This script resolves the active pins_arduino.h, then calls generate_hx_build_layout.py.

from __future__ import annotations

Import("env")

import pathlib
import subprocess
import sys


PROJECT_DIR = pathlib.Path(env.subst("$PROJECT_DIR")).resolve()
BUILD_FILE = PROJECT_DIR / "src" / "headers" / "hx_build.h"
PINFUNC_FILE = PROJECT_DIR / "src" / "headers" / "hx_pinfunc.h"
OUT_FILE = PROJECT_DIR / "src" / "headers" / "hx_build_layout_autogen.h"
GENERATOR = PROJECT_DIR / "environment" / "scripts" / "generate_hx_build_layout.py"


def get_target_name() -> str:
    board_mcu = str(env.get("BOARD_MCU", "")).strip().lower()
    if board_mcu:
        return board_mcu

    cpp_defines = env.get("CPPDEFINES", [])
    for item in cpp_defines:
        if isinstance(item, (list, tuple)) and len(item) >= 1:
            name = str(item[0])
        else:
            name = str(item)
        if name.startswith("CONFIG_IDF_TARGET_"):
            return name.replace("CONFIG_IDF_TARGET_", "").lower()

    raise RuntimeError("Unable to resolve active target name for HexaOS pinmap generator")


def candidate_pins_paths() -> list[pathlib.Path]:
    candidates: list[pathlib.Path] = []

    override = env.get("HX_LAYOUT_PINS_FILE") or env.GetProjectOption("custom_hx_layout_pins", default="")
    if override:
        candidates.append((PROJECT_DIR / str(override)).resolve())

    variant = env.BoardConfig().get("build.variant", "")
    if variant:
        candidates.append((PROJECT_DIR / "environment" / "boards" / "variants" / variant / "pins_arduino.h").resolve())
        candidates.append((PROJECT_DIR / "variants" / variant / "pins_arduino.h").resolve())
        try:
            platform = env.PioPlatform()
            framework_dir = platform.get_package_dir("framework-arduinoespressif32")
            if framework_dir:
                candidates.append((pathlib.Path(framework_dir) / "variants" / variant / "pins_arduino.h").resolve())
        except Exception:
            pass

    candidates.append((PROJECT_DIR / "pins_arduino.h").resolve())

    unique: list[pathlib.Path] = []
    seen = set()
    for item in candidates:
        key = str(item)
        if key in seen:
            continue
        seen.add(key)
        unique.append(item)
    return unique


def resolve_pins_file() -> pathlib.Path:
    for candidate in candidate_pins_paths():
        if candidate.exists():
            return candidate
    tried = "\n  ".join(str(p) for p in candidate_pins_paths())
    raise RuntimeError(f"Unable to find pins_arduino.h for HexaOS pinmap generator. Tried:\n  {tried}")


def run() -> None:
    target = get_target_name()
    pins_file = resolve_pins_file()

    cmd = [
        sys.executable,
        str(GENERATOR),
        "--pins", str(pins_file),
        "--build", str(BUILD_FILE),
        "--pinfunc", str(PINFUNC_FILE),
        "--target", target,
        "--out", str(OUT_FILE),
    ]

    print(f"[HexaOS] pinmap generator target={target} pins={pins_file}")
    subprocess.check_call(cmd)


run()

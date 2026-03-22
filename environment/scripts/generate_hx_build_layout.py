#!/usr/bin/env python3
"""
HexaOS build layout generator.
Generates headers/hx_build_layout_autogen.h from pins_arduino.h and headers/hx_build.h.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from typing import Dict, List, Tuple

PIN_ALIAS_TO_FUNCTION = {
    "TX": "HX_PIN_UART0_TX",
    "RX": "HX_PIN_UART0_RX",
    "TX1": "HX_PIN_UART1_TX",
    "RX1": "HX_PIN_UART1_RX",
    "TX2": "HX_PIN_UART2_TX",
    "RX2": "HX_PIN_UART2_RX",
    "TX3": "HX_PIN_UART3_TX",
    "RX3": "HX_PIN_UART3_RX",
    "TX4": "HX_PIN_UART4_TX",
    "RX4": "HX_PIN_UART4_RX",
    "LP_TX": "HX_PIN_LP_UART_TX",
    "LP_RX": "HX_PIN_LP_UART_RX",
    "SDA": "HX_PIN_I2C0_SDA",
    "SCL": "HX_PIN_I2C0_SCL",
    "SDA1": "HX_PIN_I2C1_SDA",
    "SCL1": "HX_PIN_I2C1_SCL",
    "SDA2": "HX_PIN_I2C2_SDA",
    "SCL2": "HX_PIN_I2C2_SCL",
    "I2C_SDA": "HX_PIN_I2C1_SDA",
    "I2C_SCL": "HX_PIN_I2C1_SCL",
    "LP_SDA": "HX_PIN_LP_I2C_SDA",
    "LP_SCL": "HX_PIN_LP_I2C_SCL",
    "MOSI": "HX_PIN_SPI0_MOSI",
    "MISO": "HX_PIN_SPI0_MISO",
    "SCK": "HX_PIN_SPI0_SCLK",
    "SS": "HX_PIN_SPI0_SS",
    "MOSI1": "HX_PIN_SPI1_MOSI",
    "MISO1": "HX_PIN_SPI1_MISO",
    "SCK1": "HX_PIN_SPI1_SCLK",
    "I2S_MCLK": "HX_PIN_I2S0_MCLK",
    "I2S_SCLK": "HX_PIN_I2S0_BCLK",
    "I2S_LRCK": "HX_PIN_I2S0_WS",
    "I2S_ASDOUT": "HX_PIN_I2S0_DOUT",
    "I2S_DSDIN": "HX_PIN_I2S0_DIN",
    "AMP_CTRL": "HX_PIN_I2S0_AMP_CTRL",
    "DAC1": "HX_PIN_DAC1",
    "DAC2": "HX_PIN_DAC2",
    "USB_DM": "HX_PIN_USB_DM",
    "USB_DP": "HX_PIN_USB_DP",
    "LCD_BL_IO": "HX_PIN_DISPLAY_BL",
    "LCD_RST_IO": "HX_PIN_DISPLAY_RST",
    "CTP_RST": "HX_PIN_TOUCH_RST",
    "CTP_INT": "HX_PIN_TOUCH_INT",
    "PIN_RGB_LED": "HX_PIN_STATUS_LED",
    "LED_BUILTIN": "HX_PIN_STATUS_LED",
    "BUILTIN_LED": "HX_PIN_STATUS_LED",
    "RGB_BUILTIN": "HX_PIN_STATUS_LED",
    "ETH_PHY_MDC": "HX_PIN_ETH0_MDC",
    "ETH_PHY_MDIO": "HX_PIN_ETH0_MDIO",
    "ETH_PHY_POWER": "HX_PIN_ETH0_POWER",
    "ETH_RMII_TX_EN": "HX_PIN_ETH0_RMII_TX_EN",
    "ETH_RMII_TX0": "HX_PIN_ETH0_RMII_TXD0",
    "ETH_RMII_TX1": "HX_PIN_ETH0_RMII_TXD1",
    "ETH_RMII_RX0": "HX_PIN_ETH0_RMII_RXD0",
    "ETH_RMII_RX1": "HX_PIN_ETH0_RMII_RXD1",
    "ETH_RMII_RX1_EN": "HX_PIN_ETH0_RMII_RXD1",
    "ETH_RMII_CRS_DV": "HX_PIN_ETH0_RMII_CRS_DV",
    "ETH_RMII_CLK": "HX_PIN_ETH0_RMII_REF_CLK",
    "BOARD_SDIO_ESP_HOSTED_CLK": "HX_PIN_HOSTED0_SDIO_CLK",
    "BOARD_SDIO_ESP_HOSTED_CMD": "HX_PIN_HOSTED0_SDIO_CMD",
    "BOARD_SDIO_ESP_HOSTED_D0": "HX_PIN_HOSTED0_SDIO_D0",
    "BOARD_SDIO_ESP_HOSTED_D1": "HX_PIN_HOSTED0_SDIO_D1",
    "BOARD_SDIO_ESP_HOSTED_D2": "HX_PIN_HOSTED0_SDIO_D2",
    "BOARD_SDIO_ESP_HOSTED_D3": "HX_PIN_HOSTED0_SDIO_D3",
    "BOARD_SDIO_ESP_HOSTED_RESET": "HX_PIN_HOSTED0_RESET",
    "BOARD_SDIO_ESP_HOSTED_BOOT": "HX_PIN_HOSTED0_BOOT",
    "SDMMC_CLK": "HX_PIN_SDMMC0_CLK",
    "SDMMC_CMD": "HX_PIN_SDMMC0_CMD",
    "SDMMC_D0": "HX_PIN_SDMMC0_D0",
    "SDMMC_D1": "HX_PIN_SDMMC0_D1",
    "SDMMC_D2": "HX_PIN_SDMMC0_D2",
    "SDMMC_D3": "HX_PIN_SDMMC0_D3",
}


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8")


def strip_cpp_comments(text: str) -> str:
    out: List[str] = []
    i = 0
    n = len(text)
    in_line = False
    in_block = False
    in_string = False
    in_char = False
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if in_line:
            if ch == "\n":
                in_line = False
                out.append(ch)
            i += 1
            continue

        if in_block:
            if ch == "*" and nxt == "/":
                in_block = False
                i += 2
            else:
                if ch == "\n":
                    out.append(ch)
                i += 1
            continue

        if in_string:
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if ch == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if ch == "'":
                in_char = False
            i += 1
            continue

        if ch == "/" and nxt == "/":
            in_line = True
            i += 2
            continue

        if ch == "/" and nxt == "*":
            in_block = True
            i += 2
            continue

        out.append(ch)
        if ch == '"':
            in_string = True
        elif ch == "'":
            in_char = True
        i += 1

    return "".join(out)


def parse_pinfunc_ids(path: pathlib.Path) -> Dict[str, int]:
    text = strip_cpp_comments(read_text(path))
    pattern = re.compile(r"\b(HX_PIN_[A-Z0-9_]+)\s*=\s*(\d+)")
    return {name: int(value) for name, value in pattern.findall(text)}


def parse_target_gpio_counts(path: pathlib.Path) -> Dict[str, int]:
    text = strip_cpp_comments(read_text(path))
    pattern = re.compile(
        r'HX_TARGET_CAPS_[A-Z0-9]+_DEF\s*=\s*\{\s*HX_TARGET_KIND_[A-Z0-9]+\s*,\s*"([a-z0-9]+)"\s*,\s*(\d+)\s*\}'
    )
    counts = {name: int(count) for name, count in pattern.findall(text)}
    if not counts:
        raise SystemExit(f"Unable to parse target GPIO counts from {path}")
    return counts


def eval_pin_expression(expr: str, symbols: Dict[str, int]) -> int | None:
    expr = expr.strip()
    if not expr:
        return None

    builtins = {
        "SOC_GPIO_PIN_COUNT": 0,
        "LOW": 0,
        "HIGH": 1,
        "true": 1,
        "false": 0,
        "TRUE": 1,
        "FALSE": 0,
    }

    token_pattern = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")

    def replace_token(match: re.Match[str]) -> str:
        name = match.group(0)
        if name in symbols:
            return str(symbols[name])
        if name in builtins:
            return str(builtins[name])
        raise KeyError(name)

    normalized = expr.replace("&&", " and ").replace("||", " or ")
    try:
        normalized = token_pattern.sub(replace_token, normalized)
    except KeyError:
        return None

    normalized = re.sub(r"!([^=])", r" not \1", normalized)

    try:
        value = eval(normalized, {"__builtins__": {}}, {})
    except Exception:
        return None

    if isinstance(value, bool):
        return 1 if value else 0
    if isinstance(value, int):
        return value
    return None


def parse_pins_arduino(path: pathlib.Path) -> Dict[str, int]:
    text = strip_cpp_comments(read_text(path))
    result: Dict[str, int] = {}
    const_pattern = re.compile(r"^\s*static\s+const\s+uint8_t\s+([A-Za-z0-9_]+)\s*=\s*(.+?)\s*;\s*$")
    define_pattern = re.compile(r"^\s*#define\s+([A-Za-z0-9_]+)\s+(.+?)\s*$")

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        const_match = const_pattern.match(raw_line)
        if const_match:
            name = const_match.group(1)
            value = eval_pin_expression(const_match.group(2), result)
            if value is not None:
                result[name] = value
            continue

        define_match = define_pattern.match(raw_line)
        if define_match:
            name = define_match.group(1)
            value = eval_pin_expression(define_match.group(2), result)
            if value is not None:
                result[name] = value

    return result


def parse_macro_body(text: str, macro_name: str) -> str:
    lines = text.splitlines()
    prefix = f"#define {macro_name}(X)"
    start_index = -1
    for index, raw_line in enumerate(lines):
        if raw_line.lstrip().startswith(prefix):
            start_index = index
            break
    if start_index < 0:
        return ""

    body_lines = []
    first_line = lines[start_index]
    body_lines.append(first_line.split(prefix, 1)[1].rstrip())

    line_index = start_index + 1
    while line_index < len(lines):
        previous = lines[line_index - 1].rstrip()
        if not previous.endswith("\\"):
            break
        body_lines.append(lines[line_index].rstrip())
        line_index += 1

    normalized = []
    for raw_line in body_lines:
        line = raw_line.strip()
        if line.endswith("\\"):
            line = line[:-1].rstrip()
        if line:
            normalized.append(line)
    return "\n".join(normalized)


def parse_registry_names(text: str, macro_name: str) -> List[str]:
    body = parse_macro_body(text, macro_name)
    return re.findall(r"X\(([A-Z0-9_]+)\)", body)


def parse_pin_overrides(text: str) -> List[Tuple[str, int]]:
    body = parse_macro_body(text, "HX_BUILD_PIN_OVERRIDE_LIST")
    return [(func_name, int(gpio, 10)) for func_name, gpio in re.findall(r"X\((HX_PIN_[A-Z0-9_]+)\s*,\s*(-?\d+)\)", body)]


def build_dense_pinmap(target: str, gpio_count: int, pin_ids: Dict[str, int], pins: Dict[str, int], overrides: List[Tuple[str, int]]) -> List[int]:
    dense = [0] * gpio_count

    for alias_name, gpio in pins.items():
        func_name = PIN_ALIAS_TO_FUNCTION.get(alias_name)
        if func_name is None:
            continue
        func_id = pin_ids.get(func_name)
        if func_id is None:
            raise SystemExit(f"Unknown pin function name in mapping table: {func_name}")
        if 0 <= gpio < gpio_count:
            dense[gpio] = func_id

    for func_name, gpio in overrides:
        func_id = pin_ids.get(func_name)
        if func_id is None:
            raise SystemExit(f"Unknown override function name: {func_name}")
        if not (0 <= gpio < gpio_count):
            raise SystemExit(f"Override gpio out of range for target {target}: {gpio}")
        for index, existing in enumerate(dense):
            if existing == func_id:
                dense[index] = 0
        dense[gpio] = func_id

    return dense


def parse_define_int(text: str, name: str, default: int | None = None) -> int:
    match = re.search(rf"^\s*#define\s+{re.escape(name)}\s+(.+?)\s*$", text, re.MULTILINE)
    if not match:
        if default is None:
            raise KeyError(name)
        return default
    raw = match.group(1).split("//", 1)[0].strip()
    return int(raw, 0)


def parse_enabled_define(text: str, name: str) -> bool:
    try:
        return parse_define_int(text, name, default=0) != 0
    except ValueError:
        return False


def build_i2c_bindings(text: str, type_names: List[str]) -> Dict[str, Dict[str, Dict[str, int]]]:
    result: Dict[str, Dict[str, Dict[str, int]]] = {}
    for type_name in type_names:
        instances: Dict[str, Dict[str, int]] = {}
        pattern = re.compile(rf"^\s*#define\s+HX_I2C_DRIVER_{re.escape(type_name)}_(\d+)_ENABLED\s+(.+?)\s*$", re.MULTILINE)
        for instance_text, _ in pattern.findall(text):
            instance = int(instance_text, 10)
            enabled_name = f"HX_I2C_DRIVER_{type_name}_{instance}_ENABLED"
            if not parse_enabled_define(text, enabled_name):
                continue
            port = parse_define_int(text, f"HX_I2C_DRIVER_{type_name}_{instance}_PORT")
            address = parse_define_int(text, f"HX_I2C_DRIVER_{type_name}_{instance}_ADDRESS")
            instances[str(instance)] = {"i2c": port, "address": address}
        if instances:
            result[type_name] = instances
    return result


def build_uart_bindings(text: str, type_names: List[str]) -> Dict[str, Dict[str, Dict[str, int]]]:
    result: Dict[str, Dict[str, Dict[str, int]]] = {}
    for type_name in type_names:
        instances: Dict[str, Dict[str, int]] = {}
        pattern = re.compile(rf"^\s*#define\s+HX_UART_DRIVER_{re.escape(type_name)}_(\d+)_ENABLED\s+(.+?)\s*$", re.MULTILINE)
        for instance_text, _ in pattern.findall(text):
            instance = int(instance_text, 10)
            enabled_name = f"HX_UART_DRIVER_{type_name}_{instance}_ENABLED"
            if not parse_enabled_define(text, enabled_name):
                continue
            entry = {
                "uart": parse_define_int(text, f"HX_UART_DRIVER_{type_name}_{instance}_PORT"),
                "txen": parse_define_int(text, f"HX_UART_DRIVER_{type_name}_{instance}_TXEN_GPIO", default=-1),
                "re": parse_define_int(text, f"HX_UART_DRIVER_{type_name}_{instance}_RE_GPIO", default=-1),
                "de": parse_define_int(text, f"HX_UART_DRIVER_{type_name}_{instance}_DE_GPIO", default=-1),
            }
            instances[str(instance)] = entry
        if instances:
            result[type_name] = instances
    return result


def escape_c_string(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def write_autogen(path: pathlib.Path, pinmap_json: str, bindings_json: str) -> None:
    content = (
        "/*\n"
        "  HexaOS - hx_build_layout_autogen.h\n\n"
        "  Copyright (C) 2026 Martin Macak\n"
        "  SPDX-License-Identifier: GPL-3.0-only\n\n"
        "  Description\n"
        "  Auto-generated build layout defaults produced from pins_arduino.h and hx_build.h.\n"
        "  Do not edit this file manually. Regenerate it from the prebuild step.\n"
        "*/\n\n"
        "#pragma once\n\n"
        f"#define HX_BUILD_DEFAULT_BOARD_PINMAP_JSON \"{escape_c_string(pinmap_json)}\"\n"
        f"#define HX_BUILD_DEFAULT_DRIVERS_BINDINGS_JSON \"{escape_c_string(bindings_json)}\"\n"
    )
    path.write_text(content, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pins", required=True, type=pathlib.Path)
    parser.add_argument("--build", required=True, type=pathlib.Path)
    parser.add_argument("--pinfunc", required=True, type=pathlib.Path)
    parser.add_argument("--targetcaps", required=True, type=pathlib.Path)
    parser.add_argument("--target", required=True)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()

    target_gpio_counts = parse_target_gpio_counts(args.targetcaps)
    if args.target not in target_gpio_counts:
        raise SystemExit(f"Unknown target for HexaOS layout generator: {args.target}")

    pin_ids = parse_pinfunc_ids(args.pinfunc)
    pins = parse_pins_arduino(args.pins)
    build_text = strip_cpp_comments(read_text(args.build))
    overrides = parse_pin_overrides(build_text)

    dense = build_dense_pinmap(args.target, target_gpio_counts[args.target], pin_ids, pins, overrides)
    i2c_types = parse_registry_names(build_text, "HX_BUILD_I2C_DRIVER_TYPE_LIST")
    uart_types = parse_registry_names(build_text, "HX_BUILD_UART_DRIVER_TYPE_LIST")

    bindings = {}
    bindings.update(build_i2c_bindings(build_text, i2c_types))
    bindings.update(build_uart_bindings(build_text, uart_types))

    pinmap_json = json.dumps(dense, separators=(",", ":"))
    bindings_json = json.dumps(bindings, separators=(",", ":"), sort_keys=True)

    write_autogen(args.out, pinmap_json, bindings_json)
    print(f"Generated {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

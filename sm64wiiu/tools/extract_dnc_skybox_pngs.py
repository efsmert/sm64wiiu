#!/usr/bin/env python3
"""Extract day-night-cycle custom skybox PNGs from a DynOS .bin actor asset.

The day-night-cycle mod ships its skybox textures inside `dnc_skybox_geo.bin`.
This script decompresses the DynOS container, finds embedded PNG payloads, maps
them to known texture names, and writes PNG files into `textures/skyboxes/` so
the existing skybox build rules can compile them for Wii U.
"""

from __future__ import annotations

import argparse
import re
import struct
import zlib
from pathlib import Path

PNG_SIG = b"\x89PNG\r\n\x1a\n"
PNG_IEND = b"IEND"
DYNOS_MAGIC = b"DYNOSBIN"

# DynOS token prefix -> output texture basename expected by skybox.c
EXPECTED_TEXTURES = {
    "dnc_skybox_water_night_rgba16": "dnc_water_night",
    "dnc_skybox_water_sunrise_rgba16": "dnc_water_sunrise",
    "dnc_skybox_water_sunset_rgba16": "dnc_water_sunset",
    "dnc_skybox_wdw_night_rgba16": "dnc_wdw_night",
    "dnc_skybox_wdw_sunrise_rgba16": "dnc_wdw_sunrise",
    "dnc_skybox_wdw_sunset_rgba16": "dnc_wdw_sunset",
    "dnc_skybox_cloud_floor_night_rgba16": "dnc_cloud_floor_night",
    "dnc_skybox_cloud_floor_sunrise_rgba16": "dnc_cloud_floor_sunrise",
    "dnc_skybox_cloud_floor_sunset_rgba16": "dnc_cloud_floor_sunset",
    "dnc_skybox_ccm_night_rgba16": "dnc_ccm_night",
    "dnc_skybox_ccm_sunrise_rgba16": "dnc_ccm_sunrise",
    "dnc_skybox_ccm_sunset_rgba16": "dnc_ccm_sunset",
    "dnc_skybox_ssl_night_rgba16": "dnc_ssl_night",
    "dnc_skybox_ssl_sunrise_rgba16": "dnc_ssl_sunrise",
    "dnc_skybox_ssl_sunset_rgba16": "dnc_ssl_sunset",
    "dnc_skybox_clouds_night_rgba16": "dnc_clouds_night",
    "dnc_skybox_clouds_sunrise_rgba16": "dnc_clouds_sunrise",
    "dnc_skybox_clouds_sunset_rgba16": "dnc_clouds_sunset",
}


def maybe_decompress_dynos_bin(blob: bytes) -> bytes:
    """Return decompressed DynOS payload when file starts with DYNOSBIN."""
    if not blob.startswith(DYNOS_MAGIC):
        return blob
    if len(blob) < 16:
        raise ValueError("invalid DynOS bin: truncated header")

    expected_size = struct.unpack_from("<Q", blob, 8)[0]
    payload = zlib.decompress(blob[16:])
    if len(payload) != expected_size:
        raise ValueError(
            f"invalid DynOS bin: decompressed size {len(payload)} != {expected_size}"
        )
    return payload


def find_png_ranges(blob: bytes) -> list[tuple[int, int]]:
    """Return (start,end) byte ranges for all complete PNG files in blob."""
    ranges: list[tuple[int, int]] = []
    cursor = 0
    while True:
        start = blob.find(PNG_SIG, cursor)
        if start < 0:
            return ranges

        pos = start + len(PNG_SIG)
        while pos + 8 <= len(blob):
            chunk_len = int.from_bytes(blob[pos : pos + 4], "big")
            chunk_type = blob[pos + 4 : pos + 8]
            pos += 8 + chunk_len + 4
            if chunk_type == PNG_IEND:
                break
        if pos > len(blob):
            raise ValueError(f"corrupt PNG stream at offset {start}")

        ranges.append((start, pos))
        cursor = pos


def find_named_textures(blob: bytes) -> list[tuple[int, str]]:
    """Return positions and names for candidate texture tokens near PNG data."""
    pattern = re.compile(rb"dnc_skybox_[A-Za-z0-9_]+rgba16[A-Za-z0-9_]*")
    return [(m.start(), m.group(0).decode("utf-8", "ignore")) for m in pattern.finditer(blob)]


def canonicalize_texture_name(raw_name: str) -> str | None:
    """Map raw DynOS token to canonical DNC texture key when known."""
    for key in EXPECTED_TEXTURES:
        if raw_name.startswith(key):
            return key
    return None


def main() -> int:
    """Run extraction and write all required custom skybox PNGs."""
    parser = argparse.ArgumentParser()
    parser.add_argument("input_bin", type=Path, help="Path to dnc_skybox_geo.bin")
    parser.add_argument("output_dir", type=Path, help="Directory for extracted PNGs")
    args = parser.parse_args()

    raw = maybe_decompress_dynos_bin(args.input_bin.read_bytes())
    png_ranges = find_png_ranges(raw)
    named_tokens = find_named_textures(raw)

    output_map: dict[str, bytes] = {}
    for start, end in png_ranges:
        nearest_name: str | None = None
        nearest_pos = -1
        for pos, token in named_tokens:
            if pos < start and pos > nearest_pos:
                nearest_pos = pos
                nearest_name = token
        if nearest_name is None:
            continue

        canonical = canonicalize_texture_name(nearest_name)
        if canonical is None:
            continue
        output_map[canonical] = raw[start:end]

    missing = sorted(set(EXPECTED_TEXTURES) - set(output_map))
    if missing:
        raise RuntimeError(f"missing expected textures: {', '.join(missing)}")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for canonical, out_name in EXPECTED_TEXTURES.items():
        (args.output_dir / f"{out_name}.png").write_bytes(output_map[canonical])
        print(f"wrote {out_name}.png")

    print(f"done: wrote {len(EXPECTED_TEXTURES)} textures to {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

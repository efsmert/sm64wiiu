#!/usr/bin/env python3
"""Generate Phase 0/1 parity artifacts for Co-op DX -> Wii U porting.

Outputs:
- sm64wiiu/parity/phase0_matrix.json
- sm64wiiu/parity/phase0_matrix.md
- sm64wiiu/parity/phase1_lua_port_queue.md
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Set, Tuple


RE_SMLUA_SET_GLOBAL_FUNCTION = re.compile(
    r'smlua_set_global_function\s*\(\s*L\s*,\s*"([^"]+)"', re.MULTILINE
)
RE_SMLUA_BIND_FUNCTION = re.compile(
    r'smlua_bind_function\s*\(\s*L\s*,\s*"([^"]+)"', re.MULTILINE
)
RE_LUA_PUSHC_AND_SETGLOBAL = re.compile(
    r'lua_pushcfunction\s*\(\s*L\s*,\s*[A-Za-z_][A-Za-z0-9_]*\s*\)\s*;\s*'
    r'lua_setglobal\s*\(\s*L\s*,\s*"([^"]+)"\s*\)',
    re.MULTILINE | re.DOTALL,
)
RE_HOOK_ENUM = re.compile(r"^\s*(HOOK_[A-Z0-9_]+),", re.MULTILINE)
RE_ACTION_HOOK_ENUM = re.compile(r"^\s*(ACTION_HOOK_[A-Z0-9_]+),", re.MULTILINE)
RE_ENUM_HOOK_EVENT_TYPE_BLOCK = re.compile(
    r"enum\s+LuaHookedEventType\s*{(?P<body>.*?)}\s*;",
    re.MULTILINE | re.DOTALL,
)
RE_ENUM_ACTION_HOOK_TYPE_BLOCK = re.compile(
    r"enum\s+LuaActionHookType\s*{(?P<body>.*?)}\s*;",
    re.MULTILINE | re.DOTALL,
)
RE_FUNC_CALL = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
RE_HOOK_TOKEN = re.compile(r"\b(HOOK_[A-Z0-9_]+)\b")
RE_HOOK_CALLSITE = re.compile(r"\b(smlua_call_[A-Za-z0-9_]+)\b")
RE_COMMENT_LINE = re.compile(r"--.*$")
RE_COMMENT_BLOCK = re.compile(r"--\[\[.*?\]\]", re.DOTALL)


@dataclass(frozen=True)
class UsageRef:
    path: str
    line: int


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


def discover_workspace_root(start: Path) -> Path:
    for candidate in [start, *start.parents]:
        if (candidate / "sm64coopdx").is_dir() and (candidate / "sm64wiiu").is_dir():
            return candidate
    raise FileNotFoundError(
        f"Could not discover workspace root from {start}; expected directories sm64coopdx/ and sm64wiiu/."
    )


def iter_files(root: Path, pattern: str) -> Iterable[Path]:
    return sorted(root.glob(pattern))


def collect_registered_globals(lua_root: Path) -> Tuple[Set[str], Dict[str, List[str]]]:
    names: Set[str] = set()
    providers: Dict[str, List[str]] = defaultdict(list)
    for path in iter_files(lua_root, "**/*.c"):
        text = read_text(path)
        rel = str(path)
        for m in RE_SMLUA_SET_GLOBAL_FUNCTION.finditer(text):
            name = m.group(1)
            names.add(name)
            providers[name].append(rel)
        for m in RE_SMLUA_BIND_FUNCTION.finditer(text):
            name = m.group(1)
            names.add(name)
            providers[name].append(rel)
        for m in RE_LUA_PUSHC_AND_SETGLOBAL.finditer(text):
            name = m.group(1)
            names.add(name)
            providers[name].append(rel)
    for key in providers:
        providers[key] = sorted(set(providers[key]))
    return names, providers


def collect_hook_enums(hooks_header: Path) -> Dict[str, List[str]]:
    text = read_text(hooks_header)
    hooks: List[str] = []
    action_hooks: List[str] = []

    hook_block_match = RE_ENUM_HOOK_EVENT_TYPE_BLOCK.search(text)
    if hook_block_match:
        hook_block = hook_block_match.group("body")
        hooks = [h for h in RE_HOOK_ENUM.findall(hook_block) if h != "HOOK_MAX"]

    action_hook_block_match = RE_ENUM_ACTION_HOOK_TYPE_BLOCK.search(text)
    if action_hook_block_match:
        action_hook_block = action_hook_block_match.group("body")
        action_hooks = [h for h in RE_ACTION_HOOK_ENUM.findall(action_hook_block) if h != "ACTION_HOOK_MAX"]

    return {
        "hooks": hooks,
        "action_hooks": action_hooks,
    }


def collect_mod_function_usage(mods_root: Path, known_symbols: Set[str]) -> Dict[str, List[UsageRef]]:
    usage: Dict[str, List[UsageRef]] = defaultdict(list)
    for path in iter_files(mods_root, "**/*.lua"):
        text = read_text(path)
        text = RE_COMMENT_BLOCK.sub("", text)
        for line_no, line in enumerate(text.splitlines(), start=1):
            line = RE_COMMENT_LINE.sub("", line)
            for match in RE_FUNC_CALL.finditer(line):
                name = match.group(1)
                if name in known_symbols:
                    usage[name].append(UsageRef(path=str(path), line=line_no))
    return usage


def collect_mod_hook_usage(mods_root: Path) -> Dict[str, List[UsageRef]]:
    usage: Dict[str, List[UsageRef]] = defaultdict(list)
    for path in iter_files(mods_root, "**/*.lua"):
        text = read_text(path)
        text = RE_COMMENT_BLOCK.sub("", text)
        for line_no, line in enumerate(text.splitlines(), start=1):
            line = RE_COMMENT_LINE.sub("", line)
            for match in RE_HOOK_TOKEN.finditer(line):
                usage[match.group(1)].append(UsageRef(path=str(path), line=line_no))
    return usage


def strip_workspace_prefix(path: Path, workspace: Path) -> str:
    try:
        return str(path.relative_to(workspace))
    except ValueError:
        return str(path)


def collect_tree_presence(coop_root: Path, wiiu_root: Path, subdir: str) -> Dict[str, object]:
    coop_base = coop_root / subdir
    wiiu_base = wiiu_root / subdir
    coop_files = sorted(
        p.relative_to(coop_base).as_posix()
        for p in coop_base.rglob("*")
        if p.is_file()
    )
    wiiu_files = sorted(
        p.relative_to(wiiu_base).as_posix()
        for p in wiiu_base.rglob("*")
        if p.is_file()
    ) if wiiu_base.exists() else []

    coop_set = set(coop_files)
    wiiu_set = set(wiiu_files)
    return {
        "subdir": subdir,
        "donor_file_count": len(coop_set),
        "wiiu_file_count": len(wiiu_set),
        "present_count": len(coop_set & wiiu_set),
        "missing_count": len(coop_set - wiiu_set),
        "extra_count": len(wiiu_set - coop_set),
        "missing_files": sorted(coop_set - wiiu_set),
        "present_files": sorted(coop_set & wiiu_set),
        "extra_files": sorted(wiiu_set - coop_set),
    }


def collect_hook_callsites(project_root: Path) -> Dict[str, object]:
    src_roots = [
        project_root / "src/game",
        project_root / "src/engine",
        project_root / "src/audio",
    ]
    callsite_counts: Dict[str, int] = defaultdict(int)
    total = 0

    for root in src_roots:
        if not root.exists():
            continue
        for path in root.rglob("*.c"):
            text = read_text(path)
            for m in RE_HOOK_CALLSITE.finditer(text):
                token = m.group(1)
                callsite_counts[token] += 1
                total += 1

    return {
        "total_callsites": total,
        "unique_callsites": sorted(callsite_counts.keys()),
        "counts": dict(sorted(callsite_counts.items())),
    }


def build_phase1_queue(
    missing_symbols: Set[str],
    donor_providers: Dict[str, List[str]],
    used_missing_symbols: Dict[str, List[UsageRef]],
) -> List[Dict[str, object]]:
    grouped: Dict[str, Dict[str, object]] = defaultdict(lambda: {
        "file": "",
        "symbols": [],
        "used_by_builtins": [],
    })

    for symbol in sorted(missing_symbols):
        providers = donor_providers.get(symbol, [])
        if not providers:
            key = "<unknown>"
            grouped[key]["file"] = key
            grouped[key]["symbols"].append(symbol)
            if symbol in used_missing_symbols:
                grouped[key]["used_by_builtins"].append(symbol)
            continue

        for provider in providers:
            grouped[provider]["file"] = provider
            grouped[provider]["symbols"].append(symbol)
            if symbol in used_missing_symbols:
                grouped[provider]["used_by_builtins"].append(symbol)

    rows: List[Dict[str, object]] = []
    for provider, row in grouped.items():
        syms = sorted(set(row["symbols"]))
        used = sorted(set(row["used_by_builtins"]))
        priority = "P1"
        if used:
            priority = "P0"
        elif "network" in provider or "djui" in provider:
            priority = "P2"
        rows.append(
            {
                "priority": priority,
                "donor_file": provider,
                "missing_symbol_count": len(syms),
                "missing_symbols": syms,
                "builtins_referencing_symbols": used,
            }
        )

    priority_order = {"P0": 0, "P1": 1, "P2": 2, "P3": 3}
    rows.sort(key=lambda r: (priority_order.get(str(r["priority"]), 9), -int(r["missing_symbol_count"]), str(r["donor_file"])))
    return rows


def write_markdown(
    out_path: Path,
    data: Dict[str, object],
) -> None:
    donor = data["donor"]
    wiiu = data["wiiu"]
    lua = data["lua_symbol_parity"]
    hooks = data["hook_parity"]
    hook_calls = data["hook_callsite_parity"]
    tree = data["tree_parity"]

    missing_used = lua["missing_used_by_builtins"]

    lines: List[str] = []
    lines.append("# Phase 0 Parity Matrix (Co-op DX -> Wii U)")
    lines.append("")
    lines.append(f"- Donor root: `{donor}`")
    lines.append(f"- Wii U root: `{wiiu}`")
    lines.append("")

    lines.append("## Lua Symbol Parity")
    lines.append("")
    lines.append(f"- Donor registered globals: **{lua['donor_count']}**")
    lines.append(f"- Wii U registered globals: **{lua['wiiu_count']}**")
    lines.append(f"- Shared globals: **{lua['shared_count']}**")
    lines.append(f"- Missing on Wii U: **{lua['missing_count']}**")
    lines.append(f"- Wii U-only globals: **{lua['wiiu_only_count']}**")
    lines.append("")

    lines.append("### Missing Globals Used By Current Wii U Built-In Mods")
    lines.append("")
    if not missing_used:
        lines.append("- None")
    else:
        for symbol, refs in sorted(missing_used.items()):
            first_refs = refs[:3]
            ref_list = ", ".join(f"`{r['path']}:{r['line']}`" for r in first_refs)
            suffix = "" if len(refs) <= 3 else f" (+{len(refs) - 3} more)"
            lines.append(f"- `{symbol}`: {ref_list}{suffix}")
    lines.append("")

    lines.append("## Hook Surface Parity")
    lines.append("")
    lines.append(f"- HOOK_* enum count (donor): **{hooks['donor_hook_count']}**")
    lines.append(f"- HOOK_* enum count (wiiu): **{hooks['wiiu_hook_count']}**")
    lines.append(f"- Shared HOOK_*: **{hooks['shared_hook_count']}**")
    lines.append(f"- Missing HOOK_* on Wii U enum: **{hooks['missing_hook_count']}**")
    lines.append(f"- ACTION_HOOK_* count (donor): **{hooks['donor_action_hook_count']}**")
    lines.append(f"- ACTION_HOOK_* count (wiiu): **{hooks['wiiu_action_hook_count']}**")
    lines.append("")
    lines.append("### Hook Dispatch Callsite Coverage")
    lines.append("")
    lines.append(
        f"- Donor total hook callsites (`src/game`, `src/engine`, `src/audio`): **{hook_calls['donor_total_callsites']}**"
    )
    lines.append(
        f"- Wii U total hook callsites (`src/game`, `src/engine`, `src/audio`): **{hook_calls['wiiu_total_callsites']}**"
    )
    lines.append(
        f"- Unique hook call helpers in donor: **{hook_calls['donor_unique_count']}**"
    )
    lines.append(
        f"- Unique hook call helpers in Wii U: **{hook_calls['wiiu_unique_count']}**"
    )
    if hook_calls["missing_unique_helpers_on_wiiu"]:
        preview = ", ".join(
            f"`{name}`" for name in hook_calls["missing_unique_helpers_on_wiiu"][:12]
        )
        lines.append(f"- Missing unique hook helper calls on Wii U (preview): {preview}")
    lines.append("")

    lines.append("## Module Tree Parity")
    lines.append("")
    for entry in tree:
        lines.append(
            f"- `{entry['subdir']}`: donor {entry['donor_file_count']}, "
            f"wiiu {entry['wiiu_file_count']}, present {entry['present_count']}, "
            f"missing {entry['missing_count']}"
        )
    lines.append("")

    lines.append("## High-Level Phase 1 Queue")
    lines.append("")
    for row in data["phase1_queue"][:20]:
        lines.append(
            f"- {row['priority']} `{row['donor_file']}`: "
            f"{row['missing_symbol_count']} missing symbols"
            + (
                f" (built-ins use: {', '.join(row['builtins_referencing_symbols'][:5])})"
                if row["builtins_referencing_symbols"]
                else ""
            )
        )

    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_phase1_markdown(out_path: Path, rows: List[Dict[str, object]]) -> None:
    lines: List[str] = []
    lines.append("# Phase 1 Lua Port Queue")
    lines.append("")
    lines.append("Prioritization:")
    lines.append("- `P0`: donor symbols missing on Wii U and referenced by current Wii U built-in mods")
    lines.append("- `P1`: donor symbols missing on Wii U (general parity)")
    lines.append("- `P2`: lower immediate impact (typically network/DJUI heavy paths)")
    lines.append("")

    for row in rows:
        lines.append(f"## {row['priority']} - `{row['donor_file']}`")
        lines.append("")
        lines.append(f"- Missing symbols: {row['missing_symbol_count']}")
        if row["builtins_referencing_symbols"]:
            lines.append(
                "- Referenced by built-in mods: "
                + ", ".join(f"`{s}`" for s in row["builtins_referencing_symbols"])
            )
        else:
            lines.append("- Referenced by built-in mods: none")
        preview = row["missing_symbols"][:25]
        lines.append("- Symbol preview: " + ", ".join(f"`{s}`" for s in preview))
        if len(row["missing_symbols"]) > len(preview):
            lines.append(f"- ... plus {len(row['missing_symbols']) - len(preview)} more")
        lines.append("")

    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    default_workspace_root = discover_workspace_root(Path(__file__).resolve())
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--workspace-root",
        type=Path,
        default=default_workspace_root,
        help="Workspace root containing sm64coopdx/ and sm64wiiu/",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory for generated parity artifacts",
    )
    args = parser.parse_args()

    workspace = args.workspace_root.resolve()
    donor_root = workspace / "sm64coopdx"
    wiiu_root = workspace / "sm64wiiu"
    out_dir = (args.out_dir.resolve() if args.out_dir else (wiiu_root / "parity"))
    out_dir.mkdir(parents=True, exist_ok=True)

    donor_lua_root = donor_root / "src/pc/lua"
    wiiu_lua_root = wiiu_root / "src/pc/lua"

    donor_symbols, donor_providers = collect_registered_globals(donor_lua_root)
    wiiu_symbols, wiiu_providers = collect_registered_globals(wiiu_lua_root)
    donor_providers = {
        sym: [strip_workspace_prefix(Path(p), workspace) for p in providers]
        for sym, providers in donor_providers.items()
    }
    wiiu_providers = {
        sym: [strip_workspace_prefix(Path(p), workspace) for p in providers]
        for sym, providers in wiiu_providers.items()
    }

    shared_symbols = donor_symbols & wiiu_symbols
    missing_symbols = donor_symbols - wiiu_symbols
    wiiu_only_symbols = wiiu_symbols - donor_symbols

    donor_hooks = collect_hook_enums(donor_lua_root / "smlua_hooks.h")
    wiiu_hooks = collect_hook_enums(wiiu_lua_root / "smlua_hooks.h")

    donor_hook_set = set(donor_hooks["hooks"])
    wiiu_hook_set = set(wiiu_hooks["hooks"])

    mods_usage = collect_mod_function_usage(wiiu_root / "mods", donor_symbols)
    missing_used_by_builtins = {
        sym: [
            {"path": strip_workspace_prefix(Path(ref.path), workspace), "line": ref.line}
            for ref in refs
        ]
        for sym, refs in mods_usage.items()
        if sym in missing_symbols
    }

    hook_usage = collect_mod_hook_usage(wiiu_root / "mods")
    missing_hook_usage = {
        hook: [
            {"path": strip_workspace_prefix(Path(ref.path), workspace), "line": ref.line}
            for ref in refs
        ]
        for hook, refs in hook_usage.items()
        if hook not in wiiu_hook_set
    }

    subdirs = [
        "src/pc/lua",
        "src/pc/mods",
        "src/pc/djui",
        "src/pc/network",
    ]
    tree_parity = [collect_tree_presence(donor_root, wiiu_root, subdir) for subdir in subdirs]
    donor_hook_calls = collect_hook_callsites(donor_root)
    wiiu_hook_calls = collect_hook_callsites(wiiu_root)

    phase1_queue = build_phase1_queue(
        missing_symbols=missing_symbols,
        donor_providers=donor_providers,
        used_missing_symbols=mods_usage,
    )

    payload = {
        "donor": strip_workspace_prefix(donor_root, workspace),
        "wiiu": strip_workspace_prefix(wiiu_root, workspace),
        "lua_symbol_parity": {
            "donor_count": len(donor_symbols),
            "wiiu_count": len(wiiu_symbols),
            "shared_count": len(shared_symbols),
            "missing_count": len(missing_symbols),
            "wiiu_only_count": len(wiiu_only_symbols),
            "missing_symbols": sorted(missing_symbols),
            "wiiu_only_symbols": sorted(wiiu_only_symbols),
            "missing_used_by_builtins": missing_used_by_builtins,
            "donor_symbol_providers": {
                sym: donor_providers[sym]
                for sym in sorted(donor_providers)
            },
            "wiiu_symbol_providers": {
                sym: wiiu_providers[sym]
                for sym in sorted(wiiu_providers)
            },
        },
        "hook_parity": {
            "donor_hook_count": len(donor_hook_set),
            "wiiu_hook_count": len(wiiu_hook_set),
            "shared_hook_count": len(donor_hook_set & wiiu_hook_set),
            "missing_hook_count": len(donor_hook_set - wiiu_hook_set),
            "missing_hooks": sorted(donor_hook_set - wiiu_hook_set),
            "donor_action_hook_count": len(set(donor_hooks["action_hooks"])),
            "wiiu_action_hook_count": len(set(wiiu_hooks["action_hooks"])),
            "missing_hook_usage_in_builtins": missing_hook_usage,
        },
        "hook_callsite_parity": {
            "donor_total_callsites": donor_hook_calls["total_callsites"],
            "wiiu_total_callsites": wiiu_hook_calls["total_callsites"],
            "donor_unique_count": len(donor_hook_calls["unique_callsites"]),
            "wiiu_unique_count": len(wiiu_hook_calls["unique_callsites"]),
            "missing_unique_helpers_on_wiiu": sorted(
                set(donor_hook_calls["unique_callsites"]) - set(wiiu_hook_calls["unique_callsites"])
            ),
            "donor_counts": donor_hook_calls["counts"],
            "wiiu_counts": wiiu_hook_calls["counts"],
        },
        "tree_parity": tree_parity,
        "phase1_queue": phase1_queue,
    }

    json_path = out_dir / "phase0_matrix.json"
    md_path = out_dir / "phase0_matrix.md"
    queue_md_path = out_dir / "phase1_lua_port_queue.md"

    json_path.write_text(json.dumps(payload, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    write_markdown(md_path, payload)
    write_phase1_markdown(queue_md_path, phase1_queue)

    print(f"Wrote: {json_path}")
    print(f"Wrote: {md_path}")
    print(f"Wrote: {queue_md_path}")


if __name__ == "__main__":
    main()

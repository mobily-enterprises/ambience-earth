#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
from collections import defaultdict
from pathlib import Path


FLASH_TYPES = set("tTrRdDwWvV")
NM_RE = re.compile(
    r"^(?P<addr>[0-9A-Fa-f]+)\s+(?P<size>[0-9A-Fa-f]+)\s+(?P<type>\w)\s+(?P<name>\S+)(?:\s+(?P<loc>.+))?$"
)


def run_cmd(cmd):
    return subprocess.check_output(cmd, text=True)


def demangle(name, cache):
    if name in cache:
        return cache[name]
    try:
        out = run_cmd(["c++filt", name]).strip()
    except Exception:
        out = name
    cache[name] = out
    return out


def strip_signature(demangled):
    clone = ""
    if " [clone " in demangled:
        base, suffix = demangled.split(" [clone ", 1)
        clone = " [clone " + suffix
    else:
        base = demangled
    base = base.replace("(anonymous namespace)", "__ANON_NS__")
    base = re.sub(r"\s+(const|volatile)\s*$", "", base)

    if base.endswith(")"):
        depth = 0
        for i in range(len(base) - 1, -1, -1):
            ch = base[i]
            if ch == ")":
                depth += 1
            elif ch == "(":
                depth -= 1
                if depth == 0:
                    base = base[:i]
                    break

    base = re.sub(r"\([^)]*\)", "", base)
    base = base.replace("__ANON_NS__", "(anonymous namespace)")
    base = re.sub(r"\s+::", "::", base)
    base = re.sub(r"::\s+", "::", base)
    base = re.sub(r"\s{2,}", " ", base).strip()
    return base + clone


def addr2line(elf_path, addr_hex):
    try:
        out = run_cmd(["avr-addr2line", "-e", str(elf_path), addr_hex]).strip()
    except Exception:
        return None, None
    if not out or out.startswith("??"):
        return None, None
    if ":" in out:
        path, line_s = out.rsplit(":", 1)
        if line_s.isdigit():
            return path, int(line_s)
        return path, None
    return out, None


def normalize_toolchain_path(path):
    p = path.replace("\\", "/")
    if "/gcc-build/" in p:
        return p.split("/gcc-build/", 1)[1]
    return p.lstrip("/")


def classify_path(path, line, repo_root):
    if not path:
        return "PACKAGE", "PACKAGE/unknown"

    p = path.replace("\\", "/")
    repo_root = repo_root.replace("\\", "/")

    if p.startswith(repo_root + "/"):
        rel = p[len(repo_root) + 1 :]
        loc = f"REPO/{rel}"
        if line:
            loc += f":{line}"
        return "REPO", loc

    if "/.arduino15/packages/arduino/" in p:
        rel = p.split("/.arduino15/packages/arduino/", 1)[1]
        loc = f"ARDUINO/{rel}"
        if line:
            loc += f":{line}"
        return "ARDUINO", loc

    if "/libgcc/" in p or "/gcc-build/" in p:
        loc = f"TOOLCHAIN/{normalize_toolchain_path(p)}"
        if line:
            loc += f":{line}"
        return "TOOLCHAIN", loc

    if "/.arduino15/packages/" in p:
        rel = p.split("/.arduino15/packages/", 1)[1]
        loc = f"PACKAGE/{rel}"
        if line:
            loc += f":{line}"
        return "PACKAGE", loc

    if "/Arduino/libraries/" in p:
        rel = p.split("/Arduino/libraries/", 1)[1]
        loc = f"PACKAGE/libraries/{rel}"
        if line:
            loc += f":{line}"
        return "PACKAGE", loc

    loc = f"PACKAGE/{p.lstrip('/')}"
    if line:
        loc += f":{line}"
    return "PACKAGE", loc


def parse_nm(elf_path, repo_root, top_n):
    nm_out = run_cmd(["avr-nm", "-S", "--size-sort", "-t", "x", "-l", str(elf_path)])

    symbols = []
    demangle_cache = {}
    addr_cache = {}

    for line in nm_out.splitlines():
        m = NM_RE.match(line.strip())
        if not m:
            continue
        sym_type = m.group("type")
        if sym_type not in FLASH_TYPES:
            continue
        addr_hex = m.group("addr")
        size = int(m.group("size"), 16)
        name = m.group("name")
        loc = m.group("loc")

        path = None
        line_no = None
        if loc:
            if ":" in loc:
                path, line_s = loc.rsplit(":", 1)
                if line_s.isdigit():
                    line_no = int(line_s)
            else:
                path = loc

        if not path:
            if addr_hex in addr_cache:
                path, line_no = addr_cache[addr_hex]
            else:
                path, line_no = addr2line(elf_path, "0x" + addr_hex)
                addr_cache[addr_hex] = (path, line_no)

        demangled = demangle(name, demangle_cache)
        short_name = strip_signature(demangled)
        label, loc_str = classify_path(path, line_no, repo_root)
        origin4 = {
            "REPO": "REPO",
            "ARDUINO": "ARDU",
            "PACKAGE": "PACK",
            "TOOLCHAIN": "TOOL",
        }.get(label, label[:4].upper())

        symbols.append(
            {
                "size": size,
                "name": short_name,
                "label": label,
                "origin4": origin4,
                "loc": loc_str,
            }
        )

    symbols.sort(key=lambda s: (-s["size"], s["name"]))
    return symbols[:top_n]


def main():
    repo_root = Path(__file__).resolve().parent.parent
    default_elf = repo_root / "main/build/arduino.avr.nano/main.ino.elf"

    parser = argparse.ArgumentParser(description="Report top flash users from an ELF.")
    parser.add_argument("--elf", default=str(default_elf), help="Path to ELF file.")
    parser.add_argument("--top", type=int, default=100, help="Number of entries to show.")
    parser.add_argument("--repo-root", default=str(repo_root), help="Repo root path.")
    args = parser.parse_args()

    elf_path = Path(args.elf)
    if not elf_path.exists():
        raise SystemExit(f"ELF not found: {elf_path}")

    symbols = parse_nm(elf_path, str(Path(args.repo_root).resolve()), args.top)
    for sym in symbols:
        print(f"{sym['size']:6d}  {sym['origin4']}  {sym['name']}  {sym['loc']}")
    totals = defaultdict(int)
    for sym in symbols:
        totals[sym["origin4"]] += sym["size"]
    if totals:
        print("")
        for label in ("REPO", "ARDU", "PACK", "TOOL"):
            if label in totals:
                print(f"{label}: {totals[label]} bytes")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compare ear6-cli vs mesen2-cli screenshots across ROMs at multiple frame counts."""

import argparse
import os
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, ".."))

EAR6_CLI = os.path.join(PROJECT_DIR, "build/app/cli/ear6-cli")
MESEN2_CLI = os.path.join(
    PROJECT_DIR, "../mesen2/dist/x86_64-PC-Linux/bin/mesen2-cli"
)
MESEN2_LIB_DIR = os.path.join(
    PROJECT_DIR, "../mesen2/dist/x86_64-PC-Linux/lib"
)
ROMS_DIR = os.path.join(PROJECT_DIR, "assets/nes/rom")

FRAMES = [64, 128, 256, 512]
TIMEOUT = 60  # seconds per screenshot command

# Report categories
CAT_PERFECT = "PERFECT"
CAT_PARTIAL = "PARTIAL"
CAT_NONE = "NONE"
CAT_EAR6_TIMEOUT = "EAR6_TIMEOUT"
CAT_EAR6_CRASH = "EAR6_CRASH"
CAT_MESEN2_TIMEOUT = "MESEN2_TIMEOUT"
CAT_MESEN2_CRASH = "MESEN2_CRASH"
CAT_BOTH_TIMEOUT = "BOTH_TIMEOUT"
CAT_BOTH_CRASH = "BOTH_CRASH"
CAT_MIXED_FAIL = "MIXED_FAIL"  # e.g. one timeout + one crash


def parse_ppm(path):
    """Parse P6 PPM, return (width, height, bytearray of RGB triples)."""
    with open(path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6":
            raise ValueError(f"Not a P6 PPM: {magic}")
        line = f.readline()
        while line.startswith(b"#"):
            line = f.readline()
        parts = line.split()
        w, h = int(parts[0]), int(parts[1])
        maxval = int(f.readline().strip())
        if maxval != 255:
            raise ValueError(f"Unexpected maxval: {maxval}")
        data = f.read()
        return w, h, data


def compare_ppm(path_a, path_b):
    """Compare two PPMs pixel-by-pixel.  Return (total_pixels, matching_pixels)."""
    w1, h1, d1 = parse_ppm(path_a)
    w2, h2, d2 = parse_ppm(path_b)
    if w1 != w2 or h1 != h2:
        return (w1 * h1, 0)
    total = w1 * h1
    matching = 0
    for i in range(total):
        off = i * 3
        if d1[off:off + 3] == d2[off:off + 3]:
            matching += 1
    return (total, matching)


def run_screenshot(cli, rom, frames, output, env=None):
    """Run a screenshot command.  Returns (status, detail)."""
    try:
        proc = subprocess.run(
            [cli, "screenshot", rom, "-f", str(frames), "-o", output],
            capture_output=True, timeout=TIMEOUT, env=env
        )
        if proc.returncode != 0:
            return ("CRASH", proc.stderr.decode("utf-8", errors="replace"))
        if not os.path.exists(output):
            return ("CRASH", "output file not created")
        return ("OK", "")
    except subprocess.TimeoutExpired:
        return ("TIMEOUT", "")
    except Exception as e:
        return ("CRASH", str(e))


def classify(match_pct):
    if match_pct == 100.0:
        return CAT_PERFECT
    elif match_pct == 0.0:
        return CAT_NONE
    else:
        return CAT_PARTIAL


def main():
    parser = argparse.ArgumentParser(
        description="Compare ear6-cli vs mesen2-cli screenshots"
    )
    parser.add_argument("roms", nargs="*", help="ROM files to test (default: all in assets)")
    parser.add_argument("--frames", type=int, nargs="+", default=FRAMES,
                        help=f"Frame counts to test (default: {' '.join(map(str, FRAMES))})")
    parser.add_argument("--ear6", default=EAR6_CLI, help="Path to ear6-cli")
    parser.add_argument("--mesen2", default=MESEN2_CLI, help="Path to mesen2-cli")
    parser.add_argument("--mesen2-lib", default=MESEN2_LIB_DIR,
                        help="Path to mesen2 lib directory (for DYLD_LIBRARY_PATH)")
    parser.add_argument("--roms-dir", default=ROMS_DIR, help="ROMs directory")
    args = parser.parse_args()

    ear6_cli = os.path.abspath(args.ear6)
    mesen2_cli = os.path.abspath(args.mesen2)
    mesen2_lib = os.path.abspath(args.mesen2_lib)
    roms_dir = os.path.abspath(args.roms_dir)
    frames_list = args.frames

    # Check CLI tools
    for name, path in [("ear6-cli", ear6_cli), ("mesen2-cli", mesen2_cli)]:
        if not os.path.isfile(path):
            print(f"ERROR: {name} not found at: {path}", file=sys.stderr)
            sys.exit(1)

    # Collect ROMs
    if args.roms:
        rom_files = [os.path.abspath(r) for r in args.roms]
    else:
        if not os.path.isdir(roms_dir):
            print(f"ERROR: ROMs directory not found: {roms_dir}", file=sys.stderr)
            sys.exit(1)
        rom_files = sorted(
            os.path.join(roms_dir, f)
            for f in os.listdir(roms_dir)
            if f.lower().endswith(".nes")
        )

    if not rom_files:
        print("ERROR: no ROM files found", file=sys.stderr)
        sys.exit(1)

    print(f"ear6-cli:   {ear6_cli}")
    print(f"mesen2-cli: {mesen2_cli}")
    print(f"ROMs:       {len(rom_files)} files")
    print(f"Frames:     {' '.join(str(f) for f in frames_list)}")
    print()

    # Results: rom -> frame -> category -> (match_pct, mesen2_pct?)
    results = {}

    mesen2_env = os.environ.copy()
    mesen2_env["DYLD_LIBRARY_PATH"] = mesen2_lib

    for rom_path in rom_files:
        rom_name = os.path.basename(rom_path)
        print(f"  {rom_name}")
        results[rom_name] = {}
        for fr in frames_list:
            with tempfile.TemporaryDirectory() as tmpdir:
                ear6_out = os.path.join(tmpdir, "ear6.ppm")
                mesen2_out = os.path.join(tmpdir, "mesen2.ppm")

                ear6_st, ear6_err = run_screenshot(ear6_cli, rom_path, fr, ear6_out)
                mesen2_st, mesen2_err = run_screenshot(
                    mesen2_cli, rom_path, fr, mesen2_out, env=mesen2_env
                )

                if ear6_st != "OK" and mesen2_st != "OK":
                    if ear6_st == "TIMEOUT" and mesen2_st == "TIMEOUT":
                        cat = CAT_BOTH_TIMEOUT
                        label = "BOTH TIMEOUT"
                    elif ear6_st == "TIMEOUT" or mesen2_st == "TIMEOUT":
                        cat = CAT_MIXED_FAIL
                        label = f"E6-{ear6_st} M2-{mesen2_st}"
                    else:
                        cat = CAT_BOTH_CRASH
                        label = "BOTH CRASH"
                    results[rom_name][fr] = (cat, 0.0)
                    print(f"    frame {fr:4d}: {label}")
                    continue
                if ear6_st != "OK":
                    cat = CAT_EAR6_TIMEOUT if ear6_st == "TIMEOUT" else CAT_EAR6_CRASH
                    label = "EAR6 TIMEOUT" if ear6_st == "TIMEOUT" else "EAR6 CRASH"
                    results[rom_name][fr] = (cat, 0.0)
                    print(f"    frame {fr:4d}: {label}")
                    continue
                if mesen2_st != "OK":
                    cat = CAT_MESEN2_TIMEOUT if mesen2_st == "TIMEOUT" else CAT_MESEN2_CRASH
                    label = "M2 TIMEOUT" if mesen2_st == "TIMEOUT" else "M2 CRASH"
                    results[rom_name][fr] = (cat, 0.0)
                    print(f"    frame {fr:4d}: {label}")
                    continue

                try:
                    total, matching = compare_ppm(ear6_out, mesen2_out)
                    pct = (matching / total) * 100.0
                except Exception as e:
                    print(f"    frame {fr:4d}: COMPARE ERROR: {e}")
                    results[rom_name][fr] = (CAT_NONE, 0.0)
                    continue

                cat = classify(pct)
                results[rom_name][fr] = (cat, pct)
                if pct < 100.0:
                    print(f"    frame {fr:4d}: {pct:6.2f}%  ({matching}/{total} px)")
                else:
                    print(f"    frame {fr:4d}: PERFECT")

    # ============================================================
    # Report
    # ============================================================
    print()
    print("=" * 78)
    print("COMPATIBILITY TEST REPORT")
    print("=" * 78)
    print()

    # Build per-frame summaries
    all_cats = [CAT_PERFECT, CAT_PARTIAL, CAT_NONE,
                CAT_EAR6_TIMEOUT, CAT_EAR6_CRASH,
                CAT_MESEN2_TIMEOUT, CAT_MESEN2_CRASH,
                CAT_BOTH_TIMEOUT, CAT_BOTH_CRASH, CAT_MIXED_FAIL]
    frame_summary = {}
    for fr in frames_list:
        counts = {c: 0 for c in all_cats}
        for rom_name, frame_data in results.items():
            if fr in frame_data:
                cat, _ = frame_data[fr]
                counts[cat] += 1
        frame_summary[fr] = counts

    # Per-frame summary table
    print(f"{'Frame':>8}  {'PERFECT':>8} {'PARTIAL':>8} {'NONE':>8} "
          f"{'E6TO':>5} {'E6KO':>5} {'M2TO':>5} {'M2KO':>5} "
          f"{'BTHTO':>5} {'BTHKO':>5} {'MIX':>5}")
    print("-" * 82)
    for fr in frames_list:
        c = frame_summary[fr]
        print(f"{fr:>8}  {c[CAT_PERFECT]:>8} {c[CAT_PARTIAL]:>8} {c[CAT_NONE]:>8} "
              f"{c[CAT_EAR6_TIMEOUT]:>5} {c[CAT_EAR6_CRASH]:>5} "
              f"{c[CAT_MESEN2_TIMEOUT]:>5} {c[CAT_MESEN2_CRASH]:>5} "
              f"{c[CAT_BOTH_TIMEOUT]:>5} {c[CAT_BOTH_CRASH]:>5} {c[CAT_MIXED_FAIL]:>5}")
    print()

    # Per-ROM detail
    print("-" * 78)
    print("DETAIL BY ROM")
    print("-" * 78)
    header = f"{'ROM':<48}"
    for fr in frames_list:
        header += f"  {fr:>10}"
    print(header)
    print("-" * (48 + (10 + 2) * len(frames_list)))

    def fmt_cell(cat, pct):
        if cat == CAT_PERFECT:
            return "  100.00%"
        elif cat == CAT_PARTIAL:
            return f"  {pct:6.2f}%"
        elif cat == CAT_NONE:
            return "   0.00%"
        elif cat == CAT_EAR6_TIMEOUT:
            return "  E6-TO"
        elif cat == CAT_EAR6_CRASH:
            return "  E6-KO"
        elif cat == CAT_MESEN2_TIMEOUT:
            return "  M2-TO"
        elif cat == CAT_MESEN2_CRASH:
            return "  M2-KO"
        elif cat == CAT_BOTH_TIMEOUT:
            return "BTH-TO"
        elif cat == CAT_BOTH_CRASH:
            return "BTH-KO"
        elif cat == CAT_MIXED_FAIL:
            return " MIXED"
        return "   ???"

    for rom_name in sorted(results.keys()):
        line = f"{rom_name[:48]:<48}"
        for fr in frames_list:
            if fr in results[rom_name]:
                cat, pct = results[rom_name][fr]
                line += fmt_cell(cat, pct)
            else:
                line += "  ------"
        print(line)

    # Scoring
    print()
    print("-" * 78)
    print("TOTALS")
    print("-" * 78)
    grand_total = sum(len(r) for r in results.values())

    def count_cat(cat):
        return sum(1 for r in results.values() for c, p in r.values() if c == cat)

    print(f"  Total tests:   {grand_total}")
    print(f"  PERFECT:       {count_cat(CAT_PERFECT):>5}  ({count_cat(CAT_PERFECT)/grand_total*100:5.1f}%)")
    print(f"  PARTIAL:       {count_cat(CAT_PARTIAL):>5}  ({count_cat(CAT_PARTIAL)/grand_total*100:5.1f}%)")
    print(f"  NONE (0%):     {count_cat(CAT_NONE):>5}  ({count_cat(CAT_NONE)/grand_total*100:5.1f}%)")
    print(f"  E6 TIMEOUT:    {count_cat(CAT_EAR6_TIMEOUT):>5}  ({count_cat(CAT_EAR6_TIMEOUT)/grand_total*100:5.1f}%)")
    print(f"  E6 CRASH:      {count_cat(CAT_EAR6_CRASH):>5}  ({count_cat(CAT_EAR6_CRASH)/grand_total*100:5.1f}%)")
    print(f"  M2 TIMEOUT:    {count_cat(CAT_MESEN2_TIMEOUT):>5}  ({count_cat(CAT_MESEN2_TIMEOUT)/grand_total*100:5.1f}%)")
    print(f"  M2 CRASH:      {count_cat(CAT_MESEN2_CRASH):>5}  ({count_cat(CAT_MESEN2_CRASH)/grand_total*100:5.1f}%)")
    print(f"  BOTH TIMEOUT:  {count_cat(CAT_BOTH_TIMEOUT):>5}  ({count_cat(CAT_BOTH_TIMEOUT)/grand_total*100:5.1f}%)")
    print(f"  BOTH CRASH:    {count_cat(CAT_BOTH_CRASH):>5}  ({count_cat(CAT_BOTH_CRASH)/grand_total*100:5.1f}%)")
    print(f"  MIXED FAIL:    {count_cat(CAT_MIXED_FAIL):>5}  ({count_cat(CAT_MIXED_FAIL)/grand_total*100:5.1f}%)")
    print()
    print("=" * 78)
    print("END OF REPORT")
    print("=" * 78)

    return 0


if __name__ == "__main__":
    sys.exit(main())

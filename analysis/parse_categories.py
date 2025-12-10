#!/usr/bin/env python3
from pathlib import Path
import json, re
import numpy as np
import pandas as pd

ROOT = Path(".")
LISTS_DIR = Path("lists")  # where category text files live

# -------------------------------------------------------------------
# Parsing parameters.txt  
# -------------------------------------------------------------------

row_re = re.compile(r"\{([^\}]*)\}")

def parse_params_text(txt: str):
    """Parse parameters.txt -> (rho, kappa[2][4], beta[2][4])"""
    rho = None; kappa = [[],[]]; beta = [[],[]]
    lines = [l.strip() for l in txt.splitlines() if l.strip()]
    for i, line in enumerate(lines):
        if line.startswith("float _rho"):
            rhs = line.split("=",1)[1].strip().rstrip(";")
            rho = float(rhs.rstrip("f"))
        elif line.startswith("float _kappa"):
            a = row_re.search(lines[i+1]).group(1)
            b = row_re.search(lines[i+2]).group(1)
            kappa = [
                [float(x.strip().rstrip("f")) for x in a.split(",")],
                [float(x.strip().rstrip("f")) for x in b.split(",")],
            ]
        elif line.startswith("float _beta"):
            a = row_re.search(lines[i+1]).group(1)
            b = row_re.search(lines[i+2]).group(1)
            beta = [
                [float(x.strip().rstrip("f")) for x in a.split(",")],
                [float(x.strip().rstrip("f")) for x in b.split(",")],
            ]
    return rho, kappa, beta

def flat(mat):
    return mat[0] + mat[1]  # 2x4 -> 8

# -------------------------------------------------------------------
# Load category lists
# -------------------------------------------------------------------

# Map category name -> list file
CATEGORIES = {
    "pineNeedle":   LISTS_DIR / "pineNeedle.txt",
    "fern":     LISTS_DIR / "fern.txt",
    "webbedFern":  LISTS_DIR / "webbedFern.txt",
    "secondaryBranches": LISTS_DIR / "secondaryBranches.txt",
    "webbedStar":      LISTS_DIR / "webbedStar.txt",
}

def load_paths_for_category(cat, list_path):
    paths = []
    if not list_path.exists():
        print(f"[warn] list file missing for {cat}: {list_path}")
        return paths
    for line in list_path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        paths.append(line)
    return paths

# -------------------------------------------------------------------
# Build dataset
# -------------------------------------------------------------------

def build_dataset():
    rows = []
    for cat, list_path in CATEGORIES.items():
        rel_paths = load_paths_for_category(cat, list_path)
        print(f"[cat] {cat}: {len(rel_paths)} samples")
        for rel in rel_paths:
            run_dir = ROOT / rel
            params_file = run_dir / "parameters.txt"
            if not params_file.exists():
                print(f"  [skip] no parameters.txt at {run_dir}")
                continue
            txt = params_file.read_text()
            try:
                rho, kappa, beta = parse_params_text(txt)
            except Exception as e:
                print(f"  [error] parsing {params_file}: {e}")
                continue

            k_flat = flat(kappa)
            b_flat = flat(beta)

            rows.append({
                "rho": rho,
                "k00": k_flat[0], "k01": k_flat[1], "k02": k_flat[2], "k03": k_flat[3],
                "k10": k_flat[4], "k11": k_flat[5], "k12": k_flat[6], "k13": k_flat[7],
                "b00": b_flat[0], "b01": b_flat[1], "b02": b_flat[2], "b03": b_flat[3],
                "b10": b_flat[4], "b11": b_flat[5], "b12": b_flat[6], "b13": b_flat[7],
                "label": cat,
                "run_dir": rel,
            })

    df = pd.DataFrame(rows)
    print(f"[done] dataset size: {len(df)} samples")
    return df

if __name__ == "__main__":
    df = build_dataset()
    out_csv = ROOT / "labeled_dataset.csv"
    df.to_csv(out_csv, index=False)
    print(f"[write] {out_csv}")

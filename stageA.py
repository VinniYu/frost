#!/usr/bin/env python3
import time
import subprocess
import shutil
import random
from pathlib import Path
from math import log, exp

# ===================== User Settings =====================

PROJECT_ROOT = Path(__file__).resolve().parent
FROST_BIN    = PROJECT_ROOT / "bin" / "frost"

RHO_VALUES = [0.075, 0.1, 0.13, 0.16, 0.3, 0.5]

# How many randomized runs per rho
RUNS_PER_RHO = 800

# ----- Separate ranges for kappa and beta -----
# tweak these to taste:
KAPPA_MIN, KAPPA_MAX = 0.01,  1.0   # e.g. relatively small diffusion-ish term
BETA_MIN,  BETA_MAX  = 0.5,  5.5   # e.g. more aggressive anisotropy/growth

# Sampling mode for each:
#   "linear" -> uniform in [min,max]
#   "log"    -> uniform in log-space between [min,max]
KAPPA_SPACE = "log"
BETA_SPACE  = "log"

# Randomization strategy per run:
#   "all"    -> randomize every element of both 2x4 matrices
#   "single" -> pick ONE random element (kappa or beta) to randomize; others fixed at defaults
RANDOM_MODE   = "all"
DEFAULT_KAPPA = 0.5    # only used if RANDOM_MODE == "single"
DEFAULT_BETA  = 6.0    # only used if RANDOM_MODE == "single"

STAGE_ROOT = PROJECT_ROOT / "experiments" / "stageA_flat"

MEDIA_FILENAMES = ["densityMap.ppm", "densityMap.png"]

# Optional reproducibility
RANDOM_SEED = None  # e.g. 12345

# ============ GPU Temperature Watchdog ============
MAX_GPU_TEMP   = 78
COOLDOWN_TEMP  = 68
CHECK_INTERVAL = 15  # seconds

def gpu_temp() -> int:
    try:
        out = subprocess.check_output(
            ["nvidia-smi", "--query-gpu=temperature.gpu",
             "--format=csv,noheader,nounits"]
        ).decode()
        temps = [int(x) for x in out.strip().splitlines() if x.strip()]
        return max(temps) if temps else 0
    except Exception:
        return 0

def wait_for_cooldown():
    t = gpu_temp()
    if t and t > MAX_GPU_TEMP:
        print(f"GPU temperature {t}°C > {MAX_GPU_TEMP}°C - cooling...")
        while True:
            time.sleep(CHECK_INTERVAL)
            t = gpu_temp()
            if not t or t <= COOLDOWN_TEMP:
                print(f"GPU cooled to {t}°C - resuming.")
                break
            print(f"   Still hot ({t}°C), waiting...")

# ===================== Helpers =====================

def ensure_dir(p: Path):
    p.mkdir(parents=True, exist_ok=True)

def format_param_block(rho, kappa, beta) -> str:
    def row_line(row):
        return "{" + ", ".join(f"{float(v)}f" for v in row) + "}"
    lines = []
    lines.append(f"float _rho = {float(rho)}f;")
    lines.append("float _kappa[2][4] = {")
    lines.append(f"{row_line(kappa[0])},")
    lines.append(row_line(kappa[1]))
    lines.append("};")
    lines.append("float _beta[2][4] = {")
    lines.append(f"{row_line(beta[0])},")
    lines.append(row_line(beta[1]))
    lines.append("};")
    return "\n".join(lines) + "\n"

def sample_value(min_v, max_v, mode):
    if mode == "log":
        lo, hi = log(min_v), log(max_v)
        return exp(random.uniform(lo, hi))
    return random.uniform(min_v, max_v)

def sample_kappa_value():
    return sample_value(KAPPA_MIN, KAPPA_MAX, KAPPA_SPACE)

def sample_beta_value():
    return sample_value(BETA_MIN, BETA_MAX, BETA_SPACE)

def random_kappa_beta():
    """
    Generate kappa,beta according to RANDOM_MODE.
    Returns two 2x4 lists of floats.
    """
    if RANDOM_MODE == "single":
        # Start with defaults everywhere
        kappa = [[DEFAULT_KAPPA]*4, [DEFAULT_KAPPA]*4]
        beta  = [[DEFAULT_BETA ]*4, [DEFAULT_BETA ]*4]
        # Choose which tensor and index to randomize
        is_kappa = bool(random.getrandbits(1))
        z = random.randrange(2)
        j = random.randrange(4)
        if is_kappa:
            kappa[z][j] = sample_kappa_value()
        else:
            beta[z][j] = sample_beta_value()
        return kappa, beta

    # RANDOM_MODE == "all": every element randomized
    kappa = [[sample_kappa_value() for _ in range(4)] for _ in range(2)]
    beta  = [[sample_beta_value()  for _ in range(4)] for _ in range(2)]
    return kappa, beta

# ===================== Run Logic =====================

def run_sim(rho, kappa, beta, run_dir: Path):
    ensure_dir(run_dir)

    # Build args: ./bin/frost rho kappaRow1 kappaRow2 betaRow1 betaRow2
    kappa_row1 = " ".join(str(x) for x in kappa[0])
    kappa_row2 = " ".join(str(x) for x in kappa[1])
    beta_row1  = " ".join(str(x) for x in beta[0])
    beta_row2  = " ".join(str(x) for x in beta[1])

    argv = [
        str(FROST_BIN),
        str(rho),
        kappa_row1,
        kappa_row2,
        beta_row1,
        beta_row2,
    ]

    wait_for_cooldown()

    t0 = time.time()
    proc = subprocess.run(argv, cwd=PROJECT_ROOT, capture_output=True, text=True)
    dt = round(time.time() - t0, 3)

    # Logs
    (run_dir / "stdout.log").write_text(proc.stdout)
    (run_dir / "stderr.log").write_text(proc.stderr)

    # Copy generated images flat into test folder
    for name in MEDIA_FILENAMES:
        src = PROJECT_ROOT / "media" / name
        if src.exists():
            shutil.copy2(src, run_dir / name)
            try:
                src.unlink()
            except Exception:
                pass

    # Parameters file
    (run_dir / "parameters.txt").write_text(format_param_block(rho, kappa, beta))

    print(f"- Run finished in {dt}s | folder: {run_dir.name}")

# ===================== Main =====================

def main():
    if RANDOM_SEED is not None:
        random.seed(RANDOM_SEED)

    if not FROST_BIN.exists():
        raise SystemExit(f"Binary not found at {FROST_BIN}")

    ensure_dir(STAGE_ROOT)
    run_count = 0

    for rho in RHO_VALUES:
        rho_dir = STAGE_ROOT / f"rho_{rho:.2f}"
        ensure_dir(rho_dir)

        for _ in range(RUNS_PER_RHO):
            run_count += 1
            run_dir = rho_dir / f"test_{run_count:04d}"
            if (run_dir / "densityMap.ppm").exists():
                continue

            kappa, beta = random_kappa_beta()
            run_sim(rho, kappa, beta, run_dir)
            time.sleep(2)

    print(f"All runs complete. Results in {STAGE_ROOT}")

if __name__ == "__main__":
    main()

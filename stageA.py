#!/usr/bin/env python3
import time
import subprocess
import shutil
from pathlib import Path

# ===================== User Settings =====================

PROJECT_ROOT = Path(__file__).resolve().parent
FROST_BIN    = PROJECT_ROOT / "bin" / "frost"

RHO_VALUES = [0.01, 0.05, 0.1, 0.13, 0.16, 0.3, 0.5]
TEST_VALUES = [0.001, 0.01, 0.1, 0.5, 5.0, 10.0]
DEFAULT_KAPPA = 0.1
DEFAULT_BETA  = 5.0

STAGE_ROOT = PROJECT_ROOT / "experiments" / "stageA_flat"

MEDIA_FILENAMES = ["densityMap.ppm", "densityMap.png"]

# ============ GPU Temperature Watchdog ============
MAX_GPU_TEMP   = 78
COOLDOWN_TEMP  = 68
CHECK_INTERVAL = 15 # seconds

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
    proc = subprocess.run(argv, cwd=PROJECT_ROOT,
                          capture_output=True, text=True)
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

    (run_dir / "parameters.txt").write_text(format_param_block(rho, kappa, beta))

    print(f"- Run finished in {dt}s | folder: {run_dir.name}")

# ===================== Main =====================

def main():
    if not FROST_BIN.exists():
        raise SystemExit(f"Binary not found at {FROST_BIN}")

    ensure_dir(STAGE_ROOT)
    run_count = 0

    for rho in RHO_VALUES:
        rho_dir = STAGE_ROOT / f"rho_{rho:.2f}"
        ensure_dir(rho_dir)

        # ----- 1. Index-wise (16 batches) -----
        for tensor_name in ("kappa", "beta"):
            for z in (0, 1):
                for t in range(4):
                    for val in TEST_VALUES:
                        run_count += 1
                        run_dir = rho_dir / f"test_{run_count:04d}"
                        if (run_dir / "densityMap.ppm").exists():
                            continue

                        kappa = [[DEFAULT_KAPPA]*4, [DEFAULT_KAPPA]*4]
                        beta  = [[DEFAULT_BETA ]*4, [DEFAULT_BETA ]*4]
                        if tensor_name == "kappa":
                            kappa[z][t] = val
                        else:
                            beta[z][t] = val

                        run_sim(rho, kappa, beta, run_dir)
                        time.sleep(2)

        # ----- 2. Column-wise (8 batches) -----
        for tensor_name in ("kappa", "beta"):
            for col in range(4):
                for val in TEST_VALUES:
                    run_count += 1
                    run_dir = rho_dir / f"test_{run_count:04d}"
                    if (run_dir / "densityMap.ppm").exists():
                        continue

                    kappa = [[DEFAULT_KAPPA]*4, [DEFAULT_KAPPA]*4]
                    beta  = [[DEFAULT_BETA ]*4, [DEFAULT_BETA ]*4]
                    if tensor_name == "kappa":
                        kappa[0][col] = val
                        kappa[1][col] = val
                    else:
                        beta[0][col] = val
                        beta[1][col] = val

                    run_sim(rho, kappa, beta, run_dir)
                    time.sleep(2)

    print(f"All runs complete. Results in {STAGE_ROOT}")

if __name__ == "__main__":
    main()

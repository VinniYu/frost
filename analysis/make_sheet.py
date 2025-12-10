#!/usr/bin/env python3
from pathlib import Path
import json, re

ROOT = Path(".")
STAGE = ROOT / "sweep_results"
OUT_HTML = ROOT / "contact_sheet.html"

DEFAULT = 1.0
EPS = 1e-9

row_re = re.compile(r"\{([^\}]*)\}")

def parse_params_text(txt: str):
  rho = None; kappa = [[],[]]; beta = [[],[]]
  lines = [l.strip() for l in txt.splitlines() if l.strip()]
  for i, line in enumerate(lines):
    if line.startswith("float _rho"):
      # float _rho = 0.1f;
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
  return mat[0] + mat[1]

def is_default(x):  # compare to DEFAULT with tolerance
  return abs(x - DEFAULT) <= EPS

def detect_label(kappa, beta):
  """
  Return a short label:
    - Single index change:  k0..k7 or b0..b7  (0-based flat index)
    - Column change:        k1..k4 or b1..b4  (1-based column)
    - Otherwise:            'mixed' or 'default'
  Assumes exactly one tensor is changed in sweep runs.
  """
  kdiff = [(i,v) for i,v in enumerate(flat(kappa)) if not is_default(v)]
  bdiff = [(i,v) for i,v in enumerate(flat(beta))  if not is_default(v)]

  if kdiff and bdiff:
    # Try to prefer the one with fewer diffs for a cleaner tag
    if len(kdiff) == 1 and len(bdiff) != 1:
      return f"k{kdiff[0][0]}"
    if len(bdiff) == 1 and len(kdiff) != 1:
      return f"b{bdiff[0][0]}"
    # Column pattern?
    col_k = _column_change_label(kappa)
    col_b = _column_change_label(beta)
    if col_k and not col_b: return col_k
    if col_b and not col_k: return col_b
    return "mixed"

  # Only kappa changed
  if kdiff and not bdiff:
    if len(kdiff) == 1:
      return f"k{kdiff[0][0]}"
    col = _column_change_label(kappa)
    return col if col else "mixed"

  # Only beta changed
  if bdiff and not kdiff:
    if len(bdiff) == 1:
      return f"b{bdiff[0][0]}"
    col = _column_change_label(beta)
    return col if col else "mixed"

  return "default"

def _column_change_label(mat):

  # exactly one column t where both rows changed (and equal), others default
  changed_cols = []
  
  for t in range(4):
    v0, v1 = mat[0][t], mat[1][t]
    col_changed = (not is_default(v0)) or (not is_default(v1))
    if col_changed:
      changed_cols.append(t)
      # both rows must be changed to the *same* value
      if abs(v0 - v1) > EPS:
        return None
      
  if len(changed_cols) != 1:
    return None
  
  # 1-based label; caller will prefix with k/b based on tensor context in detect_label
  t = changed_cols[0]
  return f"col{t+1}"

import time

def find_images_with_labels():
  t0 = time.time()
  data = {}
  if not STAGE.exists():
    raise SystemExit(f"Not found: {STAGE}")

  rho_dirs = sorted(
    [p for p in STAGE.iterdir() if p.is_dir() and p.name.startswith("rho_")],
    key=lambda p: float(p.name.split("_",1)[1])
  )
  print(f"[scan] found {len(rho_dirs)} rho folders in {time.time()-t0:.3f}s")

  total_tests = 0
  total_imgs  = 0

  for ri, rho_dir in enumerate(rho_dirs, start=1):
    t_rho = time.time()
    rho_key = rho_dir.name
    entries = []

    test_dirs = [p for p in rho_dir.iterdir() if p.is_dir()]
    print(f"[scan] rho {rho_key}: {len(test_dirs)} test dirs (#{ri}/{len(rho_dirs)})")

    for ti, test_dir in enumerate(sorted(test_dirs), start=1):
      total_tests += 1
      img = test_dir / "densityMap.png"
      params = test_dir / "parameters.txt"
      if not img.exists() or not params.exists():
        continue

      try:
        rho, kappa, beta = parse_params_text(params.read_text())
      except Exception:
        label = "unknown"
      else:
        raw_label = detect_label(kappa, beta)
        kdiff = any(not is_default(v) for v in flat(kappa))
        bdiff = any(not is_default(v) for v in flat(beta))
        if raw_label.startswith("col"):
          col_num = raw_label[3:]  # "N"
          if kdiff and not bdiff:
            label = f"k{col_num}"
          elif bdiff and not kdiff:
            label = f"b{col_num}"
          else:
            label = "mixed"
        else:
          label = raw_label

      entries.append({
        "png": str(img.relative_to(ROOT)).replace("\\","/"),
        "run_dir": str(test_dir.relative_to(ROOT)).replace("\\","/"),
        "label": label
      })
      total_imgs += 1

      # occasional progress print every 1000 tests
      if total_tests % 1000 == 0:
        print(f"  [progress] processed {total_tests} tests, {total_imgs} images so far...")

    if entries:
        data[rho_key] = entries
        print(f"  [rho done] {rho_key}: {len(entries)} images in {time.time()-t_rho:.3f}s")

  return data


def write_html(data):
  js_data = json.dumps(data, separators=(",",":"))
  first_key = sorted(data.keys(), key=lambda k: float(k.split("_",1)[1]))[0]

  html = f"""<!doctype html>
<meta charset="utf-8">
<title>FROST Contact Sheet</title>
<style>
  :root {{
    --bg: #0e0e11; --fg: #eaeaf0; --muted:#a0a3ad; --card:#15161a;
    --grid-gap: 10px; --thumb: 240px;
  }}
  html, body {{ margin:0; padding:0; background:var(--bg); color:var(--fg); font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Cantarell,Noto Sans,Arial; }}
  header {{
    position: sticky; top:0; z-index:2; background: linear-gradient(0deg, rgba(14,14,17,0.7), var(--bg));
    backdrop-filter: blur(6px);
    padding: 12px 14px; border-bottom:1px solid #22252b;
    display:grid; grid-template-columns: 1fr auto; gap:12px; align-items:center;
  }}
  h1{{ margin:0; font-size:16px; font-weight:600; }}
  .controls{{ display:flex; gap:10px; align-items:center; }}
  select{{ background:var(--card); color:var(--fg); border:1px solid #30333a; border-radius:8px; padding:8px 10px; }}
  #count{{ color:var(--muted); font-size:12px; }}
  main{{ padding:16px; }}
  .layout {{
    display:grid;
    grid-template-columns: minmax(0,3fr) minmax(220px,1fr);
    gap:16px;
    align-items:flex-start;
  }}
  .grid{{ display:grid; grid-template-columns: repeat(auto-fill, minmax(var(--thumb),1fr)); gap:var(--grid-gap); }}
  .card{{ background:var(--card); border:1px solid #1f2227; border-radius:10px; overflow:hidden; cursor:pointer; transition:border-color 0.15s, box-shadow 0.15s, transform 0.1s; }}
  .card:hover{{ border-color:#3a3f4b; transform:translateY(-1px); }}
  .card.selected{{ border-color:#b9f; box-shadow:0 0 0 2px #b9f66; }}
  .thumb-wrap{{ aspect-ratio:1/1; background:#0b0c0f; display:flex; align-items:center; justify-content:center; }}
  .thumb{{ width:100%; height:100%; object-fit:contain; }}
  .meta{{ padding:8px 10px; color:var(--muted); font-size:12px; display:flex; justify-content:space-between; gap:6px; }}
  .tag{{ color:#111; background:#b9f; border-radius:6px; padding:2px 6px; font-weight:600; }}
  .run{{ color:#c5c8ff; background:#1b1d23; padding:2px 6px; border-radius:6px; }}
  a{{ color:inherit; text-decoration:none; }}
  .sidebar {{
    background:var(--card);
    border:1px solid #1f2227;
    border-radius:10px;
    padding:10px 12px;
    display:flex;
    flex-direction:column;
    gap:8px;
  }}
  .sidebar h2 {{
    margin:0;
    font-size:14px;
    font-weight:600;
  }}
  .sidebar p {{
    margin:0;
    font-size:12px;
    color:var(--muted);
  }}
  #selectedBox {{
    width:100%;
    min-height:200px;
    resize:vertical;
    background:#0b0c0f;
    color:var(--fg);
    border:1px solid #30333a;
    border-radius:6px;
    padding:6px 8px;
    font-family:monospace;
    font-size:12px;
    white-space:pre;
  }}
</style>

<header>
  <h1>FROST Contact Sheet</h1>
  <div class="controls">
    <label>ρ folder:&nbsp;<select id="rho"></select></label>
    <span id="count"></span>
  </div>
</header>

<main>
  <div class="layout">
    <div class="grid" id="grid"></div>

    <aside class="sidebar">
      <h2>Selected images</h2>
      <p>Click thumbnails to toggle selection. Copy the list below into your notes.</p>
      <textarea id="selectedBox" readonly placeholder="No images selected"></textarea>
    </aside>
  </div>
</main>

<script>
  const DATA = {js_data};
  const rhoSel = document.getElementById('rho');
  const grid = document.getElementById('grid');
  const count = document.getElementById('count');
  const selectedBox = document.getElementById('selectedBox');

  // Now we store UNIQUE IDs like "sweep_results/rho_0.10/test_0001"
  const selected = new Set();

  function updateSelectedBox() {{
    if (!selected.size) {{
      selectedBox.value = "";
      selectedBox.placeholder = "No images selected";
      return;
    }}
    const items = Array.from(selected).sort();
    selectedBox.placeholder = "";
    selectedBox.value = items.join("\\n");
  }}

  const keys = Object.keys(DATA).sort((a,b)=>parseFloat(a.split('_')[1]) - parseFloat(b.split('_')[1]));
  for (const k of keys) {{
    const opt = document.createElement('option');
    opt.value = k; opt.textContent = k.replace('rho_','ρ=');
    rhoSel.appendChild(opt);
  }}
  rhoSel.value = "{first_key}";

  function card(rec, key) {{
    const testName = rec.run_dir.split('/').slice(-1)[0];   // "test_0001"
    const label = rec.label || '';
    const id = rec.run_dir;                                 // UNIQUE ID per image
    return `
      <div class="card" data-id="${{id}}">
        <div class="thumb-wrap">
          <img class="thumb" src="${{rec.png}}" loading="lazy" alt="${{testName}}">
        </div>
        <div class="meta">
          <span><span class="tag">${{label}}</span> · <span class="run">${{testName}}</span></span>
          <a href="${{rec.png}}" target="_blank" rel="noopener" class="open-link">open</a>
        </div>
      </div>`;
  }}

  function render(key) {{
    const items = DATA[key] || [];
    grid.innerHTML = items.map(rec => card(rec, key)).join('');
    count.textContent = items.length ? `${{items.length}} image(s)` : 'No images';
    // clear selection when switching rho folder
    selected.clear();
    updateSelectedBox();
  }}

  rhoSel.addEventListener('change', e => render(e.target.value));

  // click-to-select handler (but let "open" links behave normally)
  grid.addEventListener('click', e => {{
    const openLink = e.target.closest('a.open-link');
    if (openLink) {{
      // let the browser handle opening the image in a new tab
      return;
    }}
    const cardEl = e.target.closest('.card');
    if (!cardEl) return;

    const id = cardEl.dataset.id;   // e.g. "stageA_flat/rho_0.10/test_0001"
    if (!id) return;

    if (selected.has(id)) {{
      selected.delete(id);
      cardEl.classList.remove('selected');
    }} else {{
      selected.add(id);
      cardEl.classList.add('selected');
    }}
    updateSelectedBox();
  }});

  render(rhoSel.value);
</script>
"""
  OUT_HTML.write_text(html, encoding="utf-8")
  print(f"Wrote {OUT_HTML} — open in browser.")



if __name__ == "__main__":
  data = find_images_with_labels()
  write_html(data)

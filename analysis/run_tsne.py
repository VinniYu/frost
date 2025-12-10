#!/usr/bin/env python3
import pandas as pd
import numpy as np
from pathlib import Path

import matplotlib.pyplot as plt
from sklearn.preprocessing import StandardScaler
from sklearn.manifold import TSNE

# ---------------------------------------------------------------------
# CONFIG
# ---------------------------------------------------------------------
DATA_CSV = Path("./labeled_dataset.csv")
OUTPUT_PNG = Path("results/tsne_projection.png")

PERPLEXITY = 35     
LEARNING_RATE = 200
RANDOM_STATE = 42   

# ---------------------------------------------------------------------
# 1. Load dataset
# ---------------------------------------------------------------------
df = pd.read_csv(DATA_CSV)

feature_cols = [
    "rho",
    "k00","k01","k02","k03","k10","k11","k12","k13",
    "b00","b01","b02","b03","b10","b11","b12","b13"
]

X = df[feature_cols].values
y = df["label"].values

print(f"[data] Samples: {X.shape[0]}  Features: {X.shape[1]}")
print(f"[data] Classes: {sorted(df['label'].unique())}")

# ---------------------------------------------------------------------
# 2. Standardize features (VERY important for t-SNE)
# ---------------------------------------------------------------------
scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

# ---------------------------------------------------------------------
# 3. t-SNE
# ---------------------------------------------------------------------
print("[tsne] Running t-SNE… this may take ~5–20 seconds")

tsne = TSNE(
    n_components=2,
    perplexity=PERPLEXITY,
    learning_rate=LEARNING_RATE,
    init="random",
    random_state=RANDOM_STATE
)

X_tsne = tsne.fit_transform(X_scaled)

print("[tsne] Done. Shape =", X_tsne.shape)

# ---------------------------------------------------------------------
# 4. Plot t-SNE projection
# ---------------------------------------------------------------------
plt.figure(figsize=(10, 8))

colors = {
    cls: col for cls, col in zip(
        sorted(df["label"].unique()),
        ["#1f77b4", "#ff7f0e", "#2ca02c", "#9467bd", "#d62728", "#8c564b"]
    )
}

for cls in sorted(df["label"].unique()):
    mask = (y == cls)
    plt.scatter(
        X_tsne[mask, 0],
        X_tsne[mask, 1],
        c=colors[cls],
        s=18,
        label=cls
    )

plt.title("t-SNE projection of snowflake categories")
plt.xlabel("t-SNE 1")
plt.ylabel("t-SNE 2")
plt.legend()
plt.tight_layout()

plt.savefig(OUTPUT_PNG, dpi=200)
# plt.show()


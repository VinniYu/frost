#!/usr/bin/env python3
import pandas as pd
import numpy as np
from pathlib import Path

from sklearn.preprocessing import StandardScaler
from sklearn.discriminant_analysis import LinearDiscriminantAnalysis

import matplotlib.pyplot as plt

DATA_CSV = Path("./labeled_dataset.csv")

# ---------------------------------------------------------------------
# 1. Load dataset
# ---------------------------------------------------------------------
df = pd.read_csv(DATA_CSV)

# Features and labels
feature_cols = [
    "rho",
    "k00","k01","k02","k03","k10","k11","k12","k13",
    "b00","b01","b02","b03","b10","b11","b12","b13"
]
X = df[feature_cols].values
y = df["label"].values

print(f"[data] samples: {X.shape[0]}, features: {X.shape[1]}")
print(f"[data] classes: {sorted(df['label'].unique())}")

# ---------------------------------------------------------------------
# 2. Standardize + LDA
# ---------------------------------------------------------------------
scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

lda = LinearDiscriminantAnalysis()
X_lda = lda.fit_transform(X_scaled, y)

print("LDA components shape:", X_lda.shape)      # (n_samples, n_components)
print("Classes:", lda.classes_)
print("Coefficients shape:", lda.coef_.shape)    # (n_classes, n_features)

# ---------------------------------------------------------------------
# 3. Plot 2D LDA projection (LD1 vs LD2)
# ---------------------------------------------------------------------
plt.figure(figsize=(10, 8))
for lab in lda.classes_:
    mask = (y == lab)
    plt.scatter(X_lda[mask, 0], X_lda[mask, 1], label=lab, s=18)

plt.xlabel("LD1")
plt.ylabel("LD2")
plt.title("LDA projection of snowflake categories")
plt.legend()
plt.tight_layout()
plt.savefig("results/lda_projection_ld1_ld2.png", dpi=200)
# plt.show()

# ---------------------------------------------------------------------
# 4. Inspect coefficients (per-class feature importance)
# ---------------------------------------------------------------------
features = feature_cols
coef = lda.coef_  # shape (n_classes, n_features)

# Global importance across all classes (sum of squared weights)
global_importance = np.sum(coef**2, axis=0)

plt.figure(figsize=(12, 4))
plt.bar(features, global_importance)
plt.title("Global LDA feature importance (sum of squared coefficients)")
plt.xticks(rotation=70)
plt.tight_layout()
plt.savefig("results/lda_global_feature_importance.png", dpi=200)
# plt.show()

# per-class coefficient plots
for ci, cls in enumerate(lda.classes_):
    plt.figure(figsize=(12, 4))
    plt.bar(features, coef[ci])
    plt.title(f"LDA coefficients for class '{cls}'")
    plt.xticks(rotation=70)
    plt.tight_layout()
    plt.savefig(f"results/lda_coef_{cls}.png".replace(" ", "_"), dpi=200)
    # plt.show()

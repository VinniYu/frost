#!/usr/bin/env python3
import pandas as pd
from pathlib import Path

from sklearn.preprocessing import StandardScaler
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import cross_val_score

DATA_CSV = Path("./labeled_dataset.csv")

df = pd.read_csv(DATA_CSV)

feature_cols = [
    "rho",
    "k00","k01","k02","k03","k10","k11","k12","k13",
    "b00","b01","b02","b03","b10","b11","b12","b13"
]
X = df[feature_cols].values
y = df["label"].values

scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

clf = RandomForestClassifier(
    n_estimators=300,
    max_depth=None,
    random_state=42,
)

scores = cross_val_score(clf, X_scaled, y, cv=5)
print("CV accuracy:", scores.mean(), "+/-", scores.std())

clf.fit(X_scaled, y)
importances = clf.feature_importances_

for f, imp in sorted(zip(feature_cols, importances), key=lambda x: -x[1]):
    print(f"{f}: {imp:.4f}")
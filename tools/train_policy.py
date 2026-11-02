#!/usr/bin/env python3
"""Train NanoPass's tiny deterministic learned-inline imitation model.

This intentionally uses only Python's standard library.  Training is offline;
the resulting JSON is read by C++ and contains no executable code.  Labels are
explicitly heuristic-imitation labels, never claimed to be runtime wins.
"""
from __future__ import annotations

import argparse
import json
import math
import random
from pathlib import Path

FEATURES = ["instruction_count", "block_count", "dynamic_call_count", "callee_count", "estimated_inline_cost"]
FEATURE_SCHEMA_VERSION = 1


def feature_vector(record: dict) -> list[float]:
    features = record["features"]
    raw = [
        features["instruction_count"], features["block_count"],
        features.get("dynamic_call_count", 0), features.get("callee_count", 0),
        record["estimated_inline_cost"],
    ]
    return [math.log1p(float(value)) for value in raw]


def load_rows(path: Path, seed: int) -> tuple[dict[str, list[tuple[list[float], int]]], int]:
    groups: dict[str, list[tuple[list[float], int]]] = {}
    total = 0
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        item = json.loads(line)
        record = item.get("record", item)
        if record.get("record_kind") != "inline-imitation":
            continue
        if record.get("schema_version") != 1:
            raise ValueError(f"line {line_number}: unsupported record schema")
        label = record.get("heuristic_action")
        if label not in {"inline", "do-not-inline"}:
            raise ValueError(f"line {line_number}: missing heuristic inline label")
        fingerprint = item.get("program_fingerprint", record.get("caller", "unknown"))
        groups.setdefault(fingerprint, []).append((feature_vector(record), int(label == "inline")))
        total += 1
    # Split by program, never by repeated decisions from the same program.
    # A deterministic shuffle yields nonempty held-out groups for a corpus of
    # at least three unique programs.
    fingerprints = sorted(groups)
    random.Random(seed).shuffle(fingerprints)
    count = len(fingerprints)
    validation_groups = max(1, round(count * 0.1)) if count >= 3 else 0
    test_groups = max(1, round(count * 0.1)) if count >= 3 else 0
    train_groups = count - validation_groups - test_groups
    if train_groups < 1:
        validation_groups = test_groups = 0
        train_groups = count
    partitions = {
        "train": fingerprints[:train_groups],
        "validation": fingerprints[train_groups:train_groups + validation_groups],
        "test": fingerprints[train_groups + validation_groups:],
    }
    splits: dict[str, list[tuple[list[float], int]]] = {
        name: [row for fingerprint in selected for row in groups[fingerprint]]
        for name, selected in partitions.items()
    }
    if not splits["train"]:
        raise ValueError("dataset contains no training inline records")
    return splits, total


def sigmoid(value: float) -> float:
    return 1.0 / (1.0 + math.exp(-value)) if value >= 0 else math.exp(value) / (1.0 + math.exp(value))


def fit(rows: list[tuple[list[float], int]], epochs: int, rate: float, l2: float) -> tuple[float, list[float]]:
    intercept, weights = 0.0, [0.0] * len(FEATURES)
    for _ in range(epochs):
        gradient_b, gradient_w = 0.0, [0.0] * len(weights)
        for values, label in rows:
            error = sigmoid(intercept + sum(w * x for w, x in zip(weights, values))) - label
            gradient_b += error
            for index, value in enumerate(values):
                gradient_w[index] += error * value
        count = len(rows)
        intercept -= rate * gradient_b / count
        for index in range(len(weights)):
            weights[index] -= rate * (gradient_w[index] / count + l2 * weights[index])
    return intercept, weights


def evaluation(rows: list[tuple[list[float], int]], intercept: float, weights: list[float]) -> dict[str, float | int] | None:
    if not rows:
        return None
    tp = fp = tn = fn = 0
    for values, label in rows:
        prediction = sigmoid(intercept + sum(w * x for w, x in zip(weights, values))) >= 0.5
        if prediction and label: tp += 1
        elif prediction: fp += 1
        elif label: fn += 1
        else: tn += 1
    return {
        "accuracy": (tp + tn) / len(rows),
        "precision": tp / (tp + fp) if tp + fp else 0.0,
        "recall": tp / (tp + fn) if tp + fn else 0.0,
        "true_positive": tp, "false_positive": fp,
        "true_negative": tn, "false_negative": fn,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seed", type=int, default=20261103)
    parser.add_argument("--epochs", type=int, default=400)
    parser.add_argument("--rate", type=float, default=0.15)
    parser.add_argument("--l2", type=float, default=0.001)
    args = parser.parse_args()
    splits, total = load_rows(args.input, args.seed)
    intercept, coefficients = fit(splits["train"], args.epochs, args.rate, args.l2)
    output = {
        "model_kind": "logistic-regression",
        "model_version": "inline-imitation-v1",
        "feature_schema_version": FEATURE_SCHEMA_VERSION,
        "feature_transform": "log1p",
        "features": FEATURES,
        "intercept": intercept,
        "coefficients": coefficients,
        "threshold": 0.5,
        "training": {
            "label": "heuristic-imitation",
            "dataset_schema_version": 1,
            "dataset_rows": total,
            "split_seed": args.seed,
            "split_rows": {name: len(rows) for name, rows in splits.items()},
            "evaluation": {name: evaluation(rows, intercept, coefficients) for name, rows in splits.items()},
            "epochs": args.epochs,
            "learning_rate": args.rate,
            "l2": args.l2,
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(output["training"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

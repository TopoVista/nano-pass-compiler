#!/usr/bin/env python3
"""Run the NanoPass benchmark corpus through the versioned local service.

The harness deliberately uses only API telemetry emitted by the compiler.  It
does not infer speedups from IR size and it keeps compilation timing separate
from repeated execution timing.  JIT mode is reported as an end-to-end value
because the present service API does not yet expose materialization separately.
"""

from __future__ import annotations

import argparse
import json
import platform
import statistics
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def request(base: str, path: str, payload: dict[str, Any]) -> dict[str, Any]:
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    query = urllib.request.Request(
        f"{base.rstrip('/')}{path}", data=body,
        headers={"Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(query, timeout=30) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        detail = error.read().decode("utf-8", "replace")
        raise RuntimeError(f"{path} returned HTTP {error.code}: {detail}") from error


def health(base: str) -> dict[str, Any]:
    with urllib.request.urlopen(f"{base.rstrip('/')}/api/v1/readiness", timeout=5) as response:
        return json.loads(response.read().decode("utf-8"))


def percentile(values: list[int], fraction: float) -> int:
    ordered = sorted(values)
    if not ordered:
        return 0
    index = round((len(ordered) - 1) * fraction)
    return ordered[index]


def stage_totals(compilation: dict[str, Any]) -> dict[str, int]:
    totals = {"frontend_us": 0, "lowering_us": 0, "ssa_construction_us": 0, "optimization_us": 0,
              "llvm_lowering_us": 0, "llvm_verify_us": 0}
    for event in compilation.get("passes", []):
        category = event.get("category")
        if category == "frontend" or category == "desugar":
            totals["frontend_us"] += int(event.get("durationUs", 0))
        elif event.get("id") == "ssa-construction":
            totals["ssa_construction_us"] += int(event.get("durationUs", 0))
        elif category == "lowering":
            totals["lowering_us"] += int(event.get("durationUs", 0))
        elif category in {"optimization", "optimization-policy"}:
            totals["optimization_us"] += int(event.get("durationUs", 0))
        elif event.get("id") == "llvm-lowering":
            totals["llvm_lowering_us"] += int(event.get("durationUs", 0))
        elif event.get("id") == "llvm-verify":
            totals["llvm_verify_us"] += int(event.get("durationUs", 0))
    return totals


def structural_metrics(compilation: dict[str, Any]) -> dict[str, int]:
    events = compilation.get("passes", [])
    optimizing = [event for event in events if event.get("category") in {"optimization", "optimization-policy"}]
    last = optimizing[-1] if optimizing else {}
    counters: dict[str, int] = {}
    for event in events:
        for key, value in event.get("counters", {}).items():
            counters[key] = counters.get(key, 0) + int(value)
    llvm_instructions = sum(int(event.get("counters", {}).get("llvmInstructions", 0))
                            for event in events if event.get("id") == "llvm-lowering")
    return {
        "functions": int(compilation.get("telemetry", {}).get("functions", 0)),
        "blocks": int(compilation.get("telemetry", {}).get("blocks", 0)),
        "ssa_values": int(compilation.get("telemetry", {}).get("values", 0)),
        "optimized_ssa_instructions": int(last.get("instructionsAfter", 0)),
        "llvm_instructions": llvm_instructions,
        "calls_inlined": counters.get("callsInlined", 0),
        "blocks_cloned": counters.get("blocksCloned", 0),
        "phis_cloned": counters.get("phisCloned", 0),
        "licm_hoists": counters.get("instructionsHoisted", 0),
        "dce_removals": counters.get("instructionsRemoved", 0),
        "sccp_constants": counters.get("constantsPropagated", 0),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", default="http://127.0.0.1:8080")
    parser.add_argument("--corpus", type=Path, default=Path("benchmarks/corpus"))
    parser.add_argument("--output", type=Path, default=Path("benchmarks/results/latest.json"))
    parser.add_argument("--warmup", type=int, default=2)
    parser.add_argument("--iterations", type=int, default=7)
    parser.add_argument("--modes", default="ssa", help="comma-separated: ssa,jit,adaptive-jit")
    parser.add_argument("--policy", choices=("default", "heuristic", "learned"), default="default")
    args = parser.parse_args()
    if args.warmup < 0 or args.iterations < 3:
        parser.error("warmup must be non-negative and iterations must be at least 3")
    if not args.corpus.is_dir():
        parser.error(f"corpus directory does not exist: {args.corpus}")

    ready = health(args.server)
    if not ready.get("ready"):
        raise RuntimeError("server readiness endpoint did not report ready")
    modes = [mode.strip() for mode in args.modes.split(",") if mode.strip()]
    valid_modes = {"ssa", "jit", "adaptive-jit"}
    if not modes or any(mode not in valid_modes for mode in modes):
        parser.error("--modes accepts ssa,jit,adaptive-jit")

    workloads = sorted(args.corpus.glob("*.nano"))
    if not workloads:
        parser.error("benchmark corpus contains no .nano programs")
    rows: list[dict[str, Any]] = []
    for source_path in workloads:
        source = source_path.read_text(encoding="utf-8")
        for level in range(4):
            compile_request = {
                "schemaVersion": 1, "source": source, "optimizationLevel": level,
                "policy": args.policy, "includeTelemetry": True, "includeLLVM": True,
            }
            compilation = request(args.server, "/api/v1/compile", compile_request)
            if not compilation.get("success"):
                raise RuntimeError(f"compile failed for {source_path.name} O{level}: {compilation.get('diagnostics')}")
            for mode in modes:
                runs: list[int] = []
                for iteration in range(args.warmup + args.iterations):
                    result = request(args.server, "/api/v1/run", {**compile_request, "mode": mode})
                    if not result.get("success"):
                        raise RuntimeError(f"run failed for {source_path.name} O{level} {mode}: {result.get('stderr')}")
                    if iteration >= args.warmup:
                        runs.append(int(result.get("executionDurationUs", 0)))
                rows.append({
                    "benchmark": source_path.stem, "optimization_level": f"O{level}",
                    "policy": args.policy, "mode": mode,
                    "compile": {"total_us": int(compilation["telemetry"]["totalCompileUs"]), **stage_totals(compilation)},
                    "execution": {"warmup_iterations": args.warmup, "measured_iterations": args.iterations,
                                  "median_us": int(statistics.median(runs)), "p25_us": percentile(runs, .25),
                                  "p75_us": percentile(runs, .75), "samples_us": runs,
                                  "jit_materialization_us": None if mode == "ssa" else "not separately exposed"},
                    "structure": structural_metrics(compilation),
                    "policy_decisions": compilation.get("policyRecords", []),
                })
    result = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "methodology": "Each compilation is measured once by compiler service telemetry; execution uses warmup plus repeated service runs and reports median/p25/p75. Timings are machine-dependent and are not performance claims.",
        "environment": {"os": platform.platform(), "python": sys.version.split()[0],
                        "llvm_version": ready.get("llvmVersion"), "compiler_revision": ready.get("compilerRevision"),
                        "execution_policy": ready.get("executionPolicy"), "cpu": platform.processor() or "unavailable"},
        "rows": rows,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {len(rows)} benchmark rows for {len(workloads)} workloads to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

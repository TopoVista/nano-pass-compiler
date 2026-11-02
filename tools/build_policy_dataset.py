#!/usr/bin/env python3
"""Export deterministic heuristic-inline imitation records from Nano programs."""
from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nano", required=True, type=Path, help="built Nano compiler executable")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("sources", nargs="+", type=Path)
    args = parser.parse_args()
    records: list[str] = []
    for source in sorted(args.sources):
        with tempfile.NamedTemporaryFile(suffix=".jsonl", delete=False) as temporary:
            log = Path(temporary.name)
        try:
            completed = subprocess.run(
                [str(args.nano), "optimize", str(source), "-O2", f"--decision-log={log}"],
                text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=False)
            if completed.returncode:
                raise RuntimeError(f"{source}: compiler exited {completed.returncode}: {completed.stderr.strip()}")
            records.extend(line for line in log.read_text(encoding="utf-8").splitlines()
                           if '"record_kind":"inline-imitation"' in line)
        finally:
            log.unlink(missing_ok=True)
    if not records:
        raise RuntimeError("no inline-imitation records were exported")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(records) + "\n", encoding="utf-8")
    print(f"wrote {len(records)} heuristic-inline records to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

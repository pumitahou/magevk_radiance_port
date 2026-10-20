#!/usr/bin/env python3
"""Summarize Radiance E18/E15 per-stage GPU and CPU timing records."""
import argparse
import collections
import json
import math
import re
import statistics
from pathlib import Path


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * fraction)]


def extent(value):
    match = re.fullmatch(r"(\d+)[xX](\d+)", value)
    return f"{match.group(1)}x{match.group(2)}" if match else value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--tag", choices=("E18", "E15", "auto"), default="auto")
    parser.add_argument("--json-out", type=Path, help="write aggregate statistics to this file")
    args = parser.parse_args()
    try:
        lines = args.log.read_text(errors="replace").splitlines()
    except OSError as exc:
        parser.error(str(exc))

    groups = collections.defaultdict(lambda: {"gpu": [], "cpu": []})
    seen = collections.Counter()
    latest_output = None
    for line in lines:
        tag = "E18" if "[RADIANCE-E18-PERF]" in line else "E15" if "[RADIANCE-E15-PERF]" in line else None
        if tag is None or (args.tag != "auto" and tag != args.tag):
            continue
        fields = dict(re.findall(r"([A-Za-z][A-Za-z0-9_]*)=([^\s]+)", line))
        if tag == "E15" and "output" in fields:
            latest_output = extent(fields["output"])
        stage = next((fields[k] for k in ("stage", "pass", "module", "name") if k in fields), "(sin etapa)")
        dimensions = []
        for label, candidates in (
            ("render", ("render", "render_size", "render_extent", "internal", "internal_size", "internal_extent")),
            ("trace", ("trace", "trace_size", "trace_extent")),
            ("output", ("output", "output_size", "output_extent", "display", "display_size", "display_extent")),
        ):
            value = next((fields[k] for k in candidates if k in fields), None)
            if value is None and tag == "E15" and label == "output":
                value = latest_output
            if value is not None:
                dimensions.append(f"{label}={extent(value)}")
        size_key = " ".join(dimensions) if dimensions else "size=unspecified"
        key = (tag, size_key, stage)
        for kind, names in (
            ("gpu", ("gpu_ms", "gpu_interval_ms", "gpu_time_ms", "gpu_duration_ms", "gpu")),
            ("cpu", ("cpu_record_ms", "cpu_ms", "cpu_time_ms", "cpu_duration_ms", "cpu")),
        ):
            raw = next((fields[k] for k in names if k in fields), None)
            if raw is not None:
                try:
                    value = float(raw)
                except ValueError:
                    continue
                if math.isfinite(value):
                    groups[key][kind].append(value)
        if stage != "(sin etapa)" and (groups[key]["gpu"] or groups[key]["cpu"]):
            seen[tag] += 1

    if not seen:
        parser.error(f"no timing records found in {args.log}")
    for tag, count in sorted(seen.items()):
        print(f"{tag}: {count} records")
    summary = []
    for (tag, size, stage), metrics in sorted(groups.items()):
        if not metrics["gpu"] and not metrics["cpu"]:
            continue
        item = {"tag": tag, "sizes": dict(
            re.findall(r"(render|trace|output)=([^ ]+)", size)), "stage": stage}
        parts = [f"[{tag}] {size} {stage}"]
        for kind, values in metrics.items():
            if values:
                stats = {
                    "n": len(values),
                    "median_ms": round(statistics.median(values), 6),
                    "p10_ms": round(percentile(values, .10), 6),
                    "p90_ms": round(percentile(values, .90), 6),
                }
                item[kind] = stats
                parts.append(
                    f"{kind} n={len(values)} median={statistics.median(values):.3f}ms "
                    f"p10={percentile(values, .10):.3f}ms p90={percentile(values, .90):.3f}ms"
                )
        print(" | ".join(parts))
        summary.append(item)
    if args.tag in ("E18", "auto"):
        print("E18 intervalos inclusivos: no sumar etapas anidadas (ray_dispatch_first está dentro de frame_graph).")
    if args.json_out:
        args.json_out.write_text(json.dumps({"records_by_tag": dict(seen), "summary": summary}, indent=2) + "\n")


if __name__ == "__main__":
    main()

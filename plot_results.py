#!/usr/bin/env python3
"""
sort_compare 결과(results.csv)로 보고서용 그래프(PNG)를 그린다.

사용법
    pip install matplotlib
    python3 plot_results.py                       # results.csv -> ./charts/
    python3 plot_results.py --csv results.csv --out charts --shape-n 100000

만드는 그래프 (out 폴더)
    growth_time.png / growth_comparisons.png / growth_moves.png
        n이 커질 때 변화 (random 입력). 왼쪽 선형 축, 오른쪽 로그-로그 축
    growth_time_all_dists.png
        입력 종류 5가지별로 n에 따른 시간 (로그-로그)
    shapes_time.png / shapes_comparisons.png / shapes_moves.png
        입력 종류별 비교 (n = --shape-n). 왼쪽 선형, 오른쪽 로그
    memory_depth.png
        추가 메모리와 재귀 깊이 (n에 따라)
그리고 화면에 로그-로그 기울기(= 복잡도 지수 추정값) 표를 출력한다.
(그래프 글자는 폰트 문제를 피하려고 영어로 적었다.)
"""
import argparse
import csv
import math
import os
import sys

import matplotlib

matplotlib.use("Agg")  # 화면 없이 파일로만 저장
import matplotlib.pyplot as plt  # noqa: E402

ALGOS = ["insertion", "merge", "heap"]
DISTS = ["random", "sorted", "reversed", "nearly", "duplicates"]
COLORS = {"insertion": "#d62728", "merge": "#1f77b4", "heap": "#2ca02c"}
MARKERS = {"insertion": "o", "merge": "s", "heap": "^"}
METRICS = {
    "time_ms": "Time (ms)",
    "comparisons": "Comparisons",
    "moves": "Moves",
    "extra_bytes": "Extra memory (bytes)",
    "max_depth": "Max recursion depth",
}


def load(path):
    """results.csv -> {(distribution, n, algorithm): {metric: value}}"""
    data = {}
    with open(path, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if row["time_ms"] == "":  # skipped 행 (삽입 정렬, 큰 n)
                continue
            key = (row["distribution"], int(row["n"]), row["algorithm"])
            data[key] = {m: float(row[m]) for m in METRICS}
    return data


def sizes_of(data, dist):
    return sorted({n for (d, n, _a) in data if d == dist})


def series(data, dist, algo, metric):
    """(n 목록, 값 목록). 해당 조합이 없는 n은 빠진다."""
    ns, ys = [], []
    for n in sizes_of(data, dist):
        rec = data.get((dist, n, algo))
        if rec is not None:
            ns.append(n)
            ys.append(rec[metric])
    return ns, ys


def save(fig, out_dir, name):
    path = os.path.join(out_dir, name)
    fig.tight_layout()
    fig.savefig(path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print("saved", path)


# ---------------------------------------------------------------- 그래프들
def plot_growth(data, metric, dist, out_dir):
    """n이 커질 때: 왼쪽 선형 / 오른쪽 로그-로그"""
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    for ax, log in zip(axes, (False, True)):
        for algo in ALGOS:
            ns, ys = series(data, dist, algo, metric)
            if log:  # 0은 로그 축에 그릴 수 없다
                pts = [(n, y) for n, y in zip(ns, ys) if y > 0]
                ns, ys = [p[0] for p in pts], [p[1] for p in pts]
            if ns:
                ax.plot(ns, ys, marker=MARKERS[algo], color=COLORS[algo], label=algo)
        if log:
            ax.set_xscale("log")
            ax.set_yscale("log")
        ax.set_xlabel("n")
        ax.set_ylabel(METRICS[metric])
        ax.set_title(f"{METRICS[metric]} vs n ({dist}, {'log-log' if log else 'linear'})")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()
    name = {"time_ms": "growth_time.png", "comparisons": "growth_comparisons.png",
            "moves": "growth_moves.png"}[metric]
    save(fig, out_dir, name)


def plot_growth_all_dists(data, out_dir):
    fig, axes = plt.subplots(2, 3, figsize=(14, 7.5))
    flat = axes.flatten()
    for ax, dist in zip(flat, DISTS):
        for algo in ALGOS:
            ns, ys = series(data, dist, algo, "time_ms")
            pts = [(n, y) for n, y in zip(ns, ys) if y > 0]
            if pts:
                ax.plot([p[0] for p in pts], [p[1] for p in pts],
                        marker=MARKERS[algo], color=COLORS[algo], label=algo)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(dist)
        ax.set_xlabel("n")
        ax.set_ylabel("Time (ms)")
        ax.grid(True, which="both", alpha=0.3)
    for ax in flat[len(DISTS):]:
        ax.axis("off")
    flat[0].legend()
    fig.suptitle("Time vs n for each input type (log-log)")
    save(fig, out_dir, "growth_time_all_dists.png")


def plot_shapes(data, metric, n, out_dir):
    """입력 종류별 막대: 왼쪽 선형 / 오른쪽 로그"""
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.4))
    width = 0.26
    for ax, log in zip(axes, (False, True)):
        for i, algo in enumerate(ALGOS):
            xs, ys = [], []
            for j, dist in enumerate(DISTS):
                rec = data.get((dist, n, algo))
                if rec is None:
                    continue
                v = rec[metric]
                xs.append(j + (i - 1) * width)
                ys.append(float("nan") if (log and v <= 0) else v)
            ax.bar(xs, ys, width, color=COLORS[algo], label=algo)
        ax.set_xticks(range(len(DISTS)))
        ax.set_xticklabels(DISTS)
        if log:
            ax.set_yscale("log")
        ax.set_ylabel(METRICS[metric])
        ax.set_title(f"{METRICS[metric]} by input type (n={n:,}, {'log' if log else 'linear'})")
        ax.grid(True, axis="y", which="both", alpha=0.3)
        ax.legend(ncol=3, loc="upper center", bbox_to_anchor=(0.5, -0.1))  # 막대와 안 겹치게 아래로
    name = {"time_ms": "shapes_time.png", "comparisons": "shapes_comparisons.png",
            "moves": "shapes_moves.png"}[metric]
    save(fig, out_dir, name)


def plot_memory_depth(data, out_dir):
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    for ax, metric in zip(axes, ("extra_bytes", "max_depth")):
        for algo in ALGOS:
            ns, ys = series(data, "random", algo, metric)
            if ns:
                ax.plot(ns, ys, marker=MARKERS[algo], color=COLORS[algo], label=algo)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlabel("n")
        ax.set_ylabel(METRICS[metric])
        ax.set_title(f"{METRICS[metric]} vs n (log-log)")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()
    save(fig, out_dir, "memory_depth.png")


# ---------------------------------------------------------------- 기울기 표
def loglog_slope(ns, ys):
    """log y = a * log n + b 의 기울기 a (최소제곱). 값이 2점 미만이면 None."""
    pts = [(math.log(n), math.log(y)) for n, y in zip(ns, ys) if n > 0 and y > 0]
    if len(pts) < 2:
        return None
    mx = sum(p[0] for p in pts) / len(pts)
    my = sum(p[1] for p in pts) / len(pts)
    den = sum((p[0] - mx) ** 2 for p in pts)
    if den == 0:
        return None
    return sum((p[0] - mx) * (p[1] - my) for p in pts) / den


def print_slopes(data):
    print("\n로그-로그 기울기 (복잡도 지수 추정: O(n^2)이면 약 2, O(n log n)이면 약 1.0~1.2)")
    print(f"{'input':<11}{'algorithm':<11}{'time':>8}{'compares':>10}{'moves':>8}")
    for dist in DISTS:
        for algo in ALGOS:
            cells = []
            for metric in ("time_ms", "comparisons", "moves"):
                ns, ys = series(data, dist, algo, metric)
                s = loglog_slope(ns, ys)
                cells.append("-" if s is None else f"{s:.2f}")
            print(f"{dist:<11}{algo:<11}{cells[0]:>8}{cells[1]:>10}{cells[2]:>8}")


def main():
    ap = argparse.ArgumentParser(description="results.csv 로 그래프를 그린다")
    ap.add_argument("--csv", default="results.csv", help="입력 CSV (기본 results.csv)")
    ap.add_argument("--out", default="charts", help="그래프를 저장할 폴더 (기본 charts)")
    ap.add_argument("--shape-n", type=int, default=100000,
                    help="입력 종류별 비교에 쓸 n (기본 100000, 세 알고리즘 결과가 모두 있는 크기)")
    ap.add_argument("--growth-dist", default="random", choices=DISTS,
                    help="n 증가 그래프에 쓸 입력 종류 (기본 random)")
    args = ap.parse_args()

    if not os.path.exists(args.csv):
        sys.exit(f"{args.csv} 가 없습니다. 먼저 ./sort_compare 를 실행하세요.")
    data = load(args.csv)
    if not data:
        sys.exit("CSV에 유효한 데이터가 없습니다.")
    if not any(n == args.shape_n for (_d, n, _a) in data):
        sys.exit(f"n={args.shape_n} 데이터가 CSV에 없습니다. --shape-n 을 CSV에 있는 크기로 지정하세요.")
    os.makedirs(args.out, exist_ok=True)

    for metric in ("time_ms", "comparisons", "moves"):
        plot_growth(data, metric, args.growth_dist, args.out)
        plot_shapes(data, metric, args.shape_n, args.out)
    plot_growth_all_dists(data, args.out)
    plot_memory_depth(data, args.out)
    print_slopes(data)


if __name__ == "__main__":
    main()

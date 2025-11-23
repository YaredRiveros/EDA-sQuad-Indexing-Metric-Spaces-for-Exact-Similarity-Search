import json
import matplotlib.pyplot as plt
import numpy as np
import os
from collections import defaultdict

# ============================================================
# Cargar archivo unificado (MEMORIA PRINCIPAL)
# ============================================================
with open("main_memory_results.json", "r") as f:
    data = json.load(f)

# ============================================================
# Separar MRQ y KNN
# ============================================================
mrq = [d for d in data if d["query_type"] == "MRQ"]
knn = [d for d in data if d["query_type"] in ("KNN", "MkNN")]

# ============================================================
# Definir L efectivo
# ============================================================
for d in data:
    num_pivots = d.get("num_pivots")
    num_centers = d.get("num_centers_path")
    if num_pivots is not None and 3 <= num_pivots <= 20:
        d["L"] = num_pivots
        d["L_label"] = "num_pivots"
    elif num_centers not in (None, 0):
        d["L"] = num_centers
        d["L_label"] = "num_centers_path"
    else:
        d["L"] = 1
        d["L_label"] = "default"

# Filtrar por L
mrq = [d for d in mrq if d["L"] > 0]
knn = [d for d in knn if d["L"] > 0]

# ============================================================
# Funciones auxiliares
# ============================================================
def choose_yscale(values, ratio_threshold=50, cv_threshold=3.0):
    nums = [float(v) for v in values if isinstance(v, (int, float)) and np.isfinite(v)]
    if not nums:
        return 'linear'
    if any(v <= 0 for v in nums):
        return 'symlog'
    mn, mx = min(nums), max(nums)
    if mn == 0:
        return 'linear'
    ratio = mx / mn
    mean, std = np.mean(nums), np.std(nums)
    cv = std / abs(mean) if mean != 0 else float('inf')
    if ratio >= ratio_threshold or cv >= cv_threshold:
        return 'log'
    return 'linear'

def compute_linthresh(values, factor=0.1, default=1.0):
    nums = [float(v) for v in values if isinstance(v, (int, float)) and np.isfinite(v)]
    pos = [v for v in nums if v > 0]
    if not pos:
        return default
    return max(default, np.median(pos) * factor)

# ============================================================
# Función para graficar individualmente
# ============================================================
def plot_metric(group, group_name, metric, ylabel):
    indexes = sorted(set([d["index"] for d in group]))
    datasets = sorted(set([d["dataset"] for d in group]))
    out_dir = f"main_memory_plots_{group_name}"
    os.makedirs(out_dir, exist_ok=True)
    pivot_ticks = [3, 5, 10, 15, 20]
    pivot_positions = np.arange(len(pivot_ticks))

    for ds in datasets:
        plt.figure(figsize=(7, 5))
        subset = [d for d in group if d["dataset"] == ds]
        if not subset: continue

        x_label = "num_pivots" if any(p.get("L_label") == "num_pivots" for p in subset) else (
            "num_centers_path" if any(p.get("L_label") == "num_centers_path" for p in subset) else "L"
        )

        all_ys = [p[metric] for p in subset if p["L"] in pivot_ticks]
        y_strategy = choose_yscale(all_ys)
        if y_strategy == 'log':
            pos_ys = [v for v in all_ys if v > 0]
            ymin, ymax = min(pos_ys)*0.9, max(pos_ys)*1.1
        elif y_strategy == 'symlog':
            ymin, ymax = min(all_ys), max(all_ys)
            lt = compute_linthresh(all_ys)
        else:
            ymin, ymax = min(all_ys)*0.9, max(all_ys)*1.1

        for i, idx in enumerate(indexes):
            points = [d for d in subset if d["index"] == idx]
            grouped = defaultdict(list)
            for p in points:
                if p["L"] in pivot_ticks:
                    grouped[p["L"]].append(p[metric])

            xs, ys_mean = [], []
            for pos, L in zip(pivot_positions, pivot_ticks):
                if L in grouped:
                    xs.append(pos)
                    ys_mean.append(np.mean(grouped[L]))

            if y_strategy == 'log':
                pairs = [(x, y) for x, y in zip(xs, ys_mean) if y > 0]
                if pairs:
                    xs_plot, ys_plot = zip(*pairs)
                    plt.plot(xs_plot, ys_plot, marker="o", linewidth=2, markersize=6, label=idx)
                    plt.yscale('log')
                    plt.ylim(ymin, ymax)
            elif y_strategy == 'symlog':
                plt.plot(xs, ys_mean, marker="o", linewidth=2, markersize=6, label=idx)
                plt.yscale('symlog', linthresh=lt)
            else:
                plt.plot(xs, ys_mean, marker="o", linewidth=2, markersize=6, label=idx)
                plt.ylim(ymin, ymax)

        plt.xlabel(f"L ({x_label})")
        plt.ylabel(ylabel)
        plt.title(f"{ylabel} vs L — {ds} ({group_name})")
        plt.grid(True, ls="--", alpha=0.5)
        plt.legend()
        plt.xticks(pivot_positions, labels=pivot_ticks)
        plt.xlim(-0.5, len(pivot_ticks)-0.5)
        plt.tight_layout()
        filename = f"{out_dir}/{ds}_{metric}_avg.png"
        plt.savefig(filename)
        plt.close()
        print(f"Guardado: {filename}")

# ============================================================
# Función para graficar todo combinado en una gran figura
# ============================================================
def plot_combined(group, group_name):
    indexes = sorted(set([d["index"] for d in group]))
    datasets = sorted(set([d["dataset"] for d in group]))
    metrics = ["compdists", "time_ms"]
    metric_labels = ["Comparisons", "Time (ms)"]
    out_dir = f"main_memory_plots_{group_name}"
    os.makedirs(out_dir, exist_ok=True)
    pivot_ticks = [3, 5, 10, 15, 20]
    pivot_positions = np.arange(len(pivot_ticks))
    colors = plt.cm.tab20.colors
    markers = ['o','s','^','v','D','P','*','X','H']

    fig, axes = plt.subplots(len(datasets), len(metrics), figsize=(16, 12), sharex=False)

    for row_idx, ds in enumerate(datasets):
        subset = [d for d in group if d["dataset"] == ds]
        if not subset: continue

        for col_idx, metric in enumerate(metrics):
            ax = axes[row_idx, col_idx] if len(datasets) > 1 else axes[col_idx]

            all_ys = [p[metric] for p in subset if p["L"] in pivot_ticks]
            y_strategy = choose_yscale(all_ys)
            if y_strategy == 'log':
                pos_ys = [v for v in all_ys if v > 0]
                ymin, ymax = min(pos_ys)*0.9, max(pos_ys)*1.1
            elif y_strategy == 'symlog':
                ymin, ymax = min(all_ys), max(all_ys)
                lt = compute_linthresh(all_ys)
            else:
                ymin, ymax = min(all_ys)*0.9, max(all_ys)*1.1

            for i, idx in enumerate(indexes):
                points = [d for d in subset if d["index"] == idx]
                grouped = defaultdict(list)
                for p in points:
                    if p["L"] in pivot_ticks:
                        grouped[p["L"]].append(p[metric])

                xs, ys_mean = [], []
                for pos, L in zip(pivot_positions, pivot_ticks):
                    if L in grouped:
                        xs.append(pos)
                        ys_mean.append(np.mean(grouped[L]))

                if y_strategy == 'log':
                    pairs = [(x, y) for x, y in zip(xs, ys_mean) if y > 0]
                    if pairs:
                        xs_plot, ys_plot = zip(*pairs)
                        ax.plot(xs_plot, ys_plot, marker=markers[i%len(markers)],
                                linewidth=2, markersize=6, label=idx, color=colors[i%len(colors)])
                        ax.set_yscale('log')
                        ax.set_ylim(ymin, ymax)
                elif y_strategy == 'symlog':
                    ax.plot(xs, ys_mean, marker=markers[i%len(markers)],
                            linewidth=2, markersize=6, label=idx, color=colors[i%len(colors)])
                    ax.set_yscale('symlog', linthresh=lt)
                else:
                    ax.plot(xs, ys_mean, marker=markers[i%len(markers)],
                            linewidth=2, markersize=6, label=idx, color=colors[i%len(colors)])
                    ax.set_ylim(ymin, ymax)

            ax.grid(True, ls="--", alpha=0.5)
            ax.set_xlim(-0.5, len(pivot_ticks)-0.5)
            ax.set_xticks(pivot_positions)
            ax.set_xticklabels(pivot_ticks)
            if col_idx == 0:
                ax.set_ylabel(ds)
            if row_idx == 0:
                ax.set_title(metric_labels[col_idx])

    handles, labels = axes[0,0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='lower center', ncol=len(indexes), fontsize=10)
    plt.tight_layout(rect=[0, 0.05, 1, 0.95])
    combined_filename = f"{out_dir}/combined_{group_name}.png"
    plt.savefig(combined_filename)
    plt.close()
    print(f"Guardado figura combinada: {combined_filename}")

# ============================================================
# Graficar individuales
# ============================================================
plot_metric(knn, "KNN", "compdists", "Comparisons")
plot_metric(knn, "KNN", "time_ms", "Time (ms)")
plot_metric(mrq, "MRQ", "compdists", "Comparisons")
plot_metric(mrq, "MRQ", "time_ms", "Time (ms)")

# ============================================================
# Graficar combinadas
# ============================================================
plot_combined(knn, "KNN")
plot_combined(mrq, "MRQ")

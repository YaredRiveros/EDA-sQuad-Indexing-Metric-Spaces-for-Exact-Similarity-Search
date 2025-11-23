import json
import matplotlib.pyplot as plt
import numpy as np
import os
from matplotlib.ticker import FuncFormatter

# ============================================================
# Cargar archivo unificado (MEMORIA PRINCIPAL)
# ============================================================
with open("main_memory_results.json", "r") as f:
    data = json.load(f)

# ============================================================
# Separar MRQ y KNN (se llaman MkNN en tu JSON)
# ============================================================
mrq = [d for d in data if d["query_type"] == "MRQ"]
knn = [d for d in data if d["query_type"] in ("KNN", "MkNN")]

# ============================================================
# Definir L efectivo para cada elemento (num_centers_path o num_pivots)
# ============================================================

# Prioridad: num_pivots en rango [3, 20], si no, num_centers_path
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

# Actualizar las listas para usar L
mrq = [d for d in mrq if d["L"] > 0]
knn = [d for d in knn if d["L"] > 0]

# ============================================================
# Función genérica para graficar métricas vs L
# ============================================================

def plot_metric(group, group_name, metric, ylabel):
    indexes = sorted(set([d["index"] for d in group]))
    datasets = sorted(set([d["dataset"] for d in group]))

    out_dir = f"main_memory_plots_{group_name}"
    os.makedirs(out_dir, exist_ok=True)

    for ds in datasets:
        plt.figure(figsize=(7, 5))
        subset = [d for d in group if d["dataset"] == ds]
        if not subset:
            continue

        # Recopilar valores Y y decidir estrategia a nivel de figura
        all_raw_ys = [p[metric] for p in subset if p.get(metric) is not None]

        def choose_yscale(values, ratio_threshold=50, cv_threshold=3.0):
            nums = [float(v) for v in values if isinstance(v, (int, float)) and np.isfinite(v)]
            if not nums:
                return 'linear'
            if any(v <= 0 for v in nums):
                return 'symlog'
            mn = min(nums); mx = max(nums)
            if mn == 0:
                return 'linear'
            ratio = mx / mn if mn != 0 else float('inf')
            mean = float(np.mean(nums)); std = float(np.std(nums))
            cv = (std / abs(mean)) if mean != 0 else float('inf')
            if ratio >= ratio_threshold or cv >= cv_threshold:
                return 'log'
            return 'linear'

        def compute_linthresh(values, factor=0.1, default=1.0):
            nums = [float(v) for v in values if isinstance(v, (int, float)) and np.isfinite(v)]
            if not nums:
                return default
            pos = [v for v in nums if v > 0]
            if not pos:
                return default
            med = float(np.median(pos))
            lin = max(default, med * factor)
            return lin

        def decide_log_strategy(values, empty_decade_threshold=2, ratio_threshold=1e4):
            nums = [float(v) for v in values if isinstance(v, (int, float)) and np.isfinite(v)]
            if not nums:
                return 'linear'
            if any(v <= 0 for v in nums):
                return 'symlog'
            pos = [v for v in nums if v > 0]
            if not pos:
                return 'linear'
            mn = min(pos); mx = max(pos)
            if mn == 0:
                return 'linear'
            ratio = mx / mn if mn != 0 else float('inf')
            min_dec = int(np.floor(np.log10(mn)))
            max_dec = int(np.ceil(np.log10(mx)))
            logs = np.log10(pos)
            empty = 0
            for d in range(min_dec, max_dec + 1):
                if not any((logs >= d) & (logs < d + 1)):
                    empty += 1
            if empty >= empty_decade_threshold or ratio >= ratio_threshold:
                return 'log1p'
            return 'log'

        fig_strategy = decide_log_strategy(all_raw_ys)

        # For main-memory plots, restrict X to canonical pivot ticks
        pivot_ticks = [3, 5, 10, 15, 20]
        pivot_set = set(pivot_ticks)
        # Determinar etiqueta del eje X para todo el subset: priorizar num_pivots en [3,20]
        x_label = "num_pivots (3-20)" if any((p.get("L_label") == "num_pivots") for p in subset) else (
            "num_centers_path" if any((p.get("L_label") == "num_centers_path") for p in subset) else "L"
        )

        for idx in indexes:
            points = [d for d in subset if d["index"] == idx]
            if not points:
                continue

            # Ordenar por L ascendente
            points = sorted(points, key=lambda x: x["L"])
            # Filtrar para que solo se grafiquen los pivotes canonicos
            points = [p for p in points if p["L"] in pivot_set]
            xs = [p["L"] for p in points]
            ys = [p[metric] for p in points]

            # Graficar según estrategia
            if fig_strategy == 'log1p':
                ys_plot = np.log1p(ys)
                plt.plot(xs, ys_plot, marker="o", linewidth=2, markersize=6, label=idx)
            elif fig_strategy == 'log':
                pairs = [(x, y) for (x, y) in zip(xs, ys) if isinstance(y, (int, float)) and y > 0]
                if not pairs:
                    continue
                xs2, ys2 = zip(*pairs)
                plt.plot(xs2, ys2, marker="o", linewidth=2, markersize=6, label=idx)
            else:  # linear or symlog
                plt.plot(xs, ys, marker="o", linewidth=2, markersize=6, label=idx)

        plt.xlabel(f"L ({x_label})")
        plt.ylabel(ylabel)
        plt.title(f"{ylabel} vs L — {ds} ({group_name})")
        plt.grid(True, ls="--", alpha=0.5)
        plt.legend()

        # Forcing xticks to canonical pivot ticks and setting x-limits
        try:
            plt.xticks(pivot_ticks)
            plt.xlim(min(pivot_ticks) - 0.5, max(pivot_ticks) + 0.5)
        except Exception:
            pass
        # Ajustar eje Y según la estrategia elegida
        if fig_strategy == 'log1p':
            pos = [v for v in all_raw_ys if isinstance(v, (int, float)) and np.isfinite(v)]
            if pos:
                ymin = min(pos); ymax = max(pos)
                lymin = np.log1p(ymin); lymax = np.log1p(ymax)
                if lymin == lymax:
                    lymin *= 0.9; lymax *= 1.1
                plt.ylim(lymin, lymax)
                try:
                    n_ticks = min(6, max(2, int(np.ceil(lymax - lymin)) + 1))
                    yticks = np.linspace(lymin, lymax, num=n_ticks)
                    plt.yticks(yticks, [f"{np.expm1(t):.0f}" if np.expm1(t) >= 1 else f"{np.expm1(t):.2g}" for t in yticks])
                except Exception:
                    pass
        elif fig_strategy == 'log':
            pos = [v for v in all_raw_ys if isinstance(v, (int, float)) and np.isfinite(v) and v > 0]
            if pos:
                ymin = min(pos); ymax = max(pos)
                if ymin == ymax:
                    ymin = ymin * 0.9 if ymin != 0 else 0.1
                    ymax = ymax * 1.1
                else:
                    ymin *= 0.9; ymax *= 1.1
                plt.yscale('log')
                plt.ylim(ymin, ymax)
                try:
                    n_ticks = min(6, max(2, int(np.ceil(np.log10(ymax / ymin))) + 1))
                    yticks = np.logspace(np.log10(ymin), np.log10(ymax), num=n_ticks)
                    plt.yticks(yticks, [f"{t:.0f}" if t >= 1 else f"{t:.2g}" for t in yticks])
                except Exception:
                    pass
        elif fig_strategy == 'symlog':
            lt = compute_linthresh(all_raw_ys)
            plt.yscale('symlog', linthresh=lt)

        plt.tight_layout()

        filename = f"{out_dir}/{ds}_{metric}.png"
        plt.savefig(filename)
        plt.close()
        print(f"Guardado: {filename}")

# ============================================================
# Graficar KNN por separado
# ============================================================
plot_metric(knn, "KNN", "compdists", "Comparisons")
plot_metric(knn, "KNN", "time_ms", "Time (ms)")

# ============================================================
# Graficar MRQ por separado
# ============================================================
plot_metric(mrq, "MRQ", "compdists", "Comparisons")
plot_metric(mrq, "MRQ", "time_ms", "Time (ms)")

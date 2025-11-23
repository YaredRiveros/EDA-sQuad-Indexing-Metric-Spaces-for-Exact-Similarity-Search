import json
import matplotlib.pyplot as plt
import numpy as np
import os
from matplotlib.ticker import FuncFormatter

# ============================================================
# Cargar archivo unificado
# ============================================================
with open("secondary_memory_results.json", "r") as f:
    data = json.load(f)

# ============================================================
# Filtrar solo KNN (ojo: en el JSON es "MkNN")
# ============================================================
knn = [d for d in data if d["query_type"] == "MkNN"]
for d in knn:
    d["k"] = int(d["k"])

# ============================================================
# Configuración de índices, colores y marcadores
# ============================================================
indexes = [
    "EGNAT", "DSACLT", "MTREE", "DIndex", "LC", "MB+-tree",
    "CPT", "OmniR-tree", "PMTREE", "MIndex*", "SPBTree"
]

colors = [
    "#555555", "#e41a1c", "#377eb8", "#4daf4a", "#984ea3",
    "#ff7f00", "#00ffff", "#a65628", "#b2df8a", "#ff7f00", "#80b1d3"
]

markers = ['s','o','^','v','D','P','*','X','H','<','>']  # 11 símbolos

datasets_order = ["LA", "Words", "Color", "Synthetic"]
metrics = ["pages", "compdists", "time_ms"]
metric_labels = ["Pages", "Comparisons", "Time (ms)"]

# Crear carpeta de salida
out_dir = "knn_plots_linear"
os.makedirs(out_dir, exist_ok=True)


# ============================================================
# Helper: decidir escala para el eje y (log o linear)
# Devuelve 'log' si todos los valores son positivos y el rango
# dinámico o la dispersión sugieren que log mejora la legibilidad.
# ============================================================
def choose_yscale(values, ratio_threshold=50, cv_threshold=3.0):
    # Extraer solo valores numéricos finitos
    nums = [float(v) for v in values if isinstance(v, (int, float)) and np.isfinite(v)]
    if not nums:
        return 'linear'
    # Si existe algún valor no positivo, preferimos symlog (soporta negativos/ceros)
    if any(v <= 0 for v in nums):
        return 'symlog'
    mn = min(nums)
    mx = max(nums)
    if mn == 0:
        return 'linear'
    ratio = mx / mn if mn != 0 else float('inf')
    # Coeficiente de variación (std/mean)
    mean = float(np.mean(nums))
    std = float(np.std(nums))
    cv = (std / abs(mean)) if mean != 0 else float('inf')
    # Si el rango dinámico es grande o la dispersión relativa es alta, preferimos log
    if ratio >= ratio_threshold or cv >= cv_threshold:
        return 'log'
    return 'linear'


def compute_linthresh(values, factor=0.1, default=1.0):
    # Calcular linthresh para symlog: un valor pequeño relacionado con la mediana
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
    """Decide between 'log', 'log1p' or 'symlog' for positive values.
    If distribution has many empty decades or extreme ratio, prefer 'log1p'.
    """
    nums = [float(v) for v in values if isinstance(v, (int, float)) and np.isfinite(v)]
    if not nums:
        return 'linear'
    if any(v <= 0 for v in nums):
        return 'symlog'
    pos = [v for v in nums if v > 0]
    if not pos:
        return 'linear'
    mn = min(pos)
    mx = max(pos)
    if mn == 0:
        return 'linear'
    ratio = mx / mn if mn != 0 else float('inf')
    # compute empty decades between floor(log10(mn)) and ceil(log10(mx))
    min_dec = int(np.floor(np.log10(mn)))
    max_dec = int(np.ceil(np.log10(mx)))
    decades = range(min_dec, max_dec + 1)
    logs = np.log10(pos)
    empty = 0
    for d in decades:
        if not any((logs >= d) & (logs < d + 1)):
            empty += 1
    if empty >= empty_decade_threshold or ratio >= ratio_threshold:
        return 'log1p'
    return 'log'

# ============================================================
# Función para graficar un plot individual
# ============================================================
def plot_metric_knn(metric, ylabel):
    for ds in datasets_order:
        plt.figure(figsize=(7, 5))
        subset = [d for d in knn if d["dataset"] == ds]
        xticks = [5, 10, 20, 50, 100]
        x_positions = np.linspace(5, 100, len(xticks))

        # Decide la estrategia Y a nivel de figura antes de graficar series
        all_raw_ys = [p[metric] for p in subset if p["k"] in xticks]
        fig_strategy = decide_log_strategy(all_raw_ys)

        for i, idx in enumerate(indexes):
            points = [d for d in subset if d["index"] == idx]
            if not points:
                continue
            points = sorted(points, key=lambda x: x["k"])

            # Construir pares (x,y) según la estrategia de la figura
            if fig_strategy == 'log1p':
                pairs = [(x_positions[xticks.index(p["k"])], p[metric])
                         for p in points if p["k"] in xticks]
                pairs = [(x, y) for (x, y) in pairs if isinstance(y, (int, float)) and np.isfinite(y)]
                if not pairs:
                    continue
                xs, ys = zip(*pairs)
                ys_plot = np.log1p(ys)
                plt.plot(xs, ys_plot, marker=markers[i], linewidth=2, markersize=6,
                         label=idx, color=colors[i])

            elif fig_strategy == 'log':
                pairs = [(x_positions[xticks.index(p["k"])], p[metric])
                         for p in points if p["k"] in xticks]
                pairs = [(x, y) for (x, y) in pairs if isinstance(y, (int, float)) and y > 0]
                if not pairs:
                    continue
                xs, ys = zip(*pairs)
                plt.plot(xs, ys, marker=markers[i], linewidth=2, markersize=6,
                         label=idx, color=colors[i])

            else:  # 'symlog' or 'linear'
                pairs = [(x_positions[xticks.index(p["k"])], p[metric])
                         for p in points if p["k"] in xticks]
                pairs = [(x, y) for (x, y) in pairs if isinstance(y, (int, float)) and np.isfinite(y)]
                if not pairs:
                    continue
                xs, ys = zip(*pairs)
                plt.plot(xs, ys, marker=markers[i], linewidth=2, markersize=6,
                         label=idx, color=colors[i])
        plt.xlabel("k")
        plt.ylabel(ylabel)
        # Aplicar estrategia fig_strategy determinada antes de dibujar series
        if fig_strategy == 'log1p':
            pos = [v for v in all_raw_ys if isinstance(v, (int, float)) and np.isfinite(v)]
            if pos:
                ymin = min(pos)
                ymax = max(pos)
                lymin = np.log1p(ymin)
                lymax = np.log1p(ymax)
                if lymin == lymax:
                    lymin *= 0.9
                    lymax *= 1.1
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
        plt.title(f"{metric_labels[metrics.index(metric)]} vs k (KNN) — {ds}")
        plt.grid(True, ls="--", alpha=0.5)
        plt.legend()
        plt.tight_layout()
        plt.xticks(x_positions, labels=[str(x) for x in xticks])
        plt.xlim(5, 100)
        filename = f"{out_dir}/{ds}_{metric}.png"
        plt.savefig(filename)
        plt.close()
        print(f"Guardado: {filename}")

# ============================================================
# Graficar individuales
# ============================================================
for m in metrics:
    plot_metric_knn(m, metric_labels[metrics.index(m)])

# ============================================================
# Graficar todo en una sola figura
# ============================================================
fig, axes = plt.subplots(len(datasets_order), len(metrics), figsize=(20, 15), sharex=True)

for row_idx, ds in enumerate(datasets_order):
    subset = [d for d in knn if d["dataset"] == ds]
    xticks = [5, 10, 20, 50, 100]
    x_positions = np.linspace(5, 100, len(xticks))
    for col_idx, metric in enumerate(metrics):
        ax = axes[row_idx, col_idx]
        for i, idx in enumerate(indexes):
            points = [d for d in subset if d["index"] == idx]
            if not points:
                continue
            points = sorted(points, key=lambda x: x["k"])
            # Decide scale based on raw y-values for this series
            raw_ys = [p[metric] for p in points if p["k"] in xticks]
            yscale = choose_yscale(raw_ys)
            if yscale == 'log':
                pairs = [(x_positions[xticks.index(p["k"])], p[metric])
                         for p in points if p["k"] in xticks]
                pairs = [(x, y) for (x, y) in pairs if isinstance(y, (int, float)) and y > 0]
                if not pairs:
                    continue
                xs, ys = zip(*pairs)
                ax.plot(xs, ys, marker=markers[i], linewidth=2, markersize=4,
                        label=idx, color=colors[i])
            else:
                pairs = [(x_positions[xticks.index(p["k"])], p[metric])
                         for p in points if p["k"] in xticks]
                pairs = [(x, y) for (x, y) in pairs if isinstance(y, (int, float)) and np.isfinite(y)]
                if not pairs:
                    continue
                xs, ys = zip(*pairs)
                ax.plot(xs, ys, marker=markers[i], linewidth=2, markersize=4,
                        label=idx, color=colors[i])
            # Si la serie decide 'symlog', no filtramos negativos: symlog los maneja.
            if yscale == 'symlog':
                # nothing extra here; we'll set the axis scale below for the subplot
                pass
        ax.grid(True, ls="--", alpha=0.5)
        ax.set_xlim(5, 100)
        ax.set_xticks(x_positions)
        ax.set_xticklabels([str(x) for x in xticks])
        if col_idx == 0:
            ax.set_ylabel(ds)
        if row_idx == 0:
            ax.set_title(metric_labels[metrics.index(metric)])
        # Decide la escala final del subplot: puede ser 'linear', 'log' o 'symlog'
        subplot_raw_ys = [p[metric] for p in subset if p["k"] in xticks]
        subplot_scale = choose_yscale(subplot_raw_ys)
        if subplot_scale == 'log':
            ax.set_yscale('log')
            # Ajustar límites y ticks para evitar espacios vacíos excesivos
            pos = [v for v in subplot_raw_ys if isinstance(v, (int, float)) and np.isfinite(v) and v > 0]
            if pos:
                ymin = min(pos)
                ymax = max(pos)
                if ymin == ymax:
                    ymin = ymin * 0.9 if ymin != 0 else 0.1
                    ymax = ymax * 1.1
                else:
                    ymin = ymin * 0.9
                    ymax = ymax * 1.1
                ax.set_ylim(ymin, ymax)
                try:
                    n_ticks = min(6, max(2, int(np.ceil(np.log10(ymax / ymin))) + 1))
                    yticks = np.logspace(np.log10(ymin), np.log10(ymax), num=n_ticks)
                    ax.set_yticks(yticks)
                    ax.set_yticklabels([f"{t:.0f}" if t >= 1 else f"{t:.2g}" for t in yticks])
                except Exception:
                    pass
        elif subplot_scale == 'symlog':
            lt = compute_linthresh(subplot_raw_ys)
            ax.set_yscale('symlog', linthresh=lt)

# Ajustar leyenda común
handles, labels = axes[0, 0].get_legend_handles_labels()
fig.legend(handles, labels, loc='lower center', ncol=len(indexes), fontsize=10)
plt.tight_layout(rect=[0, 0.05, 1, 0.95])

# Guardar figura combinada
combined_filename = f"{out_dir}/combined_knn.png"
plt.savefig(combined_filename)
plt.close()
print(f"Guardado figura combinada: {combined_filename}")

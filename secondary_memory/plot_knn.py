import json
import matplotlib.pyplot as plt
import numpy as np
import os

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
# Función para graficar un plot individual
# ============================================================
def plot_metric_knn(metric, ylabel):
    for ds in datasets_order:
        plt.figure(figsize=(7, 5))
        subset = [d for d in knn if d["dataset"] == ds]
        xticks = [5, 10, 20, 50, 100]
        x_positions = np.linspace(5, 100, len(xticks))
        for i, idx in enumerate(indexes):
            points = [d for d in subset if d["index"] == idx]
            if not points:
                continue
            points = sorted(points, key=lambda x: x["k"])
            xs = [x_positions[xticks.index(p["k"])] for p in points if p["k"] in xticks]
            ys = [p[metric] for p in points if p["k"] in xticks]
            plt.plot(xs, ys, marker=markers[i], linewidth=2, markersize=6,
                     label=idx, color=colors[i])
        plt.xlabel("k")
        plt.ylabel(ylabel)
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
            xs = [x_positions[xticks.index(p["k"])] for p in points if p["k"] in xticks]
            ys = [p[metric] for p in points if p["k"] in xticks]
            ax.plot(xs, ys, marker=markers[i], linewidth=2, markersize=4,
                    label=idx, color=colors[i])
        ax.grid(True, ls="--", alpha=0.5)
        ax.set_xlim(5, 100)
        ax.set_xticks(x_positions)
        ax.set_xticklabels([str(x) for x in xticks])
        if col_idx == 0:
            ax.set_ylabel(ds)
        if row_idx == 0:
            ax.set_title(metric_labels[metrics.index(metric)])

# Ajustar leyenda común
handles, labels = axes[0, 0].get_legend_handles_labels()
fig.legend(handles, labels, loc='lower center', ncol=len(indexes), fontsize=10)
plt.tight_layout(rect=[0, 0.05, 1, 0.95])

# Guardar figura combinada
combined_filename = f"{out_dir}/combined_knn.png"
plt.savefig(combined_filename)
plt.close()
print(f"Guardado figura combinada: {combined_filename}")

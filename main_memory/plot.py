import json
import matplotlib.pyplot as plt
import numpy as np
import os

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

        for idx in indexes:
            points = [d for d in subset if d["index"] == idx]
            if not points:
                continue

            # Ordenar por L ascendente
            points = sorted(points, key=lambda x: x["L"])
            xs = [p["L"] for p in points]
            ys = [p[metric] for p in points]

            # Etiqueta del eje x según prioridad
            x_label = "num_pivots (3-20)" if any((p["L_label"] == "num_pivots") for p in points) else "num_centers_path"

            plt.plot(xs, ys, marker="o", linewidth=2, markersize=6, label=idx)

        plt.xlabel(f"L ({x_label})")
        plt.ylabel(ylabel)
        plt.title(f"{ylabel} vs L — {ds} ({group_name})")
        plt.grid(True, ls="--", alpha=0.5)
        plt.legend()
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

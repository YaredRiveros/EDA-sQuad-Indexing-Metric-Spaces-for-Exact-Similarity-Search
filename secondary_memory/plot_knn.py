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

# Convertir k a número entero
for d in knn:
    d["k"] = int(d["k"])

# ============================================================
# Listas relevantes
# ============================================================
indexes = sorted(set([d["index"] for d in knn]))
datasets = sorted(set([d["dataset"] for d in knn]))

# Crear carpeta de salida
out_dir = "knn_plots_linear"
os.makedirs(out_dir, exist_ok=True)

# ============================================================
# Función para graficar una métrica contra k (no log)
# ============================================================
def plot_metric_knn(metric, ylabel):
    for ds in datasets:
        plt.figure(figsize=(7, 5))

        # Filtrar dataset
        subset = [d for d in knn if d["dataset"] == ds]

        for idx in indexes:
            points = [d for d in subset if d["index"] == idx]
            if not points:
                continue

            # Ordenar por k ascendente
            points = sorted(points, key=lambda x: x["k"])

            xs = [p["k"] for p in points]
            ys = [p[metric] for p in points]

            plt.plot(xs, ys, marker="o", linewidth=2, markersize=6, label=idx)

        plt.xlabel("k")
        plt.ylabel(ylabel)
        plt.title(f"{metric.upper()} vs k (KNN) — {ds}")
        plt.grid(True, ls="--", alpha=0.5)
        plt.legend()
        plt.tight_layout()

        # Guardar sin mostrar
        filename = f"{out_dir}/{ds}_{metric}.png"
        plt.savefig(filename)
        plt.close()

        print(f"Guardado: {filename}")

# ============================================================
# Generar gráficas lineales KNN
# ============================================================
plot_metric_knn("pages", "Pages")
plot_metric_knn("compdists", "Comparisons")
plot_metric_knn("time_ms", "Time (ms)")

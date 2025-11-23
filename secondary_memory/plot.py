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
# Filtrar solo MRQ
# ============================================================
mrq = [d for d in data if d["query_type"] == "MRQ"]

# Normalizar selectividad y volverla porcentaje
for d in mrq:
    d["selectivity"] = round(float(d["selectivity"]), 4) * 100.0

# ============================================================
# Listas relevantes
# ============================================================
indexes = sorted(set([d["index"] for d in mrq]))
datasets = sorted(set([d["dataset"] for d in mrq]))

# Crear carpeta de salida
out_dir = "mrq_plots"
os.makedirs(out_dir, exist_ok=True)

# ============================================================
# Función para graficar una métrica con log-scale
# ============================================================
def plot_metric(metric, ylabel):
    for ds in datasets:
        plt.figure(figsize=(7, 5))

        # Filtrar dataset
        subset = [d for d in mrq if d["dataset"] == ds]

        for idx in indexes:
            points = [d for d in subset if d["index"] == idx]
            if not points:
                continue

            # Ordenar por selectividad
            points = sorted(points, key=lambda x: x["selectivity"])

            xs = [p["selectivity"] for p in points]
            ys = [p[metric] for p in points]

            plt.plot(xs, ys, marker="o", linewidth=2, markersize=6, label=idx)

        plt.xscale("linear")
        plt.yscale("log")
        plt.xlabel("Selectivity (%)")
        plt.ylabel(ylabel)
        plt.title(f"{metric.upper()} vs Selectivity (MRQ) — {ds}")
        plt.grid(True, which="both", ls="--", alpha=0.5)
        plt.legend()
        plt.tight_layout()

        # Guardar sin mostrar
        filename = f"{out_dir}/{ds}_{metric}.png"
        plt.savefig(filename)
        plt.close()

        print(f"Guardado: {filename}")

# ============================================================
# Generar gráficas y guardarlas
# ============================================================
plot_metric("pages", "Pages (log scale)")
plot_metric("compdists", "Comparisons (log scale)")
plot_metric("time_ms", "Time (ms, log scale)")

#!/usr/bin/env python3
"""
Preprocesar SOLO el dataset Color de manera optimizada.
- Queries: 100 aleatorias
- Pivotes: 3, 5, 10, 15, 20 (selección HFI)
- Radios: Calculados sobre una MUESTRA para mayor velocidad
"""

import os
import json
import random
import numpy as np
from tqdm import tqdm

# Configuración
DATASET_PATH = "../Color.txt"
OUTPUT_DIR = "prepared_experiment"
SAMPLE_SIZE = 10000  # Usar muestra para calcular radios (más rápido)

def L1(a, b):
    """Métrica L1 (Manhattan) para Color"""
    return np.sum(np.abs(a - b))

def load_color(path):
    """Cargar dataset Color"""
    print(f"[INFO] Cargando {path}...")
    rows = []
    with open(path, "r") as f:
        for line in tqdm(f, desc="Cargando"):
            parts = line.strip().split()
            rows.append([float(x) for x in parts])
    return np.array(rows, dtype=np.float32)  # float32 para ahorrar memoria

def select_pivots_HFI(data, num_pivots, metric):
    """Selección de pivotes HFI (Hyperplane-based Farthest-first Initializer)"""
    N = len(data)
    pivots = [0]  # Primer pivote fijo
    score = np.zeros(N)
    
    for _ in tqdm(range(1, num_pivots), desc=f"  Seleccionando {num_pivots} pivotes"):
        last = pivots[-1]
        d = np.array([metric(data[last], data[i]) for i in range(N)])
        score += d
        next_pivot = int(np.argmax(score))
        if next_pivot in pivots:
            next_pivot = int(np.argmax(d))
        pivots.append(next_pivot)
    
    return pivots

def compute_radii_sampled(data, queries, metric, selectivities, sample_size):
    """
    Calcular radios usando una MUESTRA de datos (más rápido).
    Esto es suficiente para estimar selectividad.
    """
    N = len(data)
    
    # Usar muestra si el dataset es grande
    if N > sample_size:
        print(f"[INFO] Usando muestra de {sample_size} objetos para calcular radios")
        sample_indices = random.sample(range(N), sample_size)
        sample_data = data[sample_indices]
    else:
        sample_data = data
    
    radii = {}
    M = len(sample_data)
    
    for s in selectivities:
        rad_list = []
        for q in tqdm(queries, desc=f"  Selectivity {s*100:.0f}%"):
            d = np.array([metric(data[q], sample_data[i]) for i in range(M)])
            rad = np.percentile(d, s*100)
            rad_list.append(rad)
        radii[str(s)] = float(np.mean(rad_list))
    
    return radii

def process_color():
    """Procesar dataset Color"""
    print("\n=== Preprocesando Color ===\n")
    
    data = load_color(DATASET_PATH)
    N = len(data)
    print(f"[INFO] Cargados {N} objetos de {data.shape[1]} dimensiones")
    
    os.makedirs(f"{OUTPUT_DIR}/pivots", exist_ok=True)
    os.makedirs(f"{OUTPUT_DIR}/queries", exist_ok=True)
    os.makedirs(f"{OUTPUT_DIR}/radii", exist_ok=True)
    
    # 1. QUERIES
    print("\n[1/3] Generando queries...")
    queries = random.sample(range(N), 100)
    with open(f"{OUTPUT_DIR}/queries/Color_queries.json", "w") as f:
        json.dump(queries, f, indent=2)
    print(f"  ✓ Color_queries.json")
    
    # 2. RADIOS (con muestra)
    print("\n[2/3] Calculando radios...")
    selectivities = [0.02, 0.04, 0.08, 0.16, 0.32]
    radii = compute_radii_sampled(data, queries, L1, selectivities, SAMPLE_SIZE)
    with open(f"{OUTPUT_DIR}/radii/Color_radii.json", "w") as f:
        json.dump(radii, f, indent=2)
    print(f"  ✓ Color_radii.json")
    print(f"     Radios: {radii}")
    
    # 3. PIVOTES
    print("\n[3/3] Seleccionando pivotes...")
    for p in [3, 5, 10, 15, 20]:
        print(f"\n  Pivotes = {p}")
        pivots = select_pivots_HFI(data, p, L1)
        with open(f"{OUTPUT_DIR}/pivots/Color_pivots_{p}.json", "w") as f:
            json.dump(pivots, f, indent=2)
        print(f"  ✓ Color_pivots_{p}.json")
    
    print("\n[OK] Dataset Color preprocesado exitosamente!")
    print(f"  Queries: {OUTPUT_DIR}/queries/Color_queries.json")
    print(f"  Radios:  {OUTPUT_DIR}/radii/Color_radii.json")
    print(f"  Pivotes: {OUTPUT_DIR}/pivots/Color_pivots_*.json (3, 5, 10, 15, 20)")

if __name__ == "__main__":
    random.seed(42)
    np.random.seed(42)
    process_color()

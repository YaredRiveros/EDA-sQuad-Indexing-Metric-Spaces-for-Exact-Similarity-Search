#!/usr/bin/env python3
"""
Script de ejemplo para entender cómo funciona el cálculo de similitud CVD
entre índices de búsqueda en espacios métricos.
"""

import json
import numpy as np

print("="*80)
print("EJEMPLO: Cálculo de Similitud entre Índices")
print("="*80)

# Cargar datos
DATA_PATH = "../secondary_memory_results.json"
with open(DATA_PATH, 'r') as f:
    records = json.load(f)

print(f"\n✓ Cargados {len(records)} registros del JSON\n")

# PASO 1: Filtrar registros para un escenario específico
print("-"*80)
print("PASO 1: Filtrar registros")
print("-"*80)
print("Criterios de filtrado:")
print("  - Dataset: Words")
print("  - Query type: MkNN")
print("  - k: 20")
print("  - l: 5 (número de pivotes)")

filtered = []
for r in records:
    if (r.get('dataset') == 'Words' and
        r.get('query_type') == 'MkNN' and
        r.get('k') == 20):
        # Verificar l si existe
        l_val = r.get('l', r.get('L', None))
        if l_val is None or int(l_val) == 5:
            filtered.append(r)

print(f"\n✓ Encontrados {len(filtered)} registros que cumplen los criterios\n")

# PASO 2: Agrupar valores por índice
print("-"*80)
print("PASO 2: Agrupar valores por índice")
print("-"*80)

# Agrupar time_ms por índice
indices_time = {}
for r in filtered:
    idx = r.get('index')
    if idx:
        if idx not in indices_time:
            indices_time[idx] = []
        time_ms = r.get('time_ms')
        if time_ms is not None:
            indices_time[idx].append(float(time_ms))

print(f"Índices encontrados: {sorted(indices_time.keys())}\n")

# Mostrar algunos datos
for idx in sorted(indices_time.keys())[:3]:
    vals = indices_time[idx]
    print(f"{idx}:")
    print(f"  Número de mediciones: {len(vals)}")
    if len(vals) > 0:
        print(f"  Primeros 5 valores: {vals[:5]}")
        print(f"  Media: {np.mean(vals):.2f} ms")
        print(f"  Desv. Estándar: {np.std(vals, ddof=1):.2f} ms")
    print()

# PASO 3: Calcular CVD entre dos índices
print("-"*80)
print("PASO 3: Calcular CVD (Coefficient of Variation Distance)")
print("-"*80)

# Seleccionar dos índices para comparar
available_indices = [k for k in sorted(indices_time.keys()) if len(indices_time[k]) > 0]

if len(available_indices) >= 2:
    idx_A = available_indices[0]
    idx_B = available_indices[1]
    
    vals_A = np.array(indices_time[idx_A])
    vals_B = np.array(indices_time[idx_B])
    
    mean_A = float(np.mean(vals_A))
    mean_B = float(np.mean(vals_B))
    
    print(f"\nComparando: {idx_A} vs {idx_B}")
    print(f"\n{idx_A}:")
    print(f"  Valores individuales: {vals_A.tolist()}")
    print(f"  Media: {mean_A:.2f} ms")
    
    print(f"\n{idx_B}:")
    print(f"  Valores individuales: {vals_B.tolist()}")
    print(f"  Media: {mean_B:.2f} ms")
    
    # Calcular CVD
    max_mean = max(abs(mean_A), abs(mean_B))
    if max_mean > 0:
        cvd = abs(mean_A - mean_B) / max_mean
        
        print(f"\nCálculo de CVD:")
        print(f"  CVD = |mean_A - mean_B| / max(mean_A, mean_B)")
        print(f"      = |{mean_A:.2f} - {mean_B:.2f}| / max({mean_A:.2f}, {mean_B:.2f})")
        print(f"      = {abs(mean_A - mean_B):.2f} / {max_mean:.2f}")
        print(f"      = {cvd:.4f}")
        print(f"      = {cvd*100:.2f}%")
        
        # Decisión de conexión
        threshold = 0.15  # 15%
        print(f"\nThreshold de similitud: {threshold} ({threshold*100}%)")
        print(f"\nDecisión:")
        if cvd < threshold:
            print(f"  ✅ CVD ({cvd:.4f}) < threshold ({threshold})")
            print(f"  → SE CONECTAN (diferencia de solo {cvd*100:.1f}%)")
        else:
            print(f"  ❌ CVD ({cvd:.4f}) >= threshold ({threshold})")
            print(f"  → NO SE CONECTAN (diferencia de {cvd*100:.1f}%)")
    else:
        print("  (No se puede calcular CVD: medias son 0)")

# PASO 4: Comparar múltiples pares
print("\n" + "-"*80)
print("PASO 4: Comparación de múltiples pares")
print("-"*80)

threshold = 0.15
connections = []
comparisons = []

for i, idx_A in enumerate(available_indices):
    for idx_B in available_indices[i+1:]:
        vals_A = np.array(indices_time[idx_A])
        vals_B = np.array(indices_time[idx_B])
        
        mean_A = float(np.mean(vals_A))
        mean_B = float(np.mean(vals_B))
        
        max_mean = max(abs(mean_A), abs(mean_B))
        if max_mean > 0:
            cvd = abs(mean_A - mean_B) / max_mean
            connected = cvd < threshold
            
            comparisons.append({
                'A': idx_A,
                'B': idx_B,
                'mean_A': mean_A,
                'mean_B': mean_B,
                'cvd': cvd,
                'connected': connected
            })
            
            if connected:
                connections.append((idx_A, idx_B))

print(f"\nTotal de comparaciones: {len(comparisons)}")
print(f"Conexiones encontradas: {len(connections)}\n")

# Mostrar las primeras 10 comparaciones
print("Primeras 10 comparaciones:")
print(f"{'Índice A':<15} {'Índice B':<15} {'CVD':<10} {'Conectado?':<12}")
print("-" * 60)
for comp in comparisons[:10]:
    status = "✅ Sí" if comp['connected'] else "❌ No"
    print(f"{comp['A']:<15} {comp['B']:<15} {comp['cvd']:<10.4f} {status:<12}")

# PASO 5: Tipo de línea según categoría
print("\n" + "-"*80)
print("PASO 5: Tipo de línea según categoría")
print("-"*80)

category_map = {
    'dsaclt': 'compact', 'lc': 'compact', 'mb': 'compact',
    'mbplus': 'compact', 'mb+-tree': 'compact', 'mtree': 'compact',
    'omnir': 'pivot', 'omnirtree': 'pivot', 'omnir-tree': 'pivot',
    'spb': 'pivot', 'spbtree': 'pivot', 'spb-tree': 'pivot',
    'dindex': 'hybrid', 'd-index': 'hybrid',
    'egnat': 'hybrid', 'gnat': 'hybrid',
    'pmtree': 'hybrid', 'pm-tree': 'hybrid',
    'cpt': 'hybrid',
    'mindex': 'hybrid', 'mindex*': 'hybrid', 'm-index': 'hybrid', 'm-index*': 'hybrid'
}

def normalize(s):
    return "".join(c.lower() for c in str(s) if c.isalnum())

def get_category(idx):
    return category_map.get(normalize(idx), 'other')

print("\nEjemplo de conexiones con tipo de línea:\n")
print(f"{'Índice A':<15} {'Cat A':<10} {'Índice B':<15} {'Cat B':<10} {'Tipo Línea':<15}")
print("-" * 80)

for idx_A, idx_B in connections[:5]:
    cat_A = get_category(idx_A)
    cat_B = get_category(idx_B)
    line_type = "━━━ Sólida" if cat_A == cat_B else "┈┈┈ Punteada"
    
    print(f"{idx_A:<15} {cat_A:<10} {idx_B:<15} {cat_B:<10} {line_type:<15}")

print("\n" + "="*80)
print("FIN DEL EJEMPLO")
print("="*80)
print("\nResumen:")
print("  • CVD mide la diferencia relativa entre medias")
print("  • CVD < 0.15 (15%) → índices similares → se conectan")
print("  • Línea sólida: misma categoría")
print("  • Línea punteada: categorías diferentes")
print("="*80)

#!/usr/bin/env python3
"""
Ejemplo detallado: Proceso completo de similitud con datos reales
Muestra cómo cada registro del JSON se convierte en una conexión en el grafo
"""

import json
import numpy as np

print("="*100)
print(" "*30 + "PROCESO COMPLETO DE SIMILITUD")
print("="*100)

# Cargar datos
with open("../secondary_memory_results.json", 'r') as f:
    records = json.load(f)

print(f"\n📂 Archivo: secondary_memory_results.json")
print(f"📊 Total de registros: {len(records)}")

# Mostrar un registro completo como ejemplo
print("\n" + "="*100)
print("EJEMPLO DE REGISTRO INDIVIDUAL")
print("="*100)

# Buscar un registro específico de M-Tree
mtree_sample = None
for r in records:
    if r.get('index') == 'MTREE' and r.get('dataset') == 'Words' and r.get('query_type') == 'MkNN':
        mtree_sample = r
        break

if mtree_sample:
    print("\nRegistro de ejemplo (M-Tree, Words, MkNN):")
    print("-" * 100)
    for key, value in sorted(mtree_sample.items()):
        print(f"  {key:20s}: {value}")
    print("-" * 100)

# Filtrar para un caso específico
print("\n" + "="*100)
print("FILTRADO Y AGRUPACIÓN")
print("="*100)

dataset = "Words"
query_type = "MkNN"
k = 20

print(f"\n🔍 Filtrando registros:")
print(f"   Dataset: {dataset}")
print(f"   Query type: {query_type}")
print(f"   k: {k}")

filtered = [r for r in records if 
            r.get('dataset') == dataset and
            r.get('query_type') == query_type and
            r.get('k') == k]

print(f"\n✓ {len(filtered)} registros encontrados")

# Agrupar por índice
indices_data = {}
for r in filtered:
    idx = r.get('index')
    if idx:
        if idx not in indices_data:
            indices_data[idx] = {'time_ms': [], 'compdists': [], 'pages': []}
        
        if r.get('time_ms') is not None:
            indices_data[idx]['time_ms'].append(float(r.get('time_ms')))
        if r.get('compdists') is not None:
            indices_data[idx]['compdists'].append(float(r.get('compdists')))
        if r.get('pages') is not None:
            indices_data[idx]['pages'].append(float(r.get('pages')))

print(f"\n📋 Índices agrupados: {len(indices_data)}")
print(f"   {', '.join(sorted(indices_data.keys()))}")

# Mostrar datos detallados
print("\n" + "="*100)
print("DATOS POR ÍNDICE (métrica: time_ms)")
print("="*100)

print(f"\n{'Índice':<15} {'N':<5} {'Valores (time_ms)':<60} {'Media':<10}")
print("-" * 100)

for idx in sorted(indices_data.keys()):
    vals = indices_data[idx]['time_ms']
    if vals:
        mean = np.mean(vals)
        vals_str = str(vals[:5]) if len(vals) <= 5 else f"{vals[:3]}...+{len(vals)-3} más"
        print(f"{idx:<15} {len(vals):<5} {vals_str:<60} {mean:>8.2f} ms")

# CÁLCULO DE CVD PASO A PASO
print("\n" + "="*100)
print("CÁLCULO DE CVD PASO A PASO")
print("="*100)

# Seleccionar dos índices
indices_list = [k for k in sorted(indices_data.keys()) if indices_data[k]['time_ms']]

if len(indices_list) >= 2:
    idx_A = indices_list[0]
    idx_B = indices_list[1]
    
    vals_A = indices_data[idx_A]['time_ms']
    vals_B = indices_data[idx_B]['time_ms']
    
    print(f"\n🔷 Índice A: {idx_A}")
    print(f"   Número de queries ejecutadas: {len(vals_A)}")
    print(f"   Tiempo por query (ms): {vals_A}")
    print(f"   Media: {np.mean(vals_A):.2f} ms")
    
    print(f"\n🔶 Índice B: {idx_B}")
    print(f"   Número de queries ejecutadas: {len(vals_B)}")
    print(f"   Tiempo por query (ms): {vals_B}")
    print(f"   Media: {np.mean(vals_B):.2f} ms")
    
    mean_A = np.mean(vals_A)
    mean_B = np.mean(vals_B)
    max_mean = max(mean_A, mean_B)
    cvd = abs(mean_A - mean_B) / max_mean
    
    print(f"\n📐 Cálculo de CVD:")
    print(f"   ┌─────────────────────────────────────────────────")
    print(f"   │ Fórmula: CVD = |mean_A - mean_B| / max(mean_A, mean_B)")
    print(f"   │")
    print(f"   │ Paso 1: Diferencia absoluta")
    print(f"   │   |{mean_A:.2f} - {mean_B:.2f}| = {abs(mean_A - mean_B):.2f}")
    print(f"   │")
    print(f"   │ Paso 2: Dividir por el máximo")
    print(f"   │   {abs(mean_A - mean_B):.2f} / {max_mean:.2f} = {cvd:.4f}")
    print(f"   │")
    print(f"   │ Paso 3: Convertir a porcentaje")
    print(f"   │   {cvd:.4f} × 100 = {cvd*100:.2f}%")
    print(f"   └─────────────────────────────────────────────────")
    
    threshold = 0.15
    print(f"\n⚙️  Threshold de similitud: {threshold} = {threshold*100}%")
    print(f"\n🔍 Comparación:")
    print(f"   CVD calculado: {cvd:.4f} ({cvd*100:.2f}%)")
    print(f"   Threshold:     {threshold} ({threshold*100}%)")
    
    if cvd < threshold:
        print(f"\n   ✅ {cvd:.4f} < {threshold}")
        print(f"   ➜ SE CONECTAN en el grafo")
        print(f"   ➜ Interpretación: Los índices tienen rendimiento similar")
        print(f"   ➜ La diferencia es solo del {cvd*100:.1f}%")
    else:
        print(f"\n   ❌ {cvd:.4f} >= {threshold}")
        print(f"   ➜ NO SE CONECTAN en el grafo")
        print(f"   ➜ Interpretación: Los índices tienen rendimiento diferente")
        print(f"   ➜ La diferencia es del {cvd*100:.1f}%")

# MATRIZ DE SIMILITUD
print("\n" + "="*100)
print("MATRIZ DE SIMILITUD COMPLETA")
print("="*100)

indices_short = indices_list[:5]  # Solo los primeros 5 para no saturar

print(f"\n{'':15s}", end="")
for idx in indices_short:
    print(f"{idx:>15s}", end="")
print()
print("-" * (15 + 15 * len(indices_short)))

threshold = 0.15
for idx_A in indices_short:
    print(f"{idx_A:<15s}", end="")
    vals_A = indices_data[idx_A]['time_ms']
    mean_A = np.mean(vals_A) if vals_A else 0
    
    for idx_B in indices_short:
        vals_B = indices_data[idx_B]['time_ms']
        mean_B = np.mean(vals_B) if vals_B else 0
        
        if idx_A == idx_B:
            print(f"{'━':>15s}", end="")
        else:
            max_mean = max(mean_A, mean_B)
            if max_mean > 0:
                cvd = abs(mean_A - mean_B) / max_mean
                if cvd < threshold:
                    print(f"{'✓ ' + f'{cvd:.3f}':>15s}", end="")
                else:
                    print(f"{f'{cvd:.3f}':>15s}", end="")
            else:
                print(f"{'N/A':>15s}", end="")
    print()

print("\nLeyenda:")
print("  ✓ 0.xxx = Conectados (CVD < 0.15)")
print("  0.xxx   = No conectados (CVD >= 0.15)")
print("  ━       = Mismo índice")

print("\n" + "="*100)
print("RESUMEN FINAL")
print("="*100)
print("""
🎯 CONCLUSIÓN:

1. Cada registro del JSON representa UNA ejecución de una query
2. Se agrupan múltiples ejecuciones del mismo índice
3. Se calcula la MEDIA de los tiempos de todas las ejecuciones
4. CVD compara las medias de dos índices
5. Si CVD < 0.15 (15% de diferencia) → Se conectan en el grafo
6. El tipo de línea depende de la categoría (compact/pivot/hybrid)

📊 En el grafo final:
   - Cada nodo = un índice
   - Cada línea = similitud (CVD < 0.15)
   - Línea sólida = misma categoría
   - Línea punteada = categorías diferentes
""")
print("="*100)

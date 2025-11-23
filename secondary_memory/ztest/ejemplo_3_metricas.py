#!/usr/bin/env python3
"""
Ejemplo: Cómo se calculan las 3 métricas por separado
"""

import json
import numpy as np

print("="*100)
print(" "*25 + "LAS 3 MÉTRICAS SE CALCULAN POR SEPARADO")
print("="*100)

# Cargar datos
with open("../secondary_memory_results.json", 'r') as f:
    records = json.load(f)

# Filtrar
filtered = [r for r in records if 
            r.get('dataset') == 'Words' and
            r.get('query_type') == 'MkNN' and
            r.get('k') == 20]

# Agrupar por índice Y por métrica
indices_data = {}
for r in filtered:
    idx = r.get('index')
    if idx:
        if idx not in indices_data:
            indices_data[idx] = {
                'time_ms': [],
                'compdists': [],
                'pages': []
            }
        
        # Almacenar CADA métrica por separado
        if r.get('time_ms') is not None:
            indices_data[idx]['time_ms'].append(float(r.get('time_ms')))
        if r.get('compdists') is not None:
            indices_data[idx]['compdists'].append(float(r.get('compdists')))
        if r.get('pages') is not None:
            indices_data[idx]['pages'].append(float(r.get('pages')))

print("\n🔍 Datos de ejemplo para 2 índices:\n")

# Mostrar CPT y DIndex
for idx in ['CPT', 'DIndex']:
    if idx in indices_data:
        print(f"{'='*100}")
        print(f"Índice: {idx}")
        print(f"{'='*100}")
        
        for metric in ['time_ms', 'compdists', 'pages']:
            vals = indices_data[idx][metric]
            if vals:
                print(f"\n  📊 {metric}:")
                print(f"     Valores:  {vals}")
                print(f"     Media:    {np.mean(vals):.2f}")
            else:
                print(f"\n  📊 {metric}: (sin datos)")
        print()

# Ahora calcular CVD para CADA métrica
print("="*100)
print("CÁLCULO DE CVD PARA CADA MÉTRICA (POR SEPARADO)")
print("="*100)

idx_A = 'CPT'
idx_B = 'DIndex'

if idx_A in indices_data and idx_B in indices_data:
    
    metrics_list = ['time_ms', 'compdists', 'pages']
    
    for metric in metrics_list:
        print(f"\n{'─'*100}")
        print(f"📈 MÉTRICA: {metric}")
        print(f"{'─'*100}")
        
        vals_A = indices_data[idx_A][metric]
        vals_B = indices_data[idx_B][metric]
        
        if vals_A and vals_B:
            mean_A = np.mean(vals_A)
            mean_B = np.mean(vals_B)
            max_mean = max(mean_A, mean_B)
            
            if max_mean > 0:
                cvd = abs(mean_A - mean_B) / max_mean
                
                print(f"\n  {idx_A}:")
                print(f"    Valores de {metric}: {vals_A}")
                print(f"    Media: {mean_A:.2f}")
                
                print(f"\n  {idx_B}:")
                print(f"    Valores de {metric}: {vals_B}")
                print(f"    Media: {mean_B:.2f}")
                
                print(f"\n  Cálculo CVD:")
                print(f"    CVD = |{mean_A:.2f} - {mean_B:.2f}| / {max_mean:.2f}")
                print(f"        = {abs(mean_A - mean_B):.2f} / {max_mean:.2f}")
                print(f"        = {cvd:.4f} ({cvd*100:.2f}%)")
                
                threshold = 0.15
                if cvd < threshold:
                    print(f"\n  ✅ CONECTADOS para {metric} (CVD={cvd:.4f} < {threshold})")
                else:
                    print(f"\n  ❌ NO CONECTADOS para {metric} (CVD={cvd:.4f} >= {threshold})")
        else:
            print(f"\n  ⚠️  Sin datos suficientes para comparar {metric}")

# Resumen visual
print("\n" + "="*100)
print("RESUMEN: CONEXIONES POR MÉTRICA")
print("="*100)

print("""
Los grafos se generan así:

┌─────────────────────┬─────────────────────┬─────────────────────┐
│  GRAFO 1: pages     │  GRAFO 2: compdists │  GRAFO 3: time_ms   │
├─────────────────────┼─────────────────────┼─────────────────────┤
│                     │                     │                     │
│  CPT ━━━ MTREE     │  CPT ━━━ EGNAT     │  CPT ━━━ LC        │
│   ┃                │   ┃                │   ┃                │
│   ┃                │   ┃                │   ┃                │
│  DIndex            │  DIndex ━━━ MIndex │  DIndex            │
│                     │                     │                     │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Conexiones basadas  │ Conexiones basadas  │ Conexiones basadas  │
│ en similitud de     │ en similitud de     │ en similitud de     │
│ ACCESOS A DISCO     │ CÁLCULOS DISTANCIA  │ TIEMPO EJECUCIÓN    │
└─────────────────────┴─────────────────────┴─────────────────────┘
""")

print("🎯 PUNTO CLAVE:")
print("  • Se generan 3 grafos INDEPENDIENTES (uno por métrica)")
print("  • Cada grafo usa la media de SU métrica específica")
print("  • Un par de índices puede estar conectado en un grafo pero no en otro")
print("  • Esto permite ver similitudes en DIFERENTES aspectos del rendimiento")

print("\n" + "="*100)
print("EJEMPLO REAL DE CONEXIONES DIFERENTES POR MÉTRICA")
print("="*100)

# Comparar todos los pares para cada métrica
threshold = 0.15
indices_list = ['CPT', 'DIndex', 'EGNAT', 'LC']

for metric in ['time_ms', 'compdists', 'pages']:
    print(f"\n📊 {metric}:")
    connections = []
    
    for i, idx_A in enumerate(indices_list):
        for idx_B in indices_list[i+1:]:
            if idx_A in indices_data and idx_B in indices_data:
                vals_A = indices_data[idx_A][metric]
                vals_B = indices_data[idx_B][metric]
                
                if vals_A and vals_B:
                    mean_A = np.mean(vals_A)
                    mean_B = np.mean(vals_B)
                    max_mean = max(mean_A, mean_B)
                    
                    if max_mean > 0:
                        cvd = abs(mean_A - mean_B) / max_mean
                        
                        if cvd < threshold:
                            connections.append((idx_A, idx_B, cvd))
    
    if connections:
        for idx_A, idx_B, cvd in connections:
            print(f"  ✓ {idx_A} ━━━ {idx_B} (CVD={cvd:.4f})")
    else:
        print(f"  (Sin conexiones)")

print("\n" + "="*100)
print("CONCLUSIÓN")
print("="*100)
print("""
La media NO es solo para tiempo:
  • Media de time_ms   → tiempo promedio de ejecución
  • Media de compdists → número promedio de cálculos de distancia
  • Media de pages     → número promedio de accesos a disco

Cada métrica genera su PROPIO grafo de similitud.
Dos índices pueden ser similares en tiempo pero diferentes en compdists.
""")
print("="*100)

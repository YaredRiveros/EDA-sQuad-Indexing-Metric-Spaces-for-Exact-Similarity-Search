#!/usr/bin/env python3
"""
Script para agregar y analizar resultados de todos los benchmarks
de estructuras de índices métricos en main_memory.
"""

import json
import glob
import pandas as pd
import sys
from pathlib import Path

def load_all_results():
    """Carga todos los archivos JSON de resultados."""
    print("Buscando archivos de resultados...")
    
    # Patrones de búsqueda para las 7 estructuras
    patterns = [
        "*/results/results_*.json",
        "BST/results/*.json",
        "LAESA/results/*.json",
        "BKT/results/*.json",
        "mvpt/results/*.json",
        "EPT/results/*.json",
        "FQT/results/*.json",
        "GNAT/GNAT/results/*.json"
    ]
    
    all_files = []
    for pattern in patterns:
        files = glob.glob(pattern)
        all_files.extend(files)
    
    # Eliminar duplicados
    all_files = list(set(all_files))
    
    print(f"Encontrados {len(all_files)} archivos de resultados")
    
    all_results = []
    for file_path in all_files:
        print(f"  Cargando: {file_path}")
        try:
            with open(file_path, 'r') as f:
                data = json.load(f)
                if isinstance(data, list):
                    all_results.extend(data)
                else:
                    all_results.append(data)
        except Exception as e:
            print(f"  [ERROR] No se pudo cargar {file_path}: {e}")
    
    return all_results, all_files

def analyze_results(df):
    """Genera estadísticas básicas de los resultados."""
    print("\n" + "="*60)
    print("ANÁLISIS DE RESULTADOS")
    print("="*60)
    
    print(f"\nTotal de experimentos: {len(df)}")
    
    print(f"\nÍndices evaluados: {sorted(df['index'].unique())}")
    print(f"Datasets evaluados: {sorted(df['dataset'].unique())}")
    print(f"Tipos de query: {sorted(df['query_type'].unique())}")
    
    print("\n--- Resumen por Índice ---")
    print(df.groupby('index').size())
    
    print("\n--- Resumen por Dataset ---")
    print(df.groupby('dataset').size())
    
    print("\n--- Resumen por Tipo de Query ---")
    print(df.groupby('query_type').size())
    
    # Estadísticas de compdists
    print("\n--- Distance Computations (compdists) ---")
    print(df.groupby('index')['compdists'].agg(['mean', 'std', 'min', 'max']))
    
    # Estadísticas de tiempo
    print("\n--- Query Time (ms) ---")
    print(df.groupby('index')['time_ms'].agg(['mean', 'std', 'min', 'max']))
    
    # Categorías
    if 'category' in df.columns:
        print("\n--- Por Categoría ---")
        print(df.groupby('category')['index'].unique())

def save_results(df, all_files):
    """Guarda los resultados consolidados."""
    print("\n" + "="*60)
    print("GUARDANDO RESULTADOS")
    print("="*60)
    
    # CSV
    csv_file = "consolidated_results.csv"
    df.to_csv(csv_file, index=False)
    print(f"✓ CSV guardado: {csv_file}")
    
    # JSON
    json_file = "consolidated_results.json"
    df.to_json(json_file, orient="records", indent=2)
    print(f"✓ JSON guardado: {json_file}")
    
    # Excel (si pandas tiene soporte)
    try:
        excel_file = "consolidated_results.xlsx"
        df.to_excel(excel_file, index=False)
        print(f"✓ Excel guardado: {excel_file}")
    except ImportError:
        print("  [INFO] openpyxl no disponible, Excel no generado")
    
    # Metadata
    metadata = {
        "total_experiments": len(df),
        "structures": sorted(df['index'].unique().tolist()),
        "datasets": sorted(df['dataset'].unique().tolist()),
        "query_types": sorted(df['query_type'].unique().tolist()),
        "source_files": [str(f) for f in all_files]
    }
    
    meta_file = "consolidated_metadata.json"
    with open(meta_file, 'w') as f:
        json.dump(metadata, f, indent=2)
    print(f"✓ Metadata guardado: {meta_file}")

def generate_summary_tables(df):
    """Genera tablas resumen por estructura y dataset."""
    print("\n" + "="*60)
    print("GENERANDO TABLAS RESUMEN")
    print("="*60)
    
    # Resumen MRQ
    mrq_df = df[df['query_type'] == 'MRQ'].copy()
    if not mrq_df.empty:
        mrq_summary = mrq_df.groupby(['index', 'dataset', 'selectivity']).agg({
            'compdists': 'mean',
            'time_ms': 'mean'
        }).reset_index()
        
        mrq_file = "summary_MRQ.csv"
        mrq_summary.to_csv(mrq_file, index=False)
        print(f"✓ MRQ summary: {mrq_file}")
    
    # Resumen MkNN
    mknn_df = df[df['query_type'] == 'MkNN'].copy()
    if not mknn_df.empty:
        mknn_summary = mknn_df.groupby(['index', 'dataset', 'k']).agg({
            'compdists': 'mean',
            'time_ms': 'mean'
        }).reset_index()
        
        mknn_file = "summary_MkNN.csv"
        mknn_summary.to_csv(mknn_file, index=False)
        print(f"✓ MkNN summary: {mknn_file}")
    
    # Pivot comparison
    pivot_df = df[df['num_pivots'] > 0].copy()
    if not pivot_df.empty:
        pivot_summary = pivot_df.groupby(['index', 'num_pivots', 'query_type']).agg({
            'compdists': 'mean',
            'time_ms': 'mean'
        }).reset_index()
        
        pivot_file = "summary_by_pivots.csv"
        pivot_summary.to_csv(pivot_file, index=False)
        print(f"✓ Pivot comparison: {pivot_file}")

def main():
    print("="*60)
    print("Aggregation Script for Main-Memory Index Benchmarks")
    print("="*60)
    
    # Cargar resultados
    all_results, all_files = load_all_results()
    
    if not all_results:
        print("\n[ERROR] No se encontraron resultados para agregar")
        print("Asegúrate de haber ejecutado los benchmarks primero:")
        print("  ./run_all_benchmarks.sh")
        sys.exit(1)
    
    # Convertir a DataFrame
    df = pd.DataFrame(all_results)
    
    # Análisis
    analyze_results(df)
    
    # Guardar resultados consolidados
    save_results(df, all_files)
    
    # Generar tablas resumen
    generate_summary_tables(df)
    
    print("\n" + "="*60)
    print("COMPLETADO")
    print("="*60)
    print("\nArchivos generados:")
    print("  - consolidated_results.csv")
    print("  - consolidated_results.json")
    print("  - consolidated_metadata.json")
    print("  - summary_MRQ.csv")
    print("  - summary_MkNN.csv")
    print("  - summary_by_pivots.csv")
    print("\nPróximos pasos:")
    print("  1. Revisar los archivos CSV para análisis detallado")
    print("  2. Importar a herramientas de visualización (Excel, R, matplotlib)")
    print("  3. Comparar métricas entre estructuras")
    print("  4. Documentar observaciones en COMPARATIVE_ANALYSIS.md")
    print()

if __name__ == "__main__":
    main()

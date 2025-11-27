#!/usr/bin/env python3
"""
Genera archivos JSON con rankings de índices métricos para memoria principal y secundaria.
Rankings basados en: PA (pages/disk accesses), Compdists, Running Time
Para cada tipo de query: MRQ y MkNN
"""

import json
import os
from pathlib import Path

def calculate_rankings(results):
    """
    Calcula rankings para cada métrica y tipo de query.
    Retorna estructura: {query_type: {metric: [{index, value, rank}]}}
    """
    rankings = {}
    
    for query_type in ['MRQ', 'MkNN']:
        rankings[query_type] = {}
        
        # Filtrar resultados por query type
        query_results = [r for r in results if r.get('query_type') == query_type]
        
        if not query_results:
            continue
            
        # Para cada métrica, calcular promedio y ranking
        for metric in ['pages', 'compdists', 'time_ms']:
            metric_data = {}
            
            # Agrupar por índice y calcular promedio
            for result in query_results:
                index_name = result.get('index')
                if not index_name:
                    continue
                    
                value = result.get(metric)
                if value is None:
                    continue
                
                if index_name not in metric_data:
                    metric_data[index_name] = []
                metric_data[index_name].append(value)
            
            # Calcular promedios
            averages = []
            for index_name, values in metric_data.items():
                avg = sum(values) / len(values)
                averages.append({
                    'index': index_name,
                    'value': avg
                })
            
            # Ordenar y asignar ranks (menor es mejor)
            averages.sort(key=lambda x: x['value'])
            for rank, item in enumerate(averages, 1):
                item['rank'] = rank
            
            rankings[query_type][metric] = averages
    
    return rankings

def generate_main_memory_rankings():
    """Genera rankings para memoria principal"""
    base_path = Path(__file__).parent.parent / "main_memory"
    all_results = []
    
    # Índices de memoria principal
    indices = ['BKT', 'BST', 'EPT', 'FQT', 'GNAT', 'LAESA', 'MVPT', 'SAT']
    
    for index in indices:
        results_dir = base_path / index / "results"
        if not results_dir.exists():
            continue
            
        for json_file in results_dir.glob("*.json"):
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                    if isinstance(data, list):
                        all_results.extend(data)
                    elif isinstance(data, dict):
                        all_results.append(data)
            except Exception as e:
                print(f"Error leyendo {json_file}: {e}")
    
    rankings = calculate_rankings(all_results)
    
    # Guardar en src/data
    output_path = Path(__file__).parent / "src" / "data" / "main_memory_rankings.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        json.dump(rankings, f, indent=2)
    
    print(f"✓ Generado: {output_path}")
    print(f"  Total resultados procesados: {len(all_results)}")
    return rankings

def generate_secondary_memory_rankings():
    """Genera rankings para memoria secundaria"""
    results_file = Path(__file__).parent.parent / "secondary_memory" / "secondary_memory_results.json"
    
    if not results_file.exists():
        print(f"✗ No se encontró {results_file}")
        return None
    
    try:
        with open(results_file, 'r') as f:
            all_results = json.load(f)
        
        rankings = calculate_rankings(all_results)
        
        # Guardar en src/data
        output_path = Path(__file__).parent / "src" / "data" / "secondary_memory_rankings.json"
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            json.dump(rankings, f, indent=2)
        
        print(f"✓ Generado: {output_path}")
        print(f"  Total resultados procesados: {len(all_results)}")
        return rankings
    except Exception as e:
        print(f"✗ Error procesando memoria secundaria: {e}")
        return None

if __name__ == "__main__":
    print("Generando rankings de índices métricos...\n")
    
    print("=== Memoria Principal ===")
    main_rankings = generate_main_memory_rankings()
    
    print("\n=== Memoria Secundaria ===")
    sec_rankings = generate_secondary_memory_rankings()
    
    print("\n✓ Rankings generados exitosamente")

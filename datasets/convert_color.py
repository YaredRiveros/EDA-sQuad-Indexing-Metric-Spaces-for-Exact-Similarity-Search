#!/usr/bin/env python3
"""
Convertir dataset Color (CoPhIR MPEG-7 100k) al formato estándar del proyecto.

Entrada: Color_raw/objects.txt (vectores MPEG-7 de 282 dimensiones)
Salida: Color.txt (formato sin header, solo valores numéricos)

El dataset Color usa métrica L1 (Manhattan distance).
"""

import os
import sys

def convert_color_dataset():
    """
    Convierte objects.txt al formato estándar Color.txt
    
    Formato de objects.txt (MESSIF format):
    - Líneas con #objectKey, #filter: metadata (ignorar)
    - Líneas con tipo de feature: metadata (ignorar)
    - Líneas con vectores numéricos separados por comas
    - MPEG-7 features: 4 vectores por objeto que se concatenan
    
    Formato de salida Color.txt:
    - Sin header (a diferencia de LA/Synthetic que tienen metadata)
    - Una línea por vector completo (282 dims)
    - Valores separados por espacios
    """
    
    input_file = "Color_raw/objects.txt"
    output_file = "Color.txt"
    
    if not os.path.exists(input_file):
        print(f"[ERROR] No se encuentra {input_file}")
        print("Primero ejecuta: bash download_color.sh")
        sys.exit(1)
    
    print(f"[INFO] Leyendo {input_file}...")
    print(f"[INFO] Formato: MESSIF MPEG-7 (282 dimensiones)")
    
    vectors = []
    current_features = []
    
    with open(input_file, "r") as f:
        lines = f.readlines()
    
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        
        # Detectar inicio de nuevo objeto
        if line.startswith("#objectKey"):
            # Si tenemos features acumuladas, guardarlas
            if current_features:
                vectors.append(current_features)
                current_features = []
            i += 1
            continue
        
        # Ignorar metadata lines (que tienen palabras clave)
        if line.startswith("#") or "messif" in line.lower() or (";" in line and "Type" in line):
            i += 1
            continue
        
        # Líneas con vectores numéricos (separados por comas o punto y coma)
        if "," in line or (line and line[0].isdigit() or line[0] == '-'):
            # Reemplazar separadores por espacios
            clean_line = line.replace(",", " ").replace(";", " ")
            parts = clean_line.split()
            
            try:
                nums = [float(x) for x in parts]
                current_features.extend(nums)
            except ValueError:
                # Si falla, intentar extraer solo los números válidos
                nums = []
                for p in parts:
                    try:
                        nums.append(float(p))
                    except:
                        pass
                if nums:
                    current_features.extend(nums)
        
        i += 1
        
        # Mostrar progreso
        if len(vectors) % 10000 == 0 and len(vectors) > 0:
            print(f"  Procesados {len(vectors)} objetos...")
    
    # Agregar último objeto
    if current_features:
        vectors.append(current_features)
    
    N = len(vectors)
    dim = len(vectors[0]) if vectors else 0
    
    print(f"[INFO] Total de objetos: {N}")
    print(f"[INFO] Dimensionalidad: {dim}")
    
    # Verificar que todas tengan la misma dimensión
    dims = [len(v) for v in vectors]
    if len(set(dims)) > 1:
        print(f"[WARN] Dimensiones variables encontradas: {set(dims)}")
        # Filtrar solo los de dimensión esperada (282)
        expected_dim = max(set(dims), key=dims.count)
        vectors = [v for v in vectors if len(v) == expected_dim]
        print(f"[INFO] Filtrando a dimensión {expected_dim}: {len(vectors)} objetos")
        dim = expected_dim
    
    # Escribir archivo de salida
    print(f"[INFO] Escribiendo {output_file}...")
    
    with open(output_file, "w") as f:
        for vec in vectors:
            line = " ".join(str(x) for x in vec)
            f.write(line + "\n")
    
    print(f"[OK] Dataset Color generado exitosamente!")
    print(f"  Archivo: {output_file}")
    print(f"  Objetos: {N}")
    print(f"  Dimensiones: {dim}")
    print(f"  Métrica: L1 (Manhattan)")
    print()
    print("Siguiente paso:")
    print("  cd dataset_processing")
    print("  python main.py")

if __name__ == "__main__":
    convert_color_dataset()

# Datasets

## Datasets Disponibles

### LA (Location-Aware)
- **Fuente**: Proporcionado por los autores
- **Objetos**: 1,073,728 vectores
- **Dimensiones**: 2D (coordenadas geográficas)
- **Métrica**: L2 (Euclidiana)
- **Archivo**: `LA.txt` (formato: `dim N p` en header, luego vectores)

### Synthetic
- **Fuente**: Proporcionado por los autores
- **Objetos**: 28,659 vectores
- **Dimensiones**: 20D
- **Métrica**: L∞ (Chebyshev)
- **Archivo**: `Synthetic.txt` (formato: `dim N p` en header, luego vectores)

### Words
- **Fuente**: Moby Word Lists - https://web.archive.org/web/20170930060409/http://icon.shef.ac.uk/Moby/
- **Objetos**: 597,193 palabras
- **Métrica**: Edit Distance (Levenshtein)
- **Archivo**: `Words.txt` (texto plano, una palabra por línea)
- **Preprocesamiento**: 
  - `words_final.txt`: Lowercase + deduplicado (597,193 palabras)
  - `words_clean`: Concatenación de listas Moby, lowercase, sin duplicados (615,829 palabras)

### Color (CoPhIR MPEG-7)
- **Fuente**: CoPhIR dataset - http://cophir.isti.cnr.it/
- **Descarga directa**: https://www.fi.muni.cz/~xslanin/lmi/ (subset 100k)
- **Objetos**: 95,476 vectores (subset de 100k filtrado)
- **Dimensiones**: 282 (MPEG-7 Color features)
- **Métrica**: L1 (Manhattan)
- **Archivo**: `Color.txt` (formato: vectores sin header, valores separados por espacios)
- **Citación**: 
  ```
  Batko M., Falchi F., Lucchese C., Novak D., Perego R., Rabitti F., 
  Sedmidubsky J., Zezula P.
  Building a web-scale image similarity search system
  Multimedia Tools Appl., 47 (3) (2009), pp. 599-629
  ```

## Descarga y Preprocesamiento

### Color Dataset

Para descargar y preparar el dataset Color:

```bash
# 1. Descargar desde repositorio público
bash download_color.sh

# 2. Convertir al formato estándar
python convert_color.py

# 3. Generar pivotes, queries y radios
cd dataset_processing
python process_color.py
```

Los archivos generados estarán en:
- `Color.txt` - Dataset preprocesado (95,476 objetos × 282 dims)
- `dataset_processing/prepared_experiment/queries/Color_queries.json` - 100 queries
- `dataset_processing/prepared_experiment/radii/Color_radii.json` - Radios para selectividades [0.02, 0.04, 0.08, 0.16, 0.32]
- `dataset_processing/prepared_experiment/pivots/Color_pivots_{3,5,10,15,20}.json` - Pivotes HFI

## Estructura de Archivos

```
datasets/
├── LA.txt                    # Dataset LA (1M objetos)
├── Words.txt                 # Dataset Words (597K palabras)
├── Synthetic.txt             # Dataset Synthetic (28K vectores)
├── Color.txt                 # Dataset Color (95K vectores MPEG-7)
├── paths.hpp                 # Rutas genéricas para C++ (usado por índices)
├── README.md                 # Esta documentación
├── download_color.sh         # Script para descargar Color
├── convert_color.py          # Convertidor Color raw → formato estándar
├── Color_raw/                # [ignorado en git] Archivos descargados
└── dataset_processing/
    ├── main.py               # Preprocesador genérico (todos los datasets)
    ├── process_color.py      # Preprocesador optimizado solo para Color
    └── prepared_experiment/
        ├── queries/          # Queries para cada dataset
        ├── radii/            # Radios para MRQ por selectividad
        └── pivots/           # Pivotes HFI (3, 5, 10, 15, 20)
```

## Uso en Código C++

El archivo `paths.hpp` proporciona funciones genéricas para acceder a datasets:

```cpp
#include "paths.hpp"

// Cargar dataset
string dataset_path = path_dataset("Color");  // Resuelve "../../datasets/Color.txt"

// Cargar queries
vector<int> queries = load_queries_file(path_queries("Color"));

// Cargar radios
auto radii = load_radii_file(path_radii("Color"));

// Cargar pivotes
string pivots_path = path_pivots("Color", 10);  // 10 pivotes
```

## Notas

- **Color_raw/** está excluido del repositorio (`.gitignore`) por su tamaño (~180 MB)
- El preprocesamiento de Color usa una **muestra de 10,000 objetos** para calcular radios (suficiente para estimar selectividad)
- Todos los datasets siguen el mismo formato de salida en `prepared_experiment/`



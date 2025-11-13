# Configuración de FQT para Experimentos

## Descripción General

**FQT (Fixed Queries Tree)** es un índice **pivot-based** donde:
- **1 pivote por nivel** → l pivotes = altura l
- **Mismo pivote en todo el nivel** (diferente a BKT)
- El número de pivotes se controla indirectamente mediante `bsize = n/(arity^l)`

**Paper Reference:** Section 5.11 - FQ Family

---

## Uso con Makefile (Automático)

El Makefile calcula automáticamente el `bsize` necesario para obtener la altura (número de pivotes) deseada.

### Compilación

```bash
cd FQT/
make
```

### Ver Configuración

Antes de ejecutar, ver qué parámetros se usarán:

```bash
make info HEIGHT=5 DATASET=LA.bin
```

**Salida:**
```
==========================================
FQT - Configuración Actual
==========================================
Dataset:         LA.bin
Altura objetivo: 5 niveles (5 pivotes)
Arity:           5
Objetos (aprox): 100000
Bsize calculado: 32

Fórmula: bsize = n / (arity^height)
         32 = 100000 / (5^5)
==========================================
```

### Ejecutar con Altura Específica

```bash
# Ejecutar con HEIGHT=5 (5 pivotes)
make run-l HEIGHT=5 DATASET=LA.bin QUERIES=queries_LA.bin

# Ejecutar con HEIGHT=10 (10 pivotes)
make run-l HEIGHT=10 DATASET=LA.bin QUERIES=queries_LA.bin
```

### Ejecutar Todos los Experimentos del Paper

```bash
# Ejecuta automáticamente con HEIGHT={3,5,10,15,20}
make run-experiments DATASET=LA.bin QUERIES=queries_LA.bin
```

**Resultados generados:**
```
FQT/
├── indices/
│   ├── index_h3.bin
│   ├── index_h5.bin
│   ├── index_h10.bin
│   ├── index_h15.bin
│   └── index_h20.bin
├── resultados/
│   ├── results_h3.txt
│   ├── results_h5.txt
│   └── ...
└── logs/
    ├── build_h3.log
    ├── search_h3.log
    └── ...
```

---

## Comandos Disponibles

| Comando | Descripción |
|---------|-------------|
| `make` | Compilar FQT |
| `make info HEIGHT=5` | Ver configuración y bsize calculado |
| `make run-l HEIGHT=5` | Ejecutar con altura específica |
| `make run-experiments` | Ejecutar con HEIGHT={3,5,10,15,20} |
| `make test HEIGHT=10` | Prueba interactiva |
| `make clean` | Limpiar compilación |
| `make clean-all` | Limpiar todo (incluye resultados) |
| `make help` | Ver ayuda completa |

---

## Parámetros Configurables

```bash
# Especificar dataset y queries
make run-l HEIGHT=5 DATASET=custom.bin QUERIES=custom_queries.bin

# Cambiar arity (aunque el paper usa 5)
make run-l HEIGHT=5 ARITY=7 DATASET=LA.bin

# Mostrar resultados en pantalla
make run-l HEIGHT=5 SHOW=1 DATASET=LA.bin
```

---

## Cómo Funciona

El Makefile calcula automáticamente el `bsize` necesario:

1. **Cuenta objetos** en el dataset
2. **Calcula:** `bsize = n / (arity^HEIGHT)`
3. **Ejecuta:** `./fqt dataset.bin <bsize> <arity>`
4. **Organiza** resultados, índices y logs automáticamente

**Fórmula implementada:**
```bash
bsize = n / (5^HEIGHT)

Ejemplo: HEIGHT=5, n=100000
bsize = 100000 / (5^5) = 100000 / 3125 = 32
```

---

## Valores del Paper

**Altura (número de pivotes):**
```
HEIGHT ∈ {3, 5, 10, 15, 20}
```

**Arity:**
```
ARITY = 5 (fijo)
```

Paper: "We set arity to 5, which yields the best results"

---

## Ejemplo Completo

```bash
# 1. Navegar al directorio
cd FQT/

# 2. Compilar
make

# 3. Ver qué hará para HEIGHT=5
make info HEIGHT=5 DATASET=LA.bin

# 4. Ejecutar con altura 5 (5 pivotes)
make run-l HEIGHT=5 DATASET=LA.bin QUERIES=queries_LA.bin

# 5. O ejecutar todos los experimentos del paper
make run-experiments DATASET=LA.bin QUERIES=queries_LA.bin

# 6. Ver ayuda completa
make help
```

---

## Comparación con Otros Índices

**Para comparación justa con otros índices pivot-based:**

```bash
# FQT con 5 pivotes
make run-l HEIGHT=5 DATASET=LA.bin

# EPT* con 5 pivotes
cd ../EPT/EPT/
make run L=5 DATASET=LA.bin

# LAESA con 5 pivotes
# (configurar l=5 directamente)
```

**Todos usan exactamente 5 pivotes** → Comparación justa

---

## Referencias

**Paper:** Section 5.11 - FQ Family

- "FQT utilizes the same pivot at the same level"
- "FQT is an unbalanced tree"
- "l is the height of FQT"
- "We set arity to 5"

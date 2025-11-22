# Configuración de EPT* para Experimentos

## Descripción

**EPT*** es un índice **pivot-based** que usa el algoritmo PSA (Pivot Selection Algorithm) para seleccionar pivotes adaptativos por objeto.

---

## Compilación

```bash
cd main_memory/EPT
make
```

---

## Ejecutar con L Específico

```bash
# L = 5 pivotes (default)
make run L=5

# L = 10 pivotes
make run L=10

# Con dataset específico
make run L=5 DATASET=LA.txt QUERIES=queries_LA.txt
```

---

## Ejecutar Todos los Experimentos

```bash
# Todas las configuraciones: L = {3, 5, 10, 15, 20}
make run-experiments

# Con dataset específico
make run-experiments DATASET=LA.txt QUERIES=queries_LA.txt
```

---

## Probar un Valor de L

```bash
# Prueba rápida con L=10
make test-l L=10
```

---

## Comandos Disponibles

| Comando | Descripción |
|---------|-------------|
| `make` | Compila el programa |
| `make run L=X` | Ejecuta con L pivotes |
| `make run-experiments` | Ejecuta con L={3,5,10,15,20} |
| `make test-l L=X` | Prueba con L específico |
| `make clean` | Limpia compilación |
| `make clean-all` | Limpia todo (incluye resultados) |
| `make help` | Muestra ayuda |

---

## Parámetros

| Parámetro | Variable | Default | Descripción |
|-----------|----------|---------|-------------|
| Pivotes | `L` | 5 | Número de pivotes a usar |
| Dataset | `DATASET` | dataset.txt | Archivo de datos |
| Queries | `QUERIES` | queries.txt | Archivo de consultas |

**Nota:** EPT* usa `num_cand=40` para el algoritmo PSA (valor fijo en el código).

---

## Estructura de Salida

```
resultados/
  ├── index_l3.bin
  ├── index_l5.bin
  ├── index_l10.bin
  ├── results_l3.txt
  ├── results_l5.txt
  └── results_l10.txt

logs/
  ├── log_l3.txt
  ├── log_l5.txt
  └── log_l10.txt
```

---

## Ejemplo Completo

```bash
# 1. Compilar
make

# 2. Ejecutar experimento con L=5
make run L=5

# 3. Ejecutar todos los experimentos
make run-experiments

# 4. Ver resultados
cat resultados/results_l5.txt
cat logs/log_l5.txt

# 5. Limpiar
make clean
```

---

## Valores del Paper

Según el paper, los valores de L recomendados son:
- **L ∈ {3, 5, 10, 15, 20}**
- Default: L = 5
- Algoritmo: PSA (Pivot Selection Algorithm)
- Candidatos: 40 (num_cand fijo)

---

## Referencias

**Paper:** "On the Hierarchy of Covering-based Space-partitioning Structures for Similarity Search"

**Clasificación:** EPT* es un índice **pivot-based** con selección adaptativa de pivotes (PSA).


```
g++ -O3 -std=gnu++17 test.cpp -o bst_test
```
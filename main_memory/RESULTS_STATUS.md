# Estado de Resultados - Benchmarks de Estructuras Métricas

## Resumen Actual (16 Nov 2025, 22:45)

### ✅ Estructuras COMPLETAS (3/3 datasets cada una):

1. **BST** - 3 archivos JSON
   - ✅ results_BST_LA.json
   - ✅ results_BST_Words.json
   - ✅ results_BST_Synthetic.json

2. **LAESA** - 3 archivos JSON
   - ✅ results_LAESA_LA.json
   - ✅ results_LAESA_Words.json
   - ✅ results_LAESA_Synthetic.json

3. **BKT** - 3 archivos JSON
   - ✅ results_BKT_LA.json
   - ✅ results_BKT_Words.json
   - ✅ results_BKT_Synthetic.json

4. **MVPT** - 3 archivos JSON
   - ✅ results_MVPT_LA.json
   - ✅ results_MVPT_Words.json
   - ✅ results_MVPT_Synthetic.json

**Total: 12 archivos JSON completos**

---

### ⏳ Estructuras EN PROGRESO:

5. **GNAT** - 2/3 datasets (66% completo)
   - ✅ results_GNAT_LA.json
   - ✅ results_GNAT_Synthetic.json
   - ❌ results_GNAT_Words.json (FALTANTE)
   - 🔄 Estado: Ejecutándose (PID 1050, 13+ min de ejecución)

6. **FQT** - 1/3 datasets (33% completo)
   - ✅ results_FQT_LA.json
   - ❌ results_FQT_Words.json (FALTANTE)
   - ❌ results_FQT_Synthetic.json (FALTANTE)
   - 🔄 Estado: Ejecutándose (PID 1208, 3+ min de ejecución)

---

### ❌ Estructuras CON PROBLEMAS:

7. **EPT*** - 1/3 datasets (33% completo)
   - ❌ results_EPT_LA.json (ERROR: queries en formato incorrecto)
   - ✅ results_EPT_Words.json
   - ❌ results_EPT_Synthetic.json (NO EJECUTADO)
   - ⚠️ Problema: EPT intenta leer queries como vectores float[] pero están en JSON como índices
   - 🔴 Estado: Terminó con error "double free detected"

---

## Datasets Analizados

| Dataset    | Tipo    | Estado    | Estructuras que lo procesaron |
|------------|---------|-----------|-------------------------------|
| LA         | Vectors | ✅ Existe | BST, LAESA, BKT, MVPT, GNAT✓, FQT✓ |
| Words      | Strings | ✅ Existe | BST, LAESA, BKT, MVPT, EPT✓ |
| Synthetic  | Vectors | ✅ Existe | BST, LAESA, BKT, MVPT, GNAT✓ |
| Color      | N/A     | ❌ No existe | Ninguna (todas lo omiten) |

**Nota:** Color.txt no existe en el directorio datasets/

---

## Resumen Cuantitativo

- **Archivos JSON generados:** 16/21 (76%)
- **Estructuras completas:** 4/7 (57%)
- **Estructuras en progreso:** 2/7 (29%)
- **Estructuras con error:** 1/7 (14%)

---

## Resultados FALTANTES por Prioridad

### Alta Prioridad (En ejecución - esperar):
1. ⏳ GNAT → Words (procesando actualmente)
2. ⏳ FQT → Words, Synthetic (procesando actualmente)

### Media Prioridad (Requiere corrección):
3. ⚠️ EPT* → LA, Synthetic (requiere fix en manejo de queries)

### Baja Prioridad (No crítico):
4. ❌ Color → Todas las estructuras (dataset no existe)

---

## Problemas Identificados

### EPT* - Error en Lectura de Queries

**Error:** 
```
[ERROR] No se pudo leer dimensión 0 de query.
free(): double free detected in tcache 2
```

**Causa Raíz:**
- EPT usa función `load_queries_float()` que espera queries como vectores float[]
- Los archivos JSON contienen queries como índices enteros: `[189621, 83760, ...]`
- EPT necesita acceder a los objetos originales usando estos índices

**Solución Requerida:**
1. Modificar `load_queries_float()` para leer índices del JSON
2. Cargar el dataset completo en memoria
3. Convertir índices a vectores usando el dataset cargado
4. Alternativa: Crear archivos de queries en formato vectorial para EPT

---

## Tiempo Estimado de Finalización

### Benchmarks Actuales:
- GNAT (LA): ~5-10 min adicionales
- FQT (LA): ~10-15 min adicionales por HEIGHT
- FQT tiene 5 heights (3,5,10,15,20), actualmente en HEIGHT=15

**Total estimado:** 30-60 minutos para completar GNAT y FQT

### Corrección de EPT:
- Fix del código: 10-15 min
- Ejecución LA + Synthetic: 20-30 min
- **Total EPT:** 30-45 minutos adicionales

---

## Acciones Pendientes

1. ✅ Esperar finalización de GNAT y FQT (~30-60 min)
2. ⚠️ Corregir EPT para manejar queries desde JSON
3. ▶️ Ejecutar EPT con LA y Synthetic
4. 📊 Agregar resultados con aggregate_results.py
5. 📈 Generar análisis comparativo final

---

## Scripts de Monitoreo

```bash
# Ver progreso actual
tail -f EPT/EPT_benchmark.log
tail -f GNAT/GNAT/GNAT_benchmark.log
tail -f FQT/FQT_benchmark.log

# Verificar procesos
ps aux | grep -E '(EPT|GNAT|FQT)_test' | grep -v grep

# Listar resultados generados
find . -name "*.json" -path "*/results/*" | wc -l

# Ver último dataset procesado por cada estructura
find . -name "*.json" -path "*/results/*" | xargs ls -lt | head -10
```

---

## Notas Técnicas

### Configuraciones de Parámetros:
- **BST/BKT/GNAT/FQT:** HEIGHT = {3, 5, 10, 15, 20}
- **LAESA:** NUM_PIVOTS = {16, 32, 64, 128, 256}
- **MVPT:** NUM_PIVOTS = {1, 2, 4, 8, 16}
- **EPT*:** L = {10, 15, 20, 25, 30} (pivotes por objeto)

### Queries por Dataset:
- LA: 100 queries
- Words: 102 queries (confirmado en EPT log)
- Synthetic: 100 queries (estimado)

### Selectividades MRQ:
- {0.02, 0.04, 0.08, 0.16, 0.32}

### k-valores MkNN:
- {5, 10, 20, 50, 100}

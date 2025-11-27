# Rankings de Índices Métricos - Frontend

## Nuevas Funcionalidades

### Vista de Rankings
Se agregó una nueva vista de rankings para memoria principal y secundaria que muestra el desempeño de cada índice métrico según tres métricas clave:

- **PA (Pages/Disk Accesses)**: Accesos a disco (solo memoria secundaria)
- **Compdists**: Cálculos de distancia realizados
- **Running Time**: Tiempo de ejecución en milisegundos

### Cómo Usar

1. **Generar datos de rankings:**
   ```bash
   cd Frontend-EDA
   python3 generate_rankings.py
   ```

2. **Iniciar el frontend:**
   ```bash
   npm run dev
   ```

3. **Ver rankings:**
   - Selecciona "Main Memory" o "Secondary Memory" en el header
   - Haz clic en el botón "Ver Rankings" para alternar entre gráficos y rankings
   - Los rankings están organizados por tipo de query (MRQ y MkNN)
   - Para cada query se muestran 3 tablas (PA, Compdists, Running Time)
   - El top 3 está destacado con colores especiales:
     - 🥇 #1: Amarillo (mejor desempeño)
     - 🥈 #2: Plateado
     - 🥉 #3: Bronce

### Archivos Creados

- `generate_rankings.py`: Script para procesar resultados y generar JSONs de rankings
- `src/components/RankingTable.jsx`: Componente de tabla de rankings
- `src/data/main_memory_rankings.json`: Rankings de memoria principal
- `src/data/secondary_memory_rankings.json`: Rankings de memoria secundaria

### Archivos Modificados

- `src/pages/MainMemoryView.jsx`: Agregado botón "Ver Rankings" y vista de tabla
- `src/pages/SecondMemoryView.jsx`: Agregado botón "Ver Rankings" y vista de tabla

### Estructura de Datos de Rankings

```json
{
  "MRQ": {
    "pages": [
      {
        "index": "DIndex",
        "value": 1.4615,
        "rank": 1
      },
      ...
    ],
    "compdists": [...],
    "time_ms": [...]
  },
  "MkNN": {
    "pages": [...],
    "compdists": [...],
    "time_ms": [...]
  }
}
```

### Notas

- Los rankings se calculan promediando los resultados de todos los datasets
- Memoria principal no usa "pages" (siempre en RAM)
- Los valores más bajos representan mejor desempeño
- Los rankings se actualizan automáticamente al regenerar los archivos JSON

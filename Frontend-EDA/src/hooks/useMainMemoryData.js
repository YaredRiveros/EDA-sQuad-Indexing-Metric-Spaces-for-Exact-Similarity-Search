// src/hooks/useMainMemoryData.js
import raw from "../data/main_memory_results.json";

const PIVOT_TICKS = [3, 5, 10, 15, 20];

export function useMainMemoryData({ query = "MRQ", dataset = "LA" }) {
  // Filtrar dataset y tipo de consulta
  const filtered = raw.filter(
    (r) => r.dataset === dataset && r.query_type === query
  );

  // key = index__L  → acumulamos valores para promediar
  const groups = {};

  for (const r of filtered) {
    const numPivots = r.num_pivots;
    const numCenters = r.num_centers_path;

    let L;
    if (
      typeof numPivots === "number" &&
      numPivots >= 3 &&
      numPivots <= 20
    ) {
      L = numPivots; // L = num_pivots
    } else if (
      numCenters !== null &&
      numCenters !== undefined &&
      numCenters !== 0
    ) {
      L = numCenters; // L = num_centers_path
    } else {
      // En el script se pone 1, pero luego sólo usa L in pivot_ticks,
      // así que si no cae ahí simplemente lo ignoramos.
      continue;
    }

    // Sólo queremos L dentro de {3,5,10,15,20}
    if (!PIVOT_TICKS.includes(L)) continue;

    const key = `${r.index}__${L}`;
    if (!groups[key]) {
      groups[key] = {
        index: r.index,
        L,
        times: [],
        comps: [],
      };
    }
    groups[key].times.push(r.time_ms);
    groups[key].comps.push(r.compdists);
  }

  // Construir estructura final: series[index] = [{ x: L, time, comp }]
  const series = {};

  Object.values(groups).forEach((g) => {
    const avgTime = g.times.reduce((a, b) => a + b, 0) / g.times.length;
    const avgComp = g.comps.reduce((a, b) => a + b, 0) / g.comps.length;

    if (!series[g.index]) series[g.index] = [];
    series[g.index].push({
      x: g.L,
      time: avgTime,
      comp: avgComp,
    });
  });

  // Ordenar por L (x)
  Object.values(series).forEach((arr) => {
    arr.sort((a, b) => a.x - b.x);
  });

  return series;
}

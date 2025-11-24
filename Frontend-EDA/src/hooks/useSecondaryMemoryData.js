// src/hooks/useSecondaryMemoryData.js
import raw from "../data/secondary_memory_results.json";

export function useSecondaryMemoryData({ query = "MRQ", dataset = "Synthetic" }) {
  const filtered = raw.filter(
    (r) => r.dataset === dataset && r.query_type === query
  );

  const series = {};

  if (query === "MRQ") {
    // x = selectividad en porcentaje (round(.,4)*100) como en Python
    for (const r of filtered) {
      if (r.selectivity == null) continue;
      const sel = Number(r.selectivity);
      if (Number.isNaN(sel)) continue;

      const x = Math.round(sel * 10000) / 100; // ≈ round(sel,4)*100

      if (!series[r.index]) series[r.index] = [];
      series[r.index].push({
        x,
        time: r.time_ms,
        comp: r.compdists,
        pages: r.pages,
      });
    }
  } else {
    // MkNN: x = k (5,10,20,50,100)
    for (const r of filtered) {
      if (r.k == null) continue;
      const k = Number(r.k);
      if (Number.isNaN(k)) continue;

      if (!series[r.index]) series[r.index] = [];
      series[r.index].push({
        x: k,
        time: r.time_ms,
        comp: r.compdists,
        pages: r.pages,
      });
    }
  }

  // ordenar por X
  Object.keys(series).forEach((idx) => {
    series[idx].sort((a, b) => a.x - b.x);
  });

  return series;
}

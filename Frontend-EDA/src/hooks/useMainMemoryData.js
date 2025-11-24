// src/hooks/useMainMemoryData.js
import { useEffect, useState } from "react";

const PIVOT_TICKS = [3, 5, 10, 15, 20];

export function useMainMemoryData({
  query = "MRQ",
  dataset = "LA",
  reloadKey = 0,
}) {
  const [series, setSeries] = useState({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch(
          "http://localhost:4000/api/main_memory_results"
        );
        const raw = await res.json();

        // 1) Filtrar por dataset y tipo de consulta
        const filtered = raw.filter(
          (r) => r.dataset === dataset && r.query_type === query
        );

        // 2) Agrupar por (index, L) igual que en tu versión estática
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
            // L = num_pivots
            L = numPivots;
          } else if (
            numCenters !== null &&
            numCenters !== undefined &&
            numCenters !== 0
          ) {
            // L = num_centers_path
            L = numCenters;
          } else {
            continue;
          }

          // Sólo consideramos los L válidos (como en el script de python)
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

        // 3) Construir estructura final: series[index] = [{ x: L, time, comp }]
        const s = {};

        Object.values(groups).forEach((g) => {
          const avgTime =
            g.times.reduce((a, b) => a + b, 0) / g.times.length;
          const avgComp =
            g.comps.reduce((a, b) => a + b, 0) / g.comps.length;

          if (!s[g.index]) s[g.index] = [];
          s[g.index].push({
            x: g.L,
            time: avgTime,
            comp: avgComp,
          });
        });

        // 4) Ordenar cada serie por L
        Object.values(s).forEach((arr) => {
          arr.sort((a, b) => a.x - b.x);
        });

        setSeries(s);
      } catch (e) {
        console.error("Error cargando main_memory_results:", e);
        setSeries({});
      }
    }

    load();
  }, [query, dataset, reloadKey]);

  return series;
}

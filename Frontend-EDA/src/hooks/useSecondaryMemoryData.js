import { useEffect, useState } from "react";

export function useSecondaryMemoryData({ query = "MRQ", dataset = "Synthetic", reloadKey = 0 }) {
  const [series, setSeries] = useState({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("http://localhost:4000/api/secondary_memory_results");
        const raw = await res.json();

        const filtered = raw.filter(
          (r) => r.dataset === dataset && r.query_type === query
        );

        const s = {};

        if (query === "MRQ") {
          for (const r of filtered) {
            if (r.selectivity == null) continue;
            const sel = Number(r.selectivity);
            if (Number.isNaN(sel)) continue;
            const x = Math.round(sel * 10000) / 100;

            if (!s[r.index]) s[r.index] = [];
            s[r.index].push({
              x,
              time: r.time_ms,
              comp: r.compdists,
              pages: r.pages,
            });
          }
        } else {
          // MkNN → x = k
          for (const r of filtered) {
            if (r.k == null) continue;
            const k = Number(r.k);
            if (Number.isNaN(k)) continue;

            if (!s[r.index]) s[r.index] = [];
            s[r.index].push({
              x: k,
              time: r.time_ms,
              comp: r.compdists,
              pages: r.pages,
            });
          }
        }

        Object.keys(s).forEach((idx) => {
          s[idx].sort((a, b) => a.x - b.x);
        });

        setSeries(s);
      } catch (e) {
        console.error("Error cargando secondary_memory_results:", e);
        setSeries({});
      }
    }

    load();
  }, [query, dataset, reloadKey]);

  return series;
}

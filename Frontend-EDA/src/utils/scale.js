export function chooseYScaleFromSeries(series, key) {
  const values = [];

  Object.values(series).forEach((arr) => {
    arr.forEach((p) => {
      const v = p[key];
      if (typeof v === "number" && Number.isFinite(v)) {
        values.push(v);
      }
    });
  });

  if (values.length === 0) {
    return { type: "linear" };
  }

  // Si hay valores <= 0 evitamos logarítmica
  if (values.some((v) => v <= 0)) {
    const mn = Math.min(...values);
    const mx = Math.max(...values);
    return {
      type: "linear",
      min: mn * 0.9,
      max: mx * 1.1,
    };
  }

  const mn = Math.min(...values);
  const mx = Math.max(...values);
  if (mn === 0) {
    return { type: "linear" };
  }

  const ratio = mx / mn;
  const mean = values.reduce((a, b) => a + b, 0) / values.length;
  const variance =
    values.reduce((s, v) => s + (v - mean) * (v - mean), 0) / values.length;
  const std = Math.sqrt(variance);
  const cv = mean !== 0 ? std / Math.abs(mean) : Infinity;

  // Misma idea que Python: si rango o dispersión grande → log
  if (ratio >= 50 || cv >= 3.0) {
    return {
      type: "logarithmic",
      min: mn * 0.9,
      max: mx * 1.1,
    };
  }

  return {
    type: "linear",
    min: mn * 0.9,
    max: mx * 1.1,
  };
}

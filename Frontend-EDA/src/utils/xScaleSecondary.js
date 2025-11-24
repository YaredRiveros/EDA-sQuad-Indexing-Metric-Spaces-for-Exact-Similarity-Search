// src/utils/xScaleSecondary.js
export function buildXScale({ memory = "main", query = "MRQ", label = "X" }) {
  // por defecto: lineal
  const base = {
    type: "linear",
    title: { display: true, text: label, color: "#ccc" },
    ticks: { color: "#ccc" },
    grid: { color: "#ffffff15" },
  };

  // Solo memoria secundaria necesita cosas raras
  if (memory !== "secondary") {
    return base;
  }

  // MRQ secundario: selectividad (%) en escala log base 2
  if (query === "MRQ") {
    return {
      type: "logarithmic",
      base: 2,
      title: { display: true, text: "Selectivity (%)", color: "#ccc" },
      ticks: {
        color: "#ccc",
        callback: (value) => {
          const v = Number(value);
          // mostramos solo potencias de 2 bonitas
          if (!Number.isFinite(v) || v <= 0) return "";
          const isPow2 = (v & (v - 1)) === 0;
          return isPow2 ? v : "";
        },
      },
      grid: { color: "#ffffff15" },
      min: 2,
    };
  }

  // MkNN secundario: k en {5,10,20,50,100}
  if (query === "MkNN") {
    const allowed = [5, 10, 20, 50, 100];
    return {
      type: "linear",
      title: { display: true, text: "k", color: "#ccc" },
      min: 5,
      max: 100,
      ticks: {
        color: "#ccc",
        callback: (value) =>
          allowed.includes(Number(value)) ? value : "",
      },
      grid: { color: "#ffffff15" },
    };
  }

  return base;
}

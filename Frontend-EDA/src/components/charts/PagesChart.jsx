import { Line } from "react-chartjs-2";
import { ChartContainer } from "./ChartContainer";
import { colorForIndex } from "../../constants/colors";
import { buildXScale } from "../../utils/xScaleSecondary";

export default function PagesChart({ data, memory = "secondary", query = "MRQ" }) {
  const indices = Object.keys(data || {});
  const allY = [];

  const series = indices.map((idx) => {
    const pts = data[idx] || [];
    pts.forEach((p) => allY.push(p.pages));

    return {
      label: idx,
      data: pts.map((p) => ({ x: p.x, y: p.pages })),
      borderColor: colorForIndex(idx),
      pointBackgroundColor: colorForIndex(idx),
      borderWidth: 2,
      tension: 0.25,
    };
  });

  let yType = "linear";
  if (allY.length > 1) {
    const vals = allY.filter(
      (v) => typeof v === "number" && Number.isFinite(v) && v > 0
    );
    if (vals.length > 1) {
      const min = Math.min(...vals);
      const max = Math.max(...vals);
      const ratio = max / min;
      if (ratio >= 50) yType = "logarithmic";
    }
  }

  const xScale = buildXScale({
    memory,
    query,
    label: query === "MRQ" ? "Selectivity (%)" : "k",
  });

  return (
    <ChartContainer title="Pages (PA)">
      <Line
        data={{ datasets: series }}
        options={{
          responsive: true,
          maintainAspectRatio: false,

          plugins: {
            legend: {
              position: "bottom",
              labels: { color: "#e5e5e5" },
            },
            tooltip: {
              backgroundColor: "#1a1a1d",
              titleColor: "#fff",
              bodyColor: "#fff",
              borderColor: "#ffffff20",
              borderWidth: 1,
            },
          },

          scales: {
            x: xScale,
            y: {
              type: yType,
              title: { display: true, text: "Pages", color: "#ccc" },
              ticks: { color: "#ccc" },
              grid: { color: "#ffffff10" },
            },
          },
        }}
      />
    </ChartContainer>
  );
}

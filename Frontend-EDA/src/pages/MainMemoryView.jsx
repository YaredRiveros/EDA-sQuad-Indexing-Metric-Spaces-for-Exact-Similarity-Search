// MainMemoryView.jsx / .tsx
import { useState } from "react";
import DatasetSelector from "../components/layout/DatasetSelector";
import QueryTypeSelector from "../components/layout/QueryTypeSelector";
import MRQChart from "../components/charts/MRQChart";
import MkNNChart from "../components/charts/MkNNChart";
import RankingTable from "../components/RankingTable";
import { useMainMemoryData } from "../hooks/useMainMemoryData";
import mainMemoryRankings from "../data/main_memory_rankings.json";

export default function MainMemoryView() {
  const [query, setQuery] = useState("MRQ");
  const [dataset, setDataset] = useState("LA");
  const [reloadKey, setReloadKey] = useState(0);
  const [running, setRunning] = useState(false);
  const [showRankings, setShowRankings] = useState(false);

  const data = useMainMemoryData({ query, dataset, reloadKey });

  const handleRun = async () => {
    try {
      setRunning(true);
      await fetch("http://localhost:4000/api/run/main", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ dataset }), 
      });

      setReloadKey((k) => k + 1);
    } catch (e) {
      console.error(e);
      alert("Error al ejecutar experimentos de memoria principal");
    } finally {
      setRunning(false);
    }
  };

  return (
    <div className="min-h-screen bg-[#0f0f10] p-6 space-y-6">
      <div className="bg-[#141416] border border-white/10 shadow-lg rounded-xl p-4">
        <div className="flex flex-wrap gap-6 items-center">
          <QueryTypeSelector value={query} onChange={setQuery} />
          <DatasetSelector value={dataset} onChange={setDataset} />

          <button
            onClick={() => setShowRankings(!showRankings)}
            className="px-4 py-2 rounded-lg text-sm font-medium bg-purple-600 hover:bg-purple-500 text-white"
          >
            {showRankings ? "Ver Gráficos" : "Ver Rankings"}
          </button>

          <button
            onClick={handleRun}
            disabled={running}
            className={`ml-auto px-4 py-2 rounded-lg text-sm font-medium ${
              running
                ? "bg-gray-600 text-gray-300 cursor-wait"
                : "bg-sky-600 hover:bg-sky-500 text-white"
            }`}
          >
            {running ? "Ejecutando..." : "Run Main Experiments"}
          </button>
        </div>
      </div>

      {showRankings ? (
        <div className="max-w-7xl mx-auto">
          <RankingTable rankings={mainMemoryRankings} title="Rankings - Memoria Principal" />
        </div>
      ) : (
        <div className="max-w-6xl mx-auto grid gap-6 md:grid-cols-2">
          <MRQChart data={data} memory="main" query={query} />
          <MkNNChart data={data} memory="main" query={query} />
        </div>
      )}
    </div>
  );
}

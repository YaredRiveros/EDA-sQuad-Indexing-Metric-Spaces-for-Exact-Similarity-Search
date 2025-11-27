import { useState } from "react";
import DatasetSelector from "../components/layout/DatasetSelector";
import QueryTypeSelector from "../components/layout/QueryTypeSelector";
import MRQChart from "../components/charts/MRQChart";
import MkNNChart from "../components/charts/MkNNChart";
import PagesChart from "../components/charts/PagesChart";
import RankingTable from "../components/RankingTable";
import { useSecondaryMemoryData } from "../hooks/useSecondaryMemoryData";
import secondaryMemoryRankings from "../data/secondary_memory_rankings.json";

export default function SecondMemoryView() {
  const [query, setQuery] = useState("MRQ");
  const [dataset, setDataset] = useState("LA");
  const [reloadKey, setReloadKey] = useState(0);
  const [running, setRunning] = useState(false);
  const [showRankings, setShowRankings] = useState(false);

  const data = useSecondaryMemoryData({ query, dataset, reloadKey });

  const handleRun = async () => {
    try {
      setRunning(true);
      await fetch("http://localhost:4000/api/run/secondary", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ dataset }),
      });
      setReloadKey((k) => k + 1);
    } catch (e) {
      console.error(e);
      alert("Error al ejecutar experimentos de memoria secundaria");
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
            onClick={handleRun}
            disabled={running}
            className={`ml-auto px-4 py-2 rounded-lg text-sm font-medium ${
              running
                ? "bg-gray-600 text-gray-300 cursor-wait"
                : "bg-sky-600 hover:bg-sky-500 text-white"
            }`}
          >
            {running ? "Ejecutando..." : "Run Secondary Experiments"}
          </button>
        </div>
      </div>

      {showRankings ? (
        <div className="max-w-7xl mx-auto">
          <RankingTable rankings={secondaryMemoryRankings} title="Rankings - Memoria Secundaria" />
        </div>
      ) : (
        <div className="max-w-6xl mx-auto grid gap-6 md:grid-cols-2">
          <MRQChart data={data} memory="secondary" query={query} />
          <MkNNChart data={data} memory="secondary" query={query} />
          <div className="md:col-span-2 flex justify-center">
            <div className="w-full md:w-1/2">
              <PagesChart data={data} memory="secondary" query={query} />
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

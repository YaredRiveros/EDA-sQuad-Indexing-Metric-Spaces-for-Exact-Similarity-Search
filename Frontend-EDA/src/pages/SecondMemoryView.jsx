// src/pages/SecondMemoryView.jsx
import { useState } from "react";
import DatasetSelector from "../components/layout/DatasetSelector";
import QueryTypeSelector from "../components/layout/QueryTypeSelector";
import MRQChart from "../components/charts/MRQChart";
import MkNNChart from "../components/charts/MkNNChart";
import PagesChart from "../components/charts/PagesChart";
import { useSecondaryMemoryData } from "../hooks/useSecondaryMemoryData";

export default function SecondMemoryView() {
  const [query, setQuery] = useState("MRQ");
  const [dataset, setDataset] = useState("LA");

  const data = useSecondaryMemoryData({ query, dataset });

  return (
    <div className="min-h-screen bg-[#0f0f10] p-6 space-y-6">
      <div className="bg-[#141416] border border-white/10 shadow-lg rounded-xl p-4">
        <div className="flex flex-wrap gap-6">
          <QueryTypeSelector value={query} onChange={setQuery} />
          <DatasetSelector value={dataset} onChange={setDataset} />
        </div>
      </div>

      <div className="grid grid-cols-3 gap-6">
        <MRQChart data={data} memory="secondary" query={query} />
        <MkNNChart data={data} memory="secondary" query={query} />
        <PagesChart data={data} memory="secondary" query={query} />
      </div>
    </div>
  );
}

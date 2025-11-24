import { useState } from "react";
import MainMemoryView from "./pages/MainMemoryView";
import SecondMemoryView from "./pages/SecondMemoryView";
import Header from "./components/Header";

export default function App() {
  const [memory, setMemory] = useState("main");

  return (
    <div className="min-h-screen bg-[#0f0f10] text-gray-200">
      <Header memory={memory} setMemory={setMemory} />

      {memory === "main" ? (
        <MainMemoryView />
      ) : (
        <SecondMemoryView />
      )}
    </div>
  );
}

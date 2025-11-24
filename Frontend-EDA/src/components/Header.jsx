import React from "react";
import { useState } from "react";

export default function Header({ memory, setMemory }) {
  const [active, setActive] = useState(false);
  return (
    <header
      className="
        w-full bg-[#141416] border-b border-white/10
        px-8 py-4 flex items-center justify-between shadow-lg
      "
    >
      {/* TITLE */}
      <h1 className="text-xl font-semibold tracking-wide text-gray-100">
        Metric Dashboard
      </h1>

      {/* MEMORY SWITCH */}
      <div className="flex items-center gap-2 bg-[#0f0f10] border border-white/10 p-1 rounded-xl">

        {/* MAIN MEMORY */}
        <button
          onClick={() => setMemory("main")}
          className={`
            px-4 py-1 rounded-lg text-sm font-medium transition-all
            ${
              memory === "main"
                ? "bg-white text-black shadow"
                : "text-gray-300 hover:bg-white/10"
            }
          `}
        >
          Main Memory
        </button>

        {/* SECONDARY MEMORY */}
        <button
          onClick={() => setMemory("secondary")}
          className={`
            px-4 py-1 rounded-lg text-sm font-medium transition-all
            ${
              memory === "secondary"
                ? "bg-white text-black shadow"
                : "text-gray-300 hover:bg-white/10"
            }
          `}
        >
          Secondary Memory
        </button>

       <button
  onClick={() => {}}
  className="
    px-4 py-1 rounded-lg text-sm font-medium transition-all
    bg-transparent text-gray-300 
    active:bg-white active:text-black active:shadow
  "
>
  RUN
</button>
      </div>
    </header>
  );
}

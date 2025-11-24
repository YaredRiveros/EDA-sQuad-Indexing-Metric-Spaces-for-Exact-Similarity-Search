import React from "react";
import { Tab } from "@headlessui/react";

export default function QueryTypeSelector({ value, onChange }) {
  const tabs = ["MRQ", "MkNN"];

  return (
    <Tab.Group
      selectedIndex={tabs.indexOf(value)}
      onChange={(i) => onChange(tabs[i])}
    >
      <Tab.List
        className="
          flex bg-[#141416] border border-white/10 
          rounded-xl p-1 shadow-sm
        "
      >
        {tabs.map((t) => (
          <Tab key={t} as={React.Fragment}>
            {({ selected }) => (
              <button
                className={`
                  px-4 py-2 text-sm font-medium rounded-lg transition-all
                  ${
                    selected
                      ? "bg-white text-black shadow"
                      : "text-gray-300 hover:bg-white/10"
                  }
                `}
              >
                {t}
              </button>
            )}
          </Tab>
        ))}
      </Tab.List>
    </Tab.Group>
  );
}

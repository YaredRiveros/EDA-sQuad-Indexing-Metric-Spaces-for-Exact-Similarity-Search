import React, { Fragment } from "react";
import { Listbox, Transition } from "@headlessui/react";
import { ChevronUpDownIcon, CheckIcon } from "@heroicons/react/24/solid";

export default function DatasetSelector({ value, onChange }) {
  const datasets = ["LA", "Words", "Color", "Synthetic"];

  return (
    <Listbox value={value} onChange={onChange}>
      <div className="relative w-48 text-gray-200">
        <Listbox.Button
          className="
            w-full cursor-pointer rounded-lg 
            bg-[#141416] border border-white/10
            py-2 pl-3 pr-10 text-left text-sm 
            shadow-sm
            focus:outline-none focus:ring-2 focus:ring-white/20
          "
        >
          <span>{value}</span>

          <ChevronUpDownIcon
            className="h-5 w-5 absolute right-2 top-2.5 text-gray-400"
          />
        </Listbox.Button>

        <Transition
          as={Fragment}
          leave="transition ease-in duration-100"
          leaveFrom="opacity-100"
          leaveTo="opacity-0"
        >
          <Listbox.Options
            className="
              absolute mt-1 w-full rounded-lg 
              bg-[#141416] border border-white/10 
              shadow-xl z-10 py-1
            "
          >
            {datasets.map((d) => (
              <Listbox.Option
                key={d}
                value={d}
                className={({ active }) =>
                  `cursor-pointer select-none py-2 pl-8 pr-4 text-sm relative
                  ${
                    active
                      ? "bg-white/10 text-white"
                      : "text-gray-300"
                  }`
                }
              >
                {({ selected }) => (
                  <>
                    <span>{d}</span>

                    {selected && (
                      <CheckIcon
                        className="h-4 w-4 absolute left-2 top-2.5 text-white"
                      />
                    )}
                  </>
                )}
              </Listbox.Option>
            ))}
          </Listbox.Options>
        </Transition>
      </div>
    </Listbox>
  );
}

export function ChartContainer({ title, children }) {
  return (
    <div className="bg-[#141416] border border-white/10 p-4 rounded-xl h-[360px] shadow-lg">
      <div className="text-sm font-semibold mb-3 text-gray-200">{title}</div>

      <div className="w-full h-[300px]">
        {children}
      </div>
    </div>
  );
}

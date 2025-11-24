export const PALETTE = [
  "#4b5563", "#ef4444", "#3b82f6", "#22c55e", "#a855f7",
  "#14b8a6", "#f59e0b", "#0ea5e9", "#dc2626", "#6d28d9",
  "#65a30d", "#ea580c"
];

const cache = new Map();

export function colorForIndex(name) {
  if (cache.has(name)) return cache.get(name);
  let h = 0;
  for (let i=0; i<name.length; i++) h = (h * 31 + name.charCodeAt(i)) >>> 0;
  const color = PALETTE[h % PALETTE.length];
  cache.set(name, color);
  return color;
}

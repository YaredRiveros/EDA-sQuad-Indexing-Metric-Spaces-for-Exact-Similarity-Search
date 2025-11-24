import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  LogarithmicScale,   
  PointElement,
  LineElement,
  Tooltip,
  Legend,
} from "chart.js";

ChartJS.defaults.font.family = "Inter";
ChartJS.defaults.color = "#6b7280"; // gris suave
ChartJS.defaults.borderColor = "#e5e7eb";
ChartJS.defaults.scale.grid.color = "#f3f4f6";


ChartJS.register(
  CategoryScale,
  LinearScale,
  LogarithmicScale,  
  PointElement,
  LineElement,
  Tooltip,
  Legend
);

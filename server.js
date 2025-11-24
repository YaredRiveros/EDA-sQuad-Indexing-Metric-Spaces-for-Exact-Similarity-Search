const express = require("express");
const cors = require("cors");
const fs = require("fs").promises;
const path = require("path");
const { spawn } = require("child_process");

const app = express();
const PORT = 4000;

app.use(cors());
app.use(express.json());


async function aggregateJson(baseDir, outputFileName) {
  const entries = await fs.readdir(baseDir, { withFileTypes: true });
  const allData = [];

  for (const entry of entries) {
    if (!entry.isDirectory()) continue;

    const resultsDir = path.join(baseDir, entry.name, "results");
    try {
      const resultFiles = await fs.readdir(resultsDir);
      for (const file of resultFiles) {
        if (!file.endsWith(".json")) continue;
        const fullPath = path.join(resultsDir, file);
        const content = await fs.readFile(fullPath, "utf8");
        try {
          const data = JSON.parse(content);
          if (Array.isArray(data)) {
            allData.push(...data);
          } else {
            console.warn(`Advertencia: ${fullPath} no es una lista`);
          }
        } catch (e) {
          console.error(`Error parseando ${fullPath}:`, e.message);
        }
      }
    } catch {
      // no pasa nada :)
    }
  }

  const outPath = path.join(baseDir, outputFileName);
  await fs.writeFile(outPath, JSON.stringify(allData, null, 2), "utf8");
  console.log(
    `Generado ${outputFileName} con ${allData.length} elementos en ${baseDir}`
  );
}

function runScript(command, args, cwd) {
  return new Promise((resolve, reject) => {
    console.log(`[RUN] ${command} ${args.join(" ")} (cwd=${cwd})`);
    const child = spawn(command, args, { cwd, stdio: "inherit" });

    child.on("exit", (code) => {
      if (code === 0) resolve();
      else reject(new Error(`${command} salió con código ${code}`));
    });

    child.on("error", (err) => reject(err));
  });
}

app.get("/api/main_memory_results", async (req, res) => {
  try {
    const p = path.join(__dirname, "main_memory", "main_memory_results.json");
    const content = await fs.readFile(p, "utf8");
    res.json(JSON.parse(content));
  } catch (e) {
    console.error(e);
    res.status(500).json({ error: "No se pudo leer main_memory_results.json" });
  }
});

app.get("/api/secondary_memory_results", async (req, res) => {
  try {
    const p = path.join(
      __dirname,
      "secondary_memory",
      "secondary_memory_results.json"
    );
    const content = await fs.readFile(p, "utf8");
    res.json(JSON.parse(content));
  } catch (e) {
    console.error(e);
    res
      .status(500)
      .json({ error: "No se pudo leer secondary_memory_results.json" });
  }
});

app.post("/api/run/main", async (req, res) => {
  try {
    const baseDir = path.join(__dirname, "main_memory");
    const dataset = (req.body && req.body.dataset) || "LA";

    await runScript("bash", ["run_all_benchmarks.sh", dataset], baseDir);

    await aggregateJson(baseDir, "main_memory_results.json");

    res.json({ ok: true, dataset });
  } catch (e) {
    console.error(e);
    res.status(500).json({ ok: false, error: e.message });
  }
});

app.post("/api/run/secondary", async (req, res) => {
  try {
    const baseDir = path.join(__dirname, "secondary_memory");
    const dataset = (req.body && req.body.dataset) || "LA";

    await runScript("bash", ["run_all_secondary.sh", dataset], baseDir);

    await aggregateJson(baseDir, "secondary_memory_results.json");

    res.json({ ok: true, dataset });
  } catch (e) {
    console.error(e);
    res.status(500).json({ ok: false, error: e.message });
  }
});

app.listen(PORT, () => {
  console.log(`API listening on http://localhost:${PORT}`);
});

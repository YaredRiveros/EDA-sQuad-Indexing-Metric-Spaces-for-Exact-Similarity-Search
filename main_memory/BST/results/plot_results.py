import json
import os
import matplotlib.pyplot as plt

# ======== Config ========
DATASETS = ["LA", "Words", "Color", "Synthetic"]
PLOT_DIR = "plots"

os.makedirs(PLOT_DIR, exist_ok=True)

# ======== Process each dataset ========
for dataset in DATASETS:
    filename = f"results_BST_{dataset}.json"
    
    if not os.path.exists(filename):
        print(f"[WARN] File not found, skipping: {filename}")
        continue

    print(f"[INFO] Processing {filename}")

    # Load JSON
    with open(filename, "r") as f:
        data = json.load(f)

    # Filter only MRQ entries
    mrq = [d for d in data if d["query_type"] == "MRQ"]

    if len(mrq) == 0:
        print(f"[WARN] No MRQ entries in {dataset}, skipping.")
        continue

    # Group by height
    by_h = {}
    for d in mrq:
        h = d["num_centers_path"]
        by_h.setdefault(h, []).append(d)

    heights = sorted(by_h.keys())

    # Compute averages
    run_times = []
    comp_dists = []

    for h in heights:
        vals = by_h[h]
        avg_t = sum(v["time_ms"] for v in vals) / len(vals)
        avg_d = sum(v["compdists"] for v in vals) / len(vals)

        run_times.append(avg_t)
        comp_dists.append(avg_d / 1e4)

    # ======== PLOT 1: Running time ========
    plt.figure(figsize=(6,4))
    plt.plot(heights, run_times, marker='s', linewidth=2)
    plt.xlabel("l (height)")
    plt.ylabel("Running Time (ms)")
    plt.title(f"{dataset} — BST MRQ Running Time")
    plt.grid(True)

    plt.savefig(f"{PLOT_DIR}/BST_{dataset}_MRQ_time.png", dpi=200)
    plt.close()

    # ======== PLOT 2: Compdists ========
    plt.figure(figsize=(6,4))
    plt.plot(heights, comp_dists, marker='s', linewidth=2)
    plt.xlabel("l (height)")
    plt.ylabel("compdists (10⁴)")
    plt.title(f"{dataset} — BST MRQ compdists")
    plt.grid(True)

    plt.savefig(f"{PLOT_DIR}/BST_{dataset}_MRQ_compdists.png", dpi=200)
    plt.close()

    print(f"[OK] Saved plots for {dataset}")

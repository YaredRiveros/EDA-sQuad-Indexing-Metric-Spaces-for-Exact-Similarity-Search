import os
import json
import random
from tqdm import tqdm
import numpy as np

# =============================================================================
#  RUTAS DE DATASETS
# =============================================================================

DATASET_PATHS = {
    "LA": "../LA.txt",
    "Words": "../Words.txt",
    "Color": "../Color.txt",
    "Synthetic": "../Synthetic.txt"
}

# =============================================================================
#  MÉTRICAS POR DATASET
# =============================================================================

def L2(a, b):
    return np.linalg.norm(a - b)

def L1(a, b):
    return np.sum(np.abs(a - b))

def L_inf(a, b):
    return np.max(np.abs(a - b))

def edit_distance(a, b):
    n, m = len(a), len(b)
    dp = [[0]*(m+1) for _ in range(n+1)]
    for i in range(n+1): dp[i][0] = i
    for j in range(m+1): dp[0][j] = j
    for i in range(1,n+1):
        for j in range(1,m+1):
            cost = 0 if a[i-1] == b[j-1] else 1
            dp[i][j] = min(dp[i-1][j] + 1,
                           dp[i][j-1] + 1,
                           dp[i-1][j-1] + cost)
    return dp[n][m]

METRIC_BY_DATASET = {
    "LA": L2,
    "Words": edit_distance,
    "Color": L1,
    "Synthetic": L_inf
}

# =============================================================================
#  LECTORES DE DATASETS SEGÚN FORMATO REAL
# =============================================================================

def load_LA_or_Synthetic(path):
    """ Datasets con METADATA en la primera línea """
    with open(path, "r") as f:
        header = f.readline().strip().split()
        dim = int(header[0])

        data = []
        for line in f:
            parts = line.strip().split()
            if len(parts) == dim:
                data.append([float(x) for x in parts])

    return np.array(data, dtype=float)

def load_Color(path):
    """ Dataset Color: MPEG-7 features numéricas SIN header """
    rows = []
    with open(path, "r") as f:
        for line in f:
            parts = line.strip().split()
            try:
                row = [float(x) for x in parts]
                rows.append(row)
            except:
                pass
    return np.array(rows, dtype=float)

def load_Words(path):
    """ Dataset Words: texto plano """
    words = []
    with open(path, "r", encoding="latin1", errors="ignore") as f:
        for line in f:
            w = line.strip()
            if w:
                words.append(w)
    return np.array(words, dtype=object)


def load_dataset(path, name):
    if name == "LA":
        return load_LA_or_Synthetic(path)
    if name == "Synthetic":
        return load_LA_or_Synthetic(path)
    if name == "Color":
        return load_Color(path)
    if name == "Words":
        return load_Words(path)
    raise ValueError("Dataset desconocido")


# =============================================================================
#  HFI REAL: selección de pivotes según paper
# =============================================================================

def select_pivots_HFI(data, num_pivots, metric):
    N = len(data)
    pivots = []

    # Primer pivote fijo (usado en el paper)
    pivots.append(0)

    # score[i] = suma de distancias acumuladas a los pivotes anteriores
    score = np.zeros(N)

    for _ in range(1, num_pivots):
        last = pivots[-1]

        # calcular distancias desde el pivote recién añadido
        d = np.array([metric(data[last], data[i]) for i in range(N)])
        score += d

        # siguiente pivote = índice con score máximo
        next_pivot = int(np.argmax(score))

        # asegurar que no se repita
        if next_pivot in pivots:
            # fallback: elegir el más lejano del último pivote
            next_pivot = int(np.argmax(d))

        pivots.append(next_pivot)

    return [int(x) for x in pivots]


# =============================================================================
#  SELECCIÓN DE QUERIES
# =============================================================================
def select_queries(N, count=100):
    return random.sample(range(N), count)


# =============================================================================
#  RADIOS PARA SELECTIVIDAD
# =============================================================================
def compute_radii(data, queries, metric, selectivities):
    radii = {}
    N = len(data)

    for s in selectivities:
        rad_list = []

        for q in tqdm(queries, desc=f"Selectivity {s*100:.0f}%"):
            d = np.array([metric(data[q], data[i]) for i in range(N)])
            rad = np.percentile(d, s*100)
            rad_list.append(rad)

        radii[str(s)] = float(np.mean(rad_list))

    return radii


# =============================================================================
#  PROCESAR DATASET
# =============================================================================
def process_dataset(name, path, outdir="prepared_experiment"):
    print(f"\n=== Procesando {name} ===")

    metric = METRIC_BY_DATASET[name]
    data = load_dataset(path, name)
    N = len(data)

    os.makedirs(f"{outdir}/pivots", exist_ok=True)
    os.makedirs(f"{outdir}/queries", exist_ok=True)
    os.makedirs(f"{outdir}/radii", exist_ok=True)

    # QUERIES
    queries = select_queries(N, 100)
    with open(f"{outdir}/queries/{name}_queries.json", "w") as f:
        json.dump(queries, f, indent=2)

    # RADIOS
    selectivities = [0.02, 0.04, 0.08, 0.16, 0.32]
    radii = compute_radii(data, queries, metric, selectivities)
    with open(f"{outdir}/radii/{name}_radii.json", "w") as f:
        json.dump(radii, f, indent=2)

    # PIVOTES
    for p in [3, 5, 10, 15, 20]:
        pivots = select_pivots_HFI(data, p, metric)
        with open(f"{outdir}/pivots/{name}_pivots_{p}.json", "w") as f:
            json.dump(pivots, f, indent=2)

    print(f"✔ Dataset {name} procesado.")


# =============================================================================
#  EJECUCIÓN
# =============================================================================
if __name__ == "__main__":
    for name, path in DATASET_PATHS.items():
        if os.path.exists(path):
            process_dataset(name, path)
        else:
            print(f"⚠ Dataset {name} no encontrado → skipping ({path})")

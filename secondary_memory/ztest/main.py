# Improved significance script (replace your previous file with this)
import json, math, os
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np
import itertools

# load data (ajusta la ruta si hace falta)
DATA_PATH = "../secondary_memory_results.json"
with open(DATA_PATH, "r", encoding="utf-8") as f:
    records = json.load(f)

def norm(s):
    if s is None: return ""
    return "".join(c.lower() for c in str(s) if c.isalnum())

category_map = {
    'dsaclt': 'compact',
    'lc': 'compact',
    'mb': 'compact',
    'mbplus': 'compact',
    'mtree': 'compact',
    'omnir': 'pivot','omnirtree':'pivot','omnir-tree':'pivot',
    'spb': 'pivot','spbtree':'pivot','spb-tree':'pivot',
    'dindex':'hybrid','d-index':'hybrid',
    'egnat':'hybrid',
    'pmtree':'hybrid','pm-tree':'hybrid','pmTree':'hybrid',
    'cpt':'hybrid',
    'mindex':'hybrid','m-index':'hybrid','mindex*':'hybrid','m-index*':'hybrid'
}

def match_mrq(rec):
    """
    Filtra registros MRQ con selectivity = 0.08 y l = 5
    
    CORREGIDO: Usa 'selectivity' en vez de 'radius' exacto
    porque el radius varía según el dataset, pero selectivity es consistente.
    """
    if str(rec.get('query_type','')).lower() != 'mrq':
        return False
    
    # Usar SELECTIVITY en vez de radius (más robusto)
    sel = rec.get('selectivity', None)
    if sel is not None:
        try:
            sel_val = float(sel)
            # Aceptar selectivity = 0.08 con pequeña tolerancia
            if abs(sel_val - 0.08) > 1e-6:
                return False
        except:
            return False
    else:
        # Si no hay selectivity, intentar con radius (fallback)
        r = rec.get('r', rec.get('radius', rec.get('R', None)))
        if r is not None:
            try:
                rv = float(r)
                # Aceptar radius cercano a 8.23 (valor típico para sel=0.08 en Words)
                if not (abs(rv - 0.08) < 1e-3 or abs(rv - 8.0) < 0.5 or abs(rv - 8.23) < 0.1):
                    return False
            except:
                return False
    
    # Verificar l = 5 (si existe)
    l = rec.get('l', rec.get('L', None))
    if l is not None:
        try:
            lv = int(l)
            if lv != 5:
                return False
        except:
            return False
    
    return True

def match_mknn(rec):
    if str(rec.get('query_type','')).lower() != 'mknn':
        return False
    k = rec.get('k', None)
    if k is None:
        return True
    try:
        kv = int(k)
    except:
        return False
    if kv != 20:
        return False
    l = rec.get('l', rec.get('L', None))
    if l is None:
        return True
    try:
        lv = int(l)
    except:
        return False
    return lv == 5

dataset_names = sorted({r.get('dataset') for r in records if r.get('dataset') is not None})
dataset_names = [d for d in dataset_names if d is not None]
use_dataset = "Words" if any(d and d.lower()=="words" for d in dataset_names) else None
if use_dataset is None:
    records_filtered = records
    print("Warning: 'Words' dataset not found. Using all datasets.")
else:
    records_filtered = [r for r in records if r.get('dataset') and r.get('dataset').lower()=='words']

mrq_records = [r for r in records_filtered if match_mrq(r)]
mknn_records = [r for r in records_filtered if match_mknn(r)]

metrics = ['pages','compdists','time_ms']

# NUEVO PARADIGMA: En vez de p-values, usar distancia relativa entre medias
# similarity_threshold: umbral de similitud (0.0 = idénticos, 1.0 = 100% diferencia)
# Valores típicos: 0.1 (10%), 0.2 (20%), 0.3 (30%)
similarity_threshold = 0.1  # Conectar si diferencia < 15%

def stats_by_index(records_list, metric):
    d = defaultdict(lambda: {'vals':[]})
    for r in records_list:
        idx = r.get('index')
        if idx is None: continue
        val = r.get(metric, None)
        if val is None:
            if metric == 'time_ms':
                val = r.get('time', r.get('time_ms', None))
            elif metric == 'compdists':
                val = r.get('compdists', None)
            elif metric == 'pages':
                val = r.get('pages', None)
        try:
            if val is None: continue
            v = float(val)
        except:
            continue
        d[idx]['vals'].append(v)
    out = {}
    for idx, info in d.items():
        vals = np.array(info['vals'], dtype=float)
        n = len(vals)
        mean = float(vals.mean()) if n>0 else None
        std = float(vals.std(ddof=1)) if n>1 else (float(vals.std()) if n==1 else None)
        out[idx] = {'n': n, 'mean': mean, 'std': std, 'vals': vals.tolist()}
    return out

mrq_stats = {m: stats_by_index(mrq_records, m) for m in metrics}
mknn_stats = {m: stats_by_index(mknn_records, m) for m in metrics}

# -----------------------
# New robust p-value computation
# -----------------------
from math import erf, sqrt
def normal_cdf(x):
    # standard normal CDF
    return 0.5 * (1 + math.erf(x / math.sqrt(2)))

def compute_similarity_metric(vals1, vals2):
    """
    NUEVO ENFOQUE: Coefficient of Variation Distance (CVD)
    
    Calcula la distancia relativa entre las medias de dos muestras.
    CVD = |mean1 - mean2| / max(mean1, mean2)
    
    Ventajas:
    - Funciona con n=1 (casos comunes en benchmarks)
    - No requiere distribución normal
    - Intuitivo: 0.0 = idénticos, 1.0 = 100% de diferencia
    - Independiente de escala
    
    Returns:
        (mean1, mean2, cvd) donde cvd es la distancia relativa
    """
    n1, n2 = len(vals1), len(vals2)
    if n1 == 0 or n2 == 0:
        return None, None, None

    mean1 = float(np.mean(vals1))
    mean2 = float(np.mean(vals2))
    
    # Evitar división por cero
    max_mean = max(abs(mean1), abs(mean2))
    if max_mean == 0 or np.isclose(max_mean, 0):
        # Ambos son cero -> idénticos
        return mean1, mean2, 0.0
    
    # CVD: diferencia relativa
    cvd = abs(mean1 - mean2) / max_mean
    
    return float(mean1), float(mean2), float(cvd)

# -----------------------
# Pairwise comparisons using similarity metric
# -----------------------
def pairwise_comparisons(stats, threshold):
    """
    Compara todos los pares de índices usando CVD (Coefficient of Variation Distance).
    
    Conecta dos índices si CVD < threshold (son similares).
    """
    idxs = sorted(stats.keys())
    similarities = {}
    adjacency = {i: set() for i in idxs}
    
    for a, b in itertools.combinations(idxs, 2):
        s1 = stats[a]
        s2 = stats[b]
        
        if s1['n'] < 1 or s2['n'] < 1:
            mean1, mean2, cvd = None, None, None
        else:
            vals1 = np.array(s1.get('vals', []), dtype=float)
            vals2 = np.array(s2.get('vals', []), dtype=float)
            mean1, mean2, cvd = compute_similarity_metric(vals1, vals2)
        
        similarities[(a,b)] = {
            'mean1': mean1,
            'mean2': mean2,
            'cvd': cvd,
            'n1': s1['n'],
            'n2': s2['n']
        }
        
        # Conectar cuando SON SIMILARES (CVD < threshold)
        # CVD bajo = poca diferencia relativa = similares
        if cvd is not None and cvd < threshold:
            adjacency[a].add(b)
            adjacency[b].add(a)
    
    return idxs, similarities, adjacency

# -----------------------
# Main: build summary, plot
# -----------------------
summary = {}
all_nodes = set()
for metric in metrics:
    summary[metric] = {}
    stats_mrq = mrq_stats[metric]
    stats_mknn = mknn_stats[metric]
    
    # Usar el nuevo threshold de similitud
    idxs_mrq, sims_mrq, adj_mrq = pairwise_comparisons(stats_mrq, similarity_threshold)
    idxs_mknn, sims_mknn, adj_mknn = pairwise_comparisons(stats_mknn, similarity_threshold)
    
    # Convert tuple keys to strings for JSON serialization
    sims_mrq_serial = { f"{a}|||{b}": v for (a,b), v in sims_mrq.items() }
    sims_mknn_serial = { f"{a}|||{b}": v for (a,b), v in sims_mknn.items() }
    
    summary[metric]['MRQ'] = {
        'stats': stats_mrq,
        'similarities': sims_mrq_serial,
        'adjacency': {k: list(v) for k,v in adj_mrq.items()}
    }
    summary[metric]['MkNN'] = {
        'stats': stats_mknn,
        'similarities': sims_mknn_serial,
        'adjacency': {k: list(v) for k,v in adj_mknn.items()}
    }
    
    all_nodes.update(idxs_mrq)
    all_nodes.update(idxs_mknn)

all_nodes = sorted(list(all_nodes))

# Layout (try networkx if available, else circle)
try:
    import networkx as nx
    G_union = nx.Graph()
    G_union.add_nodes_from(all_nodes)
    # add connectivity edges to help layout
    for metric in metrics:
        for q in ['MRQ','MkNN']:
            for a, neighs in summary[metric][q]['adjacency'].items():
                for b in neighs:
                    G_union.add_edge(a,b)
    if G_union.number_of_edges() == 0 and len(all_nodes)>1:
        for i in range(len(all_nodes)-1):
            G_union.add_edge(all_nodes[i], all_nodes[i+1])
    pos = nx.spring_layout(G_union, seed=42)
except Exception as e:
    pos = {}
    n = len(all_nodes)
    for i, node in enumerate(all_nodes):
        angle = 2*math.pi * i / max(n,1)
        pos[node] = (math.cos(angle), math.sin(angle))

def draw_panel(ax, adjacency, stats, title):
    ax.set_title(title, fontsize=8, wrap=True)
    ax.set_xticks([]); ax.set_yticks([])
    drawn = set()
    for a, neighs in adjacency.items():
        for b in neighs:
            if (b,a) in drawn or (a,b) in drawn: continue
            drawn.add((a,b))
            cat_a = category_map.get(norm(a), 'other')
            cat_b = category_map.get(norm(b), 'other')
            linestyle = '-' if cat_a==cat_b else '--'
            xa, ya = pos[a]; xb, yb = pos[b]
            ax.plot([xa, xb], [ya, yb], linestyle, linewidth=0.7)
    categories = {'compact': 's', 'pivot':'o', 'hybrid':'^', 'other':'d'}
    for cat, marker in categories.items():
        group = [n for n in pos.keys() if category_map.get(norm(n), 'other')==cat]
        if not group: continue
        xs = [pos[n][0] for n in group]
        ys = [pos[n][1] for n in group]
        sizes = []
        for n in group:
            s = stats.get(n, {'n':0})['n']
            sizes.append(50 + 10*min(s,10))
        ax.scatter(xs, ys, marker=marker, s=sizes)
        for x,y,name in zip(xs, ys, group):
            ax.text(x, y, name, fontsize=6, ha='center', va='center')

fig, axes = plt.subplots(2, 3, figsize=(15, 8))
for col, metric in enumerate(metrics):
    adj_mrq = summary[metric]['MRQ']['adjacency']
    stats_mrq = summary[metric]['MRQ']['stats']
    draw_panel(axes[0, col], adj_mrq, stats_mrq, 
               f"{metric} of MRQ\n(CVD<{similarity_threshold:.0%} => connected)")
    
    adj_mknn = summary[metric]['MkNN']['adjacency']
    stats_mknn = summary[metric]['MkNN']['stats']
    draw_panel(axes[1, col], adj_mknn, stats_mknn, 
               f"{metric} of MkNN\n(CVD<{similarity_threshold:.0%} => connected)")

plt.tight_layout()
out_fig = "chen_similarity_graphs_CVD.png"
plt.savefig(out_fig, dpi=300, bbox_inches='tight')
plt.show()

out_summary = {
    'method': 'Coefficient of Variation Distance (CVD)',
    'similarity_threshold': similarity_threshold,
    'description': 'CVD = |mean1-mean2| / max(mean1,mean2). Lower CVD = more similar. Connected if CVD < threshold.',
    'results': summary
}
with open("chen_similarity_summary_CVD.json", "w", encoding="utf-8") as f:
    json.dump(out_summary, f, indent=2)

print(f"✓ Saved figure to: {out_fig}")
print(f"✓ Saved summary to: chen_similarity_summary_CVD.json")
print(f"✓ Using CVD similarity with threshold = {similarity_threshold:.0%}")

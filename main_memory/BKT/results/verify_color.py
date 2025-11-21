import json

with open('results_BKT_Color.json') as f:
    data = json.load(f)

print(f'Total experimentos: {len(data)}')

configs = {}
for d in data:
    key = d['num_pivots']
    if key not in configs:
        configs[key] = {'MRQ': 0, 'MkNN': 0}
    configs[key][d['query_type']] += 1

print('\nExperimentos por configuración:')
for pivots, counts in sorted(configs.items()):
    total = sum(counts.values())
    print(f'  Pivots={pivots}: {counts["MRQ"]} MRQ + {counts["MkNN"]} MkNN = {total} total')

print(f'\n[OK] Estructura válida: {len(data)} experimentos')
print(f'[OK] Formato JSON correcto')

# Verificar que tenga 5 configs × 10 experimentos = 50 total
expected = 5 * 10
if len(data) == expected:
    print(f'[OK] Cantidad esperada: {expected} experimentos')
else:
    print(f'[WARN] Esperados {expected}, encontrados {len(data)}')

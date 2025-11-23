import json

# Cargar JSON
with open("secondary_memory_results.json", "r") as f:
    data = json.load(f)

# Obtener todos los valores de "index"
index_names = [d["index"] for d in data if "index" in d]

# Quitar duplicados si quieres
unique_index_names = list(set(index_names))

print(unique_index_names)

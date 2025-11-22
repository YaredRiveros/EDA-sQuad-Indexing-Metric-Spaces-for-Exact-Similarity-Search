import os
import json

# Carpeta base donde están todas las carpetas (BKT, BST, ...)
base_dir = "."

# Lista para almacenar todos los diccionarios de todos los JSON
all_data = []

# Recorremos cada carpeta dentro de base_dir
for folder in os.listdir(base_dir):
    folder_path = os.path.join(base_dir, folder)
    if os.path.isdir(folder_path):
        results_path = os.path.join(folder_path, "results")
        if os.path.exists(results_path) and os.path.isdir(results_path):
            # Recorremos todos los archivos .json dentro de results
            for file_name in os.listdir(results_path):
                if file_name.endswith(".json"):
                    file_path = os.path.join(results_path, file_name)
                    with open(file_path, "r") as f:
                        try:
                            data = json.load(f)
                            if isinstance(data, list):
                                all_data.extend(data)  # agregamos los diccionarios a la lista grande
                            else:
                                print(f"Advertencia: {file_path} no es una lista")
                        except json.JSONDecodeError:
                            print(f"Error leyendo JSON: {file_path}")

# Guardamos todo en un solo archivo JSON
output_file = os.path.join(base_dir, "main_memory_results.json")
with open(output_file, "w") as f:
    json.dump(all_data, f, indent=4)

print(f"Se han combinado todos los JSON en uno solo con {len(all_data)} elementos")

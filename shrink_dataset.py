#!/usr/bin/env python3
import argparse
import random

def cut_head(input_path, output_path, n):
    """Toma las primeras n líneas del archivo."""
    with open(input_path, "r", encoding="utf-8") as fin, \
         open(output_path, "w", encoding="utf-8") as fout:
        for i, line in enumerate(fin):
            if i >= n:
                break
            fout.write(line)
    print(f"[OK] Escribidas {n} líneas (head) en {output_path}")


def cut_random(input_path, output_path, n, seed=12345):
    """Toma n líneas aleatorias usando reservoir sampling (no carga todo en RAM)."""
    random.seed(seed)
    reservoir = []
    total = 0

    with open(input_path, "r", encoding="utf-8") as fin:
        for line in fin:
            total += 1
            if len(reservoir) < n:
                reservoir.append(line)
            else:
                j = random.randint(0, total - 1)
                if j < n:
                    reservoir[j] = line

    with open(output_path, "w", encoding="utf-8") as fout:
        for line in reservoir:
            fout.write(line)

    print(f"[OK] Escribidas {len(reservoir)} líneas (random) en {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Reducir un dataset grande a N líneas (head o random)."
    )
    parser.add_argument("input", help="Ruta del dataset de entrada (ej. LA.txt)")
    parser.add_argument("output", help="Ruta del archivo reducido (ej. LA_2k.txt)")
    parser.add_argument(
        "-n", "--num-lines", type=int, default=2000,
        help="Número de líneas a conservar (por defecto 2000)"
    )
    parser.add_argument(
        "--mode", choices=["head", "random"], default="head",
        help="Modo de selección: head (primeras N) o random (muestreo aleatorio)"
    )
    parser.add_argument(
        "--seed", type=int, default=12345,
        help="Semilla para el modo random (para reproducibilidad)"
    )
    args = parser.parse_args()

    if args.mode == "head":
        cut_head(args.input, args.output, args.num_lines)
    else:
        cut_random(args.input, args.output, args.num_lines, seed=args.seed)


if __name__ == "__main__":
    main()

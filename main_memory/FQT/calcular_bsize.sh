#!/usr/bin/env bash
# calcular_bsize.sh - Calcula bsize (bsize = n / (arity^l))
# Usage: calcular_bsize.sh <n_objetos_or_dataset_file> [l_pivotes] [arity] [record_size]
# Si el primer parámetro es un fichero existente se estima n = filesize / record_size

set -euo pipefail

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

if [ $# -eq 0 ]; then
    echo -e "${YELLOW}Uso:${NC} $0 <n_objetos | dataset_file> [l_pivotes] [arity] [record_size]"
    exit 0
fi

INPUT=$1
L=${2:-5}
ARITY=${3:-5}
RECORD_SIZE=${4:-32}

# obtener n
if [ -f "$INPUT" ]; then
    BYTES=$(wc -c < "$INPUT" | tr -d ' ')
    if [ -z "$BYTES" ] || [ "$BYTES" -eq 0 ]; then
        echo "ERROR: no se pudo obtener tamaño del fichero" >&2
        exit 1
    fi
    N=$(( BYTES / RECORD_SIZE ))
    if [ $N -eq 0 ]; then N=1000; fi
else
    # asumir que el input es un número (n objetos)
    if [[ "$INPUT" =~ ^[0-9]+$$ ]]; then
        N=$INPUT
    else
        echo "ERROR: '$INPUT' no es fichero ni número" >&2
        exit 1
    fi
fi

# función para pow: preferir bc, fallback python3, fallback pow integer con awk (si disponible)
pow() {
    local base=$1; local exp=$2
    if command -v bc >/dev/null 2>&1; then
        echo "$(echo "$base ^ $exp" | bc)"
    elif command -v python3 >/dev/null 2>&1; then
        python3 - <<PY
print(int(($base) ** ($exp)))
PY
    else
        # fallback aproximado con awk (enteros pequeños)
        awk "BEGIN{print int(($base)^($exp))}"
    fi
}

DENOM=$(pow "$ARITY" "$L")
if [ -z "$DENOM" ] || [ "$DENOM" -le 0 ]; then DENOM=1; fi

BSIZE=$(( N / DENOM ))
if [ $BSIZE -lt 1 ]; then BSIZE=1; fi

echo -e "${BLUE}Configuración:${NC}"
echo -e "  n (objetos): $N"
echo -e "  arity: $ARITY"
echo -e "  l (pivotes): $L"
echo -e ""
echo -e "${GREEN}Resultado:${NC}  bsize = $N / ($ARITY ^ $L) = $BSIZE"
echo ""
echo "Comando sugerido:"
echo "  ./fqt dataset.bin $BSIZE $ARITY"

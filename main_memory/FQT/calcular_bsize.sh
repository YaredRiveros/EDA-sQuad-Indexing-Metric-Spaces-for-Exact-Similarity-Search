#!/bin/bash
# calcular_bsize.sh - Calcula bsize para obtener l pivotes en FQT
# 
# FQT es un índice pivot-based donde:
#   l (pivotes) = altura del árbol
#   1 pivote por nivel
#
# Para obtener exactamente l pivotes:
#   bsize = n / (arity^l)

# Colores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Calculadora bsize para FQT (Pivot-Based)    ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════╝${NC}\n"

# Verificar argumentos
if [ $# -eq 0 ]; then
    echo -e "${YELLOW}Uso:${NC} $0 <n_objetos> [l_pivotes] [arity]"
    echo ""
    echo -e "${YELLOW}Ejemplos:${NC}"
    echo "  $0 100000              # Calcula para l={3,5,10,15,20}, arity=5"
    echo "  $0 100000 5            # Calcula para l=5, arity=5"
    echo "  $0 100000 5 5          # Calcula para l=5, arity=5"
    echo ""
    echo -e "${YELLOW}Parámetros:${NC}"
    echo "  n_objetos  : Número de objetos en el dataset"
    echo "  l_pivotes  : Número de pivotes deseado (altura del árbol)"
    echo "  arity      : Factor de ramificación (default: 5, del paper)"
    exit 0
fi

n=$1
arity=${3:-5}  # Default: 5 (del paper)

# Función para calcular bsize
calcular_bsize() {
    local n=$1
    local l=$2
    local arity=$3
    
    local bsize=$(echo "$n / ($arity ^ $l)" | bc)
    
    # bsize mínimo = 1
    if [ $bsize -lt 1 ]; then
        bsize=1
    fi
    
    echo $bsize
}

# Función para verificar si l es alcanzable
verificar_l() {
    local n=$1
    local l=$2
    local arity=$3
    
    local altura_max=$(echo "l($n)/l($arity)" | bc -l | awk '{printf "%.0f", $0}')
    
    if [ $l -gt $altura_max ]; then
        echo -e "${YELLOW}⚠ Advertencia: l=$l es mayor que altura máxima ≈$altura_max${NC}"
        echo -e "  Considera: reducir arity o aumentar n"
    fi
}

echo -e "${GREEN}Configuración:${NC}"
echo -e "  n (objetos): $n"
echo -e "  arity: $arity"
echo ""

if [ $# -eq 1 ]; then
    # Calcular para múltiples valores de l
    echo -e "${BLUE}═══ Tabla de Configuraciones ═══${NC}\n"
    echo "┌─────────┬──────────┬──────────────────────────────┐"
    echo "│ l       │  bsize   │  Comando FQT                 │"
    echo "├─────────┼──────────┼──────────────────────────────┤"
    
    for l in 3 5 10 15 20; do
        bsize=$(calcular_bsize $n $l $arity)
        printf "│ %7d │ %8d │  ./fqt dataset.bin %d %d     │\n" $l $bsize $bsize $arity
        verificar_l $n $l $arity
    done
    
    echo "└─────────┴──────────┴──────────────────────────────┘"
    
    # Calcular altura máxima alcanzable
    altura_max=$(echo "l($n)/l($arity)" | bc -l | awk '{printf "%.0f", $0}')
    echo -e "\n${YELLOW}Altura máxima alcanzable:${NC} log_${arity}($n) ≈ $altura_max"
    
else
    # Calcular para un valor específico de l
    l=${2:-5}
    
    echo -e "${BLUE}═══ Cálculo para l=$l pivotes ═══${NC}\n"
    
    bsize=$(calcular_bsize $n $l $arity)
    
    echo -e "${GREEN}Resultado:${NC}"
    echo -e "  bsize = $n / ($arity^$l) = ${BLUE}$bsize${NC}"
    echo ""
    echo -e "${GREEN}Comando:${NC}"
    echo -e "  ${BLUE}./fqt dataset.bin $bsize $arity${NC}"
    echo ""
    
    # Verificar si es alcanzable
    verificar_l $n $l $arity
    
    # Información adicional
    echo ""
    echo -e "${YELLOW}Información:${NC}"
    echo -e "  • FQT usará ${BLUE}$l pivotes${NC} (1 por nivel)"
    echo -e "  • Altura del árbol: ${BLUE}≈$l${NC}"
    echo -e "  • Tamaño de bucket: ${BLUE}$bsize${NC} objetos"
    
    if [ $bsize -eq 1 ]; then
        echo -e "  ${YELLOW}⚠ bsize=1 (mínimo)${NC}: cada hoja contiene 1 objeto"
    fi
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════${NC}"
echo -e "${GREEN}FQT es un índice pivot-based:${NC}"
echo -e "  • l pivotes = altura del árbol"
echo -e "  • 1 pivote por nivel (mismo en todo el nivel)"
echo -e "  • Control directo del número de pivotes"
echo -e "${GREEN}═══════════════════════════════════════════════${NC}"

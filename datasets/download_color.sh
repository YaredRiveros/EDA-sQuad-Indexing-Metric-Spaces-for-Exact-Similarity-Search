#!/bin/bash
# Script para descargar el dataset Color (CoPhIR MPEG-7 100k subset)

DATA_DIR=Color_raw
mkdir -p $DATA_DIR
cd $DATA_DIR

echo "Descargando dataset Color (CoPhIR 100k)..."

wget https://www.fi.muni.cz/~xslanin/lmi/knn_gt.json
wget https://www.fi.muni.cz/~xslanin/lmi/level-1.txt
wget https://www.fi.muni.cz/~xslanin/lmi/level-2.txt
wget https://www.fi.muni.cz/~xslanin/lmi/objects.txt
wget https://www.fi.muni.cz/~xslanin/lmi/info.txt

echo ""
echo "Archivos descargados en $DATA_DIR/"
echo "Ejecuta convert_color.py para convertir a formato estándar"

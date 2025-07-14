#!/bin/bash

HOST="localhost"
PORT="8080"
NUM_CLIENTS=100

FILES=(
    "./web_files/index.html"
    "./web_files/another.html"
    "./web_files/spin.cgi?10"
    "./web_files/spin.cgi?6"
    "./web_files/non_existent_page.html"
)
NUM_FILES=${#FILES[@]}

OUTPUT_DIR="./client_outputs"
mkdir -p "$OUTPUT_DIR"

echo "Lanzando $NUM_CLIENTS clientes contra $HOST:$PORT..."
echo "Las salidas de los clientes se guardarán en $OUTPUT_DIR"

# Obtener tiempo de inicio
start_time=$(date +%s)

for i in $(seq 1 $NUM_CLIENTS)
do
    FILE_TO_REQUEST=${FILES[$((RANDOM % NUM_FILES))]}
    echo "Cliente $i: Solicitando $FILE_TO_REQUEST"
    ./wclient "$HOST" "$PORT" "$FILE_TO_REQUEST" > "$OUTPUT_DIR/client_${i}_output.txt" 2>&1 &
done

echo "Todos los clientes han sido lanzados en segundo plano."
echo "Esperando a que todos los clientes terminen..."

wait

# Obtener tiempo de finalización
end_time=$(date +%s)

# Calcular duración
duration=$((end_time - start_time))

echo "Todos los clientes han terminado."
echo "-----------------------------------------------------"
echo "TIEMPO TOTAL DE EJECUCIÓN DEL SCRIPT: $duration segundos"
echo "-----------------------------------------------------"
#!/bin/bash
# run.sh — прогон конфигураций из configs.txt (сгенерировать: python3 gen_configs.py)
BIN=./a.out
CFG=configs2.txt
OUT=results_configs2.txt

if [ ! -f "$CFG" ]; then
    echo "Не найден $CFG — сначала сгенерируй: python3 gen_configs.py"
    exit 1
fi

: > "$OUT"

total=$(wc -l < "$CFG")
n=0

while IFS= read -r jobs; do
    [ -z "$jobs" ] && continue
    n=$((n+1))
    echo "### [$n/$total] $jobs" | tee -a "$OUT"
    $BIN --jobs "$jobs" 2>/dev/null | grep -E '^(CONFIG|CORES|RESULT)' | tee -a "$OUT"
    echo "" >> "$OUT"
done < "$CFG"

echo "Готово. Обработано конфигураций: $n. Результаты в $OUT"

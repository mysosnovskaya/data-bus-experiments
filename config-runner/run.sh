#!/bin/bash
# run-mix.sh — генерация и запуск конфигураций для эксперимента конкуренции
BIN=./a.out
OUT=results_mix.txt
: > "$OUT"

run () {   # $1 = строка --jobs
    echo "### $1" | tee -a "$OUT"
    $BIN --jobs "$1" 2>/dev/null | grep -E '^(CONFIG|RESULT)' | tee -a "$OUT"
    echo "" >> "$OUT"
}

# Пул работ: имя:размер (ядра подставим при запуске)
SOLO=(
  "COPY:60" "COPY:84" "XPY:48" "XPY:60"
  "SUM:84" "SUM:108" "SUM:132" "DOT:36" "DOT:84"
  "ERF:12" "ERF:24" "ERF:48" "POW:12" "POW:24" "POW:36"
  "SQRTX:12" "SQRTX:48" "TGAMMA:12" "TGAMMA:60" "TGAMMA:120"
)

# --- 1. БЕЙЗЛАЙНЫ: каждая работа отдельно на 1..4 ядрах ---
for w in "${SOLO[@]}"; do
  for c in 1 2 3 4; do
    run "${w}:${c}"
  done
done

# --- 2. СМЕСИ ПО ДВЕ, сумма ядер = 4 ---
# разбиения 4 на две работы: (1,3) (2,2) (3,1)
PAIR_A=("COPY:60" "XPY:48" "SUM:84" "DOT:36" "TGAMMA:60" "POW:24" "ERF:24" "SQRTX:48")
PAIR_B=("COPY:84" "XPY:60" "SUM:132" "DOT:84" "TGAMMA:120" "POW:36" "ERF:48" "SQRTX:12")
for a in "${PAIR_A[@]}"; do
  for b in "${PAIR_B[@]}"; do
    run "${a}:2,${b}:2"     # 2+2
    run "${a}:1,${b}:3"     # 1+3
    run "${a}:3,${b}:1"     # 3+1
  done
done

# --- 3. СМЕСИ ПО ТРИ, сумма = 4 (разбиение 2+1+1) ---
TRIO=("COPY:60" "XPY:48" "SUM:108" "TGAMMA:60" "POW:24" "ERF:24")
for a in "${TRIO[@]}"; do
  for b in "${TRIO[@]}"; do
    [ "$a" = "$b" ] && continue
    run "${a}:2,${b}:1,TGAMMA:120:1"   # тихий третий как зонд
  done
done

# --- 4. СМЕСИ ПО ЧЕТЫРЕ, по 1 ядру (максимальная драка за шину) ---
QUAD=("COPY:60" "XPY:48" "SUM:84" "DOT:36" "ERF:24" "POW:24" "SQRTX:48" "TGAMMA:60")
for a in "${QUAD[@]}"; do
  run "${a}:1,COPY:60:1,XPY:48:1,TGAMMA:60:1"
done

echo "Готово. Результаты в $OUT"
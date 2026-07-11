#!/usr/bin/env python3
"""
gen_configs.py — генерирует configs.txt: список конфигураций для run.sh.

Соло-бейзлайны НЕ генерируются: в качестве референса используется
существующая "Общая сводка" (собрана отдельно, до этого эксперимента).
Здесь только случайные СОВМЕСТНЫЕ запуски — 2, 3 или 4 работы
одновременно, суммарно 2, 3 или 4 ядра.

Каждая строка файла — одна конфигурация в формате, который понимает
Runner.cpp (--jobs "TYPE:SIZE:CORES,TYPE:SIZE:CORES,..."):

    COPY:60:2,XPY:48:2
    ERF:24:1,POW:24:2,TGAMMA:60:1

Запуск:  python3 gen_configs.py
Результат: файл configs.txt в текущей директории.
"""

import random

random.seed(43)  # фиксируем сид — список конфигураций воспроизводим

# ---------------------------------------------------------------------------
# Курируемый пул работ: 20 (тип, размер), подобранных так, чтобы охватить
# весь диапазон потребления шины (TGAMMA ~4% ... XPY ~49%) и весь диапазон
# масштабируемости (TGAMMA ~4x ... COPY ~1x).
# ---------------------------------------------------------------------------
POOL = [
    ("COPY", 60), ("COPY", 84),
    ("XPY", 48), ("XPY", 60),
    ("SUM", 84), ("SUM", 108), ("SUM", 132),
    ("DOT", 36), ("DOT", 84),
    ("ERF", 12), ("ERF", 24), ("ERF", 48),
    ("POW", 12), ("POW", 24), ("POW", 36),
    ("SQRTX", 12), ("SQRTX", 48),
    ("TGAMMA", 12), ("TGAMMA", 60), ("TGAMMA", 120),
]

N_MIX_TARGET = 500  # сколько конфигураций хотим получить

lines = []
seen = set()
attempts = 0
max_attempts = N_MIX_TARGET * 20

while len(lines) < N_MIX_TARGET and attempts < max_attempts:
    attempts += 1

    total_cores = random.choice([2, 3, 4])
    if total_cores == 2:
        n_works = 2                                   # 1+1
    elif total_cores == 3:
        n_works = random.choice([2, 3])                # 2+1 или 1+1+1
    else:  # total_cores == 4
        n_works = random.choice([2, 2, 3, 4])           # 2+2 чаще всего, плюс 3+1/2+1+1/1+1+1+1

    # случайное разбиение total_cores на n_works положительных частей
    parts = [1] * n_works
    remaining = total_cores - n_works
    for _ in range(remaining):
        parts[random.randrange(n_works)] += 1

    chosen = [random.choice(POOL) for _ in range(n_works)]
    job_str = ",".join(f"{t}:{s}:{c}" for (t, s), c in zip(chosen, parts))

    # лёгкая дедупликация: не повторяем абсолютно идентичный набор работ+ядра
    key = tuple(sorted(job_str.split(",")))
    if key in seen:
        continue
    seen.add(key)
    lines.append(job_str)

with open("configs2.txt", "w") as f:
    f.write("\n".join(lines) + "\n")

# --- сводка по составу для проверки разнообразия ---
from collections import Counter
tc = Counter()
nw = Counter()
for l in lines:
    parts_ = l.split(',')
    total = sum(int(p.split(':')[2]) for p in parts_)
    tc[total] += 1
    nw[len(parts_)] += 1

print(f"Готово: configs.txt")
print(f"  всего конфигураций:       {len(lines)}")
print(f"  по суммарному числу ядер: {dict(sorted(tc.items()))}")
print(f"  по числу работ:           {dict(sorted(nw.items()))}")

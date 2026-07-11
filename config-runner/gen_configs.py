#!/usr/bin/env python3
"""
gen_configs_additional.py — генерирует 500 дополнительных конфигураций,
не повторяющих уже существующие в configs500.txt.

Соло-бейзлайны НЕ генерируются: используется существующая "Общая сводка".
Генерируются только случайные СОВМЕСТНЫЕ запуски — 2, 3 или 4 работы
одновременно, суммарно 2, 3 или 4 ядра.

Каждая строка — конфигурация в формате, понятном Runner.cpp:
    COPY:60:2,XPY:48:2
    ERF:24:1,POW:24:2,TGAMMA:60:1

Запуск:  python3 gen_configs_additional.py
Результат: файл configs_new.txt (500 новых наборов).
"""

import random
from collections import Counter

random.seed(42)  # фиксируем сид для воспроизводимости (пропуск старых даст новые)

# ---------------------------------------------------------------------------
# Курируемый пул работ: 20 (тип, размер)
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

N_MIX_TARGET = 500      # сколько новых конфигураций нужно сгенерировать

# ---------------------------------------------------------------------------
# Чтение существующих конфигураций из configs500.txt
# ---------------------------------------------------------------------------
existing_file = "configs.txt"
seen = set()            # множество канонических ключей (уже существующие + новые)

try:
    with open(existing_file, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # приводим к каноническому виду: сортируем работы по строке "TYPE:SIZE:CORES"
            jobs = line.split(",")
            key = tuple(sorted(jobs))
            seen.add(key)
    print(f"Загружено {len(seen)} существующих наборов из {existing_file}")
except FileNotFoundError:
    print(f"Файл {existing_file} не найден, продолжаем с пустым множеством.")
    # если файла нет, можно либо создать с нуля, либо выйти
    # но по условию он должен быть, поэтому просто предупреждаем

# ---------------------------------------------------------------------------
# Генерация новых наборов (не пересекающихся с seen)
# ---------------------------------------------------------------------------
new_lines = []
attempts = 0
max_attempts = N_MIX_TARGET * 50   # запас на случай редких комбинаций

while len(new_lines) < N_MIX_TARGET and attempts < max_attempts:
    attempts += 1

    total_cores = random.choice([2, 3, 4])
    if total_cores == 2:
        n_works = 2
    elif total_cores == 3:
        n_works = random.choice([2, 3])
    else:  # total_cores == 4
        n_works = random.choice([2, 2, 3, 4])

    # разбиение total_cores на n_works положительных частей
    parts = [1] * n_works
    remaining = total_cores - n_works
    for _ in range(remaining):
        parts[random.randrange(n_works)] += 1

    chosen = [random.choice(POOL) for _ in range(n_works)]
    job_str = ",".join(f"{t}:{s}:{c}" for (t, s), c in zip(chosen, parts))

    # канонический ключ для проверки уникальности (включая уже существующие)
    key = tuple(sorted(job_str.split(",")))
    if key in seen:
        continue
    seen.add(key)          # запоминаем, чтобы не повторяться в будущем
    new_lines.append(job_str)

# ---------------------------------------------------------------------------
# Запись результата
# ---------------------------------------------------------------------------
output_file = "configs2.txt"
with open(output_file, "w") as f:
    f.write("\n".join(new_lines) + "\n")

print(f"Сгенерировано {len(new_lines)} новых конфигураций в {output_file}")
print(f"Всего уникальных наборов (старые + новые): {len(seen)}")

# ---------------------------------------------------------------------------
# Статистика по новым наборам
# ---------------------------------------------------------------------------
tc = Counter()
nw = Counter()
for l in new_lines:
    parts_ = l.split(',')
    total = sum(int(p.split(':')[2]) for p in parts_)
    tc[total] += 1
    nw[len(parts_)] += 1

print("\nСтатистика новых конфигураций:")
print(f"  по суммарному числу ядер: {dict(sorted(tc.items()))}")
print(f"  по числу работ:           {dict(sorted(nw.items()))}")

# Также можно вывести несколько примеров
if new_lines:
    print("\nПримеры новых конфигураций (первые 5):")
    for i in range(min(5, len(new_lines))):
        print(f"  {new_lines[i]}")
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
compute_slowdown.py
 
Для каждой конфигурации из configs.txt считает замедление КАЖДОЙ работы:
  * ПРЕДСКАЗАННОЕ по правилу (данные о работах — из "Общей сводки");
  * ФАКТИЧЕСКОЕ (время в смеси из results_configs.txt / сольное время из сводки).
 
------------------------------------------------------------------------------
ПРАВИЛО (единственное, финальные коэффициенты — калибровка на 1000 конфигураций,
проверено на holdout: MAE 0.217, медианная ошибка 6.8%):
 
    замедление = 1 + f · D
 
Три уровня расчёта:
 
  1) ВЕТВЛЕНИЕ по числу ядер работы — определяет, как считать "давление" P:
       - 1 ядро   : работа сидит на одной паре ядер с общим L2. Ровно один
                    сосед делит с ней кэш (вес 1.3 — вредит сильнее, т.к. и
                    трубу забивает, и кэш вытесняет), остальные делят только
                    шину (вес 1.0). Потолок трубы C = 40.
                        P = own + Σ( вес · bus_соседа_на_ядро )
       - ≥2 ядра  : работа растянута на оба die, "чужого соседа по кэшу" нет,
                    различать не имеет смысла. Потолок C = 50.
                        P = Σшина всех работ конфигурации
 
  2) ПРЕВЫШЕНИЕ + НАСЫЩЕНИЕ:
       d = P/C − 1.  Если d ≤ 0 — замедления нет (D = 0).
       Иначе D = d, но при сильной перегрузке (d > d_knee = 1.2) рост
       замедляется — наклон срезается до 0.85:
           D = d,                              если d ≤ 1.2
           D = 1.2 + 0.85·(d − 1.2),           если d > 1.2
 
  3) ЧУВСТВИТЕЛЬНОСТЬ ТИПА f — "жадность к памяти". Своя таблица для каждого
     пути (1 ядро / ≥2 ядра). Compute-работы (TGAMMA) ~0 — почти не тормозят;
     memory-работы (COPY, SUM) — максимум.
 
Запуск:  python3 compute_slowdown.py
Файлы рядом:  configs.txt, results_configs.txt, Общая_сводка.txt
Результат:  slowdown_report.csv  +  сводка точности в консоли.
------------------------------------------------------------------------------
"""
 
import re
import csv
import statistics
from collections import defaultdict
 
SVODKA   = "Общая_сводка.txt"
CONFIGS  = "configs.txt"
RESULTS  = "results_configs.txt"
OUT_CSV  = "slowdown_report.csv"
 
# ======================= КОЭФФИЦИЕНТЫ ПРАВИЛА =======================
# (финальные, калибровка на всех 1000 конфигурациях)
 
C_DIE     = 40.0   # потолок трубы для работы на 1 ядре
C_SIMPLE  = 50.0   # потолок трубы для работы на >=2 ядрах
W_SAME    = 1.3    # вес соседа, делящего L2-кэш (тот же die)
W_OTHER   = 1.0    # вес соседа с другого die (делит только шину)
D_KNEE    = 1.2    # излом: до него драйв линеен, после — срезаем наклон
SLOPE     = 0.85   # наклон драйва после излома (насыщение)
 
# Чувствительность типа f, путь "1 ядро":
F_DIE = {'TGAMMA':0.04, 'POW':0.31, 'SQRTX':0.65, 'ERF':0.64,
         'XPY':0.93, 'DOT':0.96, 'SUM':1.06, 'COPY':1.35}
# Чувствительность типа f, путь ">=2 ядра":
F_SIMPLE = {'TGAMMA':0.09, 'POW':0.99, 'SQRTX':1.67, 'ERF':1.48,
            'XPY':0.83, 'DOT':1.45, 'SUM':1.87, 'COPY':1.37}
 
DIE_A = {0, 4}
DIE_B = {1, 5}
def die_of(core):
    return 'A' if int(core) in DIE_A else 'B'
 
 
# ============================================================================
# 1. Парсинг "Общей сводки"
#   TYPE_SIZE / RAM% / "1;time;bus" / "2;..." / "3;..." / "4;..."
# ============================================================================
def parse_svodka(path):
    raw = open(path, encoding='utf-8').read().replace('\r', '')
    base = {}   # (TYPE, SIZE, cores) -> (time_ms, bus_pct)
    ram  = {}   # (TYPE, SIZE) -> ram_pct
    for b in [x for x in raw.split('\n\n') if x.strip()]:
        lines = [l for l in b.split('\n') if l.strip()]
        m = re.match(r'([A-Z]+)_(\d+)', lines[0].strip())
        if not m:
            continue
        TYPE, SIZE = m.group(1), int(m.group(2))
 
        # авто-исправление известной опечатки: блок помечен COPY_84,
        # но c1-время ~27398 => это на самом деле COPY_60.
        c1_time = int(lines[2].split(';')[1])
        if TYPE == 'COPY' and SIZE == 84 and c1_time < 30000:
            print(f"  [исправление] блок 'COPY_84' (c1={c1_time}) — это COPY_60. Переименовано.")
            SIZE = 60
 
        ram[(TYPE, SIZE)] = float(lines[1])
        for dl in lines[2:]:
            if dl.count(';') != 2:
                continue
            c, t, bus = dl.split(';')
            base[(TYPE, SIZE, int(c))] = (int(t), float(bus))
    return base, ram
 
 
# ============================================================================
# 2. Парсинг configs.txt
# ============================================================================
def parse_configs(path):
    configs = []
    for line in open(path, encoding='utf-8').read().replace('\r', '').split('\n'):
        line = line.strip()
        if not line:
            continue
        works = [tuple([p if i == 0 else int(p) for i, p in enumerate(job.split(':'))])
                 for job in line.split(',')]
        configs.append((line, works))
    return configs
 
 
# ============================================================================
# 3. Парсинг results_configs.txt
# ============================================================================
def parse_results(path):
    raw = open(path, encoding='utf-8').read().replace('\r', '')
    results = []
    for blk in raw.split('###'):
        blk = blk.strip()
        if not blk:
            continue
        jobs = re.sub(r'^\[\d+/\d+\]\s*', '', blk.split('\n')[0]).strip()
        core_map = defaultdict(list)
        res_lines = []
        for line in blk.split('\n'):
            if line.startswith('CORES'):
                for tok in line.split()[1:]:
                    lbl, cs = tok.split('=')
                    core_map[lbl].append(cs)
            elif line.startswith('RESULT'):
                p = line.split()
                res_lines.append((p[1], int(p[2])))
        used = defaultdict(int)
        items = []
        for lbl, t in res_lines:
            i = used[lbl]
            cs = core_map[lbl][i] if lbl in core_map and i < len(core_map[lbl]) else ""
            used[lbl] += 1
            items.append((lbl, cs, t))
        results.append({'jobs': jobs, 'items': items})
    return results
 
 
def label_to_tsc(label):
    m = re.match(r'([A-Z]+)_(\d+)_c(\d+)', label)
    return m.group(1), int(m.group(2)), int(m.group(3))
 
 
# ============================================================================
# 4. ПРАВИЛО
# ============================================================================
def die_pressure(own_bus, own_cores_str, neighbors):
    """Давление на 1-ядерную работу: свой bus + соседи, где сосед по L2 весит
    W_SAME, а сосед только по шине — W_OTHER.
    neighbors: список (bus, cores_str) остальных работ конфигурации."""
    if not own_cores_str:
        return own_bus + sum(n_bus for n_bus, _ in neighbors)
    my_dies = set(die_of(c) for c in own_cores_str.split(','))
    pressure = own_bus
    for n_bus, n_cores in neighbors:
        if not n_cores:
            pressure += n_bus
            continue
        per_core = n_bus / len(n_cores.split(','))
        for c in n_cores.split(','):
            pressure += (W_SAME if die_of(c) in my_dies else W_OTHER) * per_core
    return pressure
 
 
def saturated_drive(pressure, C):
    """d = P/C - 1, линейно до D_KNEE, дальше с наклоном SLOPE (насыщение)."""
    d = pressure / C - 1.0
    if d <= 0.0:
        return 0.0
    if d <= D_KNEE:
        return d
    return D_KNEE + SLOPE * (d - D_KNEE)
 
 
def predict_slowdown(work_type, own_bus, own_cores_str, neighbors, sum_bus):
    """замедление = 1 + f · D  (см. описание в шапке файла)."""
    ncores = len(own_cores_str.split(',')) if own_cores_str else 0
    if ncores == 1:
        D = saturated_drive(die_pressure(own_bus, own_cores_str, neighbors), C_DIE)
        f = F_DIE[work_type]
    else:
        D = saturated_drive(sum_bus, C_SIMPLE)
        f = F_SIMPLE[work_type]
    return 1.0 + f * D
 
 
# ============================================================================
# MAIN
# ============================================================================
def main():
    print("Чтение 'Общей сводки'...")
    base, ram = parse_svodka(SVODKA)
    configs = parse_configs(CONFIGS)
    results = parse_results(RESULTS)
    print(f"  бейзлайнов: {len(base)},  конфигураций: {len(configs)},  блоков в results: {len(results)}")
 
    n = min(len(configs), len(results))
    if len(configs) != len(results):
        print(f"  [внимание] configs ({len(configs)}) != results ({len(results)}); беру первые {n}.")
 
    rows = []
    skipped = 0
    for i in range(n):
        cfg_line, _ = configs[i]
        items = results[i]['items']
 
        # суммарная шина конфигурации
        try:
            sum_bus = sum(base[label_to_tsc(lbl)][1] for lbl, _, _ in items)
        except KeyError:
            skipped += 1
            continue
 
        for idx, (lbl, cs, t_mix) in enumerate(items):
            T, S, C = label_to_tsc(lbl)
            if (T, S, C) not in base:
                skipped += 1
                continue
            t_solo, own_bus = base[(T, S, C)]
 
            neighbors = []
            for j, (lbl2, cs2, _) in enumerate(items):
                if j == idx:
                    continue
                neighbors.append((base[label_to_tsc(lbl2)][1], cs2))
 
            pred = predict_slowdown(T, own_bus, cs, neighbors, sum_bus)
            actual = t_mix / t_solo
 
            rows.append({
                'config_id': i + 1,
                'jobs': cfg_line,
                'work': lbl,
                'cores_used': cs,
                'own_bus': round(own_bus, 1),
                'sum_bus': round(sum_bus, 1),
                't_solo_ms': t_solo,
                't_mix_ms': t_mix,
                'slowdown_actual': round(actual, 3),
                'slowdown_pred': round(pred, 3),
                'abs_error': round(abs(pred - actual), 3),
            })
 
    with open(OUT_CSV, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
 
    mae = statistics.mean(r['abs_error'] for r in rows)
    relmed = statistics.median(r['abs_error'] / r['slowdown_actual'] for r in rows)
    within = sum(1 for r in rows if r['abs_error'] < 0.2) / len(rows) * 100
 
    print(f"\nОбработано замеров: {len(rows)}  (пропущено: {skipped})")
    print(f"Результат: {OUT_CSV}\n")
    print("=== Точность правила ===")
    print(f"  MAE (средняя абс. ошибка замедления): {mae:.3f}")
    print(f"  медианная относительная ошибка:       {relmed*100:.1f}%")
    print(f"  доля предсказаний в пределах ±0.2:     {within:.0f}%")
 
    print("\n=== Превью (первые 12 работ) ===")
    print(f"{'конфигурация':<30}{'работа':<14}{'Σшина':>7}{'факт':>7}{'правило':>9}{'ошибка':>8}")
    for r in rows[:12]:
        jobs_short = (r['jobs'][:27] + '..') if len(r['jobs']) > 29 else r['jobs']
        print(f"{jobs_short:<30}{r['work']:<14}{r['sum_bus']:>7.0f}"
              f"{r['slowdown_actual']:>7.2f}{r['slowdown_pred']:>9.2f}{r['abs_error']:>8.2f}")
 
 
if __name__ == "__main__":
    main()
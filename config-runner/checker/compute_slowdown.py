#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
compute_slowdown.py
 
Для каждой конфигурации из configs.txt считает замедление КАЖДОЙ работы двумя способами:
  1) ПРЕДСКАЗАННОЕ по правилу (данные берутся из "Общей сводки");
  2) ФАКТИЧЕСКОЕ (время в смеси из results_configs.txt / сольное время из сводки).
 
Правило (две версии, обе выводятся):
  * ПРОСТОЕ (нужны только типы/размеры/ядра из конфига):
        slowdown_i = 1 + f_i * max(0, Σшина/50 - 1)
    где Σшина — сумма сольных потреблений шины (%) всех работ конфигурации.
  * ТОЧНОЕ, die-aware (дополнительно использует раскладку ядер из CORES):
        давление_i = own + 1.3*(соседи на общем die) + 1.0*(соседи на другом die)
        slowdown_i = 1 + g_i * max(0, давление_i/40 - 1)
 
Запуск:  python3 compute_slowdown.py
Файлы рядом:  configs.txt, results_configs.txt, Общая_сводка.txt
Результат:  slowdown_report.csv  +  сводка точности в консоли.
"""
 
import re
import csv
import statistics
from collections import defaultdict
 
SVODKA   = "Общая_сводка.txt"
CONFIGS  = "configs.txt"
RESULTS  = "results_configs.txt"
OUT_CSV  = "slowdown_report.csv"
 
# --- Коэффициенты правила (калиброваны на чистых раундовых данных) ---
F_SIMPLE = {'TGAMMA':0.06,'POW':0.70,'SQRTX':1.26,'XPY':1.53,
            'ERF':1.60,'DOT':1.62,'SUM':2.11,'COPY':2.22}
C_SIMPLE = 50.0
 
G_DIE    = {'TGAMMA':0.04,'POW':0.30,'SQRTX':0.65,'ERF':0.63,
            'XPY':0.80,'DOT':0.91,'SUM':1.01,'COPY':1.23}
C_DIE    = 40.0
W_SAME   = 1.3   # вес соседа, делящего L2 (тот же die)
W_OTHER  = 1.0   # вес соседа с другого die (делит только FSB)
 
# --- ПОЛНАЯ модель (3 уровня): насыщение + ветвление по ядрам + f по типу ---
# Уровень 1: ветвление по числу ядер работы (как в гибриде) — определяет,
#            от чего считать конкуренцию:
#              1 ядро  -> die-aware давление, порог C_DIE=40
#              >=2 ядра -> простая Σшина,     порог C_SIMPLE=50
# Уровень 2: насыщение — драйв линеен до d_knee, дальше наклон срезается
#            (в сильной конкуренции замедление растёт медленнее линейного).
# Уровень 3: коэффициент f по типу; для двух путей — СВОИ таблицы.
C_SIMPLE = 50.0
D_KNEE   = 1.2    # излом нормированного драйва (для simple-пути это Σ≈110%)
SLOPE    = 0.85   # наклон драйва после излома
F_FULL_DIE = {'TGAMMA':0.04,'POW':0.31,'SQRTX':0.65,'ERF':0.67,   # путь 1 ядро
              'XPY':0.93,'DOT':0.96,'SUM':1.05,'COPY':1.35}
F_FULL_SIMPLE = {'TGAMMA':0.12,'POW':0.94,'SQRTX':1.68,'ERF':1.55, # путь >=2 ядра
                 'XPY':0.90,'DOT':1.42,'SUM':2.11,'COPY':1.49}
 
DIE_A = {0, 4}
DIE_B = {1, 5}
def die_of(core):
    return 'A' if int(core) in DIE_A else 'B'
 
 
# ============================================================================
# 1. Парсинг "Общей сводки"
# Формат блока (разделены пустой строкой):
#   TYPE_SIZE
#   RAM%
#   1;time;bus
#   2;time;bus
#   3;time;bus
#   4;time;bus
# ============================================================================
def parse_svodka(path):
    raw = open(path, encoding='utf-8').read().replace('\r', '')
    base = {}   # (TYPE, SIZE, cores) -> (time_ms, bus_pct)
    ram  = {}   # (TYPE, SIZE) -> ram_pct
    blocks = [b for b in raw.split('\n\n') if b.strip()]
 
    seen_labels = []
    for b in blocks:
        lines = [l for l in b.split('\n') if l.strip()]
        label = lines[0].strip()
        m = re.match(r'([A-Z]+)_(\d+)', label)
        if not m:
            continue
        TYPE, SIZE = m.group(1), int(m.group(2))
 
        # --- авто-исправление известной опечатки в метке ---
        # Блок помечен COPY_84, но c1-время ~27398 => это на самом деле COPY_60.
        c1_time = int(lines[2].split(';')[1])
        if TYPE == 'COPY' and SIZE == 84 and c1_time < 30000:
            print(f"  [исправление] блок '{label}' (c1={c1_time}) — это COPY_60, "
                  f"а не COPY_84. Переименовано.")
            SIZE = 60
 
        ram[(TYPE, SIZE)] = float(lines[1])
        for dl in lines[2:]:
            if dl.count(';') != 2:
                continue
            c, t, bus = dl.split(';')
            base[(TYPE, SIZE, int(c))] = (int(t), float(bus))
        seen_labels.append(f"{TYPE}_{SIZE}")
 
    return base, ram
 
 
# ============================================================================
# 2. Парсинг configs.txt — канонический список конфигураций
# ============================================================================
def parse_configs(path):
    configs = []
    for line in open(path, encoding='utf-8').read().replace('\r', '').split('\n'):
        line = line.strip()
        if not line:
            continue
        works = []
        for job in line.split(','):
            t, s, c = job.split(':')
            works.append((t, int(s), int(c)))
        configs.append((line, works))
    return configs
 
 
# ============================================================================
# 3. Парсинг results_configs.txt
# Блоки по '###'. Внутри: CORES ... и строки RESULT label time.
# ============================================================================
def parse_results(path):
    raw = open(path, encoding='utf-8').read().replace('\r', '')
    results = []   # список конфигов; каждый: {'jobs':str, 'items':[(label, cores_str, time)]}
    for blk in raw.split('###'):
        blk = blk.strip()
        if not blk:
            continue
        jobs_line = blk.split('\n')[0]
        jobs = re.sub(r'^\[\d+/\d+\]\s*', '', jobs_line).strip()
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
        # присвоить каждой RESULT-строке её ядра (по порядку появления метки)
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
# 4. Правила расчёта замедления
# ============================================================================
def predict_simple(work_type, sum_bus):
    return 1.0 + F_SIMPLE[work_type] * max(0.0, sum_bus / C_SIMPLE - 1.0)
 
 
def die_pressure(own_bus, own_cores_str, neighbors):
    """Взвешенное давление на работу: свой bus + соседи (L2-сосед x1.3, FSB-сосед x1.0).
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
            w = W_SAME if die_of(c) in my_dies else W_OTHER
            pressure += w * per_core
    return pressure
 
 
def predict_dieaware(work_type, own_bus, own_cores_str, neighbors):
    p = die_pressure(own_bus, own_cores_str, neighbors)
    return 1.0 + G_DIE[work_type] * max(0.0, p / C_DIE - 1.0)
 
 
def _saturated_drive(x, C, d_knee=D_KNEE, slope=SLOPE):
    """Нормированный драйв x/C-1, линейный до d_knee, дальше с наклоном slope<1."""
    d = x / C - 1.0
    if d <= 0.0:
        return 0.0
    if d <= d_knee:
        return d
    return d_knee + slope * (d - d_knee)
 
 
def predict_full(work_type, own_bus, own_cores_str, neighbors, sum_bus):
    """
    ПОЛНАЯ модель (рекомендуемая), три уровня:
      1) ветвление по числу ядер работы:
           1 ядро  -> драйв от die-aware давления (порог C_DIE=40),   f из F_FULL_DIE
           >=2 ядра -> драйв от Σшина            (порог C_SIMPLE=50), f из F_FULL_SIMPLE
      2) насыщение драйва: линеен до излома, дальше наклон 0.85
         (в сильной конкуренции замедление растёт медленнее линейного);
      3) f по типу — своя таблица для каждого пути.
    """
    ncores = len(own_cores_str.split(',')) if own_cores_str else 0
    if ncores == 1:
        drive = _saturated_drive(die_pressure(own_bus, own_cores_str, neighbors), C_DIE)
        f = F_FULL_DIE[work_type]
    else:
        drive = _saturated_drive(sum_bus, C_SIMPLE)
        f = F_FULL_SIMPLE[work_type]
    return 1.0 + f * drive
 
 
def predict_hybrid(pred_simple, pred_dieaware, own_cores_str):
    """
    Ветвление по числу ядер САМОЙ работы (рекомендуемое правило):
      * 1 ядро  -> die-aware: работа сидит на одном die, у неё ровно один
        L2-сосед, и его загрузка сильно влияет — есть что различать.
      * >=2 ядра -> простое: работа растянута на оба die, "соседей с другого
        die" у неё нет, различение same/other-die схлопывается, а множитель
        1.3 лишь равномерно завышает оценку. Взвешивание тут только мешает.
    """
    ncores = len(own_cores_str.split(',')) if own_cores_str else 0
    return pred_dieaware if ncores == 1 else pred_simple
 
 
# ============================================================================
# MAIN
# ============================================================================
def main():
    print("Чтение 'Общей сводки'...")
    base, ram = parse_svodka(SVODKA)
    configs = parse_configs(CONFIGS)
    results = parse_results(RESULTS)
 
    print(f"  бейзлайнов: {len(base)},  конфигураций в configs.txt: {len(configs)},"
          f"  блоков в results: {len(results)}")
 
    # сопоставление configs.txt <-> results по порядку (они в одном порядке)
    n = min(len(configs), len(results))
    if len(configs) != len(results):
        print(f"  [внимание] число строк configs ({len(configs)}) != число блоков "
              f"results ({len(results)}); беру первые {n}.")
 
    rows = []
    skipped = 0
    for i in range(n):
        cfg_line, works = configs[i]
        res = results[i]
        if res['jobs'] != cfg_line:
            # мягкая сверка; если разошлось — доверяем результату
            pass
 
        items = res['items']
        # суммарная шина всей конфигурации (по сольным bus% на нужном числе ядер)
        try:
            bus_list = []
            for (lbl, cs, t) in items:
                T, S, C = label_to_tsc(lbl)
                bus_list.append(base[(T, S, C)][1])
        except KeyError as e:
            skipped += 1
            continue
        sum_bus = sum(bus_list)
 
        for idx, (lbl, cs, t_mix) in enumerate(items):
            T, S, C = label_to_tsc(lbl)
            if (T, S, C) not in base:
                skipped += 1
                continue
            t_solo, own_bus = base[(T, S, C)]
 
            # соседи (все прочие работы конфигурации) для die-aware
            neighbors = []
            for j, (lbl2, cs2, _) in enumerate(items):
                if j == idx:
                    continue
                T2, S2, C2 = label_to_tsc(lbl2)
                neighbors.append((base[(T2, S2, C2)][1], cs2))
 
            pred_s = predict_simple(T, sum_bus)
            pred_d = predict_dieaware(T, own_bus, cs, neighbors)
            pred_h = predict_hybrid(pred_s, pred_d, cs)
            pred_f = predict_full(T, own_bus, cs, neighbors, sum_bus)
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
                'slowdown_pred_simple': round(pred_s, 3),
                'slowdown_pred_dieaware': round(pred_d, 3),
                'slowdown_pred_hybrid': round(pred_h, 3),
                'slowdown_pred_full': round(pred_f, 3),
                'err_simple': round(abs(pred_s - actual), 3),
                'err_dieaware': round(abs(pred_d - actual), 3),
                'err_hybrid': round(abs(pred_h - actual), 3),
                'err_full': round(abs(pred_f - actual), 3),
            })
 
    # --- запись CSV ---
    with open(OUT_CSV, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
 
    # --- сводка точности ---
    def mae(key):  return statistics.mean(r[key] for r in rows)
    def relmed(pk):
        return statistics.median(abs(r[pk]-r['slowdown_actual'])/r['slowdown_actual'] for r in rows)
    within = lambda k, thr=0.2: sum(1 for r in rows if r[k] < thr) / len(rows) * 100
 
    print(f"\nОбработано замеров: {len(rows)}  (пропущено из-за нехватки бейзлайна: {skipped})")
    print(f"Результат записан в: {OUT_CSV}\n")
    print("=== Точность правила ===")
    print(f"{'модель':<20}{'MAE':>8}{'медиана отн.ошибки':>22}{'доля в ±0.2':>14}")
    print(f"{'простое':<20}{mae('err_simple'):>8.3f}"
          f"{relmed('slowdown_pred_simple')*100:>20.1f}%{within('err_simple'):>13.0f}%")
    print(f"{'die-aware':<20}{mae('err_dieaware'):>8.3f}"
          f"{relmed('slowdown_pred_dieaware')*100:>20.1f}%{within('err_dieaware'):>13.0f}%")
    print(f"{'гибрид':<20}{mae('err_hybrid'):>8.3f}"
          f"{relmed('slowdown_pred_hybrid')*100:>20.1f}%{within('err_hybrid'):>13.0f}%")
    print(f"{'ПОЛНАЯ (рекоменд.)':<20}{mae('err_full'):>8.3f}"
          f"{relmed('slowdown_pred_full')*100:>20.1f}%{within('err_full'):>13.0f}%")
    print("  Полная = насыщение (Σ>110%) + ветвление по ядрам (1→die-aware, ≥2→простое) + f по типу.")
 
    # --- превью первых конфигураций ---
    print("\n=== Превью (первые 12 работ) ===")
    print(f"{'конфигурация':<26}{'работа':<13}{'Σшина':>6}{'факт':>6}"
          f"{'прост':>7}{'die':>7}{'гибр':>7}{'ПОЛН':>7}")
    for r in rows[:12]:
        jobs_short = (r['jobs'][:23] + '..') if len(r['jobs']) > 25 else r['jobs']
        print(f"{jobs_short:<26}{r['work']:<13}{r['sum_bus']:>6.0f}"
              f"{r['slowdown_actual']:>6.2f}{r['slowdown_pred_simple']:>7.2f}"
              f"{r['slowdown_pred_dieaware']:>7.2f}{r['slowdown_pred_hybrid']:>7.2f}"
              f"{r['slowdown_pred_full']:>7.2f}")
 
 
if __name__ == "__main__":
    main()
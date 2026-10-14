#!/usr/bin/env python3
"""Анализ часто повторяемых паттернов в трассе use-записей "User <- Operand".

Трасса — последовательность строк (по одной use-записи на строку) в порядке
исполнения. Для окон длиной n = 1..N считаем частоты всех подряд идущих
n-грамм (кортежей из n соседних записей) и печатаем топ-K по каждому n.

Запуск:
    python3 scripts/analyze.py build/trace.log -o stats -k 20 -n 5
"""
import argparse
import csv
import os
from collections import Counter


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("trace", help="файл трассы (строки 'User <- Operand')")
    ap.add_argument("-o", "--out", default="stats", help="каталог для статистики")
    ap.add_argument("-k", "--top", type=int, default=20, help="топ-K на окно")
    ap.add_argument("-n", "--max-window", type=int, default=5, help="макс. длина окна")
    args = ap.parse_args()

    with open(args.trace, "r", encoding="utf-8", errors="replace") as f:
        toks = [ln.rstrip("\n") for ln in f if ln.strip()]

    total = len(toks)
    os.makedirs(args.out, exist_ok=True)

    report = []
    report.append("# Анализ паттернов трассы use-записей (User <- Operand)")
    report.append(f"Файл трассы : {args.trace}")
    report.append(f"Всего записей (токенов): {total}")
    report.append(f"Уникальных записей (n=1): {len(set(toks))}")
    report.append(f"Окна: 1..{args.max_window}; топ-{args.top} на окно")
    report.append("")

    for n in range(1, args.max_window + 1):
        if total < n:
            break
        counter = Counter(
            tuple(toks[i : i + n]) for i in range(total - n + 1)
        )
        windows_total = total - n + 1
        report.append("=" * 72)
        report.append(f"[окно = {n} инстр.]  различных паттернов: {len(counter)}"
                      f"  всего окон: {windows_total}")
        report.append("-" * 72)
        report.append(f"{'count':>12}  {'доля':>7}  паттерн")
        for pat, cnt in counter.most_common(args.top):
            share = 100.0 * cnt / windows_total
            pat_str = "  ,  ".join(pat)
            report.append(f"{cnt:>12}  {share:6.2f}%  {pat_str}")
        report.append("")

        # Машиночитаемый CSV по этому окну (полный, не только топ).
        csv_path = os.path.join(args.out, f"patterns_n{n}.csv")
        with open(csv_path, "w", newline="", encoding="utf-8") as cf:
            w = csv.writer(cf)
            w.writerow([f"tok{i+1}" for i in range(n)] + ["count", "share_pct"])
            for pat, cnt in counter.most_common():
                w.writerow(list(pat) + [cnt, f"{100.0*cnt/windows_total:.6f}"])

    out_txt = os.path.join(args.out, "patterns.txt")
    with open(out_txt, "w", encoding="utf-8") as of:
        of.write("\n".join(report) + "\n")

    print("\n".join(report))
    print(f"\n[analyze] записано: {out_txt} и patterns_n1..n{args.max_window}.csv в {args.out}/")


if __name__ == "__main__":
    main()

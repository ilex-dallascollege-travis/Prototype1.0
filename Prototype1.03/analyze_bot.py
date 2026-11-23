#!/usr/bin/env python3
"""
Prototype1.0 Automated Executive Analytics Engine (analyze_bot.py)
Reads MT5's shared Prototype1_TradeLog.csv and produces an executive scorecard,
plain-text summary, and visual HTML dashboard with zero external dependencies.
"""

import argparse
import csv
import html
import os
import sys
import time
from collections import defaultdict
from datetime import datetime
from pathlib import Path


def default_csv_path() -> Path:
    appdata = os.environ.get("APPDATA", "")
    if appdata:
        mt5_common = Path(appdata) / "MetaQuotes" / "Terminal" / "Common" / "Files" / "Prototype1_TradeLog.csv"
        if mt5_common.exists():
            return mt5_common
    local = Path("Prototype1_TradeLog.csv")
    if local.exists():
        return local
    if appdata:
        return Path(appdata) / "MetaQuotes" / "Terminal" / "Common" / "Files" / "Prototype1_TradeLog.csv"
    return local


def parse_float(val: str, default: float = 0.0) -> float:
    try:
        return float(val)
    except (ValueError, TypeError):
        return default


def parse_int(val: str, default: int = 0) -> int:
    try:
        return int(float(val))
    except (ValueError, TypeError):
        return default


def parse_mt5_time(val: str):
    if not val:
        return None
    for fmt in ("%Y.%m.%d %H:%M:%S", "%Y-%m-%d %H:%M:%S"):
        try:
            return datetime.strptime(val.strip(), fmt)
        except ValueError:
            continue
    return None


def load_trades(csv_path: Path):
    if not csv_path.exists():
        return []
    seen = set()
    trades = []
    with csv_path.open("r", encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            ticket = (row.get("deal_ticket") or "").strip()
            if not ticket or ticket in seen:
                continue
            seen.add(ticket)
            close_dt = parse_mt5_time(row.get("close_time", ""))
            open_dt = parse_mt5_time(row.get("open_time", ""))
            trades.append({
                "deal_ticket": ticket,
                "strategy_tag": (row.get("strategy_tag") or "UNTAGGED").strip(),
                "symbol": (row.get("symbol") or "UNKNOWN").strip(),
                "timeframe": (row.get("timeframe") or "").strip(),
                "direction": (row.get("direction") or "").strip().upper(),
                "open_dt": open_dt,
                "close_dt": close_dt,
                "pips": parse_float(row.get("pips")),
                "gross_profit": parse_float(row.get("gross_profit")),
                "commission": parse_float(row.get("commission")),
                "swap": parse_float(row.get("swap")),
                "fee": parse_float(row.get("fee")),
                "net_profit": parse_float(row.get("net_profit")),
                "hold_seconds": parse_int(row.get("hold_seconds")),
                "spread_pips": parse_float(row.get("spread_pips")),
                "exit_reason": (row.get("exit_reason") or "OTHER").strip().upper(),
                "outcome": (row.get("outcome") or "").strip().upper(),
            })
    trades.sort(key=lambda t: (t["close_dt"] or datetime.min, t["deal_ticket"]))
    return trades


def compute_metrics(trades, min_trades: int):
    n = len(trades)
    if n == 0:
        return None

    wins = [t for t in trades if t["net_profit"] > 0]
    losses = [t for t in trades if t["net_profit"] < 0]
    bes = [t for t in trades if t["net_profit"] == 0]

    gross_win = sum(t["net_profit"] for t in wins)
    gross_loss = abs(sum(t["net_profit"] for t in losses))
    net_profit = sum(t["net_profit"] for t in trades)
    total_pips = sum(t["pips"] for t in trades)

    win_rate = (len(wins) / n) * 100.0
    wl_ratio = (len(wins) / len(losses)) if losses else float("inf")
    avg_win = (gross_win / len(wins)) if wins else 0.0
    avg_loss = (gross_loss / len(losses)) if losses else 0.0
    payoff_ratio = (avg_win / avg_loss) if avg_loss > 0 else float("inf")
    profit_factor = (gross_win / gross_loss) if gross_loss > 0 else (float("inf") if gross_win > 0 else 0.0)
    expectancy = net_profit / n
    avg_pips = total_pips / n
    avg_hold_min = (sum(t["hold_seconds"] for t in trades) / n) / 60.0
    avg_spread = sum(t["spread_pips"] for t in trades) / n

    # Equity curve, max drawdown, streaks
    cum = 0.0
    peak = 0.0
    max_dd = 0.0
    equity_curve = [0.0]
    cur_win_streak = cur_loss_streak = 0
    max_win_streak = max_loss_streak = 0

    for t in trades:
        p = t["net_profit"]
        cum += p
        equity_curve.append(cum)
        if cum > peak:
            peak = cum
        dd = peak - cum
        if dd > max_dd:
            max_dd = dd

        if p > 0:
            cur_win_streak += 1
            cur_loss_streak = 0
            max_win_streak = max(max_win_streak, cur_win_streak)
        elif p < 0:
            cur_loss_streak += 1
            cur_win_streak = 0
            max_loss_streak = max(max_loss_streak, cur_loss_streak)
        else:
            cur_win_streak = 0
            cur_loss_streak = 0

    # Daily velocity (trades per active day)
    dates = {t["close_dt"].date() for t in trades if t["close_dt"]}
    active_days = max(len(dates), 1)
    trades_per_day = n / active_days

    # Breakdowns
    by_symbol = defaultdict(list)
    by_hour = defaultdict(list)
    by_dir = defaultdict(list)
    by_reason = defaultdict(int)
    for t in trades:
        by_symbol[t["symbol"]].append(t)
        by_dir[t["direction"]].append(t)
        by_reason[t["exit_reason"]] += 1
        if t["open_dt"]:
            by_hour[t["open_dt"].hour].append(t)
        elif t["close_dt"]:
            by_hour[t["close_dt"].hour].append(t)

    symbol_stats = []
    for sym, items in by_symbol.items():
        s_net = sum(x["net_profit"] for x in items)
        s_wins = sum(1 for x in items if x["net_profit"] > 0)
        s_gw = sum(x["net_profit"] for x in items if x["net_profit"] > 0)
        s_gl = abs(sum(x["net_profit"] for x in items if x["net_profit"] < 0))
        s_pf = (s_gw / s_gl) if s_gl > 0 else (float("inf") if s_gw > 0 else 0.0)
        symbol_stats.append({
            "symbol": sym,
            "trades": len(items),
            "win_rate": (s_wins / len(items)) * 100.0,
            "net_profit": s_net,
            "profit_factor": s_pf,
            "avg_spread": sum(x["spread_pips"] for x in items) / len(items),
        })
    symbol_stats.sort(key=lambda x: x["net_profit"], reverse=True)

    hour_stats = []
    for hr in sorted(by_hour.keys()):
        items = by_hour[hr]
        h_wins = sum(1 for x in items if x["net_profit"] > 0)
        hour_stats.append({
            "hour": hr,
            "trades": len(items),
            "win_rate": (h_wins / len(items)) * 100.0,
            "net_profit": sum(x["net_profit"] for x in items),
        })

    dir_stats = {}
    for d in ("BUY", "SELL"):
        items = by_dir.get(d, [])
        d_wins = sum(1 for x in items if x["net_profit"] > 0)
        dir_stats[d] = {
            "trades": len(items),
            "win_rate": (d_wins / len(items) * 100.0) if items else 0.0,
            "net_profit": sum(x["net_profit"] for x in items),
        }

    # Executive Verdict
    if n < min_trades:
        verdict = "COLLECTING DATA"
        verdict_note = f"{n}/{min_trades} trades logged — let it run before judging."
        verdict_class = "badge-wait"
    elif profit_factor >= 1.25 and expectancy > 0 and win_rate >= 35.0:
        verdict = "PROMOTE"
        verdict_note = "Positive expectancy and Profit Factor >= 1.25. Candidate to scale or forward-test longer."
        verdict_class = "badge-promote"
    elif profit_factor >= 1.0:
        verdict = "TWEAK"
        verdict_note = "Breakeven/marginal edge. Drop worst pairs or toxic hours before killing."
        verdict_class = "badge-tweak"
    else:
        verdict = "KILL"
        verdict_note = "Negative expectancy after sample threshold. Retire and deploy next technique."
        verdict_class = "badge-kill"

    return {
        "trades": n,
        "wins": len(wins),
        "losses": len(losses),
        "bes": len(bes),
        "win_rate": win_rate,
        "wl_ratio": wl_ratio,
        "avg_win": avg_win,
        "avg_loss": avg_loss,
        "payoff_ratio": payoff_ratio,
        "net_profit": net_profit,
        "gross_win": gross_win,
        "gross_loss": gross_loss,
        "profit_factor": profit_factor,
        "expectancy": expectancy,
        "avg_pips": avg_pips,
        "max_dd": max_dd,
        "max_win_streak": max_win_streak,
        "max_loss_streak": max_loss_streak,
        "avg_hold_min": avg_hold_min,
        "avg_spread": avg_spread,
        "trades_per_day": trades_per_day,
        "equity_curve": equity_curve,
        "symbol_stats": symbol_stats,
        "hour_stats": hour_stats,
        "dir_stats": dir_stats,
        "exit_reasons": dict(by_reason),
        "verdict": verdict,
        "verdict_note": verdict_note,
        "verdict_class": verdict_class,
    }


def fmt_ratio(v: float) -> str:
    return "INF" if v == float("inf") else f"{v:.2f}"


def build_svg_equity_curve(points, width=760, height=180) -> str:
    if len(points) < 2:
        return "<div class='muted'>Not enough closed trades for an equity curve yet.</div>"
    min_y = min(points)
    max_y = max(points)
    if max_y == min_y:
        max_y += 1.0
        min_y -= 1.0

    pad_x, pad_y = 14, 16
    w = width - 2 * pad_x
    h = height - 2 * pad_y

    def sx(i):
        return pad_x + (i / (len(points) - 1)) * w

    def sy(val):
        return pad_y + (1.0 - (val - min_y) / (max_y - min_y)) * h

    zero_y = sy(0.0) if (min_y <= 0.0 <= max_y) else None
    poly = " ".join(f"{sx(i):.1f},{sy(v):.1f}" for i, v in enumerate(points))
    stroke = "#22c55e" if points[-1] >= 0 else "#ef4444"

    zero_line = (
        f"<line x1='{pad_x}' y1='{zero_y:.1f}' x2='{width - pad_x}' y2='{zero_y:.1f}' "
        f"stroke='#475569' stroke-dasharray='4 4' stroke-width='1' />"
        if zero_y is not None else ""
    )

    return (
        f"<svg viewBox='0 0 {width} {height}' class='equity-svg'>"
        f"{zero_line}"
        f"<polyline fill='none' stroke='{stroke}' stroke-width='2.2' points='{poly}' />"
        f"<text x='{pad_x}' y='13' fill='#94a3b8' font-size='11'>High: ${max_y:,.2f}</text>"
        f"<text x='{pad_x}' y='{height - 4}' fill='#94a3b8' font-size='11'>Low: ${min_y:,.2f}</text>"
        f"<text x='{width - pad_x}' y='13' text-anchor='end' fill='{stroke}' font-size='11'>End: ${points[-1]:,.2f}</text>"
        f"</svg>"
    )


def render_text_report(results: dict, csv_path: Path) -> str:
    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = [
        "=" * 72,
        "PROTOTYPE 1.0 — EXECUTIVE TRADING SCORECARD",
        f"Generated: {now_str}",
        f"Source CSV: {csv_path}",
        "=" * 72,
    ]
    if not results:
        lines.append("No closed trades recorded yet. Confirm MT5 EA is running and closing trades.")
        return "\n".join(lines) + "\n"

    for tag, m in results.items():
        lines.extend([
            "",
            f"STRATEGY: {tag}   [{m['verdict']}]",
            f"Note    : {m['verdict_note']}",
            "-" * 72,
            f"Trades       : {m['trades']} ({m['wins']}W / {m['losses']}L / {m['bes']}BE) | ~{m['trades_per_day']:.1f}/day",
            f"Win Rate     : {m['win_rate']:.1f}%   | Win/Loss Ratio : {fmt_ratio(m['wl_ratio'])}",
            f"Profit Factor: {fmt_ratio(m['profit_factor'])}   | Payoff Ratio   : {fmt_ratio(m['payoff_ratio'])}",
            f"Net Profit   : ${m['net_profit']:,.2f} | Expectancy     : ${m['expectancy']:,.2f}/trade ({m['avg_pips']:+.2f} pips)",
            f"Avg Win/Loss : +${m['avg_win']:,.2f} / -${m['avg_loss']:,.2f}",
            f"Max Drawdown : ${m['max_dd']:,.2f} | Streaks (W/L)  : {m['max_win_streak']} / {m['max_loss_streak']}",
            f"Avg Hold     : {m['avg_hold_min']:.1f} min | Avg Spread     : {m['avg_spread']:.2f} pips",
            f"BUY          : {m['dir_stats']['BUY']['trades']} trades | {m['dir_stats']['BUY']['win_rate']:.1f}% WR | ${m['dir_stats']['BUY']['net_profit']:,.2f}",
            f"SELL         : {m['dir_stats']['SELL']['trades']} trades | {m['dir_stats']['SELL']['win_rate']:.1f}% WR | ${m['dir_stats']['SELL']['net_profit']:,.2f}",
            f"Exit Reasons : {m['exit_reasons']}",
            "",
            "  Top/Bottom Pairs:",
        ])
        for s in m["symbol_stats"]:
            lines.append(
                f"    {s['symbol']:<8} | {s['trades']:>4} trades | {s['win_rate']:>5.1f}% WR | "
                f"PF {fmt_ratio(s['profit_factor']):>5} | ${s['net_profit']:>8,.2f} | spread {s['avg_spread']:.2f}p"
            )
    return "\n".join(lines) + "\n"


def render_html_dashboard(results: dict, csv_path: Path) -> str:
    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    sections = []

    if not results:
        sections.append(
            "<div class='card'><h2>Waiting for Trade Data</h2>"
            "<p class='muted'>No closed trades logged yet. Once Prototype1.0 closes its first trade, "
            "this dashboard will populate automatically.</p></div>"
        )
    else:
        for tag, m in results.items():
            pnl_cls = "pos" if m["net_profit"] >= 0 else "neg"
            sym_rows = "".join(
                f"<tr><td>{html.escape(s['symbol'])}</td><td>{s['trades']}</td>"
                f"<td>{s['win_rate']:.1f}%</td><td>{fmt_ratio(s['profit_factor'])}</td>"
                f"<td class='{'pos' if s['net_profit'] >= 0 else 'neg'}'>${s['net_profit']:,.2f}</td>"
                f"<td>{s['avg_spread']:.2f}p</td></tr>"
                for s in m["symbol_stats"]
            )
            hr_rows = "".join(
                f"<tr><td>{h['hour']:02d}:00</td><td>{h['trades']}</td>"
                f"<td>{h['win_rate']:.1f}%</td>"
                f"<td class='{'pos' if h['net_profit'] >= 0 else 'neg'}'>${h['net_profit']:,.2f}</td></tr>"
                for h in m["hour_stats"]
            )
            svg = build_svg_equity_curve(m["equity_curve"])
            sections.append(f"""
            <section class="card">
              <div class="card-header">
                <div>
                  <h2>{html.escape(tag)}</h2>
                  <p class="muted">{html.escape(m['verdict_note'])}</p>
                </div>
                <span class="badge {m['verdict_class']}">{html.escape(m['verdict'])}</span>
              </div>

              <div class="kpi-grid">
                <div class="kpi"><div class="label">Net Profit</div><div class="val {pnl_cls}">${m['net_profit']:,.2f}</div></div>
                <div class="kpi"><div class="label">Trades (~{m['trades_per_day']:.0f}/day)</div><div class="val">{m['trades']} ({m['wins']}W / {m['losses']}L)</div></div>
                <div class="kpi"><div class="label">Win Rate / W:L Ratio</div><div class="val">{m['win_rate']:.1f}% ({fmt_ratio(m['wl_ratio'])})</div></div>
                <div class="kpi"><div class="label">Profit Factor</div><div class="val">{fmt_ratio(m['profit_factor'])}</div></div>
                <div class="kpi"><div class="label">Expectancy / Trade</div><div class="val {pnl_cls}">${m['expectancy']:,.2f} ({m['avg_pips']:+.2f}p)</div></div>
                <div class="kpi"><div class="label">Max Drawdown</div><div class="val neg">${m['max_dd']:,.2f}</div></div>
                <div class="kpi"><div class="label">Avg Win / Loss</div><div class="val">+${m['avg_win']:,.2f} / -${m['avg_loss']:,.2f}</div></div>
                <div class="kpi"><div class="label">Avg Hold / Spread</div><div class="val">{m['avg_hold_min']:.1f}m / {m['avg_spread']:.2f}p</div></div>
              </div>

              <h3>Closed Equity Curve</h3>
              {svg}

              <div class="two-col">
                <div>
                  <h3>Performance by Pair</h3>
                  <table>
                    <thead><tr><th>Pair</th><th>Trades</th><th>Win %</th><th>PF</th><th>Net $</th><th>Spread</th></tr></thead>
                    <tbody>{sym_rows}</tbody>
                  </table>
                </div>
                <div>
                  <h3>Performance by Server Hour</h3>
                  <table>
                    <thead><tr><th>Hour</th><th>Trades</th><th>Win %</th><th>Net $</th></tr></thead>
                    <tbody>{hr_rows}</tbody>
                  </table>
                </div>
              </div>
            </section>
            """)

    body = "\n".join(sections)
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta http-equiv="refresh" content="60" />
  <title>Prototype1.0 — Executive Scorecard</title>
  <style>
    body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #e2e8f0; margin: 0; padding: 24px; }}
    .wrap {{ max-width: 1120px; margin: 0 auto; }}
    header {{ display: flex; justify-content: space-between; align-items: baseline; border-bottom: 1px solid #334155; padding-bottom: 12px; margin-bottom: 24px; }}
    h1 {{ margin: 0; font-size: 24px; }}
    h2 {{ margin: 0 0 4px 0; font-size: 20px; }}
    h3 {{ margin: 20px 0 10px 0; font-size: 14px; text-transform: uppercase; letter-spacing: 0.05em; color: #94a3b8; }}
    .muted {{ color: #94a3b8; font-size: 13px; margin: 0; }}
    .card {{ background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 20px; margin-bottom: 24px; }}
    .card-header {{ display: flex; justify-content: space-between; align-items: flex-start; gap: 16px; margin-bottom: 16px; }}
    .badge {{ padding: 6px 12px; border-radius: 999px; font-weight: 700; font-size: 12px; white-space: nowrap; }}
    .badge-promote {{ background: #14532d; color: #86efac; }}
    .badge-tweak {{ background: #713f12; color: #fde047; }}
    .badge-kill {{ background: #7f1d1d; color: #fca5a5; }}
    .badge-wait {{ background: #1e3a8a; color: #93c5fd; }}
    .kpi-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 12px; }}
    .kpi {{ background: #0f172a; border: 1px solid #334155; border-radius: 8px; padding: 12px; }}
    .kpi .label {{ font-size: 12px; color: #94a3b8; margin-bottom: 4px; }}
    .kpi .val {{ font-size: 18px; font-weight: 700; }}
    .pos {{ color: #4ade80; }}
    .neg {{ color: #f87171; }}
    .equity-svg {{ width: 100%; height: auto; background: #0f172a; border: 1px solid #334155; border-radius: 8px; }}
    .two-col {{ display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-top: 12px; }}
    @media (max-width: 820px) {{ .two-col {{ grid-template-columns: 1fr; }} }}
    table {{ width: 100%; border-collapse: collapse; font-size: 13px; background: #0f172a; border-radius: 8px; overflow: hidden; }}
    th, td {{ padding: 8px 10px; text-align: right; border-bottom: 1px solid #1e293b; }}
    th:first-child, td:first-child {{ text-align: left; }}
    th {{ background: #0b1220; color: #94a3b8; font-weight: 600; }}
  </style>
</head>
<body>
  <div class="wrap">
    <header>
      <div>
        <h1>Prototype1.0 — Executive Trading Dashboard</h1>
        <p class="muted">Source: {html.escape(str(csv_path))}</p>
      </div>
      <div class="muted">Updated: {now_str} (auto-refreshes every 60s)</div>
    </header>
    {body}
  </div>
</body>
</html>
"""


def run_once(csv_path: Path, out_dir: Path, min_trades: int):
    trades = load_trades(csv_path)
    grouped = defaultdict(list)
    for t in trades:
        grouped[t["strategy_tag"]].append(t)

    results = {}
    for tag in sorted(grouped.keys()):
        results[tag] = compute_metrics(grouped[tag], min_trades)

    out_dir.mkdir(parents=True, exist_ok=True)
    txt_report = render_text_report(results, csv_path)
    html_report = render_html_dashboard(results, csv_path)

    (out_dir / "executive_summary.txt").write_text(txt_report, encoding="utf-8")
    (out_dir / "executive_dashboard.html").write_text(html_report, encoding="utf-8")

    print(txt_report)
    print(f"Saved dashboard -> {(out_dir / 'executive_dashboard.html').resolve()}")


def main():
    parser = argparse.ArgumentParser(description="Analyze Prototype1.0 MT5 trade CSV logs.")
    parser.add_argument("--csv", type=Path, default=None, help="Path to Prototype1_TradeLog.csv")
    parser.add_argument("--out", type=Path, default=Path("reports"), help="Output directory for reports")
    parser.add_argument("--min-trades", type=int, default=100, help="Minimum trades required before PROMOTE/KILL verdict")
    parser.add_argument("--watch", type=int, default=0, help="Auto-refresh interval in seconds (e.g. 300 for every 5 min)")
    args = parser.parse_args()

    csv_path = args.csv if args.csv else default_csv_path()

    if args.watch > 0:
        print(f"Watching {csv_path} every {args.watch}s. Press Ctrl+C to stop.")
        try:
            while True:
                run_once(csv_path, args.out, args.min_trades)
                time.sleep(args.watch)
        except KeyboardInterrupt:
            print("Stopped watcher.")
    else:
        run_once(csv_path, args.out, args.min_trades)


if __name__ == "__main__":
    main()

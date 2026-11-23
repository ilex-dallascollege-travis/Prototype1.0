# How to Use Prototype1.0

> **Disclaimer:** Prototype1.0 is an educational student project provided "as is" with no warranty. **Use paper trading (demo accounts) only.** Automated trading carries substantial risk of financial loss. Read the full disclaimer and risk warnings in `README.md` before running any code.

---

## Overview

**Prototype1.0** is an automated technical-analysis trading system built for **MetaTrader 5 (MT5)** paired with a zero-dependency **Python Executive Analytics Dashboard**.

The system works in two automated layers:
1. **Execution & Auto-Logging (`EMA_Crossover_EA.mq5` — v1.03):** Monitors completed candles for Fast/Slow EMA crossovers, opens BUY and/or SELL demo trades with fixed Stop Loss, Take Profit, and spread protection, and automatically appends every closed trade to a shared CSV log.
2. **Automated Analysis (`analyze_bot.py`):** Continuously reads MT5's CSV trade log, computes performance metrics (Win Rate, Win/Loss Ratio, Profit Factor, Expectancy, Max Drawdown, pair/hour breakdowns), draws an equity curve, and assigns an executive verdict (`COLLECTING DATA`, `PROMOTE`, `TWEAK`, or `KILL`) to each strategy you test.

---

## Prerequisites

Before starting, make sure you have:
- **Windows 10/11 PC** (steps below are written for Windows; Mac/Linux users will need Wine/VPS for MT5).
- **MetaTrader 5** installed and logged into a **Demo Account** (never test on a live account).
- **Python 3.8+** installed (no external `pip` packages are required — `analyze_bot.py` uses only the Python standard library).

---

## Repository Files

| File | Purpose |
| :--- | :--- |
| `EMA_Crossover_EA.mq5` | MetaTrader 5 Expert Advisor source code (v1.03) |
| `analyze_bot.py` | Automated Python trade analyzer & HTML dashboard generator |
| `README.md` | Project overview, disclaimers, risk & tax warnings |
| `HOW_TO_USE.md` | This step-by-step setup and operation guide |
| `CHANGELOG_v1.01.txt` / `v1.02.txt` | Version history and change documentation |

---

## Step 1: Download the Repository

Clone or download this repository to a folder on your computer (for example, `Documents\Prototype1.0`):

```bash
git clone https://github.com/ilex-dallascollege-travis/Prototype1.0.git
```

Or click **Code → Download ZIP** on GitHub and extract the folder.

---

## Step 2: Install & Compile the Bot in MetaTrader 5

1. Open **MetaTrader 5** and log into your **Demo Account**.
2. In the top menu bar, click **File → Open Data Folder**.
3. Open `MQL5` → `Experts`.
4. Copy `EMA_Crossover_EA.mq5` from your downloaded repo into the `MQL5\Experts\` folder (or inside a subfolder like `MQL5\Experts\Advisors\`).
5. Return to MetaTrader 5 and press **F4** to open **MetaEditor**.
6. In MetaEditor's Navigator panel on the left, double-click `EMA_Crossover_EA.mq5` to open it.
7. Press **F7** (Compile). Confirm the bottom **Errors** tab shows **0 errors, 0 warnings**.
8. Close MetaEditor, return to MT5, right-click **Expert Advisors** in the **Navigator** panel (`Ctrl+N`), and click **Refresh**.
   - `EMA_Crossover_EA` will now appear under **Expert Advisors**.

---

## Step 3: Attach the Bot to Charts

Before attaching the EA, make sure the **Algo Trading** button on the top MT5 toolbar is **red (off)** so no trades execute while you configure your charts.

### Option A: Single-Chart Setup (Simple / Low Volume)
1. Open a chart for a standard Forex pair (e.g., `EURUSD`).
2. Select your desired timeframe (default in v1.03 is `M1`; use `H1` with EMA `40/200` for slower swing testing).
3. Drag `EMA_Crossover_EA` from **Navigator** onto the chart.
4. Under the **Common** tab, check **Allow Algo Trading**.
5. Under the **Inputs** tab, verify your settings and click **OK**.

### Option B: Multi-Pair M1 Setup (High Volume Data Collection)
To collect ~150–250 paper trades per day across major currency pairs:

1. Press **Ctrl+M** to open **Market Watch** and open **Chart Windows** for 7 liquid Forex pairs:
   - `EURUSD`, `GBPUSD`, `USDJPY`, `AUDUSD`, `USDCAD`, `USDCHF`, `EURJPY`
   - *(If any pair is hidden, press **Ctrl+U**, search for it, double-click it to enable it, and click **OK**.)*
2. Switch **all 7 charts** to the **M1** timeframe.
3. Drag `EMA_Crossover_EA` onto `EURUSD,M1`, check **Allow Algo Trading** on the **Common** tab, verify the default **Inputs** (`InpStrategyTag = EMA_5_15_M1_v103`, `InpLotSize = 0.01`, `InpMagicNumber = 20240501`), and click **OK**.
4. Right-click `EURUSD,M1` → **Templates → Save Template...** and save it as `Prototype1_M1`.
5. On each of the remaining 6 `M1` charts, right-click → **Templates → Prototype1_M1** to copy the EA and settings over instantly.
6. **Assign a unique Magic Number to each chart** (press **F7** on each chart → **Inputs** tab → update `InpMagicNumber` → **OK**):

| Chart (`M1`) | `InpMagicNumber` | `InpStrategyTag` |
| :--- | :--- | :--- |
| `EURUSD` | `20240501` | `EMA_5_15_M1_v103` |
| `GBPUSD` | `20240502` | `EMA_5_15_M1_v103` |
| `USDJPY` | `20240503` | `EMA_5_15_M1_v103` |
| `AUDUSD` | `20240504` | `EMA_5_15_M1_v103` |
| `USDCAD` | `20240505` | `EMA_5_15_M1_v103` |
| `USDCHF` | `20240506` | `EMA_5_15_M1_v103` |
| `EURJPY` | `20240507` | `EMA_5_15_M1_v103` |

> Keep `InpStrategyTag` identical across all charts running the same technique so the analytics dashboard groups them into one combined scorecard.

---

## Step 4: Start Automated Paper Trading

1. Go to **Tools → Options → Expert Advisors**, ensure **Allow algorithmic trading** is checked, and click **OK**.
2. Click the **Algo Trading** button on the top toolbar so it turns **green**.
3. Verify two things:
   - The small graduation-cap icon next to `EMA_Crossover_EA` in the top-right corner of every chart turns **blue** (not gray).
   - In the bottom **Toolbox** panel (`Ctrl+T`), open the **Experts** tab and confirm clean `EMA Crossover EA v1.03 initialized` messages with zero error codes.
4. Switch to the **Trade** tab to monitor open paper positions.

---

## Step 5: Launch the Automated Analytics Dashboard

Whenever the EA closes a trade, it automatically logs the full trade record (entry/exit price, pips, spread, net profit, duration, exit reason, and outcome) to:

```text
%APPDATA%\MetaQuotes\Terminal\Common\Files\Prototype1_TradeLog.csv
```

To analyze that data automatically:

1. Open your `Prototype1.0` folder in Windows File Explorer.
2. Right-click empty space inside the folder and select **Open in Terminal** (or open PowerShell in that directory).
3. Run the live watcher (refreshes every 5 minutes / 300 seconds):

```powershell
python analyze_bot.py --watch 300
```

*(If Windows says `Python was not found`, use `py analyze_bot.py --watch 300` instead.)*

4. Minimize the PowerShell window (do **not** close it while collecting data).
5. Open the newly generated `reports` folder inside `Prototype1.0` and double-click **`executive_dashboard.html`**.
   - The browser dashboard auto-refreshes every 60 seconds.
   - A plain-text version is also saved to `reports\executive_summary.txt`.

### Optional `analyze_bot.py` Command-Line Flags

| Flag | Default | Description |
| :--- | :--- | :--- |
| `--watch <seconds>` | `0` (run once) | Re-run analysis continuously every N seconds (e.g., `--watch 300`) |
| `--min-trades <n>` | `100` | Minimum closed trades required before issuing a `PROMOTE`/`TWEAK`/`KILL` verdict |
| `--csv <path>` | Auto-detected MT5 Common path | Custom path to a trade CSV file |
| `--out <folder>` | `reports` | Output folder for HTML and text reports |

---

## Step 6: Reading Your Executive Scorecard & Iterating

Once closed trades accumulate, `analyze_bot.py` evaluates each `InpStrategyTag` using these criteria:

| Verdict | Trigger Condition | Recommended Action |
| :--- | :--- | :--- |
| **COLLECTING DATA** | Fewer than `100` closed trades | Let the bot keep running to reach statistical significance. |
| **PROMOTE** | Profit Factor $\ge 1.25$, positive expectancy, Win Rate $\ge 35\%$ | Strong candidate for extended forward testing or optimization. |
| **TWEAK** | Profit Factor between $1.00$ and $1.24$ | Marginal edge — inspect the **Pair** and **Server Hour** tables to cut losing pairs or off-hours. |
| **KILL** | Profit Factor $< 1.00$ after sample threshold | Negative expectancy — retire this setup and test the next technique. |

### How to Test a New Technique Without Losing Old Data
Whenever you change EMA periods, SL/TP ratios, timeframe, or code logic, simply update **`InpStrategyTag`** (for example, from `EMA_5_15_M1_v103` to `EMA_8_21_M5_v104`). Both strategies stay archived in `Prototype1_TradeLog.csv`, and the dashboard displays separate scorecards for each tag side by side.

---

## Input Parameters Reference (v1.03)

| Group | Parameter | Default | Description |
| :--- | :--- | :--- | :--- |
| **Strategy & Logging** | `InpStrategyTag` | `EMA_5_15_M1_v103` | Label saved to CSV for comparing strategies |
| | `InpLogFileName` | `Prototype1_TradeLog.csv` | Shared CSV filename in MT5 `Common\Files` |
| | `InpEnableCSVLog` | `true` | Automatically log closed trades to CSV |
| | `InpLogInTester` | `false` | Set `true` only if you want Strategy Tester runs written to the CSV |
| **Indicators** | `InpFastEMAPeriod` | `5` | Fast EMA period |
| | `InpSlowEMAPeriod` | `15` | Slow EMA period (must be greater than Fast EMA) |
| | `InpTimeframe` | `PERIOD_M1` | Timeframe used for EMA calculation & candle close |
| **Trade Settings** | `InpEnableBuy` | `true` | Open BUY on bullish crossover |
| | `InpEnableSell` | `true` | Open SELL on bearish crossover |
| | `InpLotSize` | `0.01` | Position size in lots (auto-normalized to broker limits) |
| | `InpStopLossPips` | `10` | Static Stop Loss distance in pips (`0` = disabled) |
| | `InpTakeProfitPips` | `15` | Static Take Profit distance in pips (`0` = disabled) |
| | `InpMaxSpreadPips` | `3.0` | Skips entries if live spread exceeds this value (`0` = disabled) |
| | `InpMagicNumber` | `20240501` | Unique ID per chart to isolate position tracking |
| | `InpMaxPositions` | `1` | Max simultaneous open positions per chart/magic number |
| **Hours & Days** | `InpStartHour` / `Minute` | `00:00` | Start of allowed trading window (**broker server time**) |
| | `InpEndHour` / `Minute` | `23:59` | End of allowed trading window (**broker server time**) |
| | `InpTradeMonday`–`Sunday` | Mon–Fri `true` | Enable/disable trading by server weekday |

---

## Keeping the Bot Running Unattended

MetaTrader 5 only executes trades while the desktop app is open and connected to the internet:
- Keep your computer plugged in and set Windows **Settings → System → Power & battery → Screen and sleep → Plugged in (sleep)** to **Never**.
- Pause automatic overnight Windows Update restarts if running multi-day tests.
- Keep both **MetaTrader 5** and the **PowerShell** watcher minimized (not closed).
- For 24/5 hands-off operation without keeping a laptop awake, consider running MT5 on a Windows VPS or MetaQuotes VPS.

---

## Fast Backtesting with MT5 Strategy Tester

Before running a new parameter set live for 24 hours, you can test it over months of historical data in minutes:
1. In MT5, press **Ctrl+R** to open the **Strategy Tester**.
2. Select **Single** test → Expert: `EMA_Crossover_EA` → Symbol: `EURUSD` → Period: `M1` (or `H1`).
3. Choose **Every tick based on real ticks** for realistic spread and SL/TP simulation.
4. Set your date range, deposit, and leverage, then click **Start**.

---

## Troubleshooting

- **EA doesn't appear under Expert Advisors:** Confirm `EMA_Crossover_EA.mq5` is inside `MQL5\Experts\`, compiled with 0 errors (`F7` in MetaEditor), and right-click **Expert Advisors → Refresh** in MT5.
- **Chart icon is gray instead of blue:** Confirm the top toolbar **Algo Trading** button is green **and** that **Allow Algo Trading** is checked under chart properties (`F7` → **Common** tab).
- **Dashboard says "Waiting for Trade Data":** Normal until at least one position **closes** (hits SL, TP, or manual close). Trades are logged upon closing, not opening.
- **Trading hours don't match your local clock:** `InpStartHour` and `InpEndHour` use your **broker's server time** (visible at the top of the **Market Watch** panel), not your local PC clock.
- **Non-Forex symbols (Gold, Crypto, Indices):** `PipSize()` assumes standard 3/5-digit Forex quotes. Adjust pip values and `InpMaxSpreadPips` carefully if experimenting outside standard FX pairs.
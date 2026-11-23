//+------------------------------------------------------------------+
//|                                          EMA_Crossover_EA.mq5    |
//|                    Bi-Directional EMA Crossover Expert Advisor    |
//|                    v1.03: BUY/SELL + Auto CSV Trade Backlogging   |
//+------------------------------------------------------------------+
#property copyright   "Prototype1.0 - EMA Crossover EA"
#property version     "1.03"
#property description "BUY/SELL EMA crossover EA with spread filter and automatic CSV trade backlogging."
#property strict

#include <Trade/Trade.mqh>

//--- Signal state enum
enum ENUM_CROSS_SIGNAL
{
   SIGNAL_NONE = 0,
   SIGNAL_BUY  = 1,
   SIGNAL_SELL = -1
};

//--- Input parameters
input group "=== Strategy & Logging ==="
input string            InpStrategyTag     = "EMA_5_15_M1_v103";       // Strategy Tag (for analytics)
input string            InpLogFileName     = "Prototype1_TradeLog.csv";// Shared CSV in Common\Files
input bool              InpEnableCSVLog    = true;                     // Enable Auto CSV Logging
input bool              InpLogInTester     = false;                    // Also Log Strategy Tester Trades

input group "=== Indicator Settings ==="
input int               InpFastEMAPeriod   = 5;                        // Fast EMA Period
input int               InpSlowEMAPeriod   = 15;                       // Slow EMA Period
input ENUM_TIMEFRAMES   InpTimeframe       = PERIOD_M1;                // Trading Timeframe

input group "=== Trade Settings ==="
input bool              InpEnableBuy       = true;                     // Enable BUY on Bullish Cross
input bool              InpEnableSell      = true;                     // Enable SELL on Bearish Cross
input double            InpLotSize         = 0.01;                     // Lot Size
input int               InpStopLossPips    = 10;                       // Stop Loss (pips)
input int               InpTakeProfitPips  = 15;                       // Take Profit (pips)
input double            InpMaxSpreadPips   = 3.0;                      // Max Spread Filter (pips, 0=off)
input long              InpMagicNumber     = 20240501;                 // Magic Number (unique per chart)
input int               InpMaxPositions    = 1;                        // Max Open Positions (this chart)

input group "=== Trading Hours (Server Time) ==="
input int               InpStartHour       = 0;                        // Trading Start Hour (0-23)
input int               InpStartMinute     = 0;                        // Trading Start Minute (0-59)
input int               InpEndHour         = 23;                       // Trading End Hour (0-23)
input int               InpEndMinute       = 59;                       // Trading End Minute (0-59)

input group "=== Weekday Switches ==="
input bool              InpTradeMonday     = true;                     // Trade Monday
input bool              InpTradeTuesday    = true;                     // Trade Tuesday
input bool              InpTradeWednesday  = true;                     // Trade Wednesday
input bool              InpTradeThursday   = true;                     // Trade Thursday
input bool              InpTradeFriday     = true;                     // Trade Friday
input bool              InpTradeSaturday   = false;                    // Trade Saturday
input bool              InpTradeSunday     = false;                    // Trade Sunday

//--- Global variables
CTrade            trade;
int               g_fastHandle          = INVALID_HANDLE;
int               g_slowHandle          = INVALID_HANDLE;
datetime          g_lastBarTime         = 0;
ENUM_CROSS_SIGNAL g_pendingSignal       = SIGNAL_NONE;
bool              g_tradePlacedForCross = false;
double            g_lastEntrySpreadPips = 0.0;
string            g_cleanTag            = "";

//+------------------------------------------------------------------+
//| Expert initialization function                                    |
//+------------------------------------------------------------------+
int OnInit()
{
   //--- Validate input parameters
   if(InpFastEMAPeriod <= 0 || InpSlowEMAPeriod <= 0)
   {
      Print("ERROR: EMA periods must be greater than zero.");
      return INIT_PARAMETERS_INCORRECT;
   }
   if(InpFastEMAPeriod >= InpSlowEMAPeriod)
   {
      Print("ERROR: Fast EMA period must be less than Slow EMA period.");
      return INIT_PARAMETERS_INCORRECT;
   }
   if(!InpEnableBuy && !InpEnableSell)
   {
      Print("ERROR: Enable at least one direction (BUY or SELL).");
      return INIT_PARAMETERS_INCORRECT;
   }
   if(InpLotSize <= 0.0)
   {
      Print("ERROR

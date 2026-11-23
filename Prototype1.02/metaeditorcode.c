//+------------------------------------------------------------------+
//|                                          EMA_Crossover_EA.mq5    |
//|                        Bullish EMA Crossover Expert Advisor       |
//|                        Fast EMA 40 / Slow EMA 200 (configurable)  |
//+------------------------------------------------------------------+
#property copyright   "EMA Crossover EA"
#property version     "1.02"
#property description "Bullish EMA crossover EA. Opens BUY when fast EMA crosses above slow EMA."
#property strict

#include <Trade/Trade.mqh>

//--- Input parameters
input group "=== Indicator Settings ==="
input int               InpFastEMAPeriod   = 40;          // Fast EMA Period
input int               InpSlowEMAPeriod   = 200;         // Slow EMA Period
input ENUM_TIMEFRAMES   InpTimeframe       = PERIOD_H1;   // Trading Timeframe

input group "=== Trade Settings ==="
input double            InpLotSize         = 0.50;        // Lot Size
input int               InpStopLossPips    = 20;          // Stop Loss (pips)
input int               InpTakeProfitPips  = 40;          // Take Profit (pips)
input long              InpMagicNumber     = 20240501;    // Magic Number
input int               InpMaxPositions    = 1;           // Maximum Open Positions

input group "=== Trading Hours (Server Time) ==="
input int               InpStartHour       = 0;           // Trading Start Hour (0-23)
input int               InpStartMinute     = 0;           // Trading Start Minute (0-59)
input int               InpEndHour         = 23;          // Trading End Hour (0-23)
input int               InpEndMinute       = 59;          // Trading End Minute (0-59)

input group "=== Weekday Switches ==="
input bool              InpTradeMonday    = true;         // Trade Monday
input bool              InpTradeTuesday   = true;         // Trade Tuesday
input bool              InpTradeWednesday = true;         // Trade Wednesday
input bool              InpTradeThursday  = true;         // Trade Thursday
input bool              InpTradeFriday    = true;         // Trade Friday
input bool              InpTradeSaturday  = false;        // Trade Saturday
input bool              InpTradeSunday    = false;        // Trade Sunday

//--- Global variables
CTrade   trade;
int      g_fastHandle = INVALID_HANDLE;
int      g_slowHandle = INVALID_HANDLE;
datetime g_lastBarTime = 0;
bool     g_bullishCrossDetected = false;   // armed after a confirmed cross
bool     g_tradePlacedForCross  = false;   // prevents duplicate entry per cross

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
   if(InpLotSize <= 0.0)
   {
      Print("ERROR: Lot size must be greater than zero.");
      return INIT_PARAMETERS_INCORRECT;
   }
   if(InpStopLossPips < 0 || InpTakeProfitPips < 0)
   {
      Print("ERROR: SL/TP pips cannot be negative.");
      return INIT_PARAMETERS_INCORRECT;
   }
   if(InpMaxPositions < 1)
   {
      Print("ERROR: Maximum positions must be at least 1.");
      return INIT_PARAMETERS_INCORRECT;
   }
   if(InpStartHour < 0 || InpStartHour > 23 || InpEndHour < 0 || InpEndHour > 23 ||
      InpStartMinute < 0 || InpStartMinute > 59 || InpEndMinute < 0 || InpEndMinute > 59)
   {
      Print("ERROR: Invalid trading hour/minute inputs.");
      return INIT_PARAMETERS_INCORRECT;
   }

   //--- Create indicator handles
   g_fastHandle = iMA(_Symbol, InpTimeframe, InpFastEMAPeriod, 0, MODE_EMA, PRICE_CLOSE);
   g_slowHandle = iMA(_Symbol, InpTimeframe, InpSlowEMAPeriod, 0, MODE_EMA, PRICE_CLOSE);

   if(g_fastHandle == INVALID_HANDLE || g_slowHandle == INVALID_HANDLE)
   {
      Print("ERROR: Failed to create EMA indicator handles. Error: ", GetLastError());
      return INIT_FAILED;
   }

   //--- Configure CTrade object
   trade.SetExpertMagicNumber((ulong)InpMagicNumber);
   trade.SetDeviationInPoints(10);
   trade.SetTypeFillingBySymbol(_Symbol);
   trade.SetAsyncMode(false);
   trade.LogLevel(LOG_LEVEL_ERRORS);

   //--- Initialize the last-bar tracker so the very first tick doesn't
   //    treat an old completed bar as a new one.
   g_lastBarTime = (datetime)SeriesInfoInteger(_Symbol, InpTimeframe, SERIES_LASTBAR_DATE);

   if(!TerminalInfoInteger(TERMINAL_TRADE_ALLOWED) || !MQLInfoInteger(MQL_TRADE_ALLOWED))
      Print("WARNING: Algo Trading is currently disabled. Enable it in MT5 before expecting orders.");

   Print("EMA Crossover EA v1.02 initialized. Symbol=", _Symbol,
         " Fast=", InpFastEMAPeriod, " Slow=", InpSlowEMAPeriod,
         " TF=", EnumToString(InpTimeframe), " Magic=", InpMagicNumber);

   return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                  |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
   if(g_fastHandle != INVALID_HANDLE)
      IndicatorRelease(g_fastHandle);
   if(g_slowHandle != INVALID_HANDLE)
      IndicatorRelease(g_slowHandle);

   Print("EMA Crossover EA deinitialized. Reason=", reason);
}

//+------------------------------------------------------------------+
//| Expert tick function                                              |
//+------------------------------------------------------------------+
void OnTick()
{
   //--- 1. Work only once per new completed bar
   datetime currentBarTime = (datetime)SeriesInfoInteger(_Symbol, InpTimeframe, SERIES_LASTBAR_DATE);
   if(currentBarTime == 0 || currentBarTime == g_lastBarTime)
      return;   // History not ready or no new bar yet

   //--- 2. Refresh crossover state; retry on next tick if buffers aren't ready
   if(!UpdateCrossoverState())
      return;

   //--- 3. Evaluate entry conditions
   if(g_bullishCrossDetected && !g_tradePlacedForCross)
   {
      if(!IsWithinTradingHours())
      {
         Print("Crossover skipped: outside configured trading hours (server time).");
      }
      else if(!IsWeekdayEnabled())
      {
         Print("Crossover skipped: current weekday is disabled.");
      }
      else if(CountOpenPositions() >= InpMaxPositions || !HasNoOpenPosition())
      {
         Print("Crossover skipped: maximum open positions already reached.");
      }
      else
      {
         if(TryOpenBuy())
            g_tradePlacedForCross = true;
      }
   }

   //--- Remember which bar we processed
   g_lastBarTime = currentBarTime;
}

//+------------------------------------------------------------------+
//| Detect bullish crossover using completed candles (shift 1 and 2)  |
//+------------------------------------------------------------------+
bool UpdateCrossoverState()
{
   double fast[2], slow[2];

   //--- Copy the two most recent COMPLETED bars (shift 1 and shift 2).
   //    Static arrays in MQL5 are non-series (oldest -> newest):
   //    index 0 = shift 2 (older completed bar)
   //    index 1 =

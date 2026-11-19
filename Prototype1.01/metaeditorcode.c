#property copyright   "EMA Crossover EA"
#property version     "1.00"
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

   Print("EMA Crossover EA initialized. Fast=", InpFastEMAPeriod,
         " Slow=", InpSlowEMAPeriod, " TF=", EnumToString(InpTimeframe),
         " Magic=", InpMagicNumber);

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
   if(currentBarTime == g_lastBarTime)
      return;   // No new bar yet

   //--- A new bar has opened; the previous bar (index 1) is now completed.
   //    We retain the old bar time to detect the next bar.
   //--- Update stored time AFTER processing so we only run once per bar.
   //    (We process, then set g_lastBarTime = currentBarTime.)

   //--- 2. Refresh crossover state from completed bars
   UpdateCrossoverState();

   //--- 3. Evaluate entry conditions
   if(g_bullishCrossDetected && !g_tradePlacedForCross)
   {
      if(IsWithinTradingHours() && IsWeekdayEnabled() &&
         CountOpenPositions() < InpMaxPositions &&
         HasNoOpenPosition())
      {
         if(TryOpenBuy())
         {
            //--- Mark that this crossover has been traded so we don't
            //    re-enter on subsequent ticks until a new cross occurs.
            g_tradePlacedForCross = true;
         }
      }
   }

   //--- Remember which bar we processed
   g_lastBarTime = currentBarTime;
}

//+------------------------------------------------------------------+
//| Detect bullish crossover using completed candles (shift 1 and 2)  |
//+------------------------------------------------------------------+
void UpdateCrossoverState()
{
   double fast[2], slow[2];

   //--- Copy the two most recent COMPLETED bars.
   //    Index 0 = bar at shift 1 (last completed), index 1 = bar at shift 2.
   if(CopyBuffer(g_fastHandle, 0, 1, 2, fast) < 2)
   {
      Print("CopyBuffer fast EMA failed. Error: ", GetLastError());
      return;
   }
   if(CopyBuffer(g_slowHandle, 0, 1, 2, slow) < 2)
   {
      Print("CopyBuffer slow EMA failed. Error: ", GetLastError());
      return;
   }

   //--- Determine if a bullish crossover happened on the most recent
   //    completed bar (index 0 = shift 1).
   //    Bullish cross: previous bar fast <= slow  AND  current bar fast > slow.
bool bullishCross = (fast[0] <= slow[0]) && (fast[1] > slow[1]);

   if(bullishCross)
   {
      g_bullishCrossDetected = true;
      g_tradePlacedForCross  = false;   // Arm a fresh trade for this cross
      Print("Bullish EMA crossover detected on completed bar.");
   }
   else
   {
      //--- No new cross. If we had already traded the previous cross,
      //    keep the flag set until a new cross resets it.
      //    g_bullishCrossDetected remains true if it was true; we simply
      //    don't re-arm. This is intentional: only a NEW cross re-arms.
   }
}

//+------------------------------------------------------------------+
//| Return true if trading hours allow entry (broker server time)     |
//+------------------------------------------------------------------+
bool IsWithinTradingHours()
{
   MqlDateTime dt;
   TimeToStruct(TimeCurrent(), dt);

   int nowMinutes   = dt.hour * 60 + dt.min;
   int startMinutes = InpStartHour * 60 + InpStartMinute;
   int endMinutes   = InpEndHour   * 60 + InpEndMinute;

   if(startMinutes <= endMinutes)
      return (nowMinutes >= startMinutes && nowMinutes <= endMinutes);
   else
      // Overnight session (e.g. 22:00 -> 02:00)
      return (nowMinutes >= startMinutes || nowMinutes <= endMinutes);
}

//+------------------------------------------------------------------+
//| Return true if current weekday is enabled                         |
//+------------------------------------------------------------------+
bool IsWeekdayEnabled()
{
   MqlDateTime dt;
   TimeToStruct(TimeCurrent(), dt);

   switch(dt.day_of_week)
   {
      case 0: return InpTradeSunday;
      case 1: return InpTradeMonday;
      case 2: return InpTradeTuesday;
      case 3: return InpTradeWednesday;
      case 4: return InpTradeThursday;
      case 5: return InpTradeFriday;
      case 6: return InpTradeSaturday;
   }
   return false;
}

//+------------------------------------------------------------------+
//| Count open positions for this symbol & magic number               |
//+------------------------------------------------------------------+
int CountOpenPositions()
{
   int count = 0;
   for(int i = PositionsTotal() - 1; i >= 0; i--)
   {
      ulong ticket = PositionGetTicket(i);
      if(ticket == 0)
         continue;
      if(!PositionSelectByTicket(ticket))
         continue;
      if(PositionGetString(POSITION_SYMBOL) != _Symbol)
         continue;
      if((long)PositionGetInteger(POSITION_MAGIC) != InpMagicNumber)
         continue;
      count++;
   }
   return count;
}

//+------------------------------------------------------------------+
//| Return true if there is already an open position for this EA      |
//+------------------------------------------------------------------+
bool HasNoOpenPosition()
{
   return (CountOpenPositions() == 0);
}

//+------------------------------------------------------------------+
//| Convert "pips" to price distance, respecting 3/5 digit quotes     |
//+------------------------------------------------------------------+
double PipsToPrice(double pips)
{
   double point    = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
   int    digits   = (int)SymbolInfoInteger(_Symbol, SYMBOL_DIGITS);

   //--- For 3- and 5-digit symbols a "pip" is 10 points;
   //    otherwise 1 pip = 1 point (e.g., JPY pairs with 2/3 digits).
   double pipSize = (digits == 3 || digits == 5) ? point * 10.0 : point;

   return pips * pipSize;
}

//+------------------------------------------------------------------+
//| Validate and normalize a lot size against broker constraints      |
//+------------------------------------------------------------------+
double NormalizeLot(double lot)
{
   double minLot  = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
   double maxLot  = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
   double stepLot = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);

   if(stepLot <= 0.0)
      stepLot = 0.01;

   //--- Round to step
   lot = MathRound(lot / stepLot) * stepLot;
   lot = MathMax(lot, minLot);
   lot = MathMin(lot, maxLot);

   //--- Normalize decimals to avoid floating point artifacts
   int stepDigits = 0;
   double tmp = stepLot;
   while(tmp < 1.0 && stepDigits < 8)
   {
      tmp *= 10.0;
      stepDigits++;
      if(MathAbs(tmp - MathRound(tmp)) < 1e-9)
         break;
   }
   lot = NormalizeDouble(lot, stepDigits);
   return lot;
}

//+------------------------------------------------------------------+
//| Validate SL/TP distance against broker stops level                |
//+------------------------------------------------------------------+
bool ValidateStopDistance(double distance, string label)
{
   if(distance <= 0.0)
      return true;   // zero means "no SL/TP", which is allowed

   double stopsLevel = (double)SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL);
   double point      = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
   double minDist    = stopsLevel * point;

   //--- Some brokers report 0; use a small fallback based on spread
   if(minDist <= 0.0)
   {
      double spread = (double)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD) * point;
      minDist = spread * 1.5;
   }

   if(distance < minDist)
   {
      Print("WARNING: ", label, " distance ", DoubleToString(distance, _Digits),
            " is below broker minimum ", DoubleToString(minDist, _Digits), ". Trade skipped.");
      return false;
   }
   return true;
}

//+------------------------------------------------------------------+
//| Attempt to open a BUY position                                    |
//+------------------------------------------------------------------+
bool TryOpenBuy()
{
   //--- Refresh tick data
   MqlTick tick;
   if(!SymbolInfoTick(_Symbol, tick))
   {
      Print("SymbolInfoTick failed. Error: ", GetLastError());
      return false;
   }

   double ask = tick.ask;
   double bid = tick.bid;
   if(ask <= 0.0 || bid <= 0.0)
   {
      Print("Invalid tick prices (ask=", ask, " bid=", bid, "). Trade skipped.");
      return false;
   }

   //--- Compute SL/TP distances from pips
   double slDistance = PipsToPrice(InpStopLossPips);
   double tpDistance = PipsToPrice(InpTakeProfitPips);

   //--- Validate against broker minimum stops level
   if(slDistance > 0.0 && !ValidateStopDistance(slDistance, "SL"))
      return false;
   if(tpDistance > 0.0 && !ValidateStopDistance(tpDistance, "TP"))
      return false;

   //--- Normalize lot
   double lot = NormalizeLot(InpLotSize);
   if(lot <= 0.0)
   {
      Print("Normalized lot size is zero. Trade skipped.");
      return false;
   }

   //--- Check free margin (rough check; broker will also enforce)
   double marginRequired = 0.0;
   if(!OrderCalcMargin(ORDER_TYPE_BUY, _Symbol, lot, ask, marginRequired))
   {
      Print("OrderCalcMargin failed. Error: ", GetLastError());
      return false;
   }
   double freeMargin = AccountInfoDouble(ACCOUNT_MARGIN_FREE);
   if(marginRequired > freeMargin)
   {
      Print("Insufficient margin. Required=", marginRequired, " Free=", freeMargin);
      return false;
   }

   //--- Compute SL/TP relative to the actual entry price (ask for BUY)
   double sl = 0.0, tp = 0.0;
   if(slDistance > 0.0)
      sl = NormalizeDouble(ask - slDistance, _Digits);
   if(tpDistance > 0.0)
      tp = NormalizeDouble(ask + tpDistance, _Digits);

   //--- Normalize SL/TP once more using tick size
   double tickSize = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
   if(tickSize > 0.0)
   {
      if(sl > 0.0) sl = NormalizeDouble(MathRound(sl / tickSize) * tickSize, _Digits);
      if(tp > 0.0) tp = NormalizeDouble(MathRound(tp / tickSize) * tickSize, _Digits);
   }

   //--- Final sanity: SL must be below entry, TP above entry for a BUY
   if(sl > 0.0 && sl >= ask)
   {
      Print("SL is not below entry price. Trade skipped.");
      return false;
   }
   if(tp > 0.0 && tp <= ask)
   {
      Print("TP is not above entry price. Trade skipped.");
      return false;
   }

   //--- Execute the trade
   string comment = "EMA Cross BUY";
   bool ok = trade.Buy(lot, _Symbol, 0.0, sl, tp, comment);

   if(!ok)
   {
      uint retCode = trade.ResultRetcode();
      Print("Buy order failed. Retcode=", retCode,
            " (", trade.ResultRetcodeDescription(), ") LastError=", GetLastError());
      return false;
   }

   Print("BUY opened successfully. Ticket=", trade.ResultOrder(),
         " Price=", trade.ResultPrice(),
         " Lot=", lot, " SL=", sl, " TP=", tp);
   return true;
}

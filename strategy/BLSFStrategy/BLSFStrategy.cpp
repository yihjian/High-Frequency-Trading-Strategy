/*================================================================================                               
*     Source: ../RCM/StrategyStudio/examples/strategies/SimpleMomentumStrategy/SimpleMomentumStrategy.cpp                                                        
*     Last Update: 2013/6/1 13:55:14                                                                            
*     Contents:                                     
*     Distribution:          
*                                                                                                                
*                                                                                                                
*     Copyright (c) RCM-X, 2011 - 2013.                                                  
*     All rights reserved.                                                                                       
*                                                                                                                
*     This software is part of Licensed material, which is the property of RCM-X ("Company"), 
*     and constitutes Confidential Information of the Company.                                                  
*     Unauthorized use, modification, duplication or distribution is strictly prohibited by Federal law.         
*     No title to or ownership of this software is hereby transferred.                                          
*                                                                                                                
*     The software is provided "as is", and in no event shall the Company or any of its affiliates or successors be liable for any 
*     damages, including any lost profits or other incidental or consequential damages relating to the use of this software.       
*     The Company makes no representations or warranties, express or implied, with regards to this software.                        
/*================================================================================*/   


// cp BLSFStrategy.so ~/Desktop/strategy_studio/backtesting/strategies_dlls/
// create_instance aaaT1 BLSFStrategy UIUC SIM-1001-101 dlariviere 10000000 -symbols SPY
// create_instance WW2 SimpleTrade UIUC SIM-1001-101 dlariviere 10000000 -symbols SPY
// create_instance STY1 SimpleTrade UIUC SIM-1001-101 dlariviere 10000000 -symbols SPY


//  cp BLSFStrategy.so ~/Desktop/strategy_studio/backtesting/strategies_dlls/
// create_instance AL1123 BLSFStrategy UIUC SIM-1001-101 dlariviere 10000000 -symbols SPY
// start_backtest 2021-06-01 2021-06-01 AL1123 0
// start_backtest 2019-10-30 2019-10-30 ALPACA6 1
// export_cra_file backtesting-results/BACK_AL1123_2022-05-04_220943_start_06-01-2021_end_06-01-2021.cra backtesting-cra-exports

#ifdef _WIN32
    #include "stdafx.h"
#endif

#include "BLSFStrategy.h"

#include "FillInfo.h"
#include "AllEventMsg.h"
#include "ExecutionTypes.h"
#include <Utilities/Cast.h>
#include <Utilities/utils.h>

#include <math.h>
#include <iostream>
#include <cassert>
#include <ctime>

using namespace RCM::StrategyStudio;
using namespace RCM::StrategyStudio::MarketModels;
using namespace RCM::StrategyStudio::Utilities;

using namespace std;

BLSFStrategy::BLSFStrategy(StrategyID strategyID, const std::string& strategyName, const std::string& groupName):
    Strategy(strategyID, strategyName, groupName),
    m_momentum_map(),
    m_instrument_order_id_map(),
    m_momentum(0),
    m_aggressiveness(0),
    m_position_size(100),
    m_debug_on(true),
    m_short_window_size(10),
    m_long_window_size(30),
    currentState(BUY),
    currentDate(0)
{
}

BLSFStrategy::~BLSFStrategy()
{
}

void BLSFStrategy::OnResetStrategyState()
{
    m_momentum_map.clear();
    m_instrument_order_id_map.clear();
    m_momentum = 0;
}

void BLSFStrategy::DefineStrategyParams()
{
    CreateStrategyParamArgs arg1("aggressiveness", STRATEGY_PARAM_TYPE_RUNTIME, VALUE_TYPE_DOUBLE, m_aggressiveness);
    params().CreateParam(arg1);

    CreateStrategyParamArgs arg2("position_size", STRATEGY_PARAM_TYPE_RUNTIME, VALUE_TYPE_INT, m_position_size);
    params().CreateParam(arg2);

    CreateStrategyParamArgs arg3("short_window_size", STRATEGY_PARAM_TYPE_STARTUP, VALUE_TYPE_INT, m_short_window_size);
    params().CreateParam(arg3);
    
    CreateStrategyParamArgs arg4("long_window_size", STRATEGY_PARAM_TYPE_STARTUP, VALUE_TYPE_INT, m_long_window_size);
    params().CreateParam(arg4);
    
    CreateStrategyParamArgs arg5("debug", STRATEGY_PARAM_TYPE_RUNTIME, VALUE_TYPE_BOOL, m_debug_on);
    params().CreateParam(arg5);
}

void BLSFStrategy::DefineStrategyCommands()
{
    StrategyCommand command1(1, "Reprice Existing Orders");
    commands().AddCommand(command1);

    StrategyCommand command2(2, "Cancel All Orders");
    commands().AddCommand(command2);
}

void BLSFStrategy::RegisterForStrategyEvents(StrategyEventRegister* eventRegister, DateType currDate)
{    
    for (SymbolSetConstIter it = symbols_begin(); it != symbols_end(); ++it) {
        eventRegister->RegisterForBars(*it, BAR_TYPE_TIME, 10);
    }
}
void BLSFStrategy::OnTrade(const TradeDataEventMsg& msg)
{

    date msg_date = msg.adapter_time().date();
    // Sell at the beginning of the day
    if (currentState == SELL && msg_date != currentDate) {
        currentDate = msg_date;
        std::cout << "OnTrade(): (" << msg.adapter_time() << "): " << msg.instrument().symbol() << ": " << msg.trade().size() << " @ $" << msg.trade().price() << std::endl;
        // Sell
        this->SendSimpleOrder(&msg.instrument(), -1); //sell one share every time there is a trade
        currentState = BUY;
    }
    // Buy at the end of the day
    else {
        if(currentDate == date(0)) {
            currentDate = msg_date;
        }
        tm date_tm = to_tm(msg.adapter_time());
        if(currentState == BUY && date_tm.tm_hour == 19 && date_tm.tm_min >= 59) {
            std::cout << "OnTrade(): (" << msg.adapter_time() << "): " << msg.instrument().symbol() << ": " << msg.trade().size() << " @ $" << msg.trade().price() << std::endl;
            // Buy
            this->SendSimpleOrder(&msg.instrument(), 1);
            currentState = SELL;
        }
    }	
}

void OnTopQuote(const QuoteEventMsg& msg) {
    cout << "On top quote: !!" << endl;
}

void BLSFStrategy::OnQuote(const QuoteEventMsg & msg) {
    cout << "On quote" << endl;
}

void BLSFStrategy::OnDepth(const MarketDepthEventMsg& msg) {
    cout << "On depth" << endl;
}


void BLSFStrategy::OnBar(const BarEventMsg& msg)
{
    // cout << msg.instrument().top_quote() << endl;
    // cout << msg << endl;
    date msg_date = msg.bar_time().date();
    if (currentState == SELL && msg_date != currentDate) {
        tm date_tm = to_tm(msg.bar_time());
        // ensure the market begin time 
        // TODO check the time zone
        if(currentState == BUY && date_tm.tm_hour < 13) {
            return;
        }
        if (date_tm.tm_hour == 13 && date_tm.tm_min < 30) {
            return;
        } 
        currentDate = msg_date;
        std::cout << "Sending BAR order: (" << msg.bar_time() << "): " << msg.instrument().symbol() << std::endl;
        // TODO Change the order size
        this->SendOrder(&msg.instrument(), -1); //sell one share every time there is a trade
        currentState = BUY;
    }
    // Buy at the end of the day
    else {
        if(currentDate == date(0)) {
            currentDate = msg_date;
        }
        tm date_tm = to_tm(msg.bar_time());
        if(currentState == BUY && date_tm.tm_hour == 19 && date_tm.tm_min >= 59) {
            std::cout << "Sending BAR order: (" << msg.bar_time() << "): " << msg.instrument().symbol() << std::endl;
            // Buy
            // TODO Change the order size
            this->SendOrder(&msg.instrument(), 1);
            currentState = SELL;
        }
    }	
}

void BLSFStrategy::OnOrderUpdate(const OrderUpdateEventMsg& msg)
{    
	std::cout << "OnOrderUpdate(): " << msg.update_time() << " " << msg.name() << std::endl;
    if(msg.completes_order())
    {
		m_instrument_order_id_map[msg.order().instrument()] = 0;
		std::cout << "OnOrderUpdate(): order is complete; " << std::endl;
    }
}

void BLSFStrategy::AdjustPortfolio(const Instrument* instrument, int desired_position)
{
    int trade_size = desired_position - portfolio().position(instrument);

    if (trade_size != 0) {
        OrderID order_id = m_instrument_order_id_map[instrument];
        //if we're not working an order for the instrument already, place a new order
        if (order_id == 0) {
            SendOrder(instrument, trade_size);
        } else {  
		    //otherwise find the order and cancel it if we're now trying to trade in the other direction
            const Order* order = orders().find_working(order_id);
            if(order && ((IsBuySide(order->order_side()) && trade_size < 0) || 
			            ((IsSellSide(order->order_side()) && trade_size > 0)))) {
                trade_actions()->SendCancelOrder(order_id);
                //we're avoiding sending out a new order for the other side immediately to simplify the logic to the case where we're only tracking one order per instrument at any given time
            }
        }
    }
}

void BLSFStrategy::SendSimpleOrder(const Instrument* instrument, int trade_size)
{

	//this is simple check to avoid placing orders before the order book is actually fully initialized
	//side note: if trading certain futures contracts for example, the price of an instrument actually can be zero or even negative. here we assume cash US equities so price > 0
    
	// if(instrument->top_quote().ask()<.01 || instrument->top_quote().bid()<.01 || !instrument->top_quote().ask_side().IsValid() || !instrument->top_quote().ask_side().IsValid()) {
    //     std::stringstream ss;
    //     ss << "SendSimpleOrder(): Sending buy order for " << instrument->symbol() << " at price " << instrument->top_quote().ask() << " and quantity " << trade_size <<" with missing quote data";
    //     logger().LogToClient(LOGLEVEL_DEBUG, ss.str());
    //     std::cout << "SendSimpleOrder(): " << ss.str() << std::endl;
    //     return;
    //  }

    m_aggressiveness = 0.02; //send order two pennies more aggressive than BBO
    double last_trade_price = instrument->last_trade().price();
    double price = trade_size > 0 ? last_trade_price + m_aggressiveness : last_trade_price - m_aggressiveness;

    OrderParams params(*instrument,
        abs(trade_size),
        price,
        (instrument->type() == INSTRUMENT_TYPE_EQUITY) ? MARKET_CENTER_ID_IEX : ((instrument->type() == INSTRUMENT_TYPE_OPTION) ? MARKET_CENTER_ID_CBOE_OPTIONS : MARKET_CENTER_ID_CME_GLOBEX),
        (trade_size>0) ? ORDER_SIDE_BUY : ORDER_SIDE_SELL,
        ORDER_TIF_DAY,
        ORDER_TYPE_LIMIT);

    std::cout << "SendSimpleOrder(): about to send new order for " << trade_size << " at $" << price << std::endl;
    TradeActionResult tra = trade_actions()->SendNewOrder(params);
    if (tra == TRADE_ACTION_RESULT_SUCCESSFUL) {
        m_instrument_order_id_map[instrument] = params.order_id;
        std::cout << "SendOrder(): Sending new order successful!" << std::endl;
    }
    else
    {
    	std::cout << "SendOrder(): Error sending new order!!!" << tra << std::endl;
    }

}


void BLSFStrategy::SendOrder(const Instrument* instrument, int trade_size)
{
    if(instrument->top_quote().ask()<.01 || instrument->top_quote().bid()<.01 || !instrument->top_quote().ask_side().IsValid() || !instrument->top_quote().ask_side().IsValid()) {
        std::stringstream ss;
        // cout << "Top quote: " << instrument->top_quote().ask() << endl;
        // cout << "Top bid: " << instrument->top_quote().bid() << endl;
        ss << "Sending buy order for " << instrument->symbol() << " at price " << instrument->top_quote().ask() << " and quantity " << trade_size <<" with missing quote data";   
        logger().LogToClient(LOGLEVEL_DEBUG, ss.str());
        std::cout << "SendOrder(): " << ss.str() << std::endl;
        return;
    }

    double price = trade_size > 0 ? instrument->top_quote().bid() + m_aggressiveness : instrument->top_quote().ask() - m_aggressiveness;

    OrderParams params(*instrument, 
        abs(trade_size),
        price, 
        (instrument->type() == INSTRUMENT_TYPE_EQUITY) ? MARKET_CENTER_ID_NASDAQ : ((instrument->type() == INSTRUMENT_TYPE_OPTION) ? MARKET_CENTER_ID_CBOE_OPTIONS : MARKET_CENTER_ID_CME_GLOBEX),
        (trade_size>0) ? ORDER_SIDE_BUY : ORDER_SIDE_SELL,
        ORDER_TIF_DAY,
        ORDER_TYPE_LIMIT);

    if (trade_actions()->SendNewOrder(params) == TRADE_ACTION_RESULT_SUCCESSFUL) {
        m_instrument_order_id_map[instrument] = params.order_id;
    }
}

void BLSFStrategy::RepriceAll()
{
    for (IOrderTracker::WorkingOrdersConstIter ordit = orders().working_orders_begin(); ordit != orders().working_orders_end(); ++ordit) {
        Reprice(*ordit);
    }
}

void BLSFStrategy::Reprice(Order* order)
{
    OrderParams params = order->params();
    params.price = (order->order_side() == ORDER_SIDE_BUY) ? order->instrument()->top_quote().bid() + m_aggressiveness : order->instrument()->top_quote().ask() - m_aggressiveness;
    trade_actions()->SendCancelReplaceOrder(order->order_id(), params);
}

void BLSFStrategy::OnStrategyCommand(const StrategyCommandEventMsg& msg)
{
    switch (msg.command_id()) {
        case 1:
            RepriceAll();
            break;
        case 2:
            trade_actions()->SendCancelAll();
            break;
        default:
            logger().LogToClient(LOGLEVEL_DEBUG, "Unknown strategy command received");
            break;
    }
}

void BLSFStrategy::OnParamChanged(StrategyParam& param)
{    
	/*
    if (param.param_name() == "aggressiveness") {                         
        if (!param.Get(&m_aggressiveness))
            throw StrategyStudioException("Could not get m_aggressiveness");
    } else if (param.param_name() == "position_size") {
        if (!param.Get(&m_position_size))
            throw StrategyStudioException("Could not get position size");
    } else if (param.param_name() == "short_window_size") {
        if (!param.Get(&m_short_window_size))
            throw StrategyStudioException("Could not get trade size");
    } else if (param.param_name() == "long_window_size") {
        if (!param.Get(&m_long_window_size))
            throw StrategyStudioException("Could not get long_window_size");
    } else if (param.param_name() == "debug") {
        if (!param.Get(&m_debug_on))
            throw StrategyStudioException("Could not get trade size");
    } 
    */
}

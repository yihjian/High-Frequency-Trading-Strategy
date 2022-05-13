'''
Strategy class that contains information about each strategy
'''
import os
import sys
import pandas as pd
import numpy as np
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots

class StrategyAnalysis:
    """
    Strategy Object class
    ...

    Attributes
    ----------
    fill: Pandas Dataframe
        Strategy Fill
    order: Pandas Dataframe
        Order Fill
    pnl: Pandas Dataframe
        Profit and Lost
    name: str
        Name of the Strategy
    symbol: str
        Symbol of the given Strategy
    begin_time: str
        Begin time of the Strategy
    end_time: str
        End time of the Strategy
    initial_value: int
        Initial value that the strategy started with

    Methods
    -------
    measure_strategy(print_value)
        measure the current strategy

    """
    # @param fill_file: fill file generated by Strategy Studio Backtesting
    # @param order_file: Order file generated by Strategy Studio Backtesting
    # @param pnl_file: PnL file generated by Strategy Studio Backtesting
    def __init__(self, fill_file, order_file, pnl_file, initial_value=10000000):
        # Filepath check
        def check_path(file_path):
            """
            Valiate that the path is a valid path

            Parameters
            ----------
                types : file_path
                    path to the file we want to open
            """
            if not os.path.exists(file_path):
                print("Invalid file path!")
                sys.exit()

        check_path(fill_file)
        check_path(order_file)
        check_path(pnl_file)
        # read these files into Dataframe
        try:
            fill = pd.read_csv(fill_file)
            cols = ["StrategyName", "Symbol","TradeTime", "Price", "Quantity", "ExecutionCost"]
            assert all(col in fill.columns for col in cols)
            self.fill = fill[cols]
        except Exception as exc:
            raise Exception('Invalid fill file ' + fill_file) from exc
        try:
            order = pd.read_csv(order_file)
            cols = [
                "StrategyName",
                "LastModTime",
                "State",
                "Symbol",
                "Price",
                "Quantity",
                "DisplayQuantity",
                "FilledQty",
                "Remains",
                "AvgFillPrice",
                "ExecutionCost",
                "MarketCenter"
            ]
            assert all(col in order.columns for col in cols)
            self.order = order[cols]
        except Exception as exc:
            raise Exception('Invalid order file ' + order_file) from exc
        try:
            self.pnl = pd.read_csv(pnl_file)
        except Exception as exc:
            raise Exception('Invalid pnl file ' + pnl_file) from exc


        self.name = None
        if len(self.fill) > 0:
            symbol = self.fill["Symbol"][0]
            name = self.fill["StrategyName"][0]
            self.name = f"{name}_{symbol}"
            self.pnl["Name"] = f"{name}_{symbol}"

        self.begin_time = self.fill['TradeTime'][0]
        self.end_time = self.fill['TradeTime'][len(self.fill) - 1]
        self.initial_value = initial_value
        self.ticks = None

    def read_ticks(self, tick_files):
        ticks_df = []
        for file in tick_files:
            tick = pd.read_csv(file)
            ticks_df.append(tick)
        ticks_df = pd.concat(ticks_df)
        ticks_time_price = ticks_df[["timestamp", "price"]]
        time_price = ticks_time_price.to_numpy()
        time_price = time_price[time_price[:, 0].argsort()]

        self.date_price_full = time_price

        time_ = time_price[:, 0]
        price_ = time_price[:, 1]

        date_label = [time_[0].split(' ')[0]]
        price_by_date = []
        temp = [price_[0]]
        for i in range(1, time_price.shape[0]):
                date = time_[i].split(' ')[0]
                if date == date_label[-1]:
                        temp.append(float(price_[i]))
                else:
                        date_label.append(date)
                        price_by_date.append(np.array(temp))
                        temp = [price_[i]]
        # date
        date_label = np.array(date_label)
        self.date_label = date_label
        # each date corresponds to an array of price belonging to that day
        price_by_date.append(np.array(temp))
        date_price_dict = {
            date: {
                "open": price_by_date[i][0],
                "high": max(price_by_date[i]),
                "low": min(price_by_date[i]),
                "close": price_by_date[i][-1]
            } for i, date in enumerate(date_label)
        }
        self.date_price_dict = date_price_dict

    def measure_strategy(self, print_value = False):
        """
        Measure the current strategy with several evaluation parameters

        Parameters
        ----------
            print_value: bool
                Flag to print the values

        Returns
        ----------
            list:
                list of the values
                1. Initial Investment Value
                2. Final Investment Value
                3. Begin Time
                4. End Time
                5. Final Return
                6. Maximum Net Gain
                7. Minimum Net Gain
                8. Cumulative Returns
                9. Sharpe Ratio
                10. Maximum Drowndown
        """
        net_pnl = self.pnl["Cumulative PnL"].to_numpy()
        time_pnl = self.pnl["Time"].to_numpy()
        net_pnl_percent = net_pnl / self.initial_value

        cumulative_returns = net_pnl_percent[-1]
        pnl_std = net_pnl_percent.std()

        # Sharpe Ratio, final net return / standard deviation
        sharpe_ratio = cumulative_returns / pnl_std

        # Maximum Drowndown
        max_pnl = np.max(net_pnl)
        min_pnl = np.min(net_pnl)
        max_drowndown = (min_pnl - max_pnl) / (self.initial_value + max_pnl)

        # Begin and End investment value
        initial_value = self.initial_value
        final_val = self.initial_value + net_pnl[-1]

        if print_value:
            print("Initial Investment Value: ", initial_value)
            print("Final Investment Value: ", final_val)
            print("Begin at: ", time_pnl[0])
            print("End at: ", time_pnl[-1])
            print("Final return: ", net_pnl[-1])
            print("Maximum PnL: ", max_pnl)
            print("Minimum PnL", min_pnl)
            print("Cumulative Returns: " + f"{cumulative_returns:.4%}")
            print("Sharpe Ratio: ", sharpe_ratio)
            print("Maximum Drowndown: " + f"{max_drowndown:.4%}")
        return [
            initial_value,
            final_val,
            time_pnl[0],
            time_pnl[-1],
            net_pnl[-1],
            max_pnl,
            min_pnl,
            cumulative_returns,
            sharpe_ratio,
            max_drowndown
        ]

    # Visualize PnL CSV file
    def visualize_pnl(self):
        """
        Visualize the profit and loss values

        Parameters
        ----------
            None

        Returns
        ----------
            None
        """
        pnl_ = self.pnl[['Time', 'Cumulative PnL']]
        month_dict = {
            'Jan' : '01',
            'Feb' : '02',
            'Mar' : '03',
            'Apr' : '04',
            'May' : '05',
            'Jun' : '06',
            'Jul' : '07',
            'Aug' : '08',
            'Sep' : '09',
            'Oct' : '10',
            'Nov' : '11',
            'Dec' : '12',
        }
        pnl_['Time'] = pnl_['Time'].replace(month_dict, regex=True)
        time_pnl = pnl_.to_numpy()
        time_pnl = time_pnl[time_pnl[:, 0].argsort()]
        
        date_data = time_pnl[:, 0]

        cumulative_pnl = time_pnl[:, 1]

        time_series_fig = px.line(
                            pnl_,
                            x=pnl_['Time'].str[:19],
                            y="Cumulative PnL",
                            title=f"{self.name} PnL",
                            labels=f"{self.name}")
        time_series_fig.show()

        date_label = [date_data[0].split(' ')[0]]
        pnl_by_date = []
        temp = [cumulative_pnl[0]]
        for i in range(1, time_pnl.shape[0]):
            date = date_data[i].split(' ')[0]
            if date == date_label[-1]:
                temp.append(float(cumulative_pnl[i]))
            else:
                date_label.append(date)
                pnl_by_date.append(np.array(temp))
                temp = [cumulative_pnl[i]]
        pnl_by_date.append(temp)

        self.pnl_by_date = pnl_by_date

        if self.date_price_dict is not None:
            tick_open = [0]
            tick_high = [0]
            tick_low = [0]
            tick_close = [0]
            for i in range(0, len(date_label)):
                date_ = date_label[i]
                if date_ not in list(self.date_price_dict):
                    tick_open.append(tick_open[-1])
                    tick_high.append(tick_high[-1])
                    tick_low.append(tick_low[-1])
                    tick_close.append(tick_close[-1])
                else:
                    tick_open.append(self.date_price_dict[date_]["open"])
                    tick_high.append(self.date_price_dict[date_]["high"])
                    tick_low.append(self.date_price_dict[date_]["low"])
                    tick_close.append(self.date_price_dict[date_]["close"])


            fig = make_subplots(rows=1, cols=2)
            fig.add_trace(
                go.Candlestick(
                    x=date_label,
                    open=[arr[0] for arr in pnl_by_date],
                    high=[max(arr) for arr in pnl_by_date],
                    low=[min(arr) for arr in pnl_by_date],
                    close=[arr[-1] for arr in pnl_by_date],
                    name="Profit and Loss"
                ), row = 1, col = 1,
            )
            fig.add_trace(
                go.Candlestick(
                    x=date_label,
                    open=tick_open[1:],
                    high=tick_high[1:],
                    low=tick_low[1:],
                    close=tick_close[1:],
                    increasing_line_color= '#1F77B4',
                    decreasing_line_color= '#FECB52',
                    name = "Tick data",
                ), row = 1, col = 2
            )

            fig.update_layout(
                title=f"{self.name} PnL",
                xaxis_title="Date",
                yaxis_title="Price",
                legend_title=f"{self.name}",
            )

            fig.show()

    # Get relevant data by type
    def get_data(self, types = None):
        """
        Return the specified type of data, or all of them

        Parameters
        ----------
            types : str
                the type of the data the user want
                fill, order, pnl, or none
        """
        if types is None:
            return [self.fill, self.order, self.pnl]
        types = types.lower()
        if types == "fill":
            return self.fill
        if types == "order":
            return self.order
        if types == "pnl":
            return self.pnl
        return [self.fill, self.order, self.pnl]
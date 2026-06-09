import quantolib as ql
from pathlib import Path

DATA_DIR = Path('data/_data')



def main():
    stock_folder_path = DATA_DIR / 'equity'
    tickers = ['AAPL', 'AMZN']
    start_date = '2000-01-01'

    ql.pull_ohlcv_yf(str(stock_folder_path), tickers, True, start_date, auto_adjust=True, to_csv=True)




if __name__ == '__main__':
    main()
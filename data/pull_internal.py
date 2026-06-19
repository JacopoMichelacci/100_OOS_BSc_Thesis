"""Refresh the internal daily datasets used by reports and regressions."""

from pathlib import Path
import shutil

import polars as pl
import quantolib as ql


START_DATE = "2000-01-01"
INTERNAL_DIR = Path(__file__).resolve().parent / "_internal"

ASSETS: dict[str, list[str]] = {
    "benchmarks": ["SPY", "VGK", "EWJ"],
    "bonds": ["SHY", "IEF", "TLT"],
    "commodities": ["DBC", "DBA", "DBB", "DBE", "DBP"],
    "crypto": ["BTC-USD", "ETH-USD", "BTC-EUR", "ETH-EUR"],
}


def _reset_internal_dir() -> None:
    """Remove the previous snapshot and recreate the category directories."""
    shutil.rmtree(INTERNAL_DIR, ignore_errors=True)
    for category in ASSETS:
        (INTERNAL_DIR / category).mkdir(parents=True, exist_ok=True)


def _normalize_download(category_dir: Path, symbol: str) -> None:
    """Reduce one QuantoLib OHLCV file to the internal date/close schema."""
    raw_path = category_dir / f"{symbol}_ohlcv_{START_DATE}_yf.parquet"
    if not raw_path.exists():
        raise FileNotFoundError(f"QuantoLib did not create expected file: {raw_path}")

    data = (
        pl.read_parquet(raw_path)
        .select(
            pl.col("date").cast(pl.Date),
            pl.col("close").cast(pl.Float64),
        )
        .drop_nulls(["date", "close"])
        .filter(pl.col("close").is_not_nan())
        .unique(subset="date", keep="last")
        .sort("date")
    )

    if data.is_empty():
        raise ValueError(f"downloaded dataset is empty: {symbol}")
    if data.filter(pl.col("close") <= 0.0).height:
        raise ValueError(f"downloaded dataset contains non-positive closes: {symbol}")

    data.write_parquet(category_dir / f"{symbol}.parquet")
    raw_path.unlink()


def _pull_category(category: str, symbols: list[str]) -> None:
    """Pull and normalize every registered symbol in one category."""
    category_dir = INTERNAL_DIR / category
    ql.pull_ohlcv_yf(
        folder_path=str(category_dir),
        tickers=symbols,
        divide=True,
        start_date=START_DATE,
        timeframe="1d",
        auto_adjust=True,
        to_csv=False,
    )

    for symbol in symbols:
        _normalize_download(category_dir, symbol)


def main() -> None:
    """Replace the full internal-data snapshot with current daily data."""
    _reset_internal_dir()

    for category, symbols in ASSETS.items():
        print(f"\npulling {category}: {', '.join(symbols)}")
        _pull_category(category, symbols)

    print(f"\ninternal data refreshed in {INTERNAL_DIR}")


if __name__ == "__main__":
    main()

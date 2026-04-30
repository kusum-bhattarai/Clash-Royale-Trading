#!/usr/bin/env python3
"""
Seed realistic market data for the CR Trading analytics system.

WHY THIS EXISTS
---------------
The five analytics metrics each need a different kind of historical data:

  Metric              | Needs                | How much
  --------------------|----------------------|-----------------------------
  Spread              | Live order book      | ≥1 resting bid + ask
  TWAS proxy          | Flushed 1m candles   | ≥1 candle in price_history
  Order flow imbal.   | orders in last 5min  | ≥2 recent orders
  Price impact        | ≥2 consecutive 1m    | ≥2 candles
  Volatility          | ≥21 1m candles       | log-return requires 20 diffs
  VWAP                | Trades today         | ≥1 trade since midnight

Without this script, you'd have to wait ~20 real minutes for enough
candle periods to accumulate. This script backdates everything so
all metrics are live on first query.

USAGE
-----
  python3 backend/scripts/seed_market_data.py [--cards all|mega_knight|...]
  python3 backend/scripts/seed_market_data.py --wipe   # clear seeded data first

WHAT IT INSERTS (per card)
--------------------------
  - 60 one-minute candles  spanning the past hour  → TWAS, volatility, price impact
  - ~3-8 trades per candle spread across that hour  → VWAP
  - 20 orders placed "just now"                     → OFI (slight buy imbalance)
  - 2 resting limit orders left in order book       → instantaneous spread

Everything is generated from a geometric Brownian motion price walk
(the standard model for asset prices) so charts look realistic.
"""

import math
import random
import subprocess
import sys
import uuid
from datetime import datetime, timedelta, timezone

# ── Config ────────────────────────────────────────────────────────────────────

DB_CONTAINER = "clash_trading_postgres"
DB_USER      = "clash_user"
DB_NAME      = "clash_trading"

# Cards to seed — (card_id, display_name, base_price_gold, daily_volatility)
# daily_volatility: typical % move per day. Legendary ~8%, Common ~3%.
CARDS_TO_SEED = [
    ("26000055", "Mega Knight",  1300.0, 0.08),
    ("26000004", "P.E.K.K.A",    900.0, 0.07),
    ("26000021", "Hog Rider",    750.0, 0.06),
    ("26000000", "Knight",       150.0, 0.03),
]

N_CANDLES   = 60   # 60 one-minute candles = last hour of history
TRADES_PER_CANDLE_MIN = 3
TRADES_PER_CANDLE_MAX = 8
N_RECENT_ORDERS = 20  # orders placed "right now" for OFI signal

# ── DB helpers ────────────────────────────────────────────────────────────────

def psql(sql: str, quiet: bool = False) -> str:
    result = subprocess.run(
        ["docker", "exec", "-i", DB_CONTAINER,
         "psql", "-U", DB_USER, "-d", DB_NAME, "-c", sql],
        capture_output=True, text=True
    )
    if result.returncode != 0 and not quiet:
        print(f"  [WARN] SQL error: {result.stderr.strip()[:300]}")
    return result.stdout.strip()


def psql_query(sql: str) -> list[str]:
    """Run a SELECT and return list of stripped row values."""
    result = subprocess.run(
        ["docker", "exec", "-i", DB_CONTAINER,
         "psql", "-U", DB_USER, "-d", DB_NAME, "-t", "-A", "-c", sql],
        capture_output=True, text=True
    )
    return [r.strip() for r in result.stdout.strip().split("\n") if r.strip()]


# ── Price model ───────────────────────────────────────────────────────────────

def geometric_brownian_motion(
    s0: float, n: int, mu: float = 0.0, sigma_per_period: float = 0.01
) -> list[float]:
    """
    Generate n+1 prices using geometric Brownian motion.
    sigma_per_period is the per-step volatility (not annualised).
    GBM: S(t+1) = S(t) * exp((mu - 0.5*sigma^2)*dt + sigma*Z)
    where Z ~ N(0,1).
    """
    prices = [s0]
    for _ in range(n):
        z = random.gauss(0, 1)
        log_return = (mu - 0.5 * sigma_per_period ** 2) + sigma_per_period * z
        prices.append(max(1.0, prices[-1] * math.exp(log_return)))
    return prices


def candle_from_ticks(ticks: list[float], quantities: list[int]):
    """Derive OHLCV from a list of intra-period price ticks."""
    return {
        "open":   ticks[0],
        "high":   max(ticks),
        "low":    min(ticks),
        "close":  ticks[-1],
        "volume": sum(quantities),
        "trades": len(ticks),
    }


# ── Seed logic ────────────────────────────────────────────────────────────────

def get_or_create_test_users() -> tuple[str, str]:
    """Return (seller_id, buyer_id). Creates users if they don't exist."""
    rows = psql_query(
        "SELECT user_id, username FROM users WHERE username IN ('seed_seller','seed_buyer') ORDER BY username;"
    )
    users = {}
    for row in rows:
        if "|" in row:
            uid, uname = row.split("|", 1)
            users[uname.strip()] = uid.strip()

    if "seed_seller" not in users:
        print("  Creating seed_seller...")
        # bcrypt of 'seedpass' — precomputed so we don't need bcrypt here
        # Using the same hash function as the auth service would, but since
        # we're just seeding, any valid bcrypt hash for a throwaway password works.
        # We use a direct INSERT with a known bcrypt hash.
        uid = str(uuid.uuid4())
        psql(
            f"INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
            f"VALUES ('{uid}', 'seed_seller', 'seed_seller@dev.local', "
            f"'$2b$10$devhashplaceholderXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX', 10000000) "
            f"ON CONFLICT (username) DO NOTHING;"
        )
        users["seed_seller"] = uid

    if "seed_buyer" not in users:
        print("  Creating seed_buyer...")
        uid = str(uuid.uuid4())
        psql(
            f"INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
            f"VALUES ('{uid}', 'seed_buyer', 'seed_buyer@dev.local', "
            f"'$2b$10$devhashplaceholderXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX', 10000000) "
            f"ON CONFLICT (username) DO NOTHING;"
        )
        users["seed_buyer"] = uid

    # Re-fetch to get actual UUIDs
    rows = psql_query(
        "SELECT user_id, username FROM users WHERE username IN ('seed_seller','seed_buyer') ORDER BY username;"
    )
    users = {}
    for row in rows:
        if "|" in row:
            uid, uname = row.split("|", 1)
            users[uname.strip()] = uid.strip()

    return users["seed_seller"], users["seed_buyer"]


def ensure_inventory(seller_id: str, card_id: str, qty: int = 500):
    """Make sure the seed seller has enough cards to sell."""
    psql(
        f"INSERT INTO user_inventory (user_id, card_id, quantity, avg_purchase_price) "
        f"VALUES ('{seller_id}', '{card_id}', {qty}, 100.00) "
        f"ON CONFLICT (user_id, card_id) DO UPDATE SET quantity = GREATEST(user_inventory.quantity, {qty});"
    )


def seed_card(card_id: str, name: str, base_price: float, daily_vol: float,
              seller_id: str, buyer_id: str):
    print(f"\n  [{name}]")

    # Per-minute volatility: daily_vol / sqrt(1440 minutes/day)
    sigma_1m = daily_vol / math.sqrt(1440)

    # Generate N_CANDLES+1 "close" prices using GBM
    now_utc = datetime.now(timezone.utc).replace(second=0, microsecond=0)
    period_start = now_utc - timedelta(minutes=N_CANDLES)

    close_prices = geometric_brownian_motion(
        s0=base_price, n=N_CANDLES, mu=0.0, sigma_per_period=sigma_1m
    )

    # ── Candles ───────────────────────────────────────────────────────────────
    print(f"    Inserting {N_CANDLES} 1m candles... ", end="", flush=True)
    candle_rows = []
    all_trades = []   # (ts, price, qty) for the trades table

    for i in range(N_CANDLES):
        ts = period_start + timedelta(minutes=i)
        ts_str = ts.strftime("%Y-%m-%d %H:%M:%S+00")

        # Intra-candle ticks: interpolate between close[i] and close[i+1]
        n_ticks = random.randint(TRADES_PER_CANDLE_MIN, TRADES_PER_CANDLE_MAX)
        open_p  = close_prices[i]
        close_p = close_prices[i + 1]
        ticks   = [open_p + (close_p - open_p) * t / (n_ticks - 1) for t in range(n_ticks)]
        # Add a little noise to each tick
        ticks   = [max(1.0, t + random.gauss(0, sigma_1m * base_price)) for t in ticks]
        qtys    = [random.randint(1, 3) for _ in range(n_ticks)]

        c = candle_from_ticks(ticks, qtys)
        candle_rows.append(
            f"('{card_id}', '1m', '{ts_str}', "
            f"{c['open']:.2f}, {c['high']:.2f}, {c['low']:.2f}, {c['close']:.2f}, "
            f"{c['volume']}, {c['trades']})"
        )

        # Record intra-candle trade timestamps (spread evenly across the minute)
        for j, (price, qty) in enumerate(zip(ticks, qtys)):
            trade_ts = ts + timedelta(seconds=int(60 * j / n_ticks))
            all_trades.append((trade_ts, price, qty))

    # Insert candles in one batch
    psql(
        "INSERT INTO price_history (card_id, timeframe, timestamp, open_price, high_price, "
        "low_price, close_price, volume, trade_count) VALUES "
        + ",\n".join(candle_rows)
        + " ON CONFLICT (card_id, timeframe, timestamp) DO NOTHING;"
    )
    print("done")

    # ── Trades ────────────────────────────────────────────────────────────────
    print(f"    Inserting {len(all_trades)} trades... ", end="", flush=True)
    trade_rows = []
    order_rows = []  # We need matching orders for FK constraints

    for (trade_ts, price, qty) in all_trades:
        ts_str       = trade_ts.strftime("%Y-%m-%d %H:%M:%S+00")
        trade_id     = f"seed-{card_id[:8]}-{trade_ts.strftime('%H%M%S')}-{random.randint(100,999)}"
        buy_order_id  = str(uuid.uuid4())
        sell_order_id = str(uuid.uuid4())
        total_val     = round(price * qty, 2)
        merkle_fake   = uuid.uuid4().hex + uuid.uuid4().hex  # 64 hex chars

        order_rows.append(
            f"('{buy_order_id}',  '{buyer_id}',  '{card_id}', 'BUY',  'LIMIT', "
            f"{price:.2f}, {qty}, {qty}, 'FILLED', '{ts_str}', '{ts_str}')"
        )
        order_rows.append(
            f"('{sell_order_id}', '{seller_id}', '{card_id}', 'SELL', 'LIMIT', "
            f"{price:.2f}, {qty}, {qty}, 'FILLED', '{ts_str}', '{ts_str}')"
        )
        trade_rows.append(
            f"('{trade_id}', '{card_id}', '{buyer_id}', '{seller_id}', "
            f"{price:.2f}, {qty}, {total_val:.2f}, "
            f"'{buy_order_id}', '{sell_order_id}', '{merkle_fake}', '{ts_str}')"
        )

    # Insert orders first (FK source)
    BATCH = 50
    for start in range(0, len(order_rows), BATCH):
        psql(
            "INSERT INTO orders (order_id, user_id, card_id, order_type, order_mode, "
            "price, quantity, filled_quantity, status, created_at, updated_at) VALUES "
            + ",\n".join(order_rows[start:start + BATCH])
            + " ON CONFLICT (order_id) DO NOTHING;",
            quiet=True
        )
    # Then insert trades
    for start in range(0, len(trade_rows), BATCH):
        psql(
            "INSERT INTO trades (trade_id, card_id, buyer_id, seller_id, price, quantity, "
            "total_value, buyer_order_id, seller_order_id, merkle_hash, executed_at) VALUES "
            + ",\n".join(trade_rows[start:start + BATCH])
            + " ON CONFLICT (trade_id) DO NOTHING;",
            quiet=True
        )
    print("done")

    # ── Recent orders for OFI ─────────────────────────────────────────────────
    # Insert N_RECENT_ORDERS orders timestamped "right now" with a buy skew
    # (60% buy, 40% sell) so OFI comes out positive (~0.2).
    print(f"    Inserting {N_RECENT_ORDERS} recent orders for OFI... ", end="", flush=True)
    now_str   = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S+00")
    last_close = round(close_prices[-1], 2)
    ofi_rows  = []
    for k in range(N_RECENT_ORDERS):
        oid    = str(uuid.uuid4())
        otype  = "BUY" if k < int(N_RECENT_ORDERS * 0.6) else "SELL"
        uid    = buyer_id if otype == "BUY" else seller_id
        ofi_rows.append(
            f"('{oid}', '{uid}', '{card_id}', '{otype}', 'LIMIT', "
            f"{last_close:.2f}, 1, 1, 'FILLED', '{now_str}', '{now_str}')"
        )
    psql(
        "INSERT INTO orders (order_id, user_id, card_id, order_type, order_mode, "
        "price, quantity, filled_quantity, status, created_at, updated_at) VALUES "
        + ",\n".join(ofi_rows)
        + " ON CONFLICT (order_id) DO NOTHING;"
    )
    print("done")

    final_price = round(close_prices[-1], 2)
    print(f"    Price walk: {base_price:.0f}g → {final_price:.0f}g  "
          f"(σ/candle={sigma_1m*100:.3f}%)")


# ── Wipe helper ───────────────────────────────────────────────────────────────

def wipe_seeded_data():
    print("Wiping seeded data...")
    psql("DELETE FROM trades      WHERE trade_id LIKE 'seed-%';")
    psql("DELETE FROM orders      WHERE user_id IN (SELECT user_id FROM users WHERE username IN ('seed_seller','seed_buyer'));")
    psql("DELETE FROM price_history WHERE card_id IN (SELECT card_id FROM cards);")
    psql("DELETE FROM user_inventory WHERE user_id IN (SELECT user_id FROM users WHERE username IN ('seed_seller','seed_buyer'));")
    print("Done.")


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    wipe = "--wipe" in sys.argv

    print("CR Trading — market data seeder")
    print("=" * 40)

    # Sanity-check DB connection
    result = psql_query("SELECT 1;")
    if not result or result[0] != "1":
        print("ERROR: Cannot reach the database.")
        print("Make sure clash_trading_postgres is running:")
        print("  cd /path/to/Clash-Royale-Trading && docker-compose up postgres -d")
        sys.exit(1)

    if wipe:
        wipe_seeded_data()
        return

    print("\nStep 1/3  Setting up seed users...")
    seller_id, buyer_id = get_or_create_test_users()
    print(f"  seed_seller = {seller_id}")
    print(f"  seed_buyer  = {buyer_id}")

    print("\nStep 2/3  Seeding market data for each card...")
    for card_id, name, base_price, daily_vol in CARDS_TO_SEED:
        # Make sure seller has cards to sell
        ensure_inventory(seller_id, card_id)
        seed_card(card_id, name, base_price, daily_vol, seller_id, buyer_id)

    print("\nStep 3/3  Summary")
    print("-" * 40)
    for card_id, name, _, _ in CARDS_TO_SEED:
        rows = psql_query(
            f"SELECT COUNT(*), MIN(price)::numeric(10,2), MAX(price)::numeric(10,2), "
            f"AVG(price)::numeric(10,2) FROM trades WHERE card_id='{card_id}' "
            f"AND executed_at >= CURRENT_DATE::timestamp;"
        )
        candles = psql_query(
            f"SELECT COUNT(*) FROM price_history WHERE card_id='{card_id}' AND timeframe='1m';"
        )
        n_candles = candles[0] if candles else "?"
        if rows and "|" in rows[0]:
            cnt, mn, mx, avg = rows[0].split("|")
            print(f"  {name:<15} {cnt.strip():>4} trades  "
                  f"range {mn.strip()}–{mx.strip()}g  avg {avg.strip()}g  "
                  f"{n_candles} candles")

    print("\nAll done! Hit the analytics endpoint:")
    for card_id, name, _, _ in CARDS_TO_SEED:
        print(f"  curl -s http://localhost:8080/api/cards/{card_id}/analytics | python3 -m json.tool")


if __name__ == "__main__":
    random.seed(42)  # reproducible data; remove for different results each run
    main()

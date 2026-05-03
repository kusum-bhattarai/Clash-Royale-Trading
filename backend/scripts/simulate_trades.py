#!/usr/bin/env python3
"""
Trade simulator — drives price action between two accounts to generate candle data.
Logs in as test51 and test26, then alternates limit sell / market buy orders
across several cards with random price walks.

Usage:
  python3 simulate_trades.py [--rounds N] [--delay SECONDS]
"""

import argparse
import random
import time
import sys
import requests

BASE = "http://localhost:8080"

# Cards to trade (card_id, starting_price)
CARDS = [
    ("26000000", 150),   # Knight
    ("26000072", 2000),  # Mega Knight
    ("26000001", 120),   # Archers
    ("28000000", 10000), # Princess
]

def login(username: str, password: str) -> str:
    r = requests.post(f"{BASE}/api/v1/auth/login",
                      json={"username": username, "password": password})
    r.raise_for_status()
    token = r.json()["token"]
    print(f"  logged in as {username}")
    return token

def place_order(token: str, card_id: str, order_type: str,
                mode: str, quantity: int, price: float | None = None) -> dict:
    payload = {
        "card_id": card_id,
        "type": order_type,
        "mode": mode,
        "quantity": quantity,
    }
    if mode == "LIMIT" and price is not None:
        payload["price"] = round(price, 2)

    r = requests.post(f"{BASE}/api/v1/orders",
                      json=payload,
                      headers={"Authorization": f"Bearer {token}"})
    if r.status_code != 200:
        print(f"    order failed {r.status_code}: {r.text[:120]}")
        return {}
    return r.json()

def simulate(rounds: int, delay: float):
    print("Logging in...")
    try:
        tok_a = login("test51", "password123")
        tok_b = login("test26", "password123")
    except Exception as e:
        print(f"Login failed: {e}")
        print("Make sure the server is running and the accounts exist.")
        sys.exit(1)

    prices = {card_id: base for card_id, base in CARDS}

    print(f"\nRunning {rounds} rounds (delay={delay}s between orders)...\n")

    for i in range(rounds):
        card_id, _ = random.choice(CARDS)
        base_price = prices[card_id]

        # Random walk: ±3% per step
        change_pct = random.uniform(-0.03, 0.03)
        new_price = max(10, base_price * (1 + change_pct))
        prices[card_id] = new_price

        qty = random.randint(1, 3)

        # Alternate who sells
        if i % 2 == 0:
            seller_tok, buyer_tok = tok_a, tok_b
        else:
            seller_tok, buyer_tok = tok_b, tok_a

        # Step 1: seller places LIMIT SELL at new_price
        sell_resp = place_order(seller_tok, card_id, "SELL", "LIMIT", qty, new_price)
        if not sell_resp:
            continue

        time.sleep(0.1)  # small gap so order is in book before buy

        # Step 2: buyer hits it with MARKET BUY
        buy_resp = place_order(buyer_tok, card_id, "BUY", "MARKET", qty)
        filled = buy_resp.get("filled_quantity", 0)

        print(f"  [{i+1:3d}/{rounds}] card={card_id} price={new_price:8.2f} qty={qty} filled={filled}")

        time.sleep(delay)

    print("\nDone. Candle data should now appear in the chart.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--rounds", type=int, default=60,
                        help="Number of trades to simulate (default 60)")
    parser.add_argument("--delay", type=float, default=2.0,
                        help="Seconds between trades (default 2.0)")
    args = parser.parse_args()
    simulate(args.rounds, args.delay)

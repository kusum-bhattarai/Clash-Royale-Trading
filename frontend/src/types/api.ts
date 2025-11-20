// User representation
export interface User {
  user_id: string;
  username: string;
  email: string;
  gold_balance: number;
  trader_level: string;
  total_trades: number;
}

// Login request payload
export interface LoginRequest {
  username: string;
  password: string;
}

// Registration request payload
export interface RegisterRequest {
  username: string;
  email: string;
  password: string;
}

// Response from login/register endpoints
export interface AuthResponse {
  token: string;
  user: {
    user_id: string;
    username: string;
    email: string;
    gold_balance: number;
  };
}

// Represents a Clash Royale card
export interface Card {
  card_id: string;
  name: string;
  rarity: string;
  elixir_cost?: number;        // Optional because some cards don't have elixir cost
  max_level: number;
  icon_url: string;
  current_market_price: number;
  total_supply: number;
  usage_rate: number;
}

// Enum-like types for order properties
export type OrderType = 'BUY' | 'SELL';
export type OrderMode = 'MARKET' | 'LIMIT';
export type OrderStatus = 'PENDING' | 'PARTIAL' | 'FILLED' | 'CANCELLED';

// Represents an order in the system
export interface Order {
  order_id: string;
  user_id: string;
  card_id: string;
  order_type: OrderType;
  order_mode: OrderMode;
  price: number;
  quantity: number;
  filled_quantity: number;
  status: OrderStatus;
  created_at: string;
  updated_at: string;
}

// Request payload for placing an order
export interface PlaceOrderRequest {
  card_id: string;
  type: OrderType;
  mode: OrderMode;
  price?: number;              // Optional for market orders
  quantity: number;
}

// Response after placing an order
export interface PlaceOrderResponse {
  order_id: string;
  status: OrderStatus;
  filled_quantity: number;
  trades: Trade[];             // Trades that were executed
}

// Represents a completed trade between two users
export interface Trade {
  trade_id: string;
  card_id: string;
  buyer_id: string;
  seller_id: string;
  price: number;
  quantity: number;
  total_value: number;
  buyer_order_id: string;
  seller_order_id: string;
  executed_at?: string;
}

// Represents a user's holding of a specific card
export interface CardHolding {
  card_id: string;
  card_name: string;
  quantity: number;
  avg_purchase_price: number;
  current_market_price: number;
  total_value: number;
  unrealized_pnl: number;         // Profit/Loss
  unrealized_pnl_percent: number; // Profit/Loss percentage
}

// Represents a user's complete portfolio
export interface Portfolio {
  user_id: string;
  gold_balance: number;
  total_card_value: number;
  total_portfolio_value: number;
  holdings: CardHolding[];
}

// Represents the order book for a card
// bids and asks are arrays of [price, quantity] tuples
export interface OrderBookSnapshot {
  card_id: string;
  bids: [number, number][]; // Array of [price, quantity]
  asks: [number, number][]; // Array of [price, quantity]
}

// Represents a user's trading statistics
export interface TradingStats {
  total_trades: number;
  total_volume: number;
  buy_count: number;
  sell_count: number;
  trader_level: string;
}

// Base WebSocket message interface
export interface WSMessage {
  type: string;
  [key: string]: any;
}

// WebSocket message for trade execution
export interface WSTradeMessage extends WSMessage {
  type: 'trade_executed';
  trade_id: string;
  card_id: string;
  price: number;
  quantity: number;
  total_value: number;
}

// WebSocket message for order book updates
export interface WSOrderBookMessage extends WSMessage {
  type: 'orderbook_update';
  card_id: string;
  bids: [number, number][];
  asks: [number, number][];
}

// WebSocket message for order filled notification
export interface WSOrderFilledMessage extends WSMessage {
  type: 'order_filled';
  order_id: string;
  status: string;
  filled_quantity: number;
}

// WebSocket message for portfolio updates
export interface WSPortfolioUpdateMessage extends WSMessage {
  type: 'portfolio_update';
  user_id: string;
}
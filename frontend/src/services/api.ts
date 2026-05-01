import apiClient from '../lib/axios';
import type {
  AnalyticsSnapshot,
  AuthResponse,
  LoginRequest,
  RegisterRequest,
  Card,
  Order,
  PlaceOrderRequest,
  PlaceOrderResponse,
  Portfolio,
  TradingStats,
  OrderBookSnapshot,
  Trade,
} from '../types/api';

// Authentication API
export const authAPI = {
  // Register a new user
  register: async (data: RegisterRequest): Promise<AuthResponse> => {
    const response = await apiClient.post<AuthResponse>('/api/v1/auth/register', data);
    return response.data;
  },

  // Login existing user
  login: async (data: LoginRequest): Promise<AuthResponse> => {
    const response = await apiClient.post<AuthResponse>('/api/v1/auth/login', data);
    return response.data;
  },
};

// Cards API
export const cardsAPI = {
  // Get all cards
  getAll: async (): Promise<Card[]> => {
    const response = await apiClient.get<Card[]>('/api/v1/cards');
    return response.data;
  },

  // Get a specific card by ID
  getById: async (cardId: string): Promise<Card> => {
    const response = await apiClient.get<Card>(`/api/v1/cards/${cardId}`);
    return response.data;
  },

  // Get order book for a card
  getOrderBook: async (cardId: string): Promise<OrderBookSnapshot> => {
    const response = await apiClient.get<OrderBookSnapshot>(
      `/api/v1/cards/${cardId}/orderbook`
    );
    return response.data;
  },

  // Get recent trades for a card (supports ?limit= and ?offset= pagination)
  getTrades: async (cardId: string, limit = 50, offset = 0): Promise<Trade[]> => {
    const response = await apiClient.get<Trade[]>(
      `/api/v1/cards/${cardId}/trades?limit=${limit}&offset=${offset}`
    );
    return response.data;
  },

  // Get market microstructure analytics for a card
  getAnalytics: async (cardId: string): Promise<AnalyticsSnapshot> => {
    const response = await apiClient.get<AnalyticsSnapshot>(
      `/api/v1/cards/${cardId}/analytics`
    );
    return response.data;
  },
};

// Orders API
export const ordersAPI = {
  // Place a new order
  place: async (data: PlaceOrderRequest): Promise<PlaceOrderResponse> => {
    const response = await apiClient.post<PlaceOrderResponse>('/api/v1/orders', data);
    return response.data;
  },

  // Cancel an existing order
  cancel: async (orderId: string): Promise<void> => {
    await apiClient.delete(`/api/v1/orders/${orderId}`);
  },

  // Get order details by ID
  getById: async (orderId: string): Promise<Order> => {
    const response = await apiClient.get<Order>(`/api/v1/orders/${orderId}`);
    return response.data;
  },
};

// Users API
export const usersAPI = {
  // Get user's portfolio
  getPortfolio: async (userId: string): Promise<Portfolio> => {
    const response = await apiClient.get<Portfolio>(
      `/api/v1/users/${userId}/portfolio`
    );
    return response.data;
  },

  // Get user's orders
  getOrders: async (userId: string): Promise<Order[]> => {
    const response = await apiClient.get<Order[]>(`/api/v1/users/${userId}/orders`);
    return response.data;
  },

  // Get user's trade history (supports ?limit= and ?offset= pagination)
  getTrades: async (userId: string, limit = 50, offset = 0): Promise<Trade[]> => {
    const response = await apiClient.get<Trade[]>(
      `/api/v1/users/${userId}/trades?limit=${limit}&offset=${offset}`
    );
    return response.data;
  },

  // Get user's trading statistics
  getStats: async (userId: string): Promise<TradingStats> => {
    const response = await apiClient.get<TradingStats>(
      `/api/v1/users/${userId}/stats`
    );
    return response.data;
  },
};
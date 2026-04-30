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
    const response = await apiClient.post<AuthResponse>('/api/auth/register', data);
    return response.data;
  },

  // Login existing user
  login: async (data: LoginRequest): Promise<AuthResponse> => {
    const response = await apiClient.post<AuthResponse>('/api/auth/login', data);
    return response.data;
  },
};

// Cards API
export const cardsAPI = {
  // Get all cards
  getAll: async (): Promise<Card[]> => {
    const response = await apiClient.get<Card[]>('/api/cards');
    return response.data;
  },

  // Get a specific card by ID
  getById: async (cardId: string): Promise<Card> => {
    const response = await apiClient.get<Card>(`/api/cards/${cardId}`);
    return response.data;
  },

  // Get order book for a card
  getOrderBook: async (cardId: string): Promise<OrderBookSnapshot> => {
    const response = await apiClient.get<OrderBookSnapshot>(
      `/api/cards/${cardId}/orderbook`
    );
    return response.data;
  },

  // Get recent trades for a card
  getTrades: async (cardId: string): Promise<Trade[]> => {
    const response = await apiClient.get<Trade[]>(`/api/cards/${cardId}/trades`);
    return response.data;
  },

  // Get market microstructure analytics for a card
  getAnalytics: async (cardId: string): Promise<AnalyticsSnapshot> => {
    const response = await apiClient.get<AnalyticsSnapshot>(
      `/api/cards/${cardId}/analytics`
    );
    return response.data;
  },
};

// Orders API
export const ordersAPI = {
  // Place a new order
  place: async (data: PlaceOrderRequest): Promise<PlaceOrderResponse> => {
    const response = await apiClient.post<PlaceOrderResponse>('/api/orders', data);
    return response.data;
  },

  // Cancel an existing order
  cancel: async (orderId: string): Promise<void> => {
    await apiClient.delete(`/api/orders/${orderId}`);
  },

  // Get order details by ID
  getById: async (orderId: string): Promise<Order> => {
    const response = await apiClient.get<Order>(`/api/orders/${orderId}`);
    return response.data;
  },
};

// Users API
export const usersAPI = {
  // Get user's portfolio
  getPortfolio: async (userId: string): Promise<Portfolio> => {
    const response = await apiClient.get<Portfolio>(
      `/api/users/${userId}/portfolio`
    );
    return response.data;
  },

  // Get user's orders
  getOrders: async (userId: string): Promise<Order[]> => {
    const response = await apiClient.get<Order[]>(`/api/users/${userId}/orders`);
    return response.data;
  },

  // Get user's trade history
  getTrades: async (userId: string): Promise<Trade[]> => {
    const response = await apiClient.get<Trade[]>(`/api/users/${userId}/trades`);
    return response.data;
  },

  // Get user's trading statistics
  getStats: async (userId: string): Promise<TradingStats> => {
    const response = await apiClient.get<TradingStats>(
      `/api/users/${userId}/stats`
    );
    return response.data;
  },
};
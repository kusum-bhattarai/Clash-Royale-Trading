import axios from 'axios';
// Base API URL from environment variables
const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080';

// Create a custom axios instance with base configuration
const apiClient = axios.create({
  baseURL: API_URL,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Request interceptor to add auth token to headers, runs before every API request
apiClient.interceptors.request.use(
  (config) => {
    // Get token from localStorage
    const token = localStorage.getItem('token');
    
    // If token exists, add it to Authorization header
    if (token) {
      config.headers.Authorization = `Bearer ${token}`;
    }
    
    return config;
  },
  (error) => {
    // Handle request errors
    return Promise.reject(error);
  }
);

// Response interceptor to handle global responses, runs after every API response
apiClient.interceptors.response.use(
  (response) => {
    // If response is successful, just return it
    return response;
  },
  (error) => {
    // If we get a 401 (Unauthorized), it means token is invalid/expired
    if (error.response?.status === 401) {
      // Clear authentication data
      localStorage.removeItem('token');
      localStorage.removeItem('user');
      
      // Redirect to login page
      window.location.href = '/login';
    }
    
    return Promise.reject(error);
  }
);

export default apiClient;
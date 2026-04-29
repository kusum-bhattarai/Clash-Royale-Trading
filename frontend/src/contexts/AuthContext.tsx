import { createContext, useContext, useState, useEffect } from 'react';
import type { ReactNode } from 'react';
import { authAPI, usersAPI } from '../services/api';
import type { User, LoginRequest, RegisterRequest } from '../types/api';

interface AuthContextType {
  user: User | null;              // Current logged-in user (null if not logged in)
  token: string | null;           // JWT token
  isAuthenticated: boolean;       // Quick check if user is logged in
  isLoading: boolean;             // Loading state during initial auth check
  login: (data: LoginRequest) => Promise<void>;
  register: (data: RegisterRequest) => Promise<void>;
  logout: () => void;
  refreshUser: () => Promise<void>;
}

// Create the context with undefined default (we'll provide it via Provider)
const AuthContext = createContext<AuthContextType | undefined>(undefined);

// AuthProvider component to wrap around parts of app needing auth
interface AuthProviderProps {
  children: ReactNode;
}

export function AuthProvider({ children }: AuthProviderProps) {
  const [user, setUser] = useState<User | null>(null);
  const [token, setToken] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  // On component mount, check localStorage for existing auth data
  useEffect(() => {
    const initAuth = () => {
      try {
        const savedToken = localStorage.getItem('token');
        const savedUser = localStorage.getItem('user');

        if (savedToken && savedUser) {
          setToken(savedToken);
          setUser(JSON.parse(savedUser));
        }
      } catch (error) {
        console.error('Failed to restore auth state:', error);
        // If there's an error parsing, clear storage
        localStorage.removeItem('token');
        localStorage.removeItem('user');
      } finally {
        setIsLoading(false);
      }
    };

    initAuth();
  }, []);

  // Login function
  const login = async (data: LoginRequest) => {
    try {
      // Call backend login API
      const response = await authAPI.login(data);

      // Save token and user to state
      setToken(response.token);
      setUser(response.user as User);

      // Persist to localStorage
      localStorage.setItem('token', response.token);
      localStorage.setItem('user', JSON.stringify(response.user));
    } catch (error) {
      console.error('Login failed:', error);
      throw error; // Re-throw so component can handle it
    }
  };

  // Registration function
  const register = async (data: RegisterRequest) => {
    try {
      // Call backend register API
      const response = await authAPI.register(data);

      // Save token and user to state
      setToken(response.token);
      setUser(response.user as User);

      // Persist to localStorage
      localStorage.setItem('token', response.token);
      localStorage.setItem('user', JSON.stringify(response.user));
    } catch (error) {
      console.error('Registration failed:', error);
      throw error;
    }
  };

  // Logout function
  const logout = () => {
    // Clear state
    setUser(null);
    setToken(null);

    // Clear localStorage
    localStorage.removeItem('token');
    localStorage.removeItem('user');
  };

  const refreshUser = async () => {
    if (!user) return;
    
    try {
      const portfolio = await usersAPI.getPortfolio(user.user_id);
      const updatedUser = {
        ...user,
        gold_balance: portfolio.gold_balance
      };
      setUser(updatedUser);
      localStorage.setItem('user', JSON.stringify(updatedUser));
    } catch (error) {
      console.error('Failed to refresh user:', error);
    }
  };


  const isAuthenticated = !!user && !!token;

  // Provide the context value to children components
  const value: AuthContextType = {
    user,
    token,
    isAuthenticated,
    isLoading,
    login,
    register,
    logout,
    refreshUser,
  };

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

// Custom hook to use the AuthContext
export function useAuth() {
  const context = useContext(AuthContext);

  if (context === undefined) {
    throw new Error('useAuth must be used within an AuthProvider');
  }

  return context;
}
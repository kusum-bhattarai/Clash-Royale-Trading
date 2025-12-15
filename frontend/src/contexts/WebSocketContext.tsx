import { createContext, useContext, useEffect } from 'react';
import type { ReactNode } from 'react';
import wsClient from '../lib/websocket';

interface WebSocketContextType {
  subscribe: (channel: string) => void;
  unsubscribe: (channel: string) => void;
  on: (messageType: string, handler: (data: any) => void) => void;
  off: (messageType: string, handler: (data: any) => void) => void;
}

const WebSocketContext = createContext<WebSocketContextType | undefined>(undefined);

interface WebSocketProviderProps {
  children: ReactNode;
}

export function WebSocketProvider({ children }: WebSocketProviderProps) {
  useEffect(() => {
    wsClient.connect();

    return () => {
      wsClient.disconnect();
    };
  }, []);

  const value: WebSocketContextType = {
    subscribe: wsClient.subscribe.bind(wsClient),
    unsubscribe: wsClient.unsubscribe.bind(wsClient),
    on: wsClient.on.bind(wsClient),
    off: wsClient.off.bind(wsClient),
  };

  return (
    <WebSocketContext.Provider value={value}>
      {children}
    </WebSocketContext.Provider>
  );
}

export function useWebSocket() {
  const context = useContext(WebSocketContext);
  if (!context) {
    throw new Error('useWebSocket must be used within WebSocketProvider');
  }
  return context;
}
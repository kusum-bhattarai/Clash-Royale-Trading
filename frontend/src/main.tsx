import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { AuthProvider } from './contexts/AuthContext'
import App from './App.tsx'
import './index.css'
import { WebSocketProvider } from './contexts/WebSocketContext'

// manages all our API data fetching and caching
const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      refetchOnWindowFocus: false,  
      retry: 1,                      
      staleTime: 5 * 60 * 1000,     // Data is fresh for 5 minutes
    },
  },
});

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <QueryClientProvider client={queryClient}>
      <AuthProvider>
        <WebSocketProvider> 
        <App />
        </WebSocketProvider>
      </AuthProvider>
    </QueryClientProvider>
  </StrictMode>,
)
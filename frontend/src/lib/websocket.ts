type MessageHandler = (data: any) => void;

class WebSocketClient {
  private ws: WebSocket | null = null;
  private reconnectInterval: number = 3000;
  private reconnectTimer: number | null = null;
  private messageHandlers: Map<string, Set<MessageHandler>> = new Map();
  private url: string;
  private shouldReconnect: boolean = true;

  constructor(url: string) {
    this.url = url;
  }

  connect() {
    if (this.ws?.readyState === WebSocket.OPEN) {
      console.log('[WebSocket] Already connected');
      return;
    }

    console.log('[WebSocket] Connecting to', this.url);
    this.ws = new WebSocket(this.url);

    this.ws.onopen = () => {
      console.log('[WebSocket] Connected');
      if (this.reconnectTimer) {
        clearTimeout(this.reconnectTimer);
        this.reconnectTimer = null;
      }
      // Notify listeners so they can resync state after reconnect
      const handlers = this.messageHandlers.get('ws_connected');
      if (handlers) handlers.forEach(h => h({}));
    };

    this.ws.onmessage = (event) => {
      try {
        const message = JSON.parse(event.data);
        console.log('[WebSocket] Message received:', message);
        
        const handlers = this.messageHandlers.get(message.type);
        if (handlers) {
          handlers.forEach(handler => handler(message));
        }
      } catch (error) {
        console.error('[WebSocket] Failed to parse message:', error);
      }
    };

    this.ws.onerror = (error) => {
      console.error('[WebSocket] Error:', error);
    };

    this.ws.onclose = () => {
      console.log('[WebSocket] Disconnected');
      this.ws = null;
      
      if (this.shouldReconnect) {
        console.log(`[WebSocket] Reconnecting in ${this.reconnectInterval}ms...`);
        this.reconnectTimer = window.setTimeout(() => {
          this.connect();
        }, this.reconnectInterval);
      }
    };
  }

  disconnect() {
    this.shouldReconnect = false;
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  subscribe(channel: string) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      console.log('[WebSocket] Subscribing to:', channel);
      this.ws.send(JSON.stringify({ type: 'subscribe', channel }));
    }
  }

  unsubscribe(channel: string) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      console.log('[WebSocket] Unsubscribing from:', channel);
      this.ws.send(JSON.stringify({ type: 'unsubscribe', channel }));
    }
  }

  on(messageType: string, handler: MessageHandler) {
    if (!this.messageHandlers.has(messageType)) {
      this.messageHandlers.set(messageType, new Set());
    }
    this.messageHandlers.get(messageType)!.add(handler);
  }

  off(messageType: string, handler: MessageHandler) {
    const handlers = this.messageHandlers.get(messageType);
    if (handlers) {
      handlers.delete(handler);
    }
  }
}

// Create singleton instance
const wsClient = new WebSocketClient('ws://localhost:8081');

export default wsClient;
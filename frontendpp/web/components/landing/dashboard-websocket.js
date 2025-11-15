/**
 * Dashboard WebSocket Client
 * Connects to backend-datalink WebSocket server for real-time dashboard data
 */

export class DashboardWebSocket {
    constructor() {
        this.ws = null;
        this.connected = false;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 10;
        this.reconnectDelay = 5000;
        // Dynamic host detection for WebSocket URL
        const currentHost = window.location.hostname;
        this.url = `ws://${currentHost}:9002`; // Backend-datalink WebSocket server
        
        // Event callbacks
        this.onConnected = null;
        this.onDisconnected = null;
        this.onDataReceived = null;
        this.onUpdateReceived = null;
        this.onError = null;
        
        // Data storage
        this.dashboardData = {
            system: {},
            ram: {},
            swap: {},
            network: {},
            ultima_server: {},
            signal: {}
        };
    }

    connect() {
        try {
            console.log('[DASHBOARD-WS] Connecting to:', this.url);
            this.ws = new WebSocket(this.url);
            
            this.ws.onopen = () => {
                console.log('[DASHBOARD-WS] Connected to backend-datalink server');
                this.connected = true;
                this.reconnectAttempts = 0;
                
                // Request initial dashboard data
                this.requestDashboardData();
                
                if (this.onConnected) {
                    this.onConnected();
                }
            };

            this.ws.onmessage = (event) => {
                try {
                    const message = JSON.parse(event.data);
                    console.log('[DASHBOARD-WS] Message received:', message.type);
                    this.handleMessage(message);
                } catch (error) {
                    console.error('[DASHBOARD-WS] Error parsing message:', error);
                }
            };

            this.ws.onclose = () => {
                console.log('[DASHBOARD-WS] Disconnected from backend-datalink server');
                this.connected = false;
                
                if (this.onDisconnected) {
                    this.onDisconnected();
                }
                
                this.scheduleReconnect();
            };

            this.ws.onerror = (error) => {
                console.error('[DASHBOARD-WS] WebSocket error:', error);
                
                if (this.onError) {
                    this.onError(error);
                }
            };

        } catch (error) {
            console.error('[DASHBOARD-WS] Failed to connect:', error);
            this.scheduleReconnect();
        }
    }

    disconnect() {
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
        }
        
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        
        this.connected = false;
    }

    sendMessage(message) {
        if (this.connected && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
            console.log('[DASHBOARD-WS] Message sent:', message.type);
        } else {
            console.warn('[DASHBOARD-WS] Cannot send message - not connected');
        }
    }

    requestDashboardData(categories = null) {
        const message = {
            type: 'get_dashboard_data'
        };
        
        if (categories && Array.isArray(categories)) {
            message.categories = categories;
        }
        
        this.sendMessage(message);
    }

    subscribeToUpdates() {
        this.sendMessage({
            type: 'subscribe_updates'
        });
    }

    handleMessage(message) {
        const { type, data, category, timestamp } = message;

        switch (type) {
            case 'welcome':
                console.log('[DASHBOARD-WS] Welcome message received:', message.message);
                break;

            case 'dashboard_data':
                console.log('[DASHBOARD-WS] Initial dashboard data received');
                this.dashboardData = { ...this.dashboardData, ...data };
                
                if (this.onDataReceived) {
                    this.onDataReceived(this.dashboardData, timestamp);
                }
                break;

            case 'dashboard_update':
                console.log('[DASHBOARD-WS] Real-time update received for category:', category);
                if (category && data) {
                    this.dashboardData[category] = data;
                    
                    if (this.onUpdateReceived) {
                        this.onUpdateReceived(category, data, timestamp);
                    }
                }
                break;

            case 'subscription_confirmed':
                console.log('[DASHBOARD-WS] Subscription confirmed:', message.message);
                break;

            case 'error':
                console.error('[DASHBOARD-WS] Server error:', message.message);
                
                if (this.onError) {
                    this.onError(new Error(message.message));
                }
                break;

            default:
                console.log('[DASHBOARD-WS] Unknown message type:', type);
        }
    }

    scheduleReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`[DASHBOARD-WS] Scheduling reconnect attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts}`);
            
            this.reconnectTimer = setTimeout(() => {
                this.connect();
            }, this.reconnectDelay);
        } else {
            console.error('[DASHBOARD-WS] Max reconnect attempts reached');
            
            if (this.onError) {
                this.onError(new Error('Failed to reconnect to WebSocket server'));
            }
        }
    }

    getDashboardData() {
        return this.dashboardData;
    }

    isConnected() {
        return this.connected;
    }
}

// ES6 module export only - no fallback needed

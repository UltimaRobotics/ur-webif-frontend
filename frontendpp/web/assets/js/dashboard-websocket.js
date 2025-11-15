/**
 * WebSocket Dashboard Manager
 * Shared WebSocket management system for all dashboard components
 */

class DashboardWebSocketManager {
    constructor(options = {}) {
        this.wsClient = null;
        this.isInitialized = false;
        this.subscribers = new Map(); // Component subscribers
        this.dataCache = new Map(); // Cached data
        this.lastUpdate = null;
        
        // Configuration
        this.config = {
            url: options.url || 'ws://localhost:8765',
            reconnectInterval: options.reconnectInterval || 5000,
            maxReconnectAttempts: options.maxReconnectAttempts || 10,
            autoRefresh: options.autoRefresh !== false,
            refreshInterval: options.refreshInterval || 30000,
            ...options
        };
        
        // Event callbacks
        this.callbacks = {
            onConnect: options.onConnect || (() => {}),
            onDisconnect: options.onDisconnect || (() => {}),
            onError: options.onError || (() => {}),
            onDataUpdate: options.onDataUpdate || (() => {}),
            onSystemNotification: options.onSystemNotification || (() => {})
        };
        
        this.init();
    }
    
    /**
     * Initialize the WebSocket manager
     */
    init() {
        if (this.isInitialized) return;
        
        console.log('Initializing Dashboard WebSocket Manager...');
        this.initializeWebSocket();
        this.setupAutoRefresh();
        this.isInitialized = true;
    }
    
    /**
     * Initialize WebSocket client
     */
    initializeWebSocket() {
        this.wsClient = new WebSocketClient({
            url: this.config.url,
            reconnectInterval: this.config.reconnectInterval,
            maxReconnectAttempts: this.config.maxReconnectAttempts,
            onConnect: () => this.handleConnect(),
            onDisconnect: () => this.handleDisconnect(),
            onError: (error) => this.handleError(error),
            onInitialData: (data, timestamp) => this.handleInitialData(data, timestamp),
            onDataUpdate: (data, timestamp) => this.handleDataUpdate(data, timestamp),
            onPeriodicUpdate: (data, timestamp) => this.handlePeriodicUpdate(data, timestamp),
            onSystemNotification: (message, level, timestamp) => this.handleSystemNotification(message, level, timestamp)
        });
    }
    
    /**
     * Handle WebSocket connection
     */
    handleConnect() {
        console.log('Dashboard WebSocket connected');
        this.callbacks.onConnect();
        this.notifySubscribers('websocket_connected', { connected: true });
    }
    
    /**
     * Handle WebSocket disconnection
     */
    handleDisconnect() {
        console.log('Dashboard WebSocket disconnected');
        this.callbacks.onDisconnect();
        this.notifySubscribers('websocket_disconnected', { connected: false });
    }
    
    /**
     * Handle WebSocket errors
     */
    handleError(error) {
        console.error('Dashboard WebSocket error:', error);
        this.callbacks.onError(error);
        this.notifySubscribers('websocket_error', { error });
    }
    
    /**
     * Handle initial data from WebSocket
     */
    handleInitialData(data, timestamp) {
        console.log('Initial dashboard data received');
        this.cacheData('dashboard', data);
        this.lastUpdate = timestamp;
        this.callbacks.onDataUpdate(data, timestamp);
        this.notifySubscribers('initial_data', { data, timestamp });
    }
    
    /**
     * Handle data updates from WebSocket
     */
    handleDataUpdate(data, timestamp) {
        console.log('Dashboard data updated');
        this.cacheData('dashboard', data);
        this.lastUpdate = timestamp;
        this.callbacks.onDataUpdate(data, timestamp);
        this.notifySubscribers('data_update', { data, timestamp });
    }
    
    /**
     * Handle periodic updates from WebSocket
     */
    handlePeriodicUpdate(data, timestamp) {
        console.log('Periodic dashboard update received');
        this.cacheData('dashboard', data);
        this.lastUpdate = timestamp;
        this.notifySubscribers('periodic_update', { data, timestamp });
    }
    
    /**
     * Handle system notifications
     */
    handleSystemNotification(message, level, timestamp) {
        console.log(`System notification (${level}): ${message}`);
        this.callbacks.onSystemNotification(message, level, timestamp);
        this.notifySubscribers('system_notification', { message, level, timestamp });
    }
    
    /**
     * Subscribe to WebSocket events
     */
    subscribe(componentId, callbacks) {
        if (!this.subscribers.has(componentId)) {
            this.subscribers.set(componentId, {
                callbacks: callbacks,
                lastNotified: null
            });
        }
        
        // Send cached data if available
        const cachedData = this.getCachedData('dashboard');
        if (cachedData && callbacks.onInitialData) {
            callbacks.onInitialData(cachedData, this.lastUpdate);
        }
        
        console.log(`Component ${componentId} subscribed to WebSocket events`);
    }
    
    /**
     * Unsubscribe from WebSocket events
     */
    unsubscribe(componentId) {
        if (this.subscribers.has(componentId)) {
            this.subscribers.delete(componentId);
            console.log(`Component ${componentId} unsubscribed from WebSocket events`);
        }
    }
    
    /**
     * Notify all subscribers of an event
     */
    notifySubscribers(eventType, data) {
        this.subscribers.forEach((subscriber, componentId) => {
            try {
                const callback = subscriber.callbacks[eventType];
                if (typeof callback === 'function') {
                    callback(data);
                    subscriber.lastNotified = Date.now();
                }
            } catch (error) {
                console.error(`Error notifying subscriber ${componentId}:`, error);
            }
        });
    }
    
    /**
     * Cache data with timestamp
     */
    cacheData(key, data) {
        this.dataCache.set(key, {
            data: data,
            timestamp: Date.now()
        });
    }
    
    /**
     * Get cached data
     */
    getCachedData(key) {
        const cached = this.dataCache.get(key);
        return cached ? cached.data : null;
    }
    
    /**
     * Get cached data with metadata
     */
    getCachedDataWithMetadata(key) {
        return this.dataCache.get(key) || null;
    }
    
    /**
     * Request fresh data from server
     */
    requestData() {
        if (this.wsClient && this.wsClient.isConnected) {
            this.wsClient.requestData();
            return true;
        }
        return false;
    }
    
    /**
     * Refresh data from server
     */
    refreshData() {
        if (this.wsClient && this.wsClient.isConnected) {
            this.wsClient.refreshData();
            return true;
        }
        return false;
    }
    
    /**
     * Update data on server
     */
    updateData(data) {
        if (this.wsClient && this.wsClient.isConnected) {
            this.wsClient.updateData(data);
            return true;
        }
        return false;
    }
    
    /**
     * Send system command to server
     */
    sendSystemCommand(command, params = {}) {
        if (this.wsClient && this.wsClient.isConnected) {
            this.wsClient.sendSystemCommand(command, params);
            return true;
        }
        return false;
    }
    
    /**
     * Set up auto-refresh mechanism
     */
    setupAutoRefresh() {
        if (this.config.autoRefresh) {
            setInterval(() => {
                if (this.wsClient && this.wsClient.isConnected) {
                    this.refreshData();
                }
            }, this.config.refreshInterval);
        }
    }
    
    /**
     * Get connection status
     */
    getStatus() {
        const wsStatus = this.wsClient ? this.wsClient.getStatus() : {
            isConnected: false,
            readyState: WebSocket.CLOSED,
            reconnectAttempts: 0,
            queuedMessages: 0
        };
        
        return {
            ...wsStatus,
            isInitialized: this.isInitialized,
            subscribersCount: this.subscribers.size,
            lastUpdate: this.lastUpdate,
            cachedDataKeys: Array.from(this.dataCache.keys())
        };
    }
    
    /**
     * Get system status data
     */
    getSystemStatus() {
        const dashboardData = this.getCachedData('dashboard');
        return dashboardData ? dashboardData.system_status : null;
    }
    
    /**
     * Get network status data
     */
    getNetworkStatus() {
        const dashboardData = this.getCachedData('dashboard');
        return dashboardData ? dashboardData.network_status : null;
    }
    
    /**
     * Get cellular data
     */
    getCellularData() {
        const dashboardData = this.getCachedData('dashboard');
        return dashboardData ? dashboardData.cellular : null;
    }
    
    /**
     * Get performance data
     */
    getPerformanceData() {
        const dashboardData = this.getCachedData('dashboard');
        return dashboardData ? dashboardData.performance : null;
    }
    
    /**
     * Check if data is fresh (within specified age)
     */
    isDataFresh(maxAgeMs = 60000) {
        if (!this.lastUpdate) return false;
        return (Date.now() - new Date(this.lastUpdate).getTime()) < maxAgeMs;
    }
    
    /**
     * Register event callback
     */
    on(event, callback) {
        if (this.callbacks.hasOwnProperty(event)) {
            this.callbacks[event] = callback;
        }
    }
    
    /**
     * Unregister event callback
     */
    off(event) {
        if (this.callbacks.hasOwnProperty(event)) {
            this.callbacks[event] = () => {};
        }
    }
    
    /**
     * Destroy the WebSocket manager
     */
    destroy() {
        console.log('Destroying Dashboard WebSocket Manager...');
        
        // Clear subscribers
        this.subscribers.clear();
        
        // Clear data cache
        this.dataCache.clear();
        
        // Disconnect WebSocket
        if (this.wsClient) {
            this.wsClient.disconnect();
            this.wsClient = null;
        }
        
        this.isInitialized = false;
    }
}

// Global instance for shared use across the application
let globalDashboardWS = null;

/**
 * Get or create global dashboard WebSocket instance
 */
function getDashboardWebSocket(options = {}) {
    if (!globalDashboardWS) {
        globalDashboardWS = new DashboardWebSocketManager(options);
    }
    return globalDashboardWS;
}

/**
 * Destroy global dashboard WebSocket instance
 */
function destroyDashboardWebSocket() {
    if (globalDashboardWS) {
        globalDashboardWS.destroy();
        globalDashboardWS = null;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { DashboardWebSocketManager, getDashboardWebSocket, destroyDashboardWebSocket };
} else if (typeof window !== 'undefined') {
    window.DashboardWebSocketManager = DashboardWebSocketManager;
    window.getDashboardWebSocket = getDashboardWebSocket;
    window.destroyDashboardWebSocket = destroyDashboardWebSocket;
}

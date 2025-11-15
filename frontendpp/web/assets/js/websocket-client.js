/**
 * WebSocket Client System for UR WebIF Frontend
 * Provides real-time communication with backend WebSocket service
 */

class WebSocketClient {
    constructor(options = {}) {
        this.url = options.url || 'ws://localhost:8765';
        this.reconnectInterval = options.reconnectInterval || 5000;
        this.maxReconnectAttempts = options.maxReconnectAttempts || 10;
        this.timeout = options.timeout || 10000;
        
        this.websocket = null;
        this.reconnectAttempts = 0;
        this.isConnected = false;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.messageQueue = [];
        
        // Event callbacks
        this.callbacks = {
            onConnect: options.onConnect || (() => {}),
            onDisconnect: options.onDisconnect || (() => {}),
            onMessage: options.onMessage || (() => {}),
            onError: options.onError || (() => {}),
            onInitialData: options.onInitialData || (() => {}),
            onDataUpdate: options.onDataUpdate || (() => {}),
            onPeriodicUpdate: options.onPeriodicUpdate || (() => {}),
            onSystemNotification: options.onSystemNotification || (() => {})
        };
        
        // Message handlers for different types
        this.messageHandlers = {
            'initial_data': this.handleInitialData.bind(this),
            'data_response': this.handleDataResponse.bind(this),
            'data_update': this.handleDataUpdate.bind(this),
            'periodic_update': this.handlePeriodicUpdate.bind(this),
            'system_notification': this.handleSystemNotification.bind(this)
        };
        
        this.init();
    }
    
    /**
     * Initialize WebSocket connection
     */
    init() {
        try {
            this.connect();
        } catch (error) {
            console.error('WebSocket initialization error:', error);
            this.callbacks.onError(error);
        }
    }
    
    /**
     * Establish WebSocket connection
     */
    connect() {
        try {
            console.log(`Connecting to WebSocket server at ${this.url}...`);
            
            this.websocket = new WebSocket(this.url);
            
            // Set up event listeners
            this.websocket.onopen = this.handleOpen.bind(this);
            this.websocket.onclose = this.handleClose.bind(this);
            this.websocket.onmessage = this.handleMessage.bind(this);
            this.websocket.onerror = this.handleError.bind(this);
            
            // Set connection timeout
            setTimeout(() => {
                if (this.websocket && this.websocket.readyState === WebSocket.CONNECTING) {
                    console.warn('WebSocket connection timeout');
                    this.websocket.close();
                }
            }, this.timeout);
            
        } catch (error) {
            console.error('WebSocket connection error:', error);
            this.callbacks.onError(error);
            this.scheduleReconnect();
        }
    }
    
    /**
     * Handle WebSocket open event
     */
    handleOpen(event) {
        console.log('WebSocket connection established');
        this.isConnected = true;
        this.reconnectAttempts = 0;
        
        // Clear any pending reconnect timer
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        // Start heartbeat
        this.startHeartbeat();
        
        // Send queued messages
        this.flushMessageQueue();
        
        // Notify callbacks
        this.callbacks.onConnect(event);
    }
    
    /**
     * Handle WebSocket close event
     */
    handleClose(event) {
        console.log(`WebSocket connection closed: ${event.code} - ${event.reason}`);
        this.isConnected = false;
        this.websocket = null;
        
        // Stop heartbeat
        this.stopHeartbeat();
        
        // Notify callbacks
        this.callbacks.onDisconnect(event);
        
        // Schedule reconnection if not a normal closure
        if (event.code !== 1000) {
            this.scheduleReconnect();
        }
    }
    
    /**
     * Handle WebSocket message event
     */
    handleMessage(event) {
        try {
            const message = JSON.parse(event.data);
            console.log('WebSocket message received:', message);
            
            // Call general message callback
            this.callbacks.onMessage(message);
            
            // Route to specific handler based on message type
            const messageType = message.type;
            if (this.messageHandlers[messageType]) {
                this.messageHandlers[messageType](message);
            } else {
                console.warn(`Unknown message type: ${messageType}`);
            }
            
        } catch (error) {
            console.error('Error parsing WebSocket message:', error);
        }
    }
    
    /**
     * Handle WebSocket error event
     */
    handleError(event) {
        console.error('WebSocket error:', event);
        this.callbacks.onError(event);
    }
    
    /**
     * Handle initial data message
     */
    handleInitialData(message) {
        console.log('Initial data received:', message);
        this.callbacks.onInitialData(message.data, message.timestamp);
    }
    
    /**
     * Handle data response message
     */
    handleDataResponse(message) {
        console.log('Data response received:', message);
        this.callbacks.onDataUpdate(message.data, message.timestamp);
    }
    
    /**
     * Handle data update message
     */
    handleDataUpdate(message) {
        console.log('Data update received:', message);
        this.callbacks.onDataUpdate(message.data, message.timestamp);
    }
    
    /**
     * Handle periodic update message
     */
    handlePeriodicUpdate(message) {
        console.log('Periodic update received:', message);
        this.callbacks.onPeriodicUpdate(message.data, message.timestamp);
    }
    
    /**
     * Handle system notification message
     */
    handleSystemNotification(message) {
        console.log('System notification received:', message);
        this.callbacks.onSystemNotification(message.message, message.level, message.timestamp);
    }
    
    /**
     * Send message to WebSocket server
     */
    send(message) {
        if (this.isConnected && this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            try {
                const messageString = typeof message === 'string' ? message : JSON.stringify(message);
                this.websocket.send(messageString);
                console.log('Message sent:', message);
            } catch (error) {
                console.error('Error sending message:', error);
                this.callbacks.onError(error);
            }
        } else {
            // Queue message for later sending
            this.messageQueue.push(message);
            console.log('Message queued (not connected):', message);
        }
    }
    
    /**
     * Send queued messages
     */
    flushMessageQueue() {
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            this.send(message);
        }
    }
    
    /**
     * Request current data from server
     */
    requestData() {
        this.send({
            type: 'get_data',
            timestamp: new Date().toISOString()
        });
    }
    
    /**
     * Update data on server
     */
    updateData(data) {
        this.send({
            type: 'update_data',
            data: data,
            timestamp: new Date().toISOString()
        });
    }
    
    /**
     * Request data refresh from server
     */
    refreshData() {
        this.send({
            type: 'refresh_data',
            timestamp: new Date().toISOString()
        });
    }
    
    /**
     * Send system command to server
     */
    sendSystemCommand(command, params = {}) {
        this.send({
            type: 'system_command',
            command: command,
            params: params,
            timestamp: new Date().toISOString()
        });
    }
    
    /**
     * Start heartbeat to keep connection alive
     */
    startHeartbeat() {
        this.heartbeatTimer = setInterval(() => {
            if (this.isConnected) {
                this.send({ type: 'heartbeat', timestamp: new Date().toISOString() });
            }
        }, 30000); // Send heartbeat every 30 seconds
    }
    
    /**
     * Stop heartbeat
     */
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
        }
    }
    
    /**
     * Schedule reconnection attempt
     */
    scheduleReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            const delay = this.reconnectInterval * Math.pow(1.5, this.reconnectAttempts - 1); // Exponential backoff
            
            console.log(`Scheduling reconnection attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts} in ${delay}ms`);
            
            this.reconnectTimer = setTimeout(() => {
                this.connect();
            }, delay);
        } else {
            console.error('Maximum reconnection attempts reached');
            this.callbacks.onError(new Error('Maximum reconnection attempts reached'));
        }
    }
    
    /**
     * Close WebSocket connection
     */
    disconnect() {
        console.log('Disconnecting WebSocket...');
        
        // Clear timers
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        this.stopHeartbeat();
        
        // Close connection
        if (this.websocket) {
            this.websocket.close(1000, 'Client disconnect');
            this.websocket = null;
        }
        
        this.isConnected = false;
        this.reconnectAttempts = 0;
    }
    
    /**
     * Get connection status
     */
    getStatus() {
        return {
            isConnected: this.isConnected,
            readyState: this.websocket ? this.websocket.readyState : WebSocket.CLOSED,
            reconnectAttempts: this.reconnectAttempts,
            queuedMessages: this.messageQueue.length
        };
    }
    
    /**
     * Register event callback
     */
    on(event, callback) {
        if (this.callbacks.hasOwnProperty(event)) {
            this.callbacks[event] = callback;
        } else {
            console.warn(`Unknown event: ${event}`);
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
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = WebSocketClient;
} else if (typeof window !== 'undefined') {
    window.WebSocketClient = WebSocketClient;
}

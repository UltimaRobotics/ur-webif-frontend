/**
 * WebSocket Login Manager
 * Integrates WebSocket functionality with the login page
 */

class LoginWebSocketManager {
    constructor() {
        this.wsClient = null;
        this.isAuthenticated = false;
        this.sessionToken = null;
        this.userData = null;
        
        // DOM elements
        this.loginForm = null;
        this.usernameInput = null;
        this.passwordInput = null;
        this.submitButton = null;
        this.statusIndicator = null;
        
        this.init();
    }
    
    /**
     * Initialize the login WebSocket manager
     */
    init() {
        this.cacheDOMElements();
        this.setupEventListeners();
        this.initializeWebSocket();
    }
    
    /**
     * Cache frequently used DOM elements
     */
    cacheDOMElements() {
        this.loginForm = document.getElementById('login-form');
        this.usernameInput = document.getElementById('username');
        this.passwordInput = document.getElementById('password');
        this.submitButton = this.loginForm?.querySelector('button[type="submit"]');
        this.statusIndicator = this.createStatusIndicator();
    }
    
    /**
     * Create WebSocket connection status indicator
     */
    createStatusIndicator() {
        const indicator = document.createElement('div');
        indicator.id = 'websocket-status';
        indicator.className = 'fixed top-4 right-4 px-3 py-1 rounded-full text-xs font-medium flex items-center gap-2 z-50';
        indicator.innerHTML = `
            <span class="w-2 h-2 rounded-full bg-gray-400"></span>
            <span class="status-text">Connecting...</span>
        `;
        document.body.appendChild(indicator);
        return indicator;
    }
    
    /**
     * Update status indicator
     */
    updateStatus(status, text) {
        if (!this.statusIndicator) return;
        
        const dot = this.statusIndicator.querySelector('span:first-child');
        const textElement = this.statusIndicator.querySelector('.status-text');
        
        // Remove all status classes
        this.statusIndicator.className = this.statusIndicator.className.replace(/bg-\w+-100|text-\w+-800/g, '');
        
        switch (status) {
            case 'connected':
                dot.className = 'w-2 h-2 rounded-full bg-green-500';
                this.statusIndicator.classList.add('bg-green-100', 'text-green-800');
                textElement.textContent = text || 'Connected';
                break;
            case 'connecting':
                dot.className = 'w-2 h-2 rounded-full bg-yellow-500 animate-pulse';
                this.statusIndicator.classList.add('bg-yellow-100', 'text-yellow-800');
                textElement.textContent = text || 'Connecting...';
                break;
            case 'disconnected':
                dot.className = 'w-2 h-2 rounded-full bg-red-500';
                this.statusIndicator.classList.add('bg-red-100', 'text-red-800');
                textElement.textContent = text || 'Disconnected';
                break;
            case 'authenticated':
                dot.className = 'w-2 h-2 rounded-full bg-blue-500';
                this.statusIndicator.classList.add('bg-blue-100', 'text-blue-800');
                textElement.textContent = text || 'Authenticated';
                break;
        }
    }
    
    /**
     * Initialize WebSocket client
     */
    initializeWebSocket() {
        this.wsClient = new WebSocketClient({
            url: 'ws://localhost:8765',
            onConnect: () => this.handleWebSocketConnect(),
            onDisconnect: () => this.handleWebSocketDisconnect(),
            onError: (error) => this.handleWebSocketError(error),
            onMessage: (message) => this.handleWebSocketMessage(message),
            onInitialData: (data, timestamp) => this.handleInitialData(data, timestamp),
            onDataUpdate: (data, timestamp) => this.handleDataUpdate(data, timestamp),
            onSystemNotification: (message, level, timestamp) => this.handleSystemNotification(message, level, timestamp)
        });
    }
    
    /**
     * Set up event listeners for the login form
     */
    setupEventListeners() {
        if (this.loginForm) {
            this.loginForm.addEventListener('submit', (e) => this.handleLoginSubmit(e));
        }
        
        // Add keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.key === 'F12') {
                e.preventDefault();
                this.toggleWebSocketDebug();
            }
        });
    }
    
    /**
     * Handle WebSocket connection established
     */
    handleWebSocketConnect() {
        console.log('Login WebSocket connected');
        this.updateStatus('connected', 'WebSocket Connected');
        
        // Enable login form if it was disabled
        if (this.submitButton) {
            this.submitButton.disabled = false;
            this.submitButton.innerHTML = `
                <i class="fa-solid fa-sign-in-alt mr-2"></i>
                Sign In
            `;
        }
    }
    
    /**
     * Handle WebSocket disconnection
     */
    handleWebSocketDisconnect() {
        console.log('Login WebSocket disconnected');
        this.updateStatus('disconnected', 'WebSocket Disconnected');
        
        // Disable login form if WebSocket is not available
        if (this.submitButton) {
            this.submitButton.disabled = true;
            this.submitButton.innerHTML = `
                <i class="fa-solid fa-exclamation-triangle mr-2"></i>
                No Connection
            `;
        }
    }
    
    /**
     * Handle WebSocket errors
     */
    handleWebSocketError(error) {
        console.error('Login WebSocket error:', error);
        this.updateStatus('disconnected', 'Connection Error');
        
        // Show error notification
        this.showNotification('WebSocket connection error. Some features may be unavailable.', 'error');
    }
    
    /**
     * Handle WebSocket messages
     */
    handleWebSocketMessage(message) {
        console.log('Login WebSocket message:', message);
        
        // Handle specific message types for login page
        switch (message.type) {
            case 'auth_challenge':
                this.handleAuthChallenge(message);
                break;
            case 'auth_response':
                this.handleAuthResponse(message);
                break;
        }
    }
    
    /**
     * Handle initial data from WebSocket
     */
    handleInitialData(data, timestamp) {
        console.log('Initial data received:', data);
        
        // Store system status for potential use in login
        this.systemData = data;
        
        // Update status with system info
        if (data.system_status) {
            const uptime = data.system_status.uptime;
            this.updateStatus('connected', `Connected (${uptime})`);
        }
    }
    
    /**
     * Handle data updates from WebSocket
     */
    handleDataUpdate(data, timestamp) {
        console.log('Data update received:', data);
        this.systemData = data;
    }
    
    /**
     * Handle system notifications from WebSocket
     */
    handleSystemNotification(message, level, timestamp) {
        console.log(`System notification (${level}):`, message);
        this.showNotification(message, level);
    }
    
    /**
     * Handle login form submission
     */
    async handleLoginSubmit(event) {
        event.preventDefault();
        
        const username = this.usernameInput?.value;
        const password = this.passwordInput?.value;
        
        if (!username || !password) {
            this.showNotification('Please enter both username and password', 'error');
            return;
        }
        
        // Show loading state
        this.setLoadingState(true);
        
        try {
            // Perform HTTP login first
            const loginResult = await this.performHttpLogin(username, password);
            
            if (loginResult.success) {
                // Store session data
                this.sessionToken = loginResult.session_token;
                this.userData = loginResult.user;
                this.isAuthenticated = true;
                
                // Notify WebSocket server about successful login
                this.notifyWebSocketLogin(username, loginResult);
                
                // Update status
                this.updateStatus('authenticated', `Authenticated as ${username}`);
                
                // Show success message
                this.showNotification('Login successful! Redirecting...', 'success');
                
                // Redirect to dashboard after a short delay
                setTimeout(() => {
                    this.redirectToDashboard();
                }, 1500);
                
            } else {
                this.showNotification(loginResult.message || 'Login failed', 'error');
            }
            
        } catch (error) {
            console.error('Login error:', error);
            this.showNotification('Login error: ' + error.message, 'error');
        } finally {
            this.setLoadingState(false);
        }
    }
    
    /**
     * Perform HTTP login request
     */
    async performHttpLogin(username, password) {
        const response = await fetch('/api/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                username: username,
                password: password
            })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        return await response.json();
    }
    
    /**
     * Notify WebSocket server about successful login
     */
    notifyWebSocketLogin(username, loginData) {
        if (this.wsClient && this.wsClient.isConnected) {
            this.wsClient.send({
                type: 'user_login',
                username: username,
                user_data: loginData.user,
                session_token: loginData.session_token,
                timestamp: new Date().toISOString()
            });
        }
    }
    
    /**
     * Set loading state for the login form
     */
    setLoadingState(isLoading) {
        if (this.submitButton) {
            this.submitButton.disabled = isLoading;
            if (isLoading) {
                this.submitButton.innerHTML = `
                    <i class="fa-solid fa-spinner fa-spin mr-2"></i>
                    Signing In...
                `;
            } else {
                this.submitButton.innerHTML = `
                    <i class="fa-solid fa-sign-in-alt mr-2"></i>
                    Sign In
                `;
            }
        }
        
        // Disable inputs during loading
        if (this.usernameInput) this.usernameInput.disabled = isLoading;
        if (this.passwordInput) this.passwordInput.disabled = isLoading;
    }
    
    /**
     * Show notification message
     */
    showNotification(message, type = 'info') {
        // Create notification element
        const notification = document.createElement('div');
        notification.className = `fixed top-20 right-4 max-w-sm p-4 rounded-lg shadow-lg z-50 transform transition-all duration-300 translate-x-full`;
        
        // Set color based on type
        switch (type) {
            case 'success':
                notification.classList.add('bg-green-500', 'text-white');
                break;
            case 'error':
                notification.classList.add('bg-red-500', 'text-white');
                break;
            case 'warning':
                notification.classList.add('bg-yellow-500', 'text-white');
                break;
            default:
                notification.classList.add('bg-blue-500', 'text-white');
        }
        
        notification.innerHTML = `
            <div class="flex items-center">
                <i class="fa-solid fa-${type === 'success' ? 'check-circle' : type === 'error' ? 'exclamation-circle' : 'info-circle'} mr-3"></i>
                <span class="flex-1">${message}</span>
                <button class="ml-3 hover:opacity-75" onclick="this.parentElement.parentElement.remove()">
                    <i class="fa-solid fa-times"></i>
                </button>
            </div>
        `;
        
        document.body.appendChild(notification);
        
        // Slide in animation
        setTimeout(() => {
            notification.classList.remove('translate-x-full');
            notification.classList.add('translate-x-0');
        }, 100);
        
        // Auto-remove after 5 seconds
        setTimeout(() => {
            if (notification.parentElement) {
                notification.classList.add('translate-x-full');
                setTimeout(() => notification.remove(), 300);
            }
        }, 5000);
    }
    
    /**
     * Redirect to dashboard after successful login
     */
    redirectToDashboard() {
        // Store session data for dashboard
        if (this.sessionToken) {
            sessionStorage.setItem('session_token', this.sessionToken);
            sessionStorage.setItem('user_data', JSON.stringify(this.userData));
            sessionStorage.setItem('websocket_connected', this.wsClient.isConnected.toString());
        }
        
        // Redirect to main page
        window.location.href = '/source.html';
    }
    
    /**
     * Toggle WebSocket debug information
     */
    toggleWebSocketDebug() {
        const debugInfo = document.getElementById('websocket-debug');
        
        if (debugInfo) {
            debugInfo.remove();
        } else {
            const debugPanel = document.createElement('div');
            debugPanel.id = 'websocket-debug';
            debugPanel.className = 'fixed bottom-4 right-4 w-80 bg-gray-900 text-white p-4 rounded-lg shadow-lg z-50 font-mono text-xs';
            debugPanel.innerHTML = `
                <div class="flex justify-between items-center mb-2">
                    <h3 class="font-bold">WebSocket Debug</h3>
                    <button onclick="this.parentElement.parentElement.remove()" class="text-gray-400 hover:text-white">
                        <i class="fa-solid fa-times"></i>
                    </button>
                </div>
                <div class="space-y-1">
                    <div>Status: ${this.wsClient ? JSON.stringify(this.wsClient.getStatus(), null, 2) : 'Not initialized'}</div>
                    <div>URL: ws://localhost:8765</div>
                    <div>Authenticated: ${this.isAuthenticated}</div>
                    <div>Session Token: ${this.sessionToken ? '***' + this.sessionToken.slice(-4) : 'None'}</div>
                </div>
                <div class="mt-2 pt-2 border-t border-gray-700">
                    <button onclick="window.loginWSManager.requestData()" class="bg-blue-600 hover:bg-blue-700 px-2 py-1 rounded text-xs">
                        Request Data
                    </button>
                    <button onclick="window.loginWSManager.refreshData()" class="bg-green-600 hover:bg-green-700 px-2 py-1 rounded text-xs ml-2">
                        Refresh Data
                    </button>
                </div>
            `;
            document.body.appendChild(debugPanel);
        }
    }
    
    /**
     * Request data from WebSocket server
     */
    requestData() {
        if (this.wsClient && this.wsClient.isConnected) {
            this.wsClient.requestData();
        } else {
            this.showNotification('WebSocket not connected', 'error');
        }
    }
    
    /**
     * Refresh data from WebSocket server
     */
    refreshData() {
        if (this.wsClient && this.wsClient.isConnected) {
            this.wsClient.refreshData();
        } else {
            this.showNotification('WebSocket not connected', 'error');
        }
    }
    
    /**
     * Cleanup resources
     */
    destroy() {
        if (this.wsClient) {
            this.wsClient.disconnect();
        }
        
        // Remove status indicator
        if (this.statusIndicator) {
            this.statusIndicator.remove();
        }
        
        // Remove debug panel if exists
        const debugPanel = document.getElementById('websocket-debug');
        if (debugPanel) {
            debugPanel.remove();
        }
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.loginWSManager = new LoginWebSocketManager();
    
    // Cleanup on page unload
    window.addEventListener('beforeunload', () => {
        if (window.loginWSManager) {
            window.loginWSManager.destroy();
        }
    });
});

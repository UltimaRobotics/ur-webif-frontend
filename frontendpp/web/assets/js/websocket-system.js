/**
 * WebSocket System Startup Script
 * Initializes and manages the entire WebSocket system
 */

class WebSocketSystem {
    constructor() {
        this.isInitialized = false;
        this.loginManager = null;
        this.dashboardManager = null;
        this.config = {
            websocketUrl: 'ws://localhost:8765',
            autoStart: true,
            enableTesting: true,
            debugMode: false
        };
    }
    
    /**
     * Initialize the WebSocket system
     */
    init(options = {}) {
        if (this.isInitialized) {
            console.warn('WebSocket system already initialized');
            return;
        }
        
        // Merge configuration
        this.config = { ...this.config, ...options };
        
        console.log('🚀 Initializing WebSocket System...');
        console.log('📡 WebSocket URL:', this.config.websocketUrl);
        console.log('🔧 Auto-start:', this.config.autoStart);
        console.log('🧪 Testing enabled:', this.config.enableTesting);
        
        try {
            // Initialize login manager if available
            if (typeof window.loginWSManager !== 'undefined') {
                this.loginManager = window.loginWSManager;
                console.log('✅ Login WebSocket manager found');
            }
            
            // Initialize dashboard manager
            this.dashboardManager = window.getDashboardWebSocket({
                url: this.config.websocketUrl,
                autoRefresh: true,
                refreshInterval: 30000
            });
            
            console.log('✅ Dashboard WebSocket manager initialized');
            
            // Set up global testing functions if enabled
            if (this.config.enableTesting) {
                this.setupTestingFunctions();
            }
            
            // Set up debug functions if debug mode is enabled
            if (this.config.debugMode) {
                this.setupDebugFunctions();
            }
            
            // Set up keyboard shortcuts
            this.setupKeyboardShortcuts();
            
            // Set up error handling
            this.setupErrorHandling();
            
            this.isInitialized = true;
            console.log('🎉 WebSocket system initialization complete');
            
            // Notify of successful initialization
            this.showSystemNotification('WebSocket system initialized successfully', 'success');
            
        } catch (error) {
            console.error('❌ WebSocket system initialization failed:', error);
            this.showSystemNotification('WebSocket system initialization failed: ' + error.message, 'error');
        }
    }
    
    /**
     * Setup testing functions
     */
    setupTestingFunctions() {
        // Add test button to page
        const testButton = document.createElement('button');
        testButton.id = 'websocket-test-btn';
        testButton.className = 'fixed bottom-4 left-4 bg-purple-600 hover:bg-purple-700 text-white px-4 py-2 rounded-lg shadow-lg z-40 text-sm font-medium';
        testButton.innerHTML = '<i class="fa-solid fa-flask mr-2"></i>Test WebSocket';
        testButton.onclick = () => this.runTests();
        document.body.appendChild(testButton);
        
        // Make testing functions globally available
        window.testWebSocket = () => this.runTests();
        window.testIntegration = () => this.runIntegrationTests();
        window.websocketStatus = () => this.getStatus();
    }
    
    /**
     * Setup debug functions
     */
    setupDebugFunctions() {
        // Add debug panel toggle button
        const debugButton = document.createElement('button');
        debugButton.id = 'websocket-debug-btn';
        debugButton.className = 'fixed bottom-4 left-48 bg-gray-800 hover:bg-gray-900 text-white px-4 py-2 rounded-lg shadow-lg z-40 text-sm font-medium';
        debugButton.innerHTML = '<i class="fa-solid fa-bug mr-2"></i>Debug';
        debugButton.onclick = () => this.toggleDebugPanel();
        document.body.appendChild(debugButton);
        
        window.toggleWebSocketDebug = () => this.toggleDebugPanel();
        window.getWebSocketInfo = () => this.getDebugInfo();
    }
    
    /**
     * Setup keyboard shortcuts
     */
    setupKeyboardShortcuts() {
        document.addEventListener('keydown', (e) => {
            // Ctrl+Shift+W: Run WebSocket tests
            if (e.ctrlKey && e.shiftKey && e.key === 'W') {
                e.preventDefault();
                this.runTests();
            }
            
            // Ctrl+Shift+D: Toggle debug panel
            if (e.ctrlKey && e.shiftKey && e.key === 'D') {
                e.preventDefault();
                this.toggleDebugPanel();
            }
            
            // Ctrl+Shift+S: Show WebSocket status
            if (e.ctrlKey && e.shiftKey && e.key === 'S') {
                e.preventDefault();
                this.showStatus();
            }
        });
    }
    
    /**
     * Setup error handling
     */
    setupErrorHandling() {
        window.addEventListener('error', (event) => {
            if (event.message && event.message.toLowerCase().includes('websocket')) {
                console.error('WebSocket error caught by global handler:', event.error);
                this.showSystemNotification('WebSocket error: ' + event.message, 'error');
            }
        });
        
        window.addEventListener('unhandledrejection', (event) => {
            if (event.reason && event.reason.toString().toLowerCase().includes('websocket')) {
                console.error('WebSocket promise rejection caught:', event.reason);
                this.showSystemNotification('WebSocket promise rejection: ' + event.reason, 'error');
            }
        });
    }
    
    /**
     * Run WebSocket tests
     */
    async runTests() {
        console.log('🧪 Running WebSocket system tests...');
        
        if (typeof window.runWebSocketTests !== 'undefined') {
            await window.runWebSocketTests();
        } else {
            console.error('WebSocket tester not available');
            this.showSystemNotification('WebSocket tester not available', 'error');
        }
    }
    
    /**
     * Run integration tests
     */
    async runIntegrationTests() {
        console.log('🔗 Running WebSocket integration tests...');
        
        if (typeof window.runIntegrationTests !== 'undefined') {
            await window.runIntegrationTests();
        } else {
            console.error('Integration tests not available');
            this.showSystemNotification('Integration tests not available', 'error');
        }
    }
    
    /**
     * Get WebSocket system status
     */
    getStatus() {
        const status = {
            initialized: this.isInitialized,
            loginManager: null,
            dashboardManager: null
        };
        
        if (this.loginManager) {
            status.loginManager = {
                connected: this.loginManager.wsClient ? this.loginManager.wsClient.isConnected : false,
                authenticated: this.loginManager.isAuthenticated,
                hasSessionToken: !!this.loginManager.sessionToken
            };
        }
        
        if (this.dashboardManager) {
            status.dashboardManager = this.dashboardManager.getStatus();
        }
        
        return status;
    }
    
    /**
     * Show WebSocket status
     */
    showStatus() {
        const status = this.getStatus();
        console.log('📊 WebSocket System Status:', status);
        
        // Create status popup
        const statusDiv = document.createElement('div');
        statusDiv.className = 'fixed top-20 left-4 w-96 bg-white rounded-lg shadow-xl p-6 z-50';
        statusDiv.innerHTML = `
            <div class="flex justify-between items-center mb-4">
                <h3 class="text-lg font-bold text-gray-900">WebSocket System Status</h3>
                <button onclick="this.parentElement.parentElement.remove()" class="text-gray-400 hover:text-gray-600">
                    <i class="fa-solid fa-times"></i>
                </button>
            </div>
            <div class="space-y-3 text-sm">
                <div class="flex justify-between">
                    <span class="font-medium">System Initialized:</span>
                    <span class="${status.initialized ? 'text-green-600' : 'text-red-600'}">${status.initialized ? 'Yes' : 'No'}</span>
                </div>
                ${status.loginManager ? `
                <div class="border-t pt-3">
                    <div class="font-medium mb-2">Login Manager:</div>
                    <div class="ml-4 space-y-1">
                        <div class="flex justify-between">
                            <span>Connected:</span>
                            <span class="${status.loginManager.connected ? 'text-green-600' : 'text-red-600'}">${status.loginManager.connected ? 'Yes' : 'No'}</span>
                        </div>
                        <div class="flex justify-between">
                            <span>Authenticated:</span>
                            <span class="${status.loginManager.authenticated ? 'text-green-600' : 'text-gray-600'}">${status.loginManager.authenticated ? 'Yes' : 'No'}</span>
                        </div>
                    </div>
                </div>
                ` : ''}
                ${status.dashboardManager ? `
                <div class="border-t pt-3">
                    <div class="font-medium mb-2">Dashboard Manager:</div>
                    <div class="ml-4 space-y-1">
                        <div class="flex justify-between">
                            <span>Connected:</span>
                            <span class="${status.dashboardManager.isConnected ? 'text-green-600' : 'text-red-600'}">${status.dashboardManager.isConnected ? 'Yes' : 'No'}</span>
                        </div>
                        <div class="flex justify-between">
                            <span>Subscribers:</span>
                            <span>${status.dashboardManager.subscribersCount}</span>
                        </div>
                        <div class="flex justify-between">
                            <span>Last Update:</span>
                            <span>${status.dashboardManager.lastUpdate ? new Date(status.dashboardManager.lastUpdate).toLocaleTimeString() : 'Never'}</span>
                        </div>
                    </div>
                </div>
                ` : ''}
            </div>
            <div class="mt-4 pt-4 border-t flex gap-2">
                <button onclick="window.testWebSocket()" class="bg-blue-600 hover:bg-blue-700 text-white px-3 py-1 rounded text-xs">
                    Run Tests
                </button>
                <button onclick="window.toggleWebSocketDebug()" class="bg-gray-600 hover:bg-gray-700 text-white px-3 py-1 rounded text-xs">
                    Debug Info
                </button>
            </div>
        `;
        
        document.body.appendChild(statusDiv);
    }
    
    /**
     * Toggle debug panel
     */
    toggleDebugPanel() {
        const existingPanel = document.getElementById('websocket-system-debug');
        
        if (existingPanel) {
            existingPanel.remove();
            return;
        }
        
        const debugInfo = this.getDebugInfo();
        
        const debugPanel = document.createElement('div');
        debugPanel.id = 'websocket-system-debug';
        debugPanel.className = 'fixed bottom-20 right-4 w-96 bg-gray-900 text-white p-4 rounded-lg shadow-xl z-50 font-mono text-xs';
        debugPanel.innerHTML = `
            <div class="flex justify-between items-center mb-3">
                <h3 class="font-bold text-sm">WebSocket System Debug</h3>
                <button onclick="this.parentElement.parentElement.remove()" class="text-gray-400 hover:text-white">
                    <i class="fa-solid fa-times"></i>
                </button>
            </div>
            <div class="space-y-2 max-h-64 overflow-y-auto">
                <div class="text-green-400">System Status:</div>
                <pre class="ml-2 text-gray-300">${JSON.stringify(debugInfo, null, 2)}</pre>
            </div>
            <div class="mt-3 pt-3 border-t border-gray-700 flex gap-2">
                <button onclick="navigator.clipboard.writeText(JSON.stringify(window.getWebSocketInfo(), null, 2))" class="bg-blue-600 hover:bg-blue-700 px-2 py-1 rounded text-xs">
                    Copy Info
                </button>
                <button onclick="window.testWebSocket()" class="bg-green-600 hover:bg-green-700 px-2 py-1 rounded text-xs">
                    Test Connection
                </button>
                <button onclick="window.location.reload()" class="bg-red-600 hover:bg-red-700 px-2 py-1 rounded text-xs">
                    Reload Page
                </button>
            </div>
        `;
        
        document.body.appendChild(debugPanel);
    }
    
    /**
     * Get debug information
     */
    getDebugInfo() {
        const info = {
            timestamp: new Date().toISOString(),
            userAgent: navigator.userAgent,
            websocketUrl: this.config.websocketUrl,
            systemInitialized: this.isInitialized,
            loginManager: null,
            dashboardManager: null
        };
        
        if (this.loginManager && this.loginManager.wsClient) {
            info.loginManager = {
                status: this.loginManager.wsClient.getStatus(),
                authenticated: this.loginManager.isAuthenticated,
                hasSessionToken: !!this.loginManager.sessionToken
            };
        }
        
        if (this.dashboardManager) {
            info.dashboardManager = this.dashboardManager.getStatus();
        }
        
        return info;
    }
    
    /**
     * Show system notification
     */
    showSystemNotification(message, type = 'info') {
        // Use login manager's notification if available, otherwise create simple notification
        if (this.loginManager && typeof this.loginManager.showNotification === 'function') {
            this.loginManager.showNotification(message, type);
        } else {
            console.log(`[${type.toUpperCase()}] ${message}`);
        }
    }
    
    /**
     * Destroy the WebSocket system
     */
    destroy() {
        console.log('🛑 Destroying WebSocket system...');
        
        if (this.dashboardManager) {
            this.dashboardManager.destroy();
            this.dashboardManager = null;
        }
        
        if (this.loginManager) {
            this.loginManager.destroy();
            this.loginManager = null;
        }
        
        // Remove UI elements
        const elements = ['websocket-test-btn', 'websocket-debug-btn', 'websocket-system-debug'];
        elements.forEach(id => {
            const element = document.getElementById(id);
            if (element) element.remove();
        });
        
        this.isInitialized = false;
        console.log('✅ WebSocket system destroyed');
    }
}

// Global WebSocket system instance
let wsSystem = null;

/**
 * Initialize WebSocket system
 */
function initWebSocketSystem(options = {}) {
    if (!wsSystem) {
        wsSystem = new WebSocketSystem();
    }
    wsSystem.init(options);
    return wsSystem;
}

/**
 * Get WebSocket system instance
 */
function getWebSocketSystem() {
    return wsSystem;
}

/**
 * Destroy WebSocket system
 */
function destroyWebSocketSystem() {
    if (wsSystem) {
        wsSystem.destroy();
        wsSystem = null;
    }
}

// Auto-initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    // Wait a moment for other scripts to load
    setTimeout(() => {
        initWebSocketSystem({
            debugMode: window.location.search.includes('debug=true'),
            enableTesting: window.location.search.includes('testing=true') || true
        });
    }, 100);
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    destroyWebSocketSystem();
});

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { WebSocketSystem, initWebSocketSystem, getWebSocketSystem, destroyWebSocketSystem };
} else if (typeof window !== 'undefined') {
    window.WebSocketSystem = WebSocketSystem;
    window.initWebSocketSystem = initWebSocketSystem;
    window.getWebSocketSystem = getWebSocketSystem;
    window.destroyWebSocketSystem = destroyWebSocketSystem;
}

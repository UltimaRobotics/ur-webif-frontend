/**
 * Main Dashboard Integration
 * Connects WebSocket client to landing-section.html UI elements
 */

console.log('[MAIN.JS] Module loading...');

import { DashboardWebSocket } from './dashboard-websocket.js';

class DashboardManager {
    constructor() {
        this.wsClient = new DashboardWebSocket();
        this.autoRefreshEnabled = true;
        this.lastUpdate = null;
        
        this.setupWebSocketCallbacks();
        this.setupUIEventHandlers();
        this.updateConnectionStatus(false);
    }

    initialize() {
        console.log('[DASHBOARD] Initializing dashboard manager...');
        this.wsClient.connect();
    }

    setupWebSocketCallbacks() {
        // WebSocket connection events
        this.wsClient.onConnected = () => {
            console.log('[DASHBOARD] WebSocket connected');
            this.updateConnectionStatus(true);
            this.subscribeToUpdates();
        };

        this.wsClient.onDisconnected = () => {
            console.log('[DASHBOARD] WebSocket disconnected');
            this.updateConnectionStatus(false);
        };

        this.wsClient.onError = (error) => {
            console.error('[DASHBOARD] WebSocket error:', error);
            this.showNotification('Connection error: ' + error.message, 'error');
        };

        // Data events
        this.wsClient.onDataReceived = (data, timestamp) => {
            console.log('[DASHBOARD] Initial data received');
            this.updateAllUI(data);
            this.lastUpdate = new Date(timestamp * 1000);
            this.updateLastUpdateTime();
        };

        this.wsClient.onUpdateReceived = (category, data, timestamp) => {
            console.log('[DASHBOARD] Real-time update for category:', category);
            this.updateCategoryUI(category, data);
            this.lastUpdate = new Date(timestamp * 1000);
            this.updateLastUpdateTime();
        };
    }

    setupUIEventHandlers() {
        // Refresh button
        const refreshBtn = document.getElementById('refresh-data-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.requestFreshData();
            });
        }

        // Auto-refresh toggle
        const autoRefreshToggle = document.getElementById('auto-refresh-toggle');
        if (autoRefreshToggle) {
            autoRefreshToggle.addEventListener('click', () => {
                this.toggleAutoRefresh();
            });
        }
    }

    subscribeToUpdates() {
        this.wsClient.subscribeToUpdates();
    }

    requestFreshData() {
        console.log('[DASHBOARD] Requesting fresh data');
        this.wsClient.requestDashboardData();
        
        // Show loading state
        const refreshIcon = document.getElementById('refresh-icon');
        if (refreshIcon) {
            refreshIcon.classList.add('fa-spin');
            setTimeout(() => {
                refreshIcon.classList.remove('fa-spin');
            }, 1000);
        }
    }

    toggleAutoRefresh() {
        this.autoRefreshEnabled = !this.autoRefreshEnabled;
        
        const autoRefreshIcon = document.getElementById('auto-refresh-icon');
        const autoRefreshText = document.getElementById('auto-refresh-text');
        
        if (autoRefreshIcon && autoRefreshText) {
            if (this.autoRefreshEnabled) {
                autoRefreshIcon.classList.remove('fa-clock');
                autoRefreshIcon.classList.add('fa-pause');
                autoRefreshText.textContent = 'Auto';
            } else {
                autoRefreshIcon.classList.remove('fa-pause');
                autoRefreshIcon.classList.add('fa-clock');
                autoRefreshText.textContent = 'Manual';
            }
        }
    }

    updateAllUI(data) {
        this.updateSystemUI(data.system || {});
        this.updateRAMUI(data.ram || {});
        this.updateSwapUI(data.swap || {});
        this.updateNetworkUI(data.network || {});
        this.updateUltimaServerUI(data.ultima_server || {});
        this.updateSignalUI(data.signal || {});
    }

    updateCategoryUI(category, data) {
        switch (category) {
            case 'system':
                this.updateSystemUI(data);
                break;
            case 'ram':
                this.updateRAMUI(data);
                break;
            case 'swap':
                this.updateSwapUI(data);
                break;
            case 'network':
                this.updateNetworkUI(data);
                break;
            case 'ultima_server':
                this.updateUltimaServerUI(data);
                break;
            case 'signal':
                this.updateSignalUI(data);
                break;
        }
    }

    updateSystemUI(data) {
        this.updateMetric('cpu-usage', data.usage_percent, '%');
        this.updateProgressBar('cpu-progress', data.usage_percent);
        this.updateElement('cpu-cores', `${data.cores || 0} Cores`);
        this.updateElement('cpu-temp', `${data.temperature || 0}°C`);
        this.updateElement('cpu-base-clock', `${(data.base_clock || 0).toFixed(2)} GHz`);
        this.updateElement('cpu-boost-clock', `${(data.boost_clock || 0).toFixed(2)} GHz`);
    }

    updateRAMUI(data) {
        this.updateMetric('ram-usage', data.usage_percent, '%');
        this.updateProgressBar('ram-progress', data.usage_percent);
        this.updateElement('ram-used', `${(data.used_gb || 0).toFixed(2)} GB`);
        this.updateElement('ram-total', `${(data.total_gb || 0).toFixed(2)} GB`);
        this.updateElement('ram-available', `${(data.available_gb || 0).toFixed(2)} GB`);
        this.updateElement('ram-type', data.type || 'N/A');
    }

    updateSwapUI(data) {
        this.updateMetric('swap-usage', data.usage_percent, '%');
        this.updateProgressBar('swap-progress', data.usage_percent);
        this.updateElement('swap-used', `${(data.used_mb || 0).toFixed(0)} MB`);
        this.updateElement('swap-total', `${(data.total_gb || 0).toFixed(2)} GB`);
        this.updateElement('swap-available', `${(data.available_gb || 0).toFixed(2)} GB`);
        this.updateElement('swap-priority', data.priority || 'Normal');
    }

    updateNetworkUI(data) {
        // Internet status
        const internetConnected = data.status === 'connected';
        this.updateConnectionIndicator('internet-indicator', 'internet-status', internetConnected);
        this.updateElement('external-ip', data.external_ip || 'N/A');
        this.updateElement('dns-primary', data.dns_primary || 'N/A');
        this.updateElement('dns-secondary', data.dns_secondary || 'N/A');
        this.updateElement('latency', `${data.latency_ms || 0} ms`);
        this.updateElement('bandwidth', data.bandwidth || 'N/A');
    }

    updateUltimaServerUI(data) {
        const serverConnected = data.status === 'connected';
        this.updateConnectionIndicator('server-indicator', 'server-status', serverConnected);
        this.updateElement('server-hostname', data.hostname || 'N/A');
        this.updateElement('server-port', data.port || 0);
        this.updateElement('server-protocol', data.protocol || 'N/A');
        this.updateElement('last-ping', `${data.ping_ms || 0} ms`);
        this.updateElement('session-duration', data.session_duration || 'N/A');
    }

    updateSignalUI(data) {
        // Signal strength visualization
        this.updateSignalBars(data.rssi || -100);
        this.updateElement('rssi', `${data.rssi || 0} dBm`);
        this.updateElement('rsrp', `${data.rsrp || 0} dBm`);
        this.updateElement('rsrq', `${data.rsrq || 0} dB`);
        this.updateElement('sinr', `${data.sinr || 0} dB`);
        this.updateElement('cell-id', data.cell_id || 'N/A');
        
        // Connection status
        const cellularConnected = data.status === 'connected';
        this.updateConnectionIndicator('cellular-indicator', 'cellular-status', cellularConnected);
        this.updateElement('network-name', data.operator || 'N/A');
        this.updateElement('technology', data.technology || 'N/A');
        this.updateElement('band', data.band || 'N/A');
        this.updateElement('apn', data.apn || 'N/A');
        this.updateElement('data-usage', `${data.data_usage_mb || 0} MB`);
    }

    updateSignalBars(rssi) {
        const signalBars = document.querySelector('[data-signal-bars]');
        if (!signalBars) return;

        const bars = signalBars.querySelectorAll('div');
        const signalStrength = this.calculateSignalStrength(rssi);
        
        bars.forEach((bar, index) => {
            if (index < signalStrength) {
                bar.classList.remove('bg-neutral-300');
                bar.classList.add('bg-green-500');
            } else {
                bar.classList.remove('bg-green-500');
                bar.classList.add('bg-neutral-300');
            }
        });

        // Update signal status text
        const signalStatus = document.querySelector('[data-signal-status]');
        if (signalStatus) {
            const statusText = this.getSignalStatusText(rssi);
            signalStatus.textContent = statusText;
        }
    }

    calculateSignalStrength(rssi) {
        if (rssi > -50) return 4;      // Excellent
        if (rssi > -60) return 3;      // Good
        if (rssi > -70) return 2;      // Fair
        if (rssi > -80) return 1;      // Poor
        return 0;                      // No signal
    }

    getSignalStatusText(rssi) {
        if (rssi > -50) return 'Excellent';
        if (rssi > -60) return 'Good';
        if (rssi > -70) return 'Fair';
        if (rssi > -80) return 'Poor';
        return 'No Signal';
    }

    updateConnectionStatus(connected) {
        const statusDot = document.getElementById('status-dot');
        const refreshBtn = document.getElementById('refresh-data-btn');
        
        if (statusDot) {
            if (connected) {
                statusDot.className = 'w-2 h-2 bg-green-500 rounded-full animate-pulse';
            } else {
                statusDot.className = 'w-2 h-2 bg-red-500 rounded-full';
            }
        }
        
        if (refreshBtn) {
            if (connected) {
                refreshBtn.disabled = false;
                refreshBtn.classList.remove('opacity-50', 'cursor-not-allowed');
            } else {
                refreshBtn.disabled = true;
                refreshBtn.classList.add('opacity-50', 'cursor-not-allowed');
            }
        }
    }

    updateLastUpdateTime() {
        const lastUpdateElement = document.querySelector('[data-last-update]');
        if (lastUpdateElement && this.lastUpdate) {
            const now = new Date();
            const diff = Math.floor((now - this.lastUpdate) / 1000);
            
            if (diff < 60) {
                lastUpdateElement.textContent = `Updated ${diff}s ago`;
            } else if (diff < 3600) {
                lastUpdateElement.textContent = `Updated ${Math.floor(diff / 60)}m ago`;
            } else {
                lastUpdateElement.textContent = `Updated ${Math.floor(diff / 3600)}h ago`;
            }
        }
    }

    // Helper functions
    updateMetric(elementId, value, suffix = '') {
        const element = document.querySelector(`[data-${elementId}]`);
        if (element && value !== undefined) {
            element.textContent = `${Math.round(value)}${suffix}`;
        }
    }

    updateProgressBar(elementId, value) {
        const element = document.querySelector(`[data-${elementId}]`);
        if (element && value !== undefined) {
            element.style.width = `${Math.min(100, Math.max(0, value))}%`;
            
            // Update color based on value
            element.className = 'h-3 rounded-full transition-all duration-300';
            if (value > 80) {
                element.classList.add('bg-red-500');
            } else if (value > 60) {
                element.classList.add('bg-yellow-500');
            } else {
                element.classList.add('bg-green-500');
            }
        }
    }

    updateElement(elementId, value) {
        const element = document.querySelector(`[data-${elementId}]`);
        if (element && value !== undefined) {
            element.textContent = value;
        }
    }

    updateConnectionIndicator(indicatorId, statusId, connected) {
        const indicator = document.querySelector(`[data-${indicatorId}]`);
        const status = document.querySelector(`[data-${statusId}]`);
        
        if (indicator) {
            if (connected) {
                indicator.classList.remove('bg-neutral-500', 'bg-red-500');
                indicator.classList.add('bg-green-500');
            } else {
                indicator.classList.remove('bg-green-500');
                indicator.classList.add('bg-red-500');
            }
        }
        
        if (status) {
            status.textContent = connected ? 'Connected' : 'Disconnected';
        }
    }

    showNotification(message, level = 'info') {
        console.log(`[DASHBOARD] ${level}: ${message}`);
        // Could implement a toast notification here if needed
    }

    destroy() {
        console.log('[DASHBOARD] Destroying dashboard manager');
        this.wsClient.disconnect();
    }
}

// Initialize dashboard when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    console.log('[DASHBOARD] DOM loaded, initializing dashboard');
    
    const dashboard = new DashboardManager();
    dashboard.initialize();
    
    // Make dashboard available globally for debugging
    window.dashboardManager = dashboard;
});

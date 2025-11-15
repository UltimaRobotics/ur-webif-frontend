/**
 * JWT Token Management System for Frontend
 * Handles token storage, validation, and automatic refresh
 */

class JWTTokenManager {
    constructor(options = {}) {
        this.storageKey = options.storageKey || 'ur_webif_token';
        this.refreshKey = options.refreshKey || 'ur_webif_refresh_token';
        this.userKey = options.userKey || 'ur_webif_user';
        this.apiBaseUrl = options.apiBaseUrl || `${window.location.origin}/api/auth`;
        console.log('[JWT-TOKEN-MANAGER] Initialized with apiBaseUrl:', this.apiBaseUrl);
        this.autoRefreshEnabled = options.autoRefresh !== false;
        this.refreshThreshold = options.refreshThreshold || 15 * 60 * 1000; // 15 minutes
        this.refreshTimer = null;
        
        // Event callbacks
        this.callbacks = {
            onTokenRefresh: options.onTokenRefresh || (() => {}),
            onTokenExpired: options.onTokenExpired || (() => {}),
            onLogout: options.onLogout || (() => {}),
            onAuthError: options.onAuthError || (() => {})
        };
        
        this.init();
    }
    
    /**
     * Initialize the token manager
     */
    init() {
        // Set up auto-refresh if enabled
        if (this.autoRefreshEnabled) {
            this.setupAutoRefresh();
        }
        
        // Check for existing token on page load
        this.validateStoredToken();
        
        console.log('JWT Token Manager initialized');
    }
    
    /**
     * Store authentication data
     */
    storeAuthData(token, refreshToken, user) {
        try {
            localStorage.setItem(this.storageKey, token);
            if (refreshToken) {
                localStorage.setItem(this.refreshKey, refreshToken);
            }
            if (user) {
                localStorage.setItem(this.userKey, JSON.stringify(user));
            }
            
            console.log('Authentication data stored successfully');
            this.setupAutoRefresh();
            
        } catch (error) {
            console.error('Error storing auth data:', error);
            this.callbacks.onAuthError(error);
        }
    }
    
    /**
     * Get stored access token
     */
    getAccessToken() {
        try {
            return localStorage.getItem(this.storageKey);
        } catch (error) {
            console.error('Error getting access token:', error);
            return null;
        }
    }
    
    /**
     * Get stored refresh token
     */
    getRefreshToken() {
        try {
            return localStorage.getItem(this.refreshKey);
        } catch (error) {
            console.error('Error getting refresh token:', error);
            return null;
        }
    }
    
    /**
     * Get stored user data
     */
    getUserData() {
        try {
            const userData = localStorage.getItem(this.userKey);
            return userData ? JSON.parse(userData) : null;
        } catch (error) {
            console.error('Error getting user data:', error);
            return null;
        }
    }
    
    /**
     * Clear all stored authentication data
     */
    clearAuthData() {
        try {
            localStorage.removeItem(this.storageKey);
            localStorage.removeItem(this.refreshKey);
            localStorage.removeItem(this.userKey);
            
            if (this.refreshTimer) {
                clearTimeout(this.refreshTimer);
                this.refreshTimer = null;
            }
            
            console.log('Authentication data cleared');
            this.callbacks.onLogout();
            
        } catch (error) {
            console.error('Error clearing auth data:', error);
        }
    }
    
    /**
     * Parse JWT token payload
     */
    parseToken(token) {
        try {
            const base64Url = token.split('.')[1];
            const base64 = base64Url.replace(/-/g, '+').replace(/_/g, '/');
            const jsonPayload = decodeURIComponent(
                atob(base64)
                    .split('')
                    .map(c => '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2))
                    .join('')
            );
            return JSON.parse(jsonPayload);
        } catch (error) {
            console.error('Error parsing token:', error);
            return null;
        }
    }
    
    /**
     * Check if token is expired
     */
    isTokenExpired(token) {
        try {
            const payload = this.parseToken(token);
            if (!payload || !payload.exp) {
                return true; // Invalid token
            }
            
            const currentTime = Math.floor(Date.now() / 1000);
            return payload.exp < currentTime;
        } catch (error) {
            console.error('Error checking token expiration:', error);
            return true;
        }
    }
    
    /**
     * Check if token will expire soon
     */
    isTokenExpiringSoon(token, thresholdMs = this.refreshThreshold) {
        try {
            const payload = this.parseToken(token);
            if (!payload || !payload.exp) {
                return true;
            }
            
            const expirationTime = payload.exp * 1000;
            const currentTime = Date.now();
            const timeUntilExpiration = expirationTime - currentTime;
            
            return timeUntilExpiration < thresholdMs;
        } catch (error) {
            console.error('Error checking token expiration:', error);
            return true;
        }
    }
    
    /**
     * Validate stored token
     */
    validateStoredToken() {
        const token = this.getAccessToken();
        if (!token) {
            return false;
        }
        
        if (this.isTokenExpired(token)) {
            console.log('Stored token is expired');
            this.clearAuthData();
            this.callbacks.onTokenExpired();
            return false;
        }
        
        return true;
    }
    
    /**
     * Setup automatic token refresh
     */
    setupAutoRefresh() {
        // Clear existing timer
        if (this.refreshTimer) {
            clearTimeout(this.refreshTimer);
        }
        
        if (!this.autoRefreshEnabled) {
            return;
        }
        
        const token = this.getAccessToken();
        if (!token) {
            return;
        }
        
        const payload = this.parseToken(token);
        if (!payload || !payload.exp) {
            return;
        }
        
        const expirationTime = payload.exp * 1000;
        const currentTime = Date.now();
        const refreshTime = expirationTime - this.refreshThreshold;
        
        if (refreshTime <= currentTime) {
            // Token is expiring soon, refresh now
            this.refreshToken();
        } else {
            // Schedule refresh
            const delay = refreshTime - currentTime;
            this.refreshTimer = setTimeout(() => {
                this.refreshToken();
            }, delay);
            
            console.log(`Token refresh scheduled in ${Math.round(delay / 1000 / 60)} minutes`);
        }
    }
    
    /**
     * Refresh the access token
     */
    async refreshToken() {
        try {
            const refreshToken = this.getRefreshToken();
            if (!refreshToken) {
                console.log('No refresh token available');
                this.clearAuthData();
                return false;
            }
            
            console.log('Refreshing access token...');
            
            const response = await fetch(`${this.apiBaseUrl}/refresh`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    token: refreshToken
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                // Update stored token
                localStorage.setItem(this.storageKey, data.access_token);
                
                // Setup next refresh
                this.setupAutoRefresh();
                
                console.log('Token refreshed successfully');
                this.callbacks.onTokenRefresh(data.access_token);
                return true;
            } else {
                console.error('Token refresh failed:', data.message);
                this.clearAuthData();
                this.callbacks.onTokenExpired();
                return false;
            }
            
        } catch (error) {
            console.error('Error refreshing token:', error);
            this.clearAuthData();
            this.callbacks.onAuthError(error);
            return false;
        }
    }
    
    /**
     * Verify token with server
     */
    async verifyToken(token = null) {
        try {
            const tokenToVerify = token || this.getAccessToken();
            if (!tokenToVerify) {
                return { valid: false, message: 'No token provided' };
            }
            
            const response = await fetch(`${this.apiBaseUrl}/verify`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    token: tokenToVerify
                })
            });
            
            const data = await response.json();
            return data;
            
        } catch (error) {
            console.error('Error verifying token:', error);
            return { valid: false, message: error.message };
        }
    }
    
    /**
     * Get authorization header
     */
    getAuthHeader() {
        const token = this.getAccessToken();
        return token ? `Bearer ${token}` : null;
    }
    
    /**
     * Make authenticated API request
     */
    async authenticatedFetch(url, options = {}) {
        const token = this.getAccessToken();
        if (!token) {
            throw new Error('No authentication token available');
        }
        
        // Check if token is expired
        if (this.isTokenExpired(token)) {
            // Try to refresh token
            const refreshed = await this.refreshToken();
            if (!refreshed) {
                throw new Error('Token expired and refresh failed');
            }
        }
        
        const authOptions = {
            ...options,
            headers: {
                ...options.headers,
                'Authorization': this.getAuthHeader()
            }
        };
        
        try {
            const response = await fetch(url, authOptions);
            
            // Handle 401 Unauthorized (token might be expired)
            if (response.status === 401) {
                // Try to refresh token and retry once
                const refreshed = await this.refreshToken();
                if (refreshed) {
                    authOptions.headers['Authorization'] = this.getAuthHeader();
                    return await fetch(url, authOptions);
                }
            }
            
            return response;
            
        } catch (error) {
            console.error('Authenticated fetch error:', error);
            throw error;
        }
    }
    
    /**
     * Logout user
     */
    async logout() {
        try {
            const token = this.getAccessToken();
            if (token) {
                // Notify server about logout
                await fetch(`${this.apiBaseUrl}/logout`, {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                        'Authorization': this.getAuthHeader()
                    }
                });
            }
        } catch (error) {
            console.error('Error during logout:', error);
        } finally {
            // Always clear local data
            this.clearAuthData();
        }
    }
    
    /**
     * Get current authentication status
     */
    getAuthStatus() {
        const token = this.getAccessToken();
        const user = this.getUserData();
        
        if (!token) {
            return {
                authenticated: false,
                user: null,
                tokenValid: false
            };
        }
        
        const tokenValid = !this.isTokenExpired(token);
        const tokenExpiringSoon = this.isTokenExpiringSoon(token);
        
        return {
            authenticated: tokenValid,
            user: user,
            tokenValid: tokenValid,
            tokenExpiringSoon: tokenExpiringSoon,
            hasRefreshToken: !!this.getRefreshToken()
        };
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
     * Destroy the token manager
     */
    destroy() {
        if (this.refreshTimer) {
            clearTimeout(this.refreshTimer);
            this.refreshTimer = null;
        }
        
        console.log('JWT Token Manager destroyed');
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = JWTTokenManager;
} else if (typeof window !== 'undefined') {
    window.JWTTokenManager = JWTTokenManager;
}

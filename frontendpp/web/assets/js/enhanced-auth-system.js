/**
 * Enhanced Authentication System for Login Page
 * Integrates JWT token management with login functionality
 */

class EnhancedAuthSystem {
    constructor(options = {}) {
        this.apiBaseUrl = options.apiBaseUrl || 'http://localhost:9090/api/auth';
        this.tokenManager = null;
        this.isInitialized = false;
        
        // DOM elements
        this.loginForm = null;
        this.usernameInput = null;
        this.passwordInput = null;
        this.submitButton = null;
        this.errorMessage = null;
        this.successMessage = null;
        this.loadingIndicator = null;
        
        // Configuration
        this.config = {
            redirectUrl: options.redirectUrl || '/source.html',
            enableWebSocketAuth: options.enableWebSocketAuth !== false,
            autoRedirectDelay: options.autoRedirectDelay || 1500,
            maxLoginAttempts: options.maxLoginAttempts || 5,
            lockoutDuration: options.lockoutDuration || 15 * 60 * 1000, // 15 minutes
            ...options
        };
        
        // State
        this.loginAttempts = 0;
        this.lockoutUntil = null;
        
        this.init();
    }
    
    /**
     * Initialize the authentication system
     */
    init() {
        if (this.isInitialized) return;
        
        console.log('🔐 Initializing Enhanced Authentication System...');
        
        // Initialize JWT token manager
        this.tokenManager = new JWTTokenManager({
            apiBaseUrl: this.apiBaseUrl,
            autoRefresh: true,
            onTokenExpired: () => this.handleTokenExpired(),
            onAuthError: (error) => this.handleAuthError(error),
            onLogout: () => this.handleLogout()
        });
        
        // Cache DOM elements
        this.cacheDOMElements();
        
        // Setup event listeners
        this.setupEventListeners();
        
        // Check for existing session
        this.checkExistingSession();
        
        // Setup form validation
        this.setupFormValidation();
        
        this.isInitialized = true;
        console.log('✅ Enhanced Authentication System initialized');
    }
    
    /**
     * Cache frequently used DOM elements
     */
    cacheDOMElements() {
        this.loginForm = document.getElementById('login-form');
        this.usernameInput = document.getElementById('username');
        this.passwordInput = document.getElementById('password');
        this.submitButton = this.loginForm?.querySelector('button[type="submit"]');
        
        // Create or find message containers
        this.errorMessage = this.findOrCreateMessageContainer('error-message');
        this.successMessage = this.findOrCreateMessageContainer('success-message');
        this.loadingIndicator = this.createLoadingIndicator();
    }
    
    /**
     * Find or create message container
     */
    findOrCreateMessageContainer(className) {
        let container = document.querySelector(`.${className}`);
        if (!container) {
            container = document.createElement('div');
            container.className = className;
            container.style.display = 'none';
            
            // Insert after the login form
            if (this.loginForm) {
                this.loginForm.parentNode.insertBefore(container, this.loginForm.nextSibling);
            }
        }
        return container;
    }
    
    /**
     * Create loading indicator
     */
    createLoadingIndicator() {
        const indicator = document.createElement('div');
        indicator.className = 'auth-loading-indicator';
        indicator.innerHTML = `
            <div class="flex items-center justify-center">
                <div class="animate-spin rounded-full h-5 w-5 border-b-2 border-blue-600"></div>
                <span class="ml-2 text-sm text-gray-600">Authenticating...</span>
            </div>
        `;
        indicator.style.display = 'none';
        
        // Insert after the submit button
        if (this.submitButton) {
            this.submitButton.parentNode.insertBefore(indicator, this.submitButton.nextSibling);
        }
        
        return indicator;
    }
    
    /**
     * Setup event listeners
     */
    setupEventListeners() {
        if (this.loginForm) {
            this.loginForm.addEventListener('submit', (e) => this.handleLoginSubmit(e));
        }
        
        // Add input event listeners for real-time validation
        if (this.usernameInput) {
            this.usernameInput.addEventListener('input', () => this.validateUsername());
            this.usernameInput.addEventListener('blur', () => this.validateUsername());
        }
        
        if (this.passwordInput) {
            this.passwordInput.addEventListener('input', () => this.validatePassword());
            this.passwordInput.addEventListener('blur', () => this.validatePassword());
        }
        
        // Add keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && e.ctrlKey) {
                e.preventDefault();
                this.handleLogin();
            }
        });
    }
    
    /**
     * Setup form validation
     */
    setupFormValidation() {
        // Add real-time validation feedback
        this.setupValidationFeedback();
    }
    
    /**
     * Setup validation feedback
     */
    setupValidationFeedback() {
        const createFeedbackElement = (input, type) => {
            const feedback = document.createElement('div');
            feedback.className = `validation-feedback validation-${type}`;
            feedback.style.fontSize = '0.875rem';
            feedback.style.marginTop = '0.25rem';
            feedback.style.display = 'none';
            
            input.parentNode.appendChild(feedback);
            return feedback;
        };
        
        if (this.usernameInput) {
            this.usernameFeedback = createFeedbackElement(this.usernameInput, 'username');
        }
        
        if (this.passwordInput) {
            this.passwordFeedback = createFeedbackElement(this.passwordInput, 'password');
        }
    }
    
    /**
     * Validate username field
     */
    validateUsername() {
        if (!this.usernameInput) return true;
        
        const username = this.usernameInput.value.trim();
        let isValid = true;
        let message = '';
        
        if (!username) {
            isValid = false;
            message = 'Username is required';
        } else if (username.length < 3) {
            isValid = false;
            message = 'Username must be at least 3 characters';
        } else if (!/^[a-zA-Z0-9_-]+$/.test(username)) {
            isValid = false;
            message = 'Username can only contain letters, numbers, underscores, and hyphens';
        }
        
        this.updateValidationFeedback(this.usernameInput, this.usernameFeedback, isValid, message);
        return isValid;
    }
    
    /**
     * Validate password field
     */
    validatePassword() {
        if (!this.passwordInput) return true;
        
        const password = this.passwordInput.value;
        let isValid = true;
        let message = '';
        
        if (!password) {
            isValid = false;
            message = 'Password is required';
        } else if (password.length < 6) {
            isValid = false;
            message = 'Password must be at least 6 characters';
        }
        
        this.updateValidationFeedback(this.passwordInput, this.passwordFeedback, isValid, message);
        return isValid;
    }
    
    /**
     * Update validation feedback
     */
    updateValidationFeedback(input, feedback, isValid, message) {
        if (!feedback) return;
        
        if (isValid) {
            input.classList.remove('border-red-500');
            input.classList.add('border-green-500');
            feedback.style.display = 'none';
        } else {
            input.classList.remove('border-green-500');
            input.classList.add('border-red-500');
            feedback.textContent = message;
            feedback.style.display = 'block';
            feedback.style.color = '#ef4444';
        }
    }
    
    /**
     * Check for existing session
     */
    async checkExistingSession() {
        const authStatus = this.tokenManager.getAuthStatus();
        
        if (authStatus.authenticated && authStatus.user) {
            console.log('🔓 Existing valid session found');
            
            // Verify token with server
            const verification = await this.tokenManager.verifyToken();
            
            if (verification.valid) {
                this.showSuccessMessage('Already logged in. Redirecting...', true);
                setTimeout(() => {
                    this.redirectToDashboard();
                }, 1000);
            } else {
                console.log('Session invalid, clearing tokens');
                this.tokenManager.clearAuthData();
            }
        }
    }
    
    /**
     * Handle login form submission
     */
    async handleLoginSubmit(event) {
        event.preventDefault();
        await this.handleLogin();
    }
    
    /**
     * Handle login process
     */
    async handleLogin() {
        // Check if user is locked out
        if (this.isUserLockedOut()) {
            this.showErrorMessage(`Account temporarily locked. Try again in ${Math.ceil((this.lockoutUntil - Date.now()) / 60000)} minutes.`);
            return;
        }
        
        // Validate form
        const isUsernameValid = this.validateUsername();
        const isPasswordValid = this.validatePassword();
        
        if (!isUsernameValid || !isPasswordValid) {
            this.showErrorMessage('Please fix the validation errors');
            return;
        }
        
        const username = this.usernameInput.value.trim();
        const password = this.passwordInput.value;
        
        // Show loading state
        this.setLoadingState(true);
        this.hideMessages();
        
        try {
            console.log(`🔐 Attempting login for user: ${username}`);
            
            // Make login request
            const response = await fetch(`${this.apiBaseUrl}/login`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    username: username,
                    password: password
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                // Login successful
                await this.handleLoginSuccess(data, username);
            } else {
                // Login failed
                await this.handleLoginFailure(data.message, username);
            }
            
        } catch (error) {
            console.error('Login error:', error);
            this.showErrorMessage('Network error. Please try again.');
            this.incrementLoginAttempts();
        } finally {
            this.setLoadingState(false);
        }
    }
    
    /**
     * Handle successful login
     */
    async handleLoginSuccess(data, username) {
        console.log('✅ Login successful');
        
        // Store authentication data
        this.tokenManager.storeAuthData(
            data.access_token,
            data.refresh_token,
            data.user
        );
        
        // Reset login attempts
        this.loginAttempts = 0;
        
        // Show success message
        this.showSuccessMessage('Login successful! Redirecting...', true);
        
        // Notify WebSocket system if enabled
        if (this.config.enableWebSocketAuth) {
            this.notifyWebSocketLogin(username, data);
        }
        
        // Redirect to dashboard
        setTimeout(() => {
            this.redirectToDashboard();
        }, this.config.autoRedirectDelay);
    }
    
    /**
     * Handle login failure
     */
    async handleLoginFailure(message, username) {
        console.log(`❌ Login failed for user: ${username} - ${message}`);
        
        this.incrementLoginAttempts();
        this.showErrorMessage(message || 'Login failed. Please try again.');
        
        // Shake the login form for visual feedback
        this.shakeElement(this.loginForm);
    }
    
    /**
     * Increment login attempts and handle lockout
     */
    incrementLoginAttempts() {
        this.loginAttempts++;
        
        if (this.loginAttempts >= this.config.maxLoginAttempts) {
            this.lockoutUntil = Date.now() + this.config.lockoutDuration;
            console.log(`🔒 User locked out until ${new Date(this.lockoutUntil).toLocaleString()}`);
        }
    }
    
    /**
     * Check if user is locked out
     */
    isUserLockedOut() {
        if (!this.lockoutUntil) return false;
        
        if (Date.now() > this.lockoutUntil) {
            // Lockout period has expired
            this.lockoutUntil = null;
            this.loginAttempts = 0;
            return false;
        }
        
        return true;
    }
    
    /**
     * Notify WebSocket system about successful login
     */
    notifyWebSocketLogin(username, loginData) {
        if (typeof window.loginWSManager !== 'undefined' && 
            window.loginWSManager.wsClient && 
            window.loginWSManager.wsClient.isConnected) {
            
            window.loginWSManager.notifyWebSocketLogin(username, loginData);
        }
    }
    
    /**
     * Redirect to dashboard
     */
    redirectToDashboard() {
        // Store session info for dashboard
        const authStatus = this.tokenManager.getAuthStatus();
        if (authStatus.user) {
            sessionStorage.setItem('user_data', JSON.stringify(authStatus.user));
            sessionStorage.setItem('login_time', new Date().toISOString());
        }
        
        window.location.href = this.config.redirectUrl;
    }
    
    /**
     * Set loading state
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
        
        if (this.usernameInput) this.usernameInput.disabled = isLoading;
        if (this.passwordInput) this.passwordInput.disabled = isLoading;
        
        if (this.loadingIndicator) {
            this.loadingIndicator.style.display = isLoading ? 'block' : 'none';
        }
    }
    
    /**
     * Show error message
     */
    showErrorMessage(message, autoHide = false) {
        this.hideMessages();
        
        if (this.errorMessage) {
            this.errorMessage.innerHTML = `
                <div class="flex items-center p-4 bg-red-50 border border-red-200 rounded-lg">
                    <i class="fa-solid fa-exclamation-circle text-red-600 mr-3"></i>
                    <span class="text-red-800">${message}</span>
                    <button onclick="this.parentElement.parentElement.style.display='none'" class="ml-auto text-red-600 hover:text-red-800">
                        <i class="fa-solid fa-times"></i>
                    </button>
                </div>
            `;
            this.errorMessage.style.display = 'block';
            
            if (autoHide) {
                setTimeout(() => this.hideMessages(), 5000);
            }
        }
    }
    
    /**
     * Show success message
     */
    showSuccessMessage(message, autoHide = false) {
        this.hideMessages();
        
        if (this.successMessage) {
            this.successMessage.innerHTML = `
                <div class="flex items-center p-4 bg-green-50 border border-green-200 rounded-lg">
                    <i class="fa-solid fa-check-circle text-green-600 mr-3"></i>
                    <span class="text-green-800">${message}</span>
                    <button onclick="this.parentElement.parentElement.style.display='none'" class="ml-auto text-green-600 hover:text-green-800">
                        <i class="fa-solid fa-times"></i>
                    </button>
                </div>
            `;
            this.successMessage.style.display = 'block';
            
            if (autoHide) {
                setTimeout(() => this.hideMessages(), 5000);
            }
        }
    }
    
    /**
     * Hide all messages
     */
    hideMessages() {
        if (this.errorMessage) this.errorMessage.style.display = 'none';
        if (this.successMessage) this.successMessage.style.display = 'none';
    }
    
    /**
     * Shake element for visual feedback
     */
    shakeElement(element) {
        if (!element) return;
        
        element.style.animation = 'shake 0.5s';
        setTimeout(() => {
            element.style.animation = '';
        }, 500);
    }
    
    /**
     * Handle token expiration
     */
    handleTokenExpired() {
        console.log('🔓 Token expired, user needs to login again');
        this.showErrorMessage('Your session has expired. Please login again.');
    }
    
    /**
     * Handle authentication error
     */
    handleAuthError(error) {
        console.error('Authentication error:', error);
        this.showErrorMessage('Authentication error occurred. Please try again.');
    }
    
    /**
     * Handle logout
     */
    handleLogout() {
        console.log('🔓 User logged out');
        // This would typically redirect to login page
    }
    
    /**
     * Logout current user
     */
    async logout() {
        try {
            await this.tokenManager.logout();
            this.showSuccessMessage('Logged out successfully', true);
            
            setTimeout(() => {
                window.location.reload();
            }, 1000);
        } catch (error) {
            console.error('Logout error:', error);
            this.showErrorMessage('Error during logout');
        }
    }
    
    /**
     * Get authentication status
     */
    getAuthStatus() {
        return this.tokenManager.getAuthStatus();
    }
    
    /**
     * Make authenticated API request
     */
    async authenticatedFetch(url, options = {}) {
        return await this.tokenManager.authenticatedFetch(url, options);
    }
    
    /**
     * Destroy the authentication system
     */
    destroy() {
        if (this.tokenManager) {
            this.tokenManager.destroy();
        }
        
        this.isInitialized = false;
        console.log('🔐 Enhanced Authentication System destroyed');
    }
}

// Add shake animation to page
const shakeStyle = document.createElement('style');
shakeStyle.textContent = `
    @keyframes shake {
        0%, 100% { transform: translateX(0); }
        10%, 30%, 50%, 70%, 90% { transform: translateX(-5px); }
        20%, 40%, 60%, 80% { transform: translateX(5px); }
    }
`;
document.head.appendChild(shakeStyle);

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.enhancedAuthSystem = new EnhancedAuthSystem({
        apiBaseUrl: 'http://localhost:9090/api/auth',
        enableWebSocketAuth: true,
        redirectUrl: '/source.html'
    });
    
    // Cleanup on page unload
    window.addEventListener('beforeunload', () => {
        if (window.enhancedAuthSystem) {
            window.enhancedAuthSystem.destroy();
        }
    });
});

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = EnhancedAuthSystem;
} else if (typeof window !== 'undefined') {
    window.EnhancedAuthSystem = EnhancedAuthSystem;
}

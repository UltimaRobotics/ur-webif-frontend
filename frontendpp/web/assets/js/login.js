/**
 * Login Page Script - Standalone Demo Mode
 * All authentication is bypassed - any credentials will work
 */

class LoginManager {
    constructor() {
        this.form = document.getElementById('login-form');
        this.usernameInput = document.getElementById('username');
        this.passwordInput = document.getElementById('password');
        this.loginButton = document.getElementById('login-button');
        this.loginText = document.getElementById('login-text');
        this.loginSpinner = document.getElementById('login-spinner');
        this.passwordToggle = document.getElementById('password-toggle');
        this.fileToggle = document.getElementById('file-section-toggle');
        this.fileUploadArea = document.getElementById('file-upload-area');
        this.forgotPasswordBtn = document.getElementById('forgot-password-btn');
        this.successPopup = document.getElementById('success-popup');
        this.forgotPasswordPopup = document.getElementById('forgot-password-popup');
        this.notification = document.getElementById('notification');
        
        this.init();
    }

    init() {
        console.log('[LOGIN] Initializing login manager (Demo Mode)');
        
        // Bind event listeners
        this.form.addEventListener('submit', (e) => this.handleLogin(e));
        this.passwordToggle.addEventListener('click', () => this.togglePassword());
        this.fileToggle.addEventListener('click', () => this.toggleFileUpload());
        this.forgotPasswordBtn.addEventListener('click', () => this.showForgotPassword());
        
        // Popup buttons
        document.getElementById('continue-btn').addEventListener('click', () => this.goToDashboard());
        document.getElementById('close-success-btn').addEventListener('click', () => this.closePopup());
        document.getElementById('close-forgot-btn').addEventListener('click', () => this.closePopup());
        
        // File upload
        const fileInput = document.getElementById('file-upload');
        const dropZone = document.getElementById('file-drop-zone');
        
        dropZone.addEventListener('click', () => fileInput.click());
        fileInput.addEventListener('change', (e) => this.handleFileUpload(e));
        
        // Drag and drop
        dropZone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropZone.style.borderColor = '#374151';
        });
        
        dropZone.addEventListener('dragleave', () => {
            dropZone.style.borderColor = '#d1d5db';
        });
        
        dropZone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropZone.style.borderColor = '#d1d5db';
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                this.handleFile(files[0]);
            }
        });
    }

    async handleLogin(e) {
        e.preventDefault();
        
        const username = this.usernameInput.value.trim();
        const password = this.passwordInput.value;
        
        if (!username || !password) {
            this.showNotification('Please enter both username and password', 'error');
            return;
        }
        
        // Show loading state
        this.setLoading(true);
        
        // Simulate API call delay
        await this.delay(1000);
        
        // Always succeed in demo mode
        console.log('[LOGIN] Demo mode - authentication bypassed');
        console.log('[LOGIN] Username:', username);
        
        // Store session data
        const sessionData = {
            token: 'demo_token_' + Date.now(),
            username: username,
            timestamp: Date.now(),
            demoMode: true
        };
        
        localStorage.setItem('session', JSON.stringify(sessionData));
        localStorage.setItem('demo_mode', 'true');
        
        // Show success
        this.setLoading(false);
        this.showSuccess(username);
    }

    togglePassword() {
        const type = this.passwordInput.type === 'password' ? 'text' : 'password';
        this.passwordInput.type = type;
        
        if (type === 'text') {
            this.passwordToggle.classList.remove('icon-eye-slash');
            this.passwordToggle.classList.add('icon-eye');
        } else {
            this.passwordToggle.classList.remove('icon-eye');
            this.passwordToggle.classList.add('icon-eye-slash');
        }
    }

    toggleFileUpload() {
        this.fileUploadArea.classList.toggle('hidden');
        const expandIcon = document.getElementById('expand-icon');
        expandIcon.classList.toggle('rotated');
    }

    handleFileUpload(e) {
        const file = e.target.files[0];
        if (file) {
            this.handleFile(file);
        }
    }

    handleFile(file) {
        const fileInfo = document.getElementById('file-info');
        fileInfo.textContent = `Selected: ${file.name} (${this.formatFileSize(file.size)})`;
        fileInfo.classList.remove('hidden');
        
        this.showNotification('File uploaded successfully (Demo Mode)', 'success');
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
    }

    showForgotPassword() {
        this.forgotPasswordPopup.classList.add('show');
    }

    showSuccess(username) {
        const message = document.getElementById('success-message');
        message.textContent = `Welcome back, ${username}! You have been successfully authenticated.`;
        this.successPopup.classList.add('show');
    }

    closePopup() {
        this.successPopup.classList.remove('show');
        this.forgotPasswordPopup.classList.remove('show');
    }

    goToDashboard() {
        window.location.href = 'source.html';
    }

    setLoading(isLoading) {
        if (isLoading) {
            this.loginButton.disabled = true;
            this.loginText.textContent = 'Signing in...';
            this.loginSpinner.classList.remove('hidden');
        } else {
            this.loginButton.disabled = false;
            this.loginText.textContent = 'Sign in';
            this.loginSpinner.classList.add('hidden');
        }
    }

    showNotification(message, type = 'info') {
        this.notification.textContent = message;
        this.notification.className = `notification ${type}`;
        this.notification.classList.add('show');
        
        setTimeout(() => {
            this.notification.classList.remove('show');
        }, 3000);
    }

    delay(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}

// Initialize on DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new LoginManager();
    });
} else {
    new LoginManager();
}

console.log('[LOGIN] Login page loaded (Demo Mode)');


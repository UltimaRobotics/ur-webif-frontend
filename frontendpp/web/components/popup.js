/**
 * Modular Popup System for Login Page
 * Provides reusable popup functionality with consistent UI and behavior
 */

class PopupManager {
    constructor() {
        this.activePopup = null;
        this.defaultConfig = {
            closeOnBackdrop: true,
            closeOnEscape: true,
            preventBodyScroll: true,
            showCloseButton: true,
            animationDuration: 300
        };
        // Don't initialize immediately - wait for DOM
        this.initialized = false;
    }

    /**
     * Initialize the popup manager
     */
    init() {
        if (this.initialized) return;
        
        // Wait for DOM to be ready
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this._doInit());
        } else {
            this._doInit();
        }
    }

    /**
     * Perform actual initialization
     */
    _doInit() {
        // Create popup container if it doesn't exist
        if (!document.getElementById('popup-container')) {
            const container = document.createElement('div');
            container.id = 'popup-container';
            container.className = 'fixed inset-0 z-50 pointer-events-none';
            document.body.appendChild(container);
        }
        this.initialized = true;
        console.log('[POPUP] Popup manager initialized');
    }

    /**
     * Show a popup with the specified configuration
     * @param {Object} config - Popup configuration
     * @returns {Promise} - Resolves when popup is closed
     */
    show(config) {
        return new Promise((resolve) => {
            // Ensure initialization
            if (!this.initialized) {
                this.init();
                // If DOM is still loading, wait for it
                if (!this.initialized) {
                    document.addEventListener('DOMContentLoaded', () => {
                        this._doShow(config, resolve);
                    });
                    return;
                }
            }
            
            this._doShow(config, resolve);
        });
    }

    /**
     * Internal method to show popup after initialization
     */
    _doShow(config, resolve) {
        // Merge with default config
        const popupConfig = { ...this.defaultConfig, ...config };
        
        // Close any existing popup
        if (this.activePopup) {
            this.close();
        }

        // Create popup element
        const popup = this.createPopup(popupConfig);
        const container = document.getElementById('popup-container');
        
        // Add to container
        container.appendChild(popup);
        container.classList.add('pointer-events-auto');
        
        // Prevent body scroll if configured
        if (popupConfig.preventBodyScroll) {
            document.body.style.overflow = 'hidden';
        }

        // Show popup with animation
        requestAnimationFrame(() => {
            // Add show class for overlay
            popup.classList.remove('opacity-0');
            popup.classList.add('opacity-100');
            
            // Animate content container
            const contentContainer = popup.querySelector('.bg-white, .relative');
            if (contentContainer) {
                contentContainer.classList.remove('scale-95');
                contentContainer.classList.add('scale-100');
            }
        });

        // Store active popup
        this.activePopup = {
            element: popup,
            config: popupConfig,
            resolve: resolve
        };

        // Setup event listeners
        this.setupEventListeners(popup, popupConfig);

        console.log('[POPUP] Showing popup:', popupConfig.title || 'Untitled');
    }

    /**
     * Close the current popup
     * @param {*} result - Result to resolve the promise with
     */
    close(result = null) {
        if (!this.activePopup) return;

        const { element, config, resolve } = this.activePopup;
        
        // Remove show class for animation
        element.classList.remove('opacity-100');
        element.classList.add('opacity-0');
        
        // Also animate the content container
        const contentContainer = element.querySelector('.bg-white, .relative');
        if (contentContainer) {
            contentContainer.classList.remove('scale-100');
            contentContainer.classList.add('scale-95');
        }
        
        // Wait for animation then remove element
        setTimeout(() => {
            if (element.parentNode) {
                element.parentNode.removeChild(element);
            }
            
            // Restore body scroll
            if (config.preventBodyScroll) {
                document.body.style.overflow = 'auto';
            }

            // Hide container if no more popups
            const container = document.getElementById('popup-container');
            if (container && container.children.length === 0) {
                container.classList.remove('pointer-events-auto');
            }

            // Resolve promise
            resolve(result);
            
            console.log('[POPUP] Popup closed');
        }, config.animationDuration);

        this.activePopup = null;
    }

    /**
     * Create popup DOM element
     * @param {Object} config - Popup configuration
     * @returns {HTMLElement} - Popup element
     */
    createPopup(config) {
        const popup = document.createElement('div');
        popup.className = 'popup-overlay fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center opacity-0 transition-opacity duration-300';
        
        // Backdrop click area
        const backdrop = document.createElement('div');
        backdrop.className = 'absolute inset-0';
        if (config.closeOnBackdrop) {
            backdrop.onclick = () => this.close('backdrop');
        }
        popup.appendChild(backdrop);

        // Close button
        if (config.showCloseButton) {
            const closeBtn = document.createElement('button');
            closeBtn.className = 'absolute top-6 right-6 text-white hover:text-gray-300 transition-colors z-10';
            closeBtn.innerHTML = '<i class="fas fa-times text-2xl"></i>';
            closeBtn.onclick = () => this.close('close-button');
            popup.appendChild(closeBtn);
        }

        // Popup content
        const content = document.createElement('div');
        content.className = 'relative bg-white rounded-2xl shadow-2xl p-12 max-w-md w-full mx-4 text-center transform scale-95 transition-transform duration-300';
        content.onclick = (e) => e.stopPropagation();

        // Icon
        if (config.icon) {
            const iconContainer = document.createElement('div');
            iconContainer.className = 'flex justify-center mb-6';
            
            const iconWrapper = document.createElement('div');
            iconWrapper.className = `w-16 h-16 ${config.iconBg || 'bg-gray-100'} rounded-xl flex items-center justify-center`;
            
            const icon = document.createElement('i');
            icon.className = `${config.icon} text-2xl ${config.iconColor || 'text-gray-400'}`;
            
            iconWrapper.appendChild(icon);
            iconContainer.appendChild(iconWrapper);
            content.appendChild(iconContainer);
        }

        // Title
        if (config.title) {
            const title = document.createElement('h2');
            title.className = 'text-2xl font-bold text-gray-900 mb-4';
            title.textContent = config.title;
            content.appendChild(title);
        }

        // Description
        if (config.description) {
            const description = document.createElement('p');
            description.className = 'text-gray-600 leading-relaxed mb-6';
            description.textContent = config.description;
            content.appendChild(description);
        }

        // Actions
        if (config.actions && config.actions.length > 0) {
            const actionsContainer = document.createElement('div');
            actionsContainer.className = 'space-y-4';

            config.actions.forEach(action => {
                if (action.type === 'button') {
                    const button = document.createElement('button');
                    button.className = `w-full ${action.className || 'bg-gray-900 hover:bg-gray-800 text-white font-semibold py-4 px-8 rounded-xl transition-colors'}`;
                    button.textContent = action.text;
                    button.onclick = () => {
                        if (action.onClick) {
                            action.onClick();
                        }
                        this.close(action.value || action.text);
                    };
                    actionsContainer.appendChild(button);
                } else if (action.type === 'text') {
                    const text = document.createElement('p');
                    text.className = `${action.className || 'text-sm text-gray-500 cursor-pointer hover:text-gray-700 transition-colors'}`;
                    text.textContent = action.text;
                    text.onclick = () => {
                        if (action.onClick) {
                            action.onClick();
                        }
                        this.close(action.value || action.text);
                    };
                    actionsContainer.appendChild(text);
                }
            });

            content.appendChild(actionsContainer);
        }

        // Custom content
        if (config.customContent) {
            content.appendChild(config.customContent);
        }

        popup.appendChild(content);
        return popup;
    }

    /**
     * Setup event listeners for popup
     * @param {HTMLElement} popup - Popup element
     * @param {Object} config - Popup configuration
     */
    setupEventListeners(popup, config) {
        // Escape key handler
        if (config.closeOnEscape) {
            const escapeHandler = (e) => {
                if (e.key === 'Escape' && this.activePopup) {
                    this.close('escape');
                    document.removeEventListener('keydown', escapeHandler);
                }
            };
            document.addEventListener('keydown', escapeHandler);
        }

        // Add show class for animation - find the content container properly
        setTimeout(() => {
            const contentContainer = popup.querySelector('.bg-white, .relative');
            if (contentContainer) {
                contentContainer.classList.remove('scale-95');
                contentContainer.classList.add('scale-100');
            }
            // Also show the overlay
            popup.classList.remove('opacity-0');
            popup.classList.add('opacity-100');
        }, 10);
    }

    /**
     * Show a success popup
     * @param {Object} options - Success popup options
     */
    showSuccess(options = {}) {
        return this.show({
            icon: 'fas fa-check',
            iconBg: 'bg-green-100',
            iconColor: 'text-green-600',
            title: options.title || 'Success!',
            description: options.description || 'Operation completed successfully.',
            actions: [
                {
                    type: 'button',
                    text: options.buttonText || 'Continue',
                    className: options.buttonClass || 'bg-green-600 hover:bg-green-700 text-white font-semibold py-4 px-8 rounded-xl transition-colors',
                    onClick: options.onButtonClick
                }
            ],
            ...options
        });
    }

    /**
     * Show an error popup
     * @param {Object} options - Error popup options
     */
    showError(options = {}) {
        return this.show({
            icon: 'fas fa-exclamation-triangle',
            iconBg: 'bg-red-100',
            iconColor: 'text-red-600',
            title: options.title || 'Error!',
            description: options.description || 'An error occurred. Please try again.',
            actions: [
                {
                    type: 'button',
                    text: options.buttonText || 'Try Again',
                    className: options.buttonClass || 'bg-red-600 hover:bg-red-700 text-white font-semibold py-4 px-8 rounded-xl transition-colors',
                    onClick: options.onButtonClick
                }
            ],
            ...options
        });
    }

    /**
     * Show an info popup
     * @param {Object} options - Info popup options
     */
    showInfo(options = {}) {
        return this.show({
            icon: 'fas fa-info-circle',
            iconBg: 'bg-blue-100',
            iconColor: 'text-blue-600',
            title: options.title || 'Information',
            description: options.description || 'Here is some information for you.',
            actions: [
                {
                    type: 'button',
                    text: options.buttonText || 'OK',
                    className: options.buttonClass || 'bg-blue-600 hover:bg-blue-700 text-white font-semibold py-4 px-8 rounded-xl transition-colors',
                    onClick: options.onButtonClick
                }
            ],
            ...options
        });
    }

    /**
     * Show a confirmation popup
     * @param {Object} options - Confirmation popup options
     */
    showConfirmation(options = {}) {
        return this.show({
            icon: 'fas fa-question-circle',
            iconBg: 'bg-yellow-100',
            iconColor: 'text-yellow-600',
            title: options.title || 'Confirm Action',
            description: options.description || 'Are you sure you want to proceed?',
            actions: [
                {
                    type: 'button',
                    text: options.confirmText || 'Confirm',
                    className: options.confirmClass || 'bg-green-600 hover:bg-green-700 text-white font-semibold py-4 px-8 rounded-xl transition-colors',
                    value: 'confirm',
                    onClick: options.onConfirm
                },
                {
                    type: 'text',
                    text: options.cancelText || 'Cancel',
                    className: 'text-sm text-gray-500 cursor-pointer hover:text-gray-700 transition-colors',
                    value: 'cancel',
                    onClick: options.onCancel
                }
            ],
            ...options
        });
    }
}

// Create global popup manager instance
const popupManager = new PopupManager();

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { PopupManager, popupManager };
} else {
    window.PopupManager = PopupManager;
    window.popupManager = popupManager;
}

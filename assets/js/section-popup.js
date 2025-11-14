/**
 * Section Popup Manager
 * A modular popup system inspired by VPN configuration section popups
 * Provides consistent popup UI, animations, and behavior across all section components
 */

class SectionPopupManager {
    constructor() {
        this.activePopup = null;
        this.popupCounter = 0;
        this.defaultConfig = {
            maxWidth: '1000px',
            width: '90%',
            maxHeight: '90vh',
            closeOnBackdrop: true,
            closeOnEscape: true,
            showCloseButton: true,
            animationDuration: 300
        };
        
        this.init();
    }

    init() {
        console.log('[SECTION POPUP] Initializing section popup manager');
        this.setupGlobalEventListeners();
        this.injectStyles();
    }

    /**
     * Setup global event listeners for popup management
     */
    setupGlobalEventListeners() {
        // Close on escape key
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.activePopup) {
                this.close();
            }
        });

        // Close on backdrop click
        document.addEventListener('click', (e) => {
            if (e.target.classList.contains('section-popup-overlay') && 
                this.activePopup && 
                this.activePopup.config.closeOnBackdrop) {
                this.close();
            }
        });
    }

    /**
     * Inject popup styles
     */
    injectStyles() {
        const styleId = 'section-popup-styles';
        if (document.getElementById(styleId)) return;

        const styles = `
            .section-popup-overlay {
                position: fixed;
                inset: 0;
                background: rgba(0, 0, 0, 0.5);
                display: none;
                align-items: center;
                justify-content: center;
                z-index: 1000;
                opacity: 0;
                visibility: hidden;
                transition: opacity ${this.defaultConfig.animationDuration}ms ease, 
                           visibility ${this.defaultConfig.animationDuration}ms ease;
            }

            .section-popup-overlay.show {
                opacity: 1;
                visibility: visible;
            }

            .section-popup-content {
                background: white;
                border-radius: 12px;
                padding: 0;
                max-width: ${this.defaultConfig.maxWidth};
                width: ${this.defaultConfig.width};
                max-height: ${this.defaultConfig.maxHeight};
                overflow-y: auto;
                transform: scale(0.9);
                transition: transform ${this.defaultConfig.animationDuration}ms ease;
                box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);
            }

            .section-popup-overlay.show .section-popup-content {
                transform: scale(1);
            }

            .section-popup-content:focus {
                outline: none;
            }

            .section-popup-header {
                border-bottom: 1px solid #e5e7eb;
                padding: 1.5rem;
            }

            .section-popup-body {
                padding: 1.5rem;
            }

            .section-popup-footer {
                border-top: 1px solid #e5e7eb;
                padding: 1.5rem;
                display: flex;
                justify-content: flex-end;
                align-items: center;
                gap: 0.75rem;
            }

            .section-popup-close-btn {
                width: 2rem;
                height: 2rem;
                border-radius: 50%;
                background: #f3f4f6;
                border: none;
                cursor: pointer;
                display: flex;
                align-items: center;
                justify-content: center;
                transition: background-color 0.2s ease;
            }

            .section-popup-close-btn:hover {
                background: #e5e7eb;
            }

            .section-popup-icon {
                width: 2.5rem;
                height: 2.5rem;
                border-radius: 0.5rem;
                display: flex;
                align-items: center;
                justify-content: center;
                flex-shrink: 0;
            }

            .section-popup-title {
                font-size: 1.5rem;
                font-weight: 600;
                color: #111827;
                margin: 0;
            }

            .section-popup-description {
                font-size: 0.875rem;
                color: #6b7280;
                margin: 0.25rem 0 0 0;
            }

            .section-popup-btn {
                padding: 0.5rem 1.5rem;
                border-radius: 0.5rem;
                font-weight: 500;
                cursor: pointer;
                transition: all 0.2s ease;
                border: 1px solid transparent;
            }

            .section-popup-btn-primary {
                background: #111827;
                color: white;
            }

            .section-popup-btn-primary:hover {
                background: #1f2937;
            }

            .section-popup-btn-secondary {
                background: white;
                color: #374151;
                border-color: #d1d5db;
            }

            .section-popup-btn-secondary:hover {
                background: #f9fafb;
            }

            /* Responsive design */
            @media (max-width: 768px) {
                .section-popup-content {
                    width: 95%;
                    max-height: 95vh;
                    margin: 1rem;
                }

                .section-popup-header,
                .section-popup-body,
                .section-popup-footer {
                    padding: 1rem;
                }
            }
        `;

        const styleElement = document.createElement('style');
        styleElement.id = styleId;
        styleElement.textContent = styles;
        document.head.appendChild(styleElement);
    }

    /**
     * Show a popup with the provided configuration
     */
    show(config = {}) {
        // Merge with default config
        const finalConfig = { ...this.defaultConfig, ...config };
        
        // Generate unique popup ID
        const popupId = `section-popup-${++this.popupCounter}`;
        
        // Create popup element
        const popup = this.createPopupElement(popupId, finalConfig);
        
        // Add to DOM
        document.body.appendChild(popup);
        
        // Store reference
        this.activePopup = {
            element: popup,
            config: finalConfig,
            id: popupId
        };

        // Show with animation
        this.showWithAnimation(popup);

        // Setup event listeners
        this.setupPopupEventListeners(popup, finalConfig);

        // Focus first element
        this.focusFirstElement(popup);

        console.log(`[SECTION POPUP] Showing popup: ${popupId}`);
        
        // Return promise that resolves when popup is closed
        return new Promise((resolve) => {
            this.activePopup.resolve = resolve;
        });
    }

    /**
     * Create popup DOM element
     */
    createPopupElement(popupId, config) {
        const popup = document.createElement('div');
        popup.className = 'section-popup-overlay';
        popup.id = popupId;

        // Create header
        const header = this.createHeader(config);
        
        // Create body
        const body = this.createBody(config);
        
        // Create footer
        const footer = this.createFooter(config);

        // Assemble popup
        popup.innerHTML = `
            <div class="section-popup-content">
                ${header}
                ${body}
                ${footer}
            </div>
        `;

        return popup;
    }

    /**
     * Create popup header
     */
    createHeader(config) {
        const iconHtml = config.icon ? `
            <div class="section-popup-icon" style="background-color: ${config.iconColor || '#f3f4f6'};">
                <i class="${config.icon}" style="color: ${config.iconTextColor || '#374151'};"></i>
            </div>
        ` : '';

        const titleHtml = config.title ? `
            <h1 class="section-popup-title">${config.title}</h1>
        ` : '';

        const descriptionHtml = config.description ? `
            <p class="section-popup-description">${config.description}</p>
        ` : '';

        const closeButtonHtml = config.showCloseButton ? `
            <button class="section-popup-close-btn" data-action="close">
                <i class="fa-solid fa-xmark" style="color: #6b7280;"></i>
            </button>
        ` : '';

        return `
            <div class="section-popup-header">
                <div style="display: flex; align-items: center; justify-content: space-between; width: 100%;">
                    <div style="display: flex; align-items: center; gap: 0.75rem;">
                        ${iconHtml}
                        <div>
                            ${titleHtml}
                            ${descriptionHtml}
                        </div>
                    </div>
                    ${closeButtonHtml}
                </div>
            </div>
        `;
    }

    /**
     * Create popup body
     */
    createBody(config) {
        if (config.customContent) {
            return `<div class="section-popup-body">${config.customContent.outerHTML || config.customContent}</div>`;
        } else if (config.content) {
            return `<div class="section-popup-body">${config.content}</div>`;
        } else {
            return `<div class="section-popup-body"></div>`;
        }
    }

    /**
     * Create popup footer
     */
    createFooter(config) {
        const buttons = [];

        if (config.showCancel !== false) {
            buttons.push(`
                <button class="section-popup-btn section-popup-btn-secondary" data-action="cancel">
                    ${config.cancelText || 'Cancel'}
                </button>
            `);
        }

        if (config.showConfirm !== false) {
            buttons.push(`
                <button class="section-popup-btn section-popup-btn-primary" data-action="confirm">
                    ${config.confirmText || 'Confirm'}
                </button>
            `);
        }

        return buttons.length > 0 ? `
            <div class="section-popup-footer">
                ${buttons.join('')}
            </div>
        ` : '';
    }

    /**
     * Show popup with animation
     */
    showWithAnimation(popup) {
        // Trigger reflow
        popup.offsetHeight;
        
        // Add show class
        popup.classList.add('show');
        
        // Prevent body scroll
        document.body.style.overflow = 'hidden';
    }

    /**
     * Hide popup with animation
     */
    hideWithAnimation(popup) {
        return new Promise((resolve) => {
            popup.classList.remove('show');
            
            setTimeout(() => {
                popup.style.display = 'none';
                document.body.style.overflow = '';
                resolve();
            }, this.defaultConfig.animationDuration);
        });
    }

    /**
     * Setup event listeners for popup
     */
    setupPopupEventListeners(popup, config) {
        // Close button
        const closeBtn = popup.querySelector('[data-action="close"]');
        if (closeBtn) {
            closeBtn.addEventListener('click', () => this.close('close'));
        }

        // Cancel button
        const cancelBtn = popup.querySelector('[data-action="cancel"]');
        if (cancelBtn) {
            cancelBtn.addEventListener('click', () => this.close('cancel'));
        }

        // Confirm button
        const confirmBtn = popup.querySelector('[data-action="confirm"]');
        if (confirmBtn) {
            confirmBtn.addEventListener('click', () => this.close('confirm'));
        }

        // Custom button handlers
        if (config.buttonHandlers) {
            Object.keys(config.buttonHandlers).forEach(action => {
                const btn = popup.querySelector(`[data-action="${action}"]`);
                if (btn) {
                    btn.addEventListener('click', () => {
                        const result = config.buttonHandlers[action]();
                        if (result !== false) {
                            this.close(action);
                        }
                    });
                }
            });
        }
    }

    /**
     * Focus first focusable element in popup
     */
    focusFirstElement(popup) {
        const focusableElements = popup.querySelectorAll(
            'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
        );
        
        if (focusableElements.length > 0) {
            focusableElements[0].focus();
        }
    }

    /**
     * Close the active popup
     */
    close(action = 'close') {
        if (!this.activePopup) return;

        const { element, resolve } = this.activePopup;

        this.hideWithAnimation(element).then(() => {
            // Remove from DOM
            element.remove();
            
            // Resolve promise
            if (resolve) {
                resolve(action);
            }
            
            // Clear active popup
            this.activePopup = null;
            
            console.log(`[SECTION POPUP] Popup closed with action: ${action}`);
        });
    }

    /**
     * Update popup content
     */
    updateContent(config) {
        if (!this.activePopup) return;

        const { element } = this.activePopup;
        
        // Update header
        if (config.title || config.description || config.icon) {
            const header = this.createHeader({ ...this.activePopup.config, ...config });
            const headerElement = element.querySelector('.section-popup-header');
            if (headerElement) {
                headerElement.outerHTML = header;
            }
        }

        // Update body
        if (config.content || config.customContent) {
            const body = this.createBody({ ...this.activePopup.config, ...config });
            const bodyElement = element.querySelector('.section-popup-body');
            if (bodyElement) {
                bodyElement.outerHTML = body;
            }
        }

        // Update config
        this.activePopup.config = { ...this.activePopup.config, ...config };
    }

    /**
     * Check if popup is currently active
     */
    isActive() {
        return this.activePopup !== null;
    }

    /**
     * Get current popup configuration
     */
    getCurrentConfig() {
        return this.activePopup ? this.activePopup.config : null;
    }
}

// Create global instance
window.sectionPopupManager = new SectionPopupManager();

// Export for module usage
if (typeof module !== 'undefined' && module.exports) {
    module.exports = SectionPopupManager;
}

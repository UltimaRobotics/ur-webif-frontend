/**
 * Page Tracking Utility
 * Tracks and restores the current opened section in the dashboard
 */

class PageTracker {
    constructor() {
        this.storageKey = 'dashboard-current-section';
        this.defaultSection = 'system-dashboard';
        this.init();
    }

    init() {
        console.log('[PAGE TRACKER] Initializing page tracking utility');
        
        // Track page navigation events
        this.setupNavigationTracking();
        
        // Track page reload/visibility changes
        this.setupPageVisibilityTracking();
        
        // Track beforeunload to save current state
        this.setupBeforeUnloadTracking();
    }

    /**
     * Setup tracking for navigation clicks
     */
    setupNavigationTracking() {
        // Track navigation item clicks
        document.addEventListener('click', (e) => {
            const navItem = e.target.closest('[data-section]');
            if (navItem) {
                const section = navItem.getAttribute('data-section');
                if (section) {
                    // Check if this is a main dashboard section or sub-section
                    const isMainSection = this.isMainDashboardSection(section);
                    
                    if (isMainSection) {
                        // Only track main dashboard sections
                        this.setCurrentSection(section);
                        console.log(`[PAGE TRACKER] Navigation to main section: ${section}`);
                    } else {
                        // For sub-sections, track the parent main section
                        const parentSection = this.getParentMainSection();
                        if (parentSection) {
                            this.setCurrentSection(parentSection);
                            console.log(`[PAGE TRACKER] Navigation to sub-section: ${section}, tracking parent: ${parentSection}`);
                        }
                    }
                }
            }
        });

        // Track sidebar toggle clicks
        const sidebarToggle = document.getElementById('sidebar-toggle');
        if (sidebarToggle) {
            sidebarToggle.addEventListener('click', () => {
                this.saveSidebarState(true);
            });
        }
    }

    isMainDashboardSection(section) {
        // Check if the section is a main dashboard section
        const mainSections = [
            'system-dashboard', 'wired-config', 'wireless-config', 'cellular-config',
            'vpn-config', 'advanced-network', 'network-benchmark', 'network-priority',
            'mavlink-overview', 'attached-ip-devices', 'mavlink-extension',
            'firmware-upgrade', 'license-management', 'backup-restore', 'about-us'
        ];
        return mainSections.includes(section);
    }

    getParentMainSection() {
        // Find the current main section based on the active navigation item
        const activeMainNavItem = document.querySelector('[data-section].bg-neutral-50, [data-section].border-l-2');
        if (activeMainNavItem) {
            const section = activeMainNavItem.getAttribute('data-section');
            if (this.isMainDashboardSection(section)) {
                return section;
            }
        }
        return null;
    }

    /**
     * Setup tracking for page visibility changes
     */
    setupPageVisibilityTracking() {
        document.addEventListener('visibilitychange', () => {
            if (!document.hidden) {
                // Page became visible again, check if we need to restore section
                this.restoreSectionIfNeeded();
            }
        });

        // Track page focus
        window.addEventListener('focus', () => {
            this.restoreSectionIfNeeded();
        });
    }

    /**
     * Setup tracking for beforeunload event
     */
    setupBeforeUnloadTracking() {
        window.addEventListener('beforeunload', () => {
            // Save current section before page unloads
            const currentSection = this.getCurrentSection();
            if (currentSection) {
                this.setCurrentSection(currentSection);
                console.log(`[PAGE TRACKER] Saved section before unload: ${currentSection}`);
            }
        });
    }

    /**
     * Set the current section in localStorage
     */
    setCurrentSection(section) {
        try {
            const trackingData = {
                section: section,
                timestamp: Date.now(),
                url: window.location.pathname
            };
            localStorage.setItem(this.storageKey, JSON.stringify(trackingData));
            console.log(`[PAGE TRACKER] Saved current section: ${section}`);
        } catch (error) {
            console.error('[PAGE TRACKER] Failed to save section:', error);
        }
    }

    /**
     * Get the current section from localStorage
     */
    getCurrentSection() {
        try {
            const data = localStorage.getItem(this.storageKey);
            if (data) {
                const trackingData = JSON.parse(data);
                return trackingData.section;
            }
        } catch (error) {
            console.error('[PAGE TRACKER] Failed to get current section:', error);
        }
        return null;
    }

    /**
     * Get the full tracking data
     */
    getTrackingData() {
        try {
            const data = localStorage.getItem(this.storageKey);
            if (data) {
                return JSON.parse(data);
            }
        } catch (error) {
            console.error('[PAGE TRACKER] Failed to get tracking data:', error);
        }
        return null;
    }

    /**
     * Clear the tracking data
     */
    clearTrackingData() {
        try {
            localStorage.removeItem(this.storageKey);
            console.log('[PAGE TRACKER] Cleared tracking data');
        } catch (error) {
            console.error('[PAGE TRACKER] Failed to clear tracking data:', error);
        }
    }

    /**
     * Restore section if needed (called on page load/visibility)
     */
    restoreSectionIfNeeded() {
        const trackingData = this.getTrackingData();
        if (trackingData && trackingData.section) {
            const currentSection = this.getCurrentSectionFromDOM();
            
            // Only restore if different from current section
            if (currentSection !== trackingData.section) {
                console.log(`[PAGE TRACKER] Restoring section: ${trackingData.section}`);
                this.navigateToSection(trackingData.section);
            }
        }
    }

    /**
     * Get current section from DOM
     */
    getCurrentSectionFromDOM() {
        // Check for active navigation item
        const activeNavItem = document.querySelector('[data-section].bg-neutral-50, [data-section].border-l-2');
        if (activeNavItem) {
            return activeNavItem.getAttribute('data-section');
        }
        return null;
    }

    /**
     * Navigate to a specific section
     */
    navigateToSection(section) {
        // Check if this is a main dashboard section
        if (this.isMainDashboardSection(section)) {
            // Find the navigation item for this main section
            const navItem = document.querySelector(`[data-section="${section}"]`);
            if (navItem) {
                // Simulate click on the navigation item
                navItem.click();
                console.log(`[PAGE TRACKER] Navigated to main section: ${section}`);
            } else {
                console.warn(`[PAGE TRACKER] Main navigation item not found for section: ${section}`);
            }
        } else {
            // This is a sub-section, find the parent main section first
            console.warn(`[PAGE TRACKER] Cannot navigate directly to sub-section: ${section}. Please navigate to parent main section first.`);
            
            // Try to find and navigate to the advanced-network section if it's a network sub-section
            if (section === 'vlans' || section === 'nat-rules' || section === 'firewall-rules' || 
                section === 'static-routes' || section === 'bridges') {
                const advancedNetworkNav = document.querySelector('[data-section="advanced-network"]');
                if (advancedNetworkNav) {
                    advancedNetworkNav.click();
                    console.log(`[PAGE TRACKER] Navigated to advanced-network section for sub-section: ${section}`);
                    
                    // After a delay, try to click on the sub-section within the advanced network
                    setTimeout(() => {
                        const subNavItem = document.querySelector(`[data-section="${section}"]`);
                        if (subNavItem) {
                            subNavItem.click();
                            console.log(`[PAGE TRACKER] Navigated to sub-section: ${section}`);
                        }
                    }, 500);
                }
            }
        }
    }

    /**
     * Save sidebar state
     */
    saveSidebarState(isOpen) {
        try {
            const trackingData = this.getTrackingData() || {};
            trackingData.sidebarOpen = isOpen;
            localStorage.setItem(this.storageKey, JSON.stringify(trackingData));
        } catch (error) {
            console.error('[PAGE TRACKER] Failed to save sidebar state:', error);
        }
    }

    /**
     * Get sidebar state
     */
    getSidebarState() {
        try {
            const trackingData = this.getTrackingData();
            return trackingData ? trackingData.sidebarOpen : false;
        } catch (error) {
            console.error('[PAGE TRACKER] Failed to get sidebar state:', error);
            return false;
        }
    }

    /**
     * Restore sidebar state
     */
    restoreSidebarState() {
        const sidebarOpen = this.getSidebarState();
        const sidebar = document.getElementById('sidebar');
        const sidebarToggle = document.getElementById('sidebar-toggle');
        
        if (sidebar && sidebarToggle && sidebarOpen) {
            // Open sidebar if it was previously open
            if (!sidebar.classList.contains('-translate-x-full')) {
                sidebarToggle.click();
            }
        }
    }

    /**
     * Initialize page restoration on load
     */
    initializePageRestore() {
        // Wait a bit for dashboard to fully load
        setTimeout(() => {
            const trackingData = this.getTrackingData();
            
            if (trackingData && trackingData.section) {
                console.log(`[PAGE TRACKER] Restoring saved section: ${trackingData.section}`);
                
                // Restore section
                this.navigateToSection(trackingData.section);
                
                // Restore sidebar state
                this.restoreSidebarState();
                
                // Clear old data (optional - comment out if you want to persist)
                // this.clearTrackingData();
            } else {
                console.log('[PAGE TRACKER] No saved section found, using default');
            }
        }, 500);
    }
}

// Initialize the page tracker when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.pageTracker = new PageTracker();
    
    // Initialize page restore after dashboard loads
    if (window.dashboardManager) {
        // If dashboard manager exists, wait for it to initialize
        setTimeout(() => {
            window.pageTracker.initializePageRestore();
        }, 1000);
    } else {
        // Otherwise initialize after a short delay
        setTimeout(() => {
            window.pageTracker.initializePageRestore();
        }, 1000);
    }
});

// Export for use in other scripts
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PageTracker;
}

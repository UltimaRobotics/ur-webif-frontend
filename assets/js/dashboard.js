/**
 * Dashboard Script - Standalone Demo Mode with Content Loading
 * Loads section HTML files dynamically into main content area
 */

class DashboardManager {
    constructor() {
        this.sidebar = document.getElementById('sidebar');
        this.sidebarToggle = document.getElementById('sidebar-toggle');
        this.mainContent = document.getElementById('main-content');
        this.settingsBtn = document.getElementById('settings-btn');
        this.logoutBtn = document.getElementById('logout-btn');
        this.settingsModal = document.getElementById('settings-modal');
        this.closeSettingsBtn = document.getElementById('close-settings-modal');
        this.notificationModal = document.getElementById('system-notification-modal');
        this.closeNotificationBtn = document.getElementById('close-notification-modal');
        
        this.currentSection = null;
        this.isSidebarOpen = false;
        
        // Section to HTML file mapping
        this.sectionMap = {
            'system-dashboard': 'landing-section.html',
            'wired-config': 'wired-config-section.html',
            'wireless-config': 'wireless-config-section.html',
            'cellular-config': 'cellular-config-section.html',
            'vpn-config': 'vpn-configuration-section.html',
            'advanced-network': 'advance-network-section.html',
            'network-benchmark': 'network-utility-section.html',
            'network-priority': 'network-priority-section.html',
            'mavlink-overview': 'mavlink-system-overview-section.html',
            'attached-ip-devices': 'attached-ip-cameras-section.html',
            'mavlink-extension': 'mavlink-extension-section.html',
            'firmware-upgrade': 'firmware-upgrade-section.html',
            'license-management': 'license-section.html',
            'backup-restore': 'backup-restore-section.html',
            'about-us': 'about-us-section.html'
        };
        
        this.init();
    }

    init() {
        console.log('[DASHBOARD] Initializing dashboard (Demo Mode)');
        
        // Check if user is logged in (demo mode - allow access)
        const session = localStorage.getItem('session');
        if (!session) {
            console.log('[DASHBOARD] No session found, but allowing demo mode access');
            // In demo mode, create a temporary session to prevent redirect loops
            localStorage.setItem('session', 'demo-session');
        }
        
        // Check if we should restore a saved section before initializing
        const trackingData = this.getTrackingData();
        const shouldRestoreSection = trackingData && trackingData.section;
        
        if (shouldRestoreSection) {
            console.log('[DASHBOARD] Found saved section, will restore:', trackingData.section);
            // Set current section to saved one to prevent landing section flash
            this.currentSection = trackingData.section;
            
            // Hide main content initially to prevent flash
            if (this.mainContent) {
                this.mainContent.style.opacity = '0';
                this.mainContent.style.transition = 'opacity 0.3s ease-in-out';
            }
        }
        
        // Bind event listeners
        this.sidebarToggle.addEventListener('click', () => this.toggleSidebar());
        this.settingsBtn.addEventListener('click', () => this.openSettings());
        this.logoutBtn.addEventListener('click', () => this.logout());
        this.closeSettingsBtn.addEventListener('click', () => this.closeSettings());
        this.closeNotificationBtn.addEventListener('click', () => this.closeNotification());
        
        // Settings toggle
        document.getElementById('user-settings-toggle').addEventListener('click', () => {
            const content = document.getElementById('user-settings-content');
            const chevron = document.getElementById('user-settings-chevron');
            content.classList.toggle('hidden');
            chevron.classList.toggle('rotate-180');
        });

        // Setup navigation
        this.setupNavigation();
        
        // Setup sidebar toggles
        this.setupSidebarToggles();
        
        // Load initial content
        if (shouldRestoreSection) {
            // Restore sidebar state first
            if (trackingData.sidebarOpen) {
                // Open sidebar before loading section
                this.sidebar.classList.remove('-translate-x-full');
                this.mainContent.classList.add('ml-64');
                this.isSidebarOpen = true;
                
                // Notify page tracker of sidebar state
                if (window.pageTracker) {
                    window.pageTracker.saveSidebarState(true);
                }
            }
            
            // Load the saved section directly
            this.loadSection(trackingData.section);
            
            // Show content with fade-in effect
            setTimeout(() => {
                if (this.mainContent) {
                    this.mainContent.style.opacity = '1';
                }
            }, 300);
        } else {
            // Load default landing section
            this.loadSection('system-dashboard');
        }
    }

    getTrackingData() {
        try {
            const data = localStorage.getItem('dashboard-current-section');
            if (data) {
                return JSON.parse(data);
            }
        } catch (error) {
            console.error('[DASHBOARD] Failed to get tracking data:', error);
        }
        return null;
    }

    setupSidebarToggles() {
        // Network toggle
        const networkToggle = document.getElementById('nav-network-toggle');
        const networkList = document.getElementById('nav-network-list');
        const networkHeader = document.getElementById('nav-network-header');
        
        networkHeader.addEventListener('click', (e) => {
            if (e.target === networkToggle || networkToggle.contains(e.target)) return;
            networkList.classList.toggle('hidden');
            networkToggle.querySelector('i').classList.toggle('fa-chevron-down');
            networkToggle.querySelector('i').classList.toggle('fa-chevron-up');
        });
        
        networkToggle.addEventListener('click', (e) => {
            e.stopPropagation();
            networkList.classList.toggle('hidden');
            networkToggle.querySelector('i').classList.toggle('fa-chevron-down');
            networkToggle.querySelector('i').classList.toggle('fa-chevron-up');
        });
        
        // Devices toggle
        const devicesToggle = document.getElementById('devices-toggle');
        const devicesList = document.getElementById('devices-list');
        const devicesHeader = document.getElementById('nav-devices-header');
        
        devicesHeader.addEventListener('click', (e) => {
            if (e.target === devicesToggle || devicesToggle.contains(e.target)) return;
            devicesList.classList.toggle('hidden');
            devicesToggle.querySelector('i').classList.toggle('fa-chevron-down');
            devicesToggle.querySelector('i').classList.toggle('fa-chevron-up');
        });
        
        devicesToggle.addEventListener('click', (e) => {
            e.stopPropagation();
            devicesList.classList.toggle('hidden');
            devicesToggle.querySelector('i').classList.toggle('fa-chevron-down');
            devicesToggle.querySelector('i').classList.toggle('fa-chevron-up');
        });
        
        // Features toggle
        const featuresToggle = document.getElementById('features-toggle');
        const featuresList = document.getElementById('features-list');
        const featuresHeader = document.getElementById('nav-features-header');
        
        featuresHeader.addEventListener('click', (e) => {
            if (e.target === featuresToggle || featuresToggle.contains(e.target)) return;
            featuresList.classList.toggle('hidden');
            featuresToggle.querySelector('i').classList.toggle('fa-chevron-down');
            featuresToggle.querySelector('i').classList.toggle('fa-chevron-up');
        });
        
        featuresToggle.addEventListener('click', (e) => {
            e.stopPropagation();
            featuresList.classList.toggle('hidden');
            featuresToggle.querySelector('i').classList.toggle('fa-chevron-down');
            featuresToggle.querySelector('i').classList.toggle('fa-chevron-up');
        });
    }

    setupNavigation() {
        const navItems = document.querySelectorAll('[data-section]');
        navItems.forEach(item => {
            item.addEventListener('click', () => {
                const section = item.getAttribute('data-section');
                this.loadSection(section);
            });
        });
    }

    toggleSidebar() {
        this.isSidebarOpen = !this.isSidebarOpen;
        if (this.isSidebarOpen) {
            this.sidebar.classList.remove('-translate-x-full');
            this.mainContent.classList.add('ml-64');
        } else {
            this.sidebar.classList.add('-translate-x-full');
            this.mainContent.classList.remove('ml-64');
        }
        
        // Notify page tracker of sidebar state change
        if (window.pageTracker) {
            window.pageTracker.saveSidebarState(this.isSidebarOpen);
        }
    }

    async loadSection(sectionId) {
        console.log('[DASHBOARD] Loading section:', sectionId);
        this.currentSection = sectionId;
        
        // Notify page tracker of section change
        if (window.pageTracker) {
            window.pageTracker.setCurrentSection(sectionId);
        }
        
        // Update active state for navigation
        this.updateNavigationState(sectionId);
        
        const htmlFile = this.sectionMap[sectionId];
        if (!htmlFile) {
            console.error('[DASHBOARD] Unknown section:', sectionId);
            this.mainContent.innerHTML = `
                <div class="text-center p-8">
                    <p class="text-neutral-600">Section not found: ${sectionId}</p>
                </div>
            `;
            return;
        }
        
        try {
            // Show loading
            this.mainContent.innerHTML = `
                <div class="flex items-center justify-center p-12">
                    <div class="text-center">
                        <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-neutral-600 mx-auto mb-4"></div>
                        <p class="text-neutral-600">Loading ${htmlFile}...</p>
                    </div>
                </div>
            `;
            
            // Fetch section HTML
            const response = await fetch(`components/${htmlFile}`);
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const html = await response.text();
            console.log('[DASHBOARD] Content loaded successfully, length:', html.length);
            
            // Inject HTML
            this.mainContent.innerHTML = html;
            
            // Execute any inline scripts
            this.executeInlineScripts(this.mainContent);
            
            console.log('[DASHBOARD] Section loaded successfully');
        } catch (error) {
            console.error('[DASHBOARD] Error loading section:', error);
            this.mainContent.innerHTML = `
                <div class="text-center p-8">
                    <p class="text-neutral-600 mb-4">Error loading content: ${error.message}</p>
                    <p class="text-sm text-neutral-500">File: components/${htmlFile}</p>
                    <button onclick="location.reload()" class="mt-4 px-4 py-2 bg-blue-500 text-white rounded hover:bg-blue-600">
                        Retry
                    </button>
                </div>
            `;
        }
    }

    updateNavigationState(sectionId) {
        // Remove active state from all navigation items
        const navItems = document.querySelectorAll('[data-section]');
        navItems.forEach(nav => {
            nav.classList.remove('bg-neutral-50', 'border-l-2', 'border-neutral-600');
        });
        
        // Add active state to current section
        const currentNavItem = document.querySelector(`[data-section="${sectionId}"]`);
        if (currentNavItem) {
            currentNavItem.classList.add('bg-neutral-50', 'border-l-2', 'border-neutral-600');
            
            // Ensure parent sections are expanded
            this.expandParentSections(currentNavItem);
        }
    }

    expandParentSections(navItem) {
        console.log('[DASHBOARD] Expanding parent sections for:', navItem.getAttribute('data-section'));
        
        // Find parent sections and expand them
        let parent = navItem.parentElement;
        
        while (parent) {
            // Check if this is a sub-section list
            if (parent.id && parent.id.endsWith('-list')) {
                // Find the header and toggle button
                const headerId = parent.id.replace('-list', '-header');
                const toggleId = parent.id.replace('-list', '-toggle');
                const header = document.getElementById(headerId);
                const toggle = document.getElementById(toggleId);
                
                if (header && toggle && parent.classList.contains('hidden')) {
                    console.log(`[DASHBOARD] Expanding parent section: ${headerId}`);
                    
                    // Expand the section
                    parent.classList.remove('hidden');
                    
                    // Update chevron icon
                    const icon = toggle.querySelector('i');
                    if (icon) {
                        icon.classList.remove('fa-chevron-down');
                        icon.classList.add('fa-chevron-up');
                    }
                }
            }
            
            parent = parent.parentElement;
        }
        
        // Also ensure the main sidebar is open if we're expanding sections
        if (!this.isSidebarOpen) {
            console.log('[DASHBOARD] Opening sidebar to show expanded sections');
            this.sidebar.classList.remove('-translate-x-full');
            this.mainContent.classList.add('ml-64');
            this.isSidebarOpen = true;
            
            // Notify page tracker of sidebar state change
            if (window.pageTracker) {
                window.pageTracker.saveSidebarState(true);
            }
        }
    }

    executeInlineScripts(container) {
        const scripts = container.querySelectorAll('script');
        scripts.forEach(script => {
            try {
                const scriptContent = script.textContent.trim();
                if (scriptContent) {
                    eval(scriptContent);
                }
            } catch (error) {
                console.error('[DASHBOARD] Script execution error:', error);
            }
        });
    }

    openSettings() {
        this.settingsModal.classList.remove('hidden');
    }

    closeSettings() {
        this.settingsModal.classList.add('hidden');
    }

    showNotification(title, message, type = 'info') {
        const iconMap = {
            'info': 'fa-info-circle text-blue-600',
            'success': 'fa-check-circle text-green-600',
            'warning': 'fa-exclamation-triangle text-yellow-600',
            'error': 'fa-times-circle text-red-600'
        };
        
        document.getElementById('notification-title').textContent = title;
        document.getElementById('notification-message').textContent = message;
        document.getElementById('notification-icon').innerHTML = `<i class="fa-solid ${iconMap[type]} text-3xl"></i>`;
        this.notificationModal.classList.remove('hidden');
    }

    closeNotification() {
        this.notificationModal.classList.add('hidden');
    }

    logout() {
        console.log('[DASHBOARD] Logging out');
        localStorage.removeItem('session');
        localStorage.removeItem('demo_mode');
        window.location.href = 'login-page.html';
    }
}

// Initialize dashboard on DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new DashboardManager();
    });
} else {
    new DashboardManager();
}

console.log('[DASHBOARD] Dashboard script loaded (Demo Mode)');

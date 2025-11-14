/**
 * Section Popup Usage Examples
 * Demonstrates how to integrate the modular popup system into section components
 */

// Example 1: Simple confirmation popup
function showConfirmPopup() {
    return window.sectionPopupManager.show({
        title: 'Confirm Action',
        description: 'Are you sure you want to perform this action?',
        icon: 'fa-solid fa-exclamation-triangle',
        iconColor: '#fef3c7',
        iconTextColor: '#d97706',
        confirmText: 'Yes, Continue',
        cancelText: 'Cancel'
    });
}

// Example 2: Form popup with custom content
function showFormPopup() {
    // Create custom form content
    const formContent = document.createElement('div');
    formContent.innerHTML = `
        <form id="example-form" class="space-y-4">
            <div>
                <label class="block text-sm font-medium text-gray-700 mb-1">Name</label>
                <input type="text" name="name" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500" required>
            </div>
            <div>
                <label class="block text-sm font-medium text-gray-700 mb-1">Email</label>
                <input type="email" name="email" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500" required>
            </div>
            <div>
                <label class="block text-sm font-medium text-gray-700 mb-1">Description</label>
                <textarea name="description" rows="3" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"></textarea>
            </div>
        </form>
    `;

    return window.sectionPopupManager.show({
        title: 'Add New Item',
        description: 'Fill in the details below to create a new item',
        icon: 'fa-solid fa-plus',
        iconColor: '#dbeafe',
        iconTextColor: '#1d4ed8',
        customContent: formContent,
        confirmText: 'Create Item',
        cancelText: 'Cancel',
        buttonHandlers: {
            confirm: () => {
                const form = document.getElementById('example-form');
                if (form.checkValidity()) {
                    const formData = new FormData(form);
                    const data = Object.fromEntries(formData);
                    console.log('Form data:', data);
                    return true; // Close popup
                } else {
                    form.reportValidity();
                    return false; // Keep popup open
                }
            }
        }
    });
}

// Example 3: Edit popup with pre-filled data
function showEditPopup(itemData) {
    const editContent = document.createElement('div');
    editContent.innerHTML = `
        <form id="edit-form" class="space-y-4">
            <div>
                <label class="block text-sm font-medium text-gray-700 mb-1">Item Name</label>
                <input type="text" name="name" value="${itemData.name || ''}" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500" required>
            </div>
            <div>
                <label class="block text-sm font-medium text-gray-700 mb-1">Status</label>
                <select name="status" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                    <option value="active" ${itemData.status === 'active' ? 'selected' : ''}>Active</option>
                    <option value="inactive" ${itemData.status === 'inactive' ? 'selected' : ''}>Inactive</option>
                </select>
            </div>
        </form>
    `;

    return window.sectionPopupManager.show({
        title: 'Edit Item',
        description: `Modify the details for "${itemData.name}"`,
        icon: 'fa-solid fa-edit',
        iconColor: '#f3f4f6',
        iconTextColor: '#374151',
        customContent: editContent,
        confirmText: 'Save Changes',
        cancelText: 'Cancel',
        buttonHandlers: {
            confirm: () => {
                const form = document.getElementById('edit-form');
                if (form.checkValidity()) {
                    const formData = new FormData(form);
                    const updatedData = Object.fromEntries(formData);
                    console.log('Updated data:', updatedData);
                    // Here you would typically make an API call
                    return true; // Close popup
                } else {
                    form.reportValidity();
                    return false; // Keep popup open
                }
            }
        }
    });
}

// Example 4: Information popup (no buttons)
function showInfoPopup(message) {
    const infoContent = document.createElement('div');
    infoContent.innerHTML = `
        <div class="prose max-w-none">
            <p>${message}</p>
        </div>
    `;

    return window.sectionPopupManager.show({
        title: 'Information',
        icon: 'fa-solid fa-info-circle',
        iconColor: '#dbeafe',
        iconTextColor: '#1d4ed8',
        customContent: infoContent,
        showCancel: false,
        showConfirm: false,
        closeOnBackdrop: true,
        closeOnEscape: true
    });
}

// Example 5: Complex multi-step popup
function showMultiStepPopup() {
    let currentStep = 1;
    const totalSteps = 3;

    function getStepContent(step) {
        const contents = {
            1: `
                <div class="space-y-4">
                    <h3 class="text-lg font-medium">Step 1: Basic Information</h3>
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-1">Title</label>
                        <input type="text" class="w-full px-3 py-2 border border-gray-300 rounded-lg">
                    </div>
                </div>
            `,
            2: `
                <div class="space-y-4">
                    <h3 class="text-lg font-medium">Step 2: Configuration</h3>
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-1">Option</label>
                        <select class="w-full px-3 py-2 border border-gray-300 rounded-lg">
                            <option>Option A</option>
                            <option>Option B</option>
                        </select>
                    </div>
                </div>
            `,
            3: `
                <div class="space-y-4">
                    <h3 class="text-lg font-medium">Step 3: Review</h3>
                    <div class="bg-gray-50 p-4 rounded-lg">
                        <p>Please review your settings before completing.</p>
                    </div>
                </div>
            `
        };
        return contents[step] || '';
    }

    function updatePopupStep() {
        const stepContent = document.createElement('div');
        stepContent.innerHTML = `
            <div class="space-y-4">
                <div class="flex items-center justify-between">
                    <div class="flex space-x-2">
                        ${Array.from({length: totalSteps}, (_, i) => `
                            <div class="w-8 h-8 rounded-full flex items-center justify-center text-sm font-medium ${
                                i + 1 <= currentStep ? 'bg-blue-600 text-white' : 'bg-gray-200 text-gray-600'
                            }">
                                ${i + 1}
                            </div>
                        `).join('')}
                    </div>
                    <span class="text-sm text-gray-600">Step ${currentStep} of ${totalSteps}</span>
                </div>
                ${getStepContent(currentStep)}
            </div>
        `;

        window.sectionPopupManager.updateContent({
            customContent: stepContent,
            confirmText: currentStep === totalSteps ? 'Complete' : 'Next',
            cancelText: currentStep === 1 ? 'Cancel' : 'Previous',
            buttonHandlers: {
                confirm: () => {
                    if (currentStep < totalSteps) {
                        currentStep++;
                        updatePopupStep();
                        return false; // Keep popup open
                    } else {
                        console.log('Multi-step form completed');
                        return true; // Close popup
                    }
                },
                cancel: () => {
                    if (currentStep > 1) {
                        currentStep--;
                        updatePopupStep();
                        return false; // Keep popup open
                    } else {
                        return true; // Close popup
                    }
                }
            }
        });
    }

    // Show initial popup
    window.sectionPopupManager.show({
        title: 'Multi-Step Wizard',
        description: 'Complete the following steps to configure your settings',
        icon: 'fa-solid fa-wizard',
        iconColor: '#f3e8ff',
        iconTextColor: '#7c3aed',
        confirmText: 'Next',
        cancelText: 'Cancel',
        buttonHandlers: {
            confirm: () => {
                currentStep++;
                updatePopupStep();
                return false; // Keep popup open
            }
        }
    });

    // Update content after popup is shown
    setTimeout(updatePopupStep, 100);
}

// Example 6: Integration in section component
class ExampleSectionComponent {
    constructor() {
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Add button
        document.addEventListener('click', (e) => {
            if (e.target.matches('#add-item-btn')) {
                this.showAddItemPopup();
            }
        });

        // Edit buttons
        document.addEventListener('click', (e) => {
            if (e.target.matches('.edit-item-btn')) {
                const itemId = e.target.dataset.id;
                this.showEditItemPopup(itemId);
            }
        });

        // Delete buttons
        document.addEventListener('click', (e) => {
            if (e.target.matches('.delete-item-btn')) {
                const itemId = e.target.dataset.id;
                this.showDeleteConfirmPopup(itemId);
            }
        });
    }

    async showAddItemPopup() {
        const result = await showFormPopup();
        if (result === 'confirm') {
            console.log('Item added successfully');
            // Refresh data or update UI
        }
    }

    async showEditItemPopup(itemId) {
        // Fetch item data (mock)
        const itemData = { id: itemId, name: `Item ${itemId}`, status: 'active' };
        
        const result = await showEditPopup(itemData);
        if (result === 'confirm') {
            console.log('Item updated successfully');
            // Refresh data or update UI
        }
    }

    async showDeleteConfirmPopup(itemId) {
        const result = await window.sectionPopupManager.show({
            title: 'Delete Item',
            description: `Are you sure you want to delete item ${itemId}? This action cannot be undone.`,
            icon: 'fa-solid fa-trash',
            iconColor: '#fee2e2',
            iconTextColor: '#dc2626',
            confirmText: 'Delete',
            cancelText: 'Cancel'
        });

        if (result === 'confirm') {
            console.log('Item deleted successfully');
            // Refresh data or update UI
        }
    }
}

// Initialize example component
window.exampleSectionComponent = new ExampleSectionComponent();

// Export functions for global access
window.showConfirmPopup = showConfirmPopup;
window.showFormPopup = showFormPopup;
window.showEditPopup = showEditPopup;
window.showInfoPopup = showInfoPopup;
window.showMultiStepPopup = showMultiStepPopup;

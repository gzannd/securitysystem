// ESP32 Device Manager - Configuration Page
class ConfigManager {
    constructor() {
        this.apiBase = '/api'; // Base API endpoint
        this.wifiNetworks = [];
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadCurrentConfig();
    }

    bindEvents() {
        // WiFi scan button
        document.getElementById('wifiScanBtn').addEventListener('click', () => {
            this.scanWiFiNetworks();
        });

        // WiFi network selection
        document.getElementById('wifiNetwork').addEventListener('change', (e) => {
            this.onNetworkSelection(e.target.value);
        });

        // Form submission
        document.getElementById('configForm').addEventListener('submit', (e) => {
            e.preventDefault();
            this.saveConfiguration();
        });

        // Reset configuration button
        document.getElementById('resetConfigBtn').addEventListener('click', () => {
            this.resetConfiguration();
        });
    }

    async loadCurrentConfig() {
        try {
            const response = await fetch(`${this.apiBase}/config/current`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const config = await response.json();
            this.populateForm(config);
            
        } catch (error) {
            console.error('Error loading current configuration:', error);
            this.showStatus('Failed to load current configuration.', 'error');
        }
    }

    populateForm(config) {
        // Populate device name
        if (config.deviceName) {
            document.getElementById('deviceName').value = config.deviceName;
        }

        // Populate WiFi network if available
        if (config.wifiSSID) {
            // If we have a current SSID, we might want to show it in the dropdown
            // This would require having the networks list populated
            this.showNetworkSelection();
        }
    }

    async scanWiFiNetworks() {
        try {
            this.showLoadingModal('Scanning for WiFi networks...');
            document.getElementById('wifiScanBtn').disabled = true;

            const response = await fetch(`${this.apiBase}/wifi/scan`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                }
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const data = await response.json();
            this.wifiNetworks = data.networks || [];
            this.populateWiFiNetworks();
            this.hideLoadingModal();
            document.getElementById('wifiScanBtn').disabled = false;

            this.showStatus(`Found ${this.wifiNetworks.length} WiFi networks.`, 'success');

        } catch (error) {
            console.error('Error scanning WiFi networks:', error);
            this.hideLoadingModal();
            document.getElementById('wifiScanBtn').disabled = false;
            this.showStatus('Failed to scan WiFi networks. Please try again.', 'error');
        }
    }

    populateWiFiNetworks() {
        const select = document.getElementById('wifiNetwork');
        const networksGroup = document.getElementById('wifiNetworksGroup');
        
        // Clear existing options except the first one
        select.innerHTML = '<option value="">Select a WiFi network...</option>';

        if (this.wifiNetworks.length === 0) {
            select.innerHTML += '<option value="" disabled>No networks found</option>';
        } else {
            this.wifiNetworks.forEach(network => {
                const option = document.createElement('option');
                option.value = network.ssid;
                option.textContent = `${network.ssid} (${network.rssi} dBm) ${network.encrypted ? '🔒' : '🔓'}`;
                select.appendChild(option);
            });
        }

        networksGroup.style.display = 'block';
    }

    onNetworkSelection(ssid) {
        const passwordGroup = document.getElementById('wifiPasswordGroup');
        
        if (ssid) {
            const selectedNetwork = this.wifiNetworks.find(network => network.ssid === ssid);
            if (selectedNetwork && selectedNetwork.encrypted) {
                passwordGroup.style.display = 'block';
                document.getElementById('wifiPassword').required = true;
            } else {
                passwordGroup.style.display = 'none';
                document.getElementById('wifiPassword').required = false;
            }
        } else {
            passwordGroup.style.display = 'none';
            document.getElementById('wifiPassword').required = false;
        }
    }

    showNetworkSelection() {
        document.getElementById('wifiNetworksGroup').style.display = 'block';
    }

    async saveConfiguration() {
        try {
            this.showStatus('Saving configuration...', 'info');
            
            const formData = new FormData(document.getElementById('configForm'));
            const config = {
                deviceName: formData.get('deviceName'),
                wifiSSID: formData.get('wifiNetwork'),
                wifiPassword: formData.get('wifiPassword') || ''
            };

            // Validate required fields
            if (!config.deviceName.trim()) {
                this.showStatus('Device name is required.', 'error');
                return;
            }

            if (!config.wifiSSID) {
                this.showStatus('Please select a WiFi network.', 'error');
                return;
            }

            const response = await fetch(`${this.apiBase}/config/save`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const result = await response.json();
            
            if (result.success) {
                this.showStatus('Configuration saved successfully! Device will restart to apply changes.', 'success');
                
                // Redirect to device info page after a delay
                setTimeout(() => {
                    window.location.href = 'index.html';
                }, 3000);
            } else {
                this.showStatus(result.message || 'Failed to save configuration.', 'error');
            }

        } catch (error) {
            console.error('Error saving configuration:', error);
            this.showStatus('Failed to save configuration. Please try again.', 'error');
        }
    }

    async resetConfiguration() {
        if (!confirm('Are you sure you want to reset the configuration to defaults? This will clear all current settings.')) {
            return;
        }

        try {
            this.showStatus('Resetting configuration...', 'info');

            const response = await fetch(`${this.apiBase}/config/reset`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                }
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const result = await response.json();
            
            if (result.success) {
                this.showStatus('Configuration reset successfully!', 'success');
                this.loadCurrentConfig(); // Reload the form
            } else {
                this.showStatus(result.message || 'Failed to reset configuration.', 'error');
            }

        } catch (error) {
            console.error('Error resetting configuration:', error);
            this.showStatus('Failed to reset configuration. Please try again.', 'error');
        }
    }

    showLoadingModal(message) {
        document.getElementById('loadingText').textContent = message;
        document.getElementById('loadingModal').style.display = 'block';
    }

    hideLoadingModal() {
        document.getElementById('loadingModal').style.display = 'none';
    }

    showStatus(message, type = 'info') {
        const statusElement = document.getElementById('statusMessage');
        statusElement.textContent = message;
        statusElement.className = `status-message ${type}`;
    }

    hideStatus() {
        const statusElement = document.getElementById('statusMessage');
        statusElement.className = 'status-message';
    }
}

// Mock API responses for development/testing
class MockConfigAPI {
    static async mockCurrentConfig() {
        await new Promise(resolve => setTimeout(resolve, 300));
        
        return {
            deviceName: 'ESP32-Device-001',
            wifiSSID: 'MyWiFiNetwork',
            wifiPassword: ''
        };
    }

    static async mockWiFiScan() {
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        return {
            networks: [
                { ssid: 'MyWiFiNetwork', rssi: -45, encrypted: true },
                { ssid: 'NeighborWiFi', rssi: -67, encrypted: true },
                { ssid: 'OpenNetwork', rssi: -72, encrypted: false },
                { ssid: 'OfficeWiFi', rssi: -55, encrypted: true },
                { ssid: 'GuestNetwork', rssi: -78, encrypted: false }
            ]
        };
    }

    static async mockSaveConfig(config) {
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        console.log('Saving configuration:', config);
        
        return {
            success: true,
            message: 'Configuration saved successfully'
        };
    }

    static async mockResetConfig() {
        await new Promise(resolve => setTimeout(resolve, 800));
        
        return {
            success: true,
            message: 'Configuration reset successfully'
        };
    }
}

// Initialize the configuration manager
document.addEventListener('DOMContentLoaded', () => {
    const configManager = new ConfigManager();
    
    // For development/testing, you can uncomment the following lines to use mock data
    // window.configManager = configManager;
    // window.mockConfigAPI = MockConfigAPI;
});

// Handle loading modal close on outside click
document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('loadingModal').addEventListener('click', (e) => {
        if (e.target.id === 'loadingModal') {
            document.getElementById('loadingModal').style.display = 'none';
        }
    });
});

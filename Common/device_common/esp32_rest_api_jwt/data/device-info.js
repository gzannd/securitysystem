// ESP32 Device Manager - Device Information Page
class DeviceInfoManager {
    constructor() {
        this.apiBase = 'http://192.168.4.1'; // Base API endpoint
        this.updateInterval = null;
        this.init();
    }

    init() {
        this.bindEvents();
        
        // Check if page was loaded with URL parameters
        const urlParams = new URLSearchParams(window.location.search);
        if (urlParams.toString().length === 0) {
            // No URL parameters, load device info normally
            this.loadDeviceInfo();
            this.startAutoRefresh();
        } else {
            // URL parameters present, don't auto-refresh initially
            // The URL parameter handler will populate the data
            console.log('Device info loaded from URL parameters, skipping initial API call');
        }
    }

    bindEvents() {
        // Reboot button
        document.getElementById('rebootBtn').addEventListener('click', () => {
            this.rebootDevice();
        });

        // Hard reset button
        document.getElementById('hardResetBtn').addEventListener('click', () => {
            this.showHardResetConfirmation();
        });

        // Refresh button
        document.getElementById('refreshBtn').addEventListener('click', () => {
            this.loadDeviceInfo();
        });

        // Hard reset modal events
        document.getElementById('confirmHardReset').addEventListener('click', () => {
            this.hardResetDevice();
        });

        document.getElementById('cancelHardReset').addEventListener('click', () => {
            this.hideHardResetModal();
        });

        // Close modal when clicking outside
        document.getElementById('hardResetModal').addEventListener('click', (e) => {
            if (e.target.id === 'hardResetModal') {
                this.hideHardResetModal();
            }
        });
    }

    async loadDeviceInfo() {
        try {
            this.showStatus('Loading device information...', 'info');
            
            const response = await fetch(`${this.apiBase}/device/info`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const data = await response.json();
            this.updateDeviceInfo(data);
            this.hideStatus();
            
        } catch (error) {
            console.error('Error loading device info:', error);
            this.showStatus('Failed to load device information. Please check your connection.', 'error');
            this.setLoadingState();
        }
    }

    updateDeviceInfo(data) {
        // Update device information fields
        document.getElementById('deviceName').textContent = data.deviceName || 'Unknown';
        document.getElementById('macAddress').textContent = data.macAddress || 'Unknown';
        document.getElementById('firmwareVersion').textContent = data.firmwareVersion || 'Unknown';
        document.getElementById('uptime').textContent = this.formatUptime(data.uptime);
        document.getElementById('wifiStatus').textContent = data.wifiStatus || 'Unknown';
        document.getElementById('ipAddress').textContent = data.ipAddress || 'Not connected';
    }

    setLoadingState() {
        const fields = ['deviceName', 'macAddress', 'firmwareVersion', 'uptime', 'wifiStatus', 'ipAddress'];
        fields.forEach(field => {
            document.getElementById(field).textContent = 'Loading...';
        });
    }

    formatUptime(seconds) {
        if (!seconds) return 'Unknown';
        
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;

        if (days > 0) {
            return `${days}d ${hours}h ${minutes}m ${secs}s`;
        } else if (hours > 0) {
            return `${hours}h ${minutes}m ${secs}s`;
        } else if (minutes > 0) {
            return `${minutes}m ${secs}s`;
        } else {
            return `${secs}s`;
        }
    }

    async rebootDevice() {
        if (!confirm('Are you sure you want to reboot the device? This will temporarily disconnect the device.')) {
            return;
        }

        try {
            this.showStatus('Rebooting device...', 'info');
            document.getElementById('rebootBtn').disabled = true;

            const response = await fetch(`${this.apiBase}/reset/soft`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                }
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            this.showStatus('Device is rebooting. Please wait a moment and refresh the page.', 'success');
            
            // Wait a bit then try to reconnect
            setTimeout(() => {
                this.loadDeviceInfo();
                document.getElementById('rebootBtn').disabled = false;
            }, 5000);

        } catch (error) {
            console.error('Error rebooting device:', error);
            this.showStatus('Failed to reboot device. Please try again.', 'error');
            document.getElementById('rebootBtn').disabled = false;
        }
    }

    showHardResetConfirmation() {
        document.getElementById('hardResetModal').style.display = 'block';
    }

    hideHardResetModal() {
        document.getElementById('hardResetModal').style.display = 'none';
    }

    async setDeviceTime() {
        var now = new Date();
        const year = now.getFullYear();
        const month = String(now.getMonth()).padStart(2, '0');
        const day = String(now.getDate()).padStart(2, '0');
        const hours = String(now.getHours()).padStart(2, '0');
        const minutes = String(now.getMinutes()).padStart(2, '0');
        const seconds = String(now.getSeconds().padStart(2, '0'));

        var formattedTime = `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
        
         const response = await fetch(`${this.apiBase}/clock`, {
                method: 'PUT',
                headers: {
                    'Content-Type': 'text/plain'
                },
                body: formattedTime
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

    }

    async hardResetDevice() {
        try {
            this.hideHardResetModal();
            this.showStatus('Performing hard reset...', 'info');
            
            // Disable all buttons during reset
            document.getElementById('rebootBtn').disabled = true;
            document.getElementById('hardResetBtn').disabled = true;
            document.getElementById('refreshBtn').disabled = true;

            const response = await fetch(`${this.apiBase}/reset/hard`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                }
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            this.showStatus('Hard reset completed. Device will restart in configuration mode.', 'success');
            
            // Redirect to configuration page after a delay
            setTimeout(() => {
                window.location.href = 'config.html';
            }, 3000);

        } catch (error) {
            console.error('Error performing hard reset:', error);
            this.showStatus('Failed to perform hard reset. Please try again.', 'error');
            
            // Re-enable buttons
            document.getElementById('rebootBtn').disabled = false;
            document.getElementById('hardResetBtn').disabled = false;
            document.getElementById('refreshBtn').disabled = false;
        }
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

    startAutoRefresh() {
        // Refresh device info every 30 seconds
        this.updateInterval = setInterval(() => {
            this.loadDeviceInfo();
        }, 30000);
    }

    stopAutoRefresh() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
        }
    }
}

// Mock API responses for development/testing
class MockAPI {
    static async mockDeviceInfo() {
        // Simulate API delay
        await new Promise(resolve => setTimeout(resolve, 500));
        
        return {
            deviceName: 'ESP32-Device-001',
            macAddress: '24:6F:28:8B:7A:3C',
            firmwareVersion: 'v1.2.3',
            uptime: 1234567, // seconds
            wifiStatus: 'Connected',
            ipAddress: '192.168.1.100'
        };
    }

    static async mockReboot() {
        await new Promise(resolve => setTimeout(resolve, 1000));
        return { success: true, message: 'Device rebooting' };
    }

    static async mockHardReset() {
        await new Promise(resolve => setTimeout(resolve, 2000));
        return { success: true, message: 'Hard reset completed' };
    }
}

// Initialize the device info manager
document.addEventListener('DOMContentLoaded', () => {
    const deviceManager = new DeviceInfoManager();
    
    // For development/testing, you can uncomment the following lines to use mock data
    // window.deviceManager = deviceManager;
    // window.mockAPI = MockAPI;
});

// Handle page visibility changes to pause/resume auto-refresh
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        // Page is hidden, stop auto-refresh
        if (window.deviceManager) {
            window.deviceManager.stopAutoRefresh();
        }
    } else {
        // Page is visible, resume auto-refresh
        if (window.deviceManager) {
            window.deviceManager.startAutoRefresh();
            window.deviceManager.loadDeviceInfo(); // Refresh immediately
        }
    }
});

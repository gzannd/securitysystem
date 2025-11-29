# ESP32 Device Manager

A modern, responsive web interface for managing ESP32 device configuration. This system provides an intuitive way to view device information, configure WiFi settings, and perform device operations like reboot and hard reset.

## Features

### Device Information Page (`index.html`)
- **Real-time device information display:**
  - Device name (user-defined)
  - MAC address
  - Firmware version
  - Uptime (formatted as days, hours, minutes, seconds)
  - WiFi connection status
  - IP address

- **Device control operations:**
  - Reboot device (with confirmation)
  - Hard reset (with safety confirmation modal)
  - Manual refresh of device information
  - Auto-refresh every 30 seconds

### Configuration Page (`config.html`)
- **Device configuration:**
  - Set custom device name
  - WiFi network scanning and selection
  - Password input for secured networks
  - Save configuration settings
  - Reset to factory defaults

- **WiFi management:**
  - Scan for available networks
  - Display signal strength (RSSI)
  - Show encryption status (🔒/🔓)
  - Automatic password field handling

## File Structure

```
esp32-config-manager/
├── index.html          # Main device information page
├── config.html         # Configuration management page
├── styles.css          # Modern, responsive CSS styling
├── device-info.js      # JavaScript for device info functionality
├── config.js           # JavaScript for configuration management
└── README.md           # This documentation file
```

## API Endpoints

The frontend expects the following REST API endpoints to be implemented on the ESP32:

### Device Information
- `GET /api/device/info` - Get device information
- `POST /api/device/reboot` - Reboot the device
- `POST /api/device/hard-reset` - Perform hard reset

### Configuration
- `GET /api/config/current` - Get current configuration
- `POST /api/config/save` - Save new configuration
- `POST /api/config/reset` - Reset to defaults

### WiFi Management
- `POST /api/wifi/scan` - Scan for available networks

## API Response Formats

### Device Info Response
```json
{
  "deviceName": "ESP32-Device-001",
  "macAddress": "24:6F:28:8B:7A:3C",
  "firmwareVersion": "v1.2.3",
  "uptime": 1234567,
  "wifiStatus": "Connected",
  "ipAddress": "192.168.1.100"
}
```

### WiFi Scan Response
```json
{
  "networks": [
    {
      "ssid": "MyWiFiNetwork",
      "rssi": -45,
      "encrypted": true
    },
    {
      "ssid": "OpenNetwork",
      "rssi": -72,
      "encrypted": false
    }
  ]
}
```

### Configuration Save Request
```json
{
  "deviceName": "MyESP32Device",
  "wifiSSID": "MyWiFiNetwork",
  "wifiPassword": "mypassword123"
}
```

## Design Features

### Modern UI/UX
- **Glassmorphism design** with backdrop blur effects
- **Responsive layout** that works on desktop, tablet, and mobile
- **Smooth animations** and hover effects
- **Intuitive navigation** between pages
- **Status feedback** with color-coded messages

### User Experience
- **Confirmation dialogs** for destructive actions
- **Loading indicators** for network operations
- **Auto-refresh** of device information
- **Form validation** with helpful error messages
- **Keyboard navigation** support

### Accessibility
- **Semantic HTML** structure
- **ARIA labels** and proper form associations
- **High contrast** color scheme
- **Responsive text** sizing
- **Focus indicators** for keyboard navigation

## Browser Compatibility

- Chrome 80+
- Firefox 75+
- Safari 13+
- Edge 80+

## Development and Testing

### Mock Data
The JavaScript files include mock API classes for development and testing:

```javascript
// In device-info.js
window.deviceManager = deviceManager;
window.mockAPI = MockAPI;

// In config.js
window.configManager = configManager;
window.mockConfigAPI = MockConfigAPI;
```

### Local Development
1. Serve the files using a local web server:
   ```bash
   # Using Python 3
   python3 -m http.server 8000
   
   # Using Node.js (if you have http-server installed)
   npx http-server
   
   # Using PHP
   php -S localhost:8000
   ```

2. Open `http://localhost:8000` in your browser

3. For testing without an ESP32, uncomment the mock API lines in the JavaScript files

## ESP32 Integration

### Required Libraries
- ESP32 WiFi library
- ESP32 WebServer library
- JSON parsing library (ArduinoJson recommended)

### Basic Implementation Steps
1. Set up ESP32 as an Access Point or connect to existing WiFi
2. Create a web server on the ESP32
3. Implement the required API endpoints
4. Serve the HTML/CSS/JS files from SPIFFS or as embedded resources
5. Handle CORS if needed for cross-origin requests

### Example ESP32 Endpoint Implementation
```cpp
// Example for device info endpoint
void handleDeviceInfo() {
  String json = "{";
  json += "\"deviceName\":\"" + getDeviceName() + "\",";
  json += "\"macAddress\":\"" + WiFi.macAddress() + "\",";
  json += "\"firmwareVersion\":\"" + FIRMWARE_VERSION + "\",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"wifiStatus\":\"" + WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected" + "\",";
  json += "\"ipAddress\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}
```

## Security Considerations

- **HTTPS** should be used in production
- **Input validation** on the ESP32 side
- **Password encryption** for stored WiFi credentials
- **CSRF protection** for form submissions
- **Rate limiting** for API endpoints

## Customization

### Styling
The `styles.css` file uses CSS custom properties for easy theming:
- Primary colors can be modified in the gradient definitions
- Button styles are consistent and easily customizable
- Responsive breakpoints are clearly defined

### Functionality
- JavaScript classes are modular and extensible
- API endpoints can be easily modified
- Mock data can be customized for testing

## License

This project is open source and available under the MIT License.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## Support

For issues and questions:
1. Check the documentation
2. Review the mock implementations
3. Test with different browsers
4. Verify ESP32 API compatibility

# ESP8266 Serial to WebSocket Bridge

A versatile and feature-rich serial-to-WebSocket bridge for the ESP8266. This project provides a web-based terminal to interact with any device connected to the ESP8266's hardware or software serial ports, making it an ideal tool for debugging, remote control, and monitoring of serial devices over a network.

The entire web interface is self-contained within the ESP8266's firmware, requiring no external files to be hosted.

## Features

- **Dual-Mode WiFi:** Automatically connects to a known WiFi network (Station mode). If the connection fails, it creates its own Access Point (AP mode), ensuring you can always access the device.
- **Web-Based Terminal:** A clean and responsive terminal interface accessible from any modern web browser.
- **Dual Serial Support:** Monitors and interacts with both the standard hardware `Serial` port and a `SoftwareSerial` port simultaneously.
- **On-the-Fly Baud Rate Configuration:** Change the baud rate for both serial ports directly from the web UI without rebooting the device.
- **Interactive Controls:**
  - **HEX View:** Toggle between ASCII and HEX views for incoming data, perfect for analyzing binary protocols.
  - **Autoscroll:** Enable or disable automatic scrolling of the terminal.
  - **Command History:** View and reuse previously sent commands.
  - **Log Export:** Download the terminal session log as a text file.
- **Firmware Dump Utility:** A feature to dump the device's firmware or memory contents (currently simulated, can be extended).
- **Real-time Status:** Monitor WebSocket connection status, bytes received, and message rates.

## Hardware Requirements

- An ESP8266-based board (e.g., NodeMCU, Wemos D1 Mini).
- A device to communicate with via serial (e.g., another microcontroller, a GPS module, a development board, etc.).

## Software & Dependencies

- [Arduino IDE](https://www.arduino.cc/en/software)
- [ESP8266 Core for Arduino](https://github.com/esp8266/Arduino)
- **Arduino Libraries:**
  - `ESP8266WebServer` (included with the ESP8266 Core)
  - `WebSocketsServer` by Markus Sattler (can be installed from the Arduino Library Manager)
  - `SoftwareSerial` (included with the ESP8266 Core)

## Installation & Setup

1.  **Configure Arduino IDE:** Make sure you have the ESP8266 board manager installed in your Arduino IDE.
2.  **Install Libraries:** Open the Library Manager (`Sketch > Include Library > Manage Libraries...`) and search for and install `WebSocketsServer`.
3.  **Configure WiFi Credentials:** Open the `esp8266_serial_bridge.ino` file and modify the following lines with your network details for Station (STA) mode:
    ```cpp
    const char* sta_ssid = "YOUR_WIFI_SSID";
    const char* sta_password = "YOUR_WIFI_PASSWORD";
    ```
4.  **Configure Serial Pins:** The software serial pins are defined at the top of the file. Change them according to your wiring:
    ```cpp
    #define RX_PIN 4 // GPIO4 (D2 on NodeMCU)
    #define TX_PIN 5 // GPIO5 (D1 on NodeMCU)
    ```
5.  **Upload:** Select your ESP8266 board from the `Tools` menu, choose the correct COM port, and click the "Upload" button.

## How to Use

1.  **Power On:** Power on your ESP8266. Open the Arduino IDE's Serial Monitor at `115200` baud to see debug messages.
2.  **Connect to the ESP8266:**
    - **Station (STA) Mode:** If the ESP8266 successfully connects to your WiFi network, the Serial Monitor will print its IP address.
    - **Access Point (AP) Mode:** If it fails to connect, it will create a WiFi network with the SSID `ESP8266-Serial` and password `12345678`. Connect your computer or phone to this network. The device IP will be `192.168.4.1`.
3.  **Open the Web Interface:** Open a web browser and navigate to the IP address of the ESP8266.
4.  **Start Communicating:**
    - You will now see the terminal interface. Any data received on either the hardware or software serial ports will appear in the terminal.
    - Type commands into the input box and click "Send" (or press Enter) to transmit them to both serial ports.

### Special Commands

The firmware recognizes special commands sent from the web interface, which are prefixed with `@`.

- **Change Baud Rate:**
  - To change the serial baud rate, use the dropdown menu in the UI or send a command in the format `@BAUD=RATE`.
  - Example: `@BAUD=115200`

- **Firmware Dump (Simulated):**
  - The "Dump Firmware" button initiates a simulated memory dump. This can be adapted to read from actual flash memory.
  - It uses the command `@DUMP=ADDRESS,SIZE`.

## Troubleshooting

- **Garbled Characters:** This is often caused by a baud rate mismatch. Ensure the baud rate selected in the web UI matches the device you are communicating with. The default is `9600`.
- **No Data in Web Terminal:**
  - Check your physical wiring to the serial pins. Remember that RX on the ESP8266 goes to TX on the other device, and vice-versa.
  - Verify that the other device is sending data.
  - Check the WebSocket connection status in the top-right corner of the web UI. If it's "Disconnected," try refreshing the page.
- **ESP8266 Fails to Connect to WiFi:** Double-check your `sta_ssid` and `sta_password`. If the credentials are correct, check your router's settings or signal strength. The device will automatically switch to AP mode if it cannot connect.

## License

This project is open-source and available under the [MIT License](LICENSE).

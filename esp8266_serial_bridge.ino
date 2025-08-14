#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

// STA (rede do roteador) - altere para sua rede
const char* sta_ssid = "Morpheus-Base";
const char* sta_password = "neves@725";

// AP (quando o ESP for ponto de acesso)
const char* ap_ssid = "ESP8266-Serial";
const char* ap_password = "12345678";

// Pinos UART
#define RX_PIN 4  // GPIO4 (D2)
#define TX_PIN 5  // GPIO5 (D1)

// Configurações da Serial por Software
SoftwareSerial mySerial(RX_PIN, TX_PIN, false);  // RX, TX, inverse_logic

// Baudrate inicial
long baud = 9600;

// Flags de controle
bool serialActive = false;
bool apMode = false;
int lastWiFiStatus = -1; // para detectar mudanças de estado

// Configurações persistentes
struct {
    long baudrate = 9600;
    String lineEnding = "\r\n";
    int bufferTimeout = 50;
} currentConfig;

// Variáveis de controle
String lineEnding = "\r\n"; // Padrão CR+LF

// Constantes para o buffer circular
const int BUFFER_SIZE = 4096;
const int READ_CHUNK_SIZE = 64;

// Buffers circulares
uint8_t hwSerialBuffer[BUFFER_SIZE];
uint8_t swSerialBuffer[BUFFER_SIZE];
int hwSerialHead = 0;
int hwSerialTail = 0;
int swSerialHead = 0;
int swSerialTail = 0;

// Timeouts
int serialReadTimeout = 1; // ms (tempo mínimo entre leituras)
int mySerialReadTimeout = 1; // ms

// Timestamps para controle
unsigned long lastSerialRead = 0;
unsigned long lastMySerialRead = 0;

// Servidor HTTP e WebSocket
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Página HTML (mantive seu conteúdo original)
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <title>ESP8266 Terminal</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="Content-Language" content="pt-BR">
    <style>
        /* seu CSS inteiro aqui (mantido igual) */
        body { 
            font-family: Arial; 
            margin: 0; 
            padding: 20px; 
            background: #2b2b2b; 
            color: #fff; 
        }
        .main-container {
            display: flex;
            gap: 20px;
            height: 400px;
        }
        .terminals-container {
            display: flex;
            gap: 20px;
            flex: 2;
        }
        .terminal-container {
            flex: 1;
            display: flex;
            flex-direction: column;
        }
        .terminal-header {
            background: #333;
            padding: 5px 10px;
            border-radius: 5px 5px 0 0;
            margin-bottom: 5px;
        }
        .history-container {
            flex: 1;
            background: #1a1a1a;
            border-radius: 5px;
            padding: 10px;
            display: flex;
            flex-direction: column;
        }
        .history-header {
            padding: 5px;
            background: #333;
            border-radius: 3px;
            margin-bottom: 10px;
            font-weight: bold;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .history-clear {
            background: #444;
            border: none;
            color: #fff;
            padding: 2px 8px;
            border-radius: 3px;
            cursor: pointer;
            font-size: 0.8em;
        }
        .history-clear:hover {
            background: #555;
        }
        #history-list {
            overflow-y: auto;
            flex: 1;
        }
        .history-item {
            padding: 5px;
            border-bottom: 1px solid #333;
            font-family: monospace;
            font-size: 0.9em;
            cursor: pointer;
        }
        .history-item:hover {
            background: #333;
        }
        .history-timestamp {
            color: #666;
            font-size: 0.8em;
            margin-right: 5px;
        }
        .terminal { 
            background: #000; 
            padding: 10px; 
            height: 400px; 
            overflow-y: auto; 
            font-family: monospace; 
            margin-bottom: 10px; 
            border-radius: 5px;
        }
        .input-area { 
            display: flex; 
            gap: 10px; 
        }
        input { 
            flex: 1; 
            padding: 10px; 
            border: none; 
            border-radius: 5px; 
            background: #333; 
            color: #fff; 
        }
        button { 
            padding: 10px 20px; 
            background: #0066cc; 
            border: none; 
            color: white; 
            border-radius: 5px; 
            cursor: pointer; 
        }
        button:hover { 
            background: #0052a3; 
        }
        .received { 
            color: #00ff00; 
        }
        .sent { 
            color: #ff9900; 
        }
        .system {
            color: #888;
            font-style: italic;
        }
        .header {
            margin-bottom: 20px;
            padding: 10px;
            background: #333;
            border-radius: 5px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .status {
            font-size: 0.9em;
            padding: 5px 10px;
            border-radius: 3px;
            background: #444;
        }
        .status.connected {
            background: #006600;
        }
        .status.disconnected {
            background: #660000;
        }
        .controls {
            display: flex;
            gap: 10px;
            align-items: center;
        }
        .baudrate-select {
            padding: 5px 10px;
            background: #444;
            color: #fff;
            border: none;
            border-radius: 3px;
            cursor: pointer;
            font-size: 0.9em;
            margin-right: 5px;
        }
        .baudrate-select:hover {
            background: #555;
        }
        .baudrate-select option {
            background: #333;
        }
        #customBaudrate {
            width: 100px;
            background: #444;
            color: #fff;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 4px 8px;
            font-size: 0.9em;
        }
        #customBaudrate:focus {
            outline: none;
            border-color: #0066cc;
        }
        .view-controls {
            display: flex;
            gap: 5px;
        }
        .control-button {
            padding: 5px 10px;
            background: #444;
            color: #fff;
            border: none;
            border-radius: 3px;
            cursor: pointer;
            font-size: 0.9em;
        }
        .control-button:hover {
            background: #555;
        }
        .control-button.active {
            background: #0066cc;
        }
        .stats-bar {
            display: flex;
            gap: 20px;
            padding: 10px;
            background: #1a1a1a;
            border-radius: 5px;
            margin-bottom: 10px;
            font-size: 0.9em;
        }
        .stat-item {
            display: flex;
            gap: 5px;
        }
        .stat-label {
            color: #888;
        }

        .hex-view {
            font-family: monospace;
        }
        .hex-view .hex {
            color: #888;
            margin-right: 10px;
        }
        .hex-view .ascii {
            color: #fff;
        }
    </style>
</head>
<body>
    <div class="header">
        <h2 style="margin: 0;">ESP8266 Serial Terminal</h2>
        <div class="controls">
            <select id="baudrate" onchange="changeBaudRate()" class="baudrate-select">
                <option value="1200">1200 baud</option>
                <option value="2400">2400 baud</option>
                <option value="4800">4800 baud</option>
                <option value="9600" selected>9600 baud</option>
                <option value="19200">19200 baud</option>
                <option value="38400">38400 baud</option>
                <option value="57600">57600 baud</option>
                <option value="115200">115200 baud</option>
                <option value="230400">230400 baud</option>
                <option value="460800">460800 baud</option>
                <option value="921600">921600 baud</option>
                <option value="custom">Personalizado...</option>
            </select>
            <input type="number" id="customBaudrate" style="display: none;" class="baudrate-select" min="1200" max="2000000" step="1200" placeholder="Baudrate...">
            <select id="lineEnding" onchange="changeLineEnding()" class="baudrate-select">
                <option value="CRLF" selected>CR+LF (\r\n)</option>
                <option value="CR">CR (\r)</option>
                <option value="LF">LF (\n)</option>
                <option value="NONE">Nenhum</option>
            </select>
            <input type="number" id="bufferTimeout" value="50" min="10" max="500" step="10" onchange="changeBufferTimeout()" class="baudrate-select" style="width: 80px;" title="Buffer Timeout (ms)">
            <div class="view-controls">
                <button onclick="toggleHexView()" class="control-button" id="hexButton">HEX</button>
                <button onclick="toggleAutoscroll()" class="control-button" id="scrollButton">Auto-scroll</button>
                <button onclick="exportData()" class="control-button">Exportar</button>
                <button onclick="startFirmwareDump()" class="control-button" id="dumpButton">Dump Firmware</button>
            </div>
            <div id="connection-status" class="status">Desconectado</div>
        </div>
    </div>
    <div class="stats-bar">
        <div class="stat-item">
            <span class="stat-label">Bytes Recebidos:</span>
            <span id="bytesReceived">0</span>
        </div>
        <div class="stat-item">
            <span class="stat-label">Última Mensagem:</span>
            <span id="lastMessageTime">--:--:--</span>
        </div>
        <div class="stat-item">
            <span class="stat-label">Mensagens/s:</span>
            <span id="messagesPerSecond">0</span>
        </div>
    </div>
    <div class="main-container">
        <div class="terminals-container">
            <div class="terminal-container">
                <div class="terminal-header">Terminal de Envio</div>
                <div id="terminal" class="terminal"></div>
                <div class="input-area">
                    <input type="text" id="commandInput" placeholder="Digite um comando...">
                    <button onclick="sendCommand()">Enviar</button>
                </div>
            </div>
            <div class="terminal-container">
                <div class="terminal-header">Terminal de Retorno</div>
                <div id="terminalEcho" class="terminal"></div>
            </div>
        </div>
        <div class="history-container">
            <div class="history-header">
                <span>Histórico de Dados</span>
                <button class="history-clear" onclick="clearHistory()">Limpar</button>
            </div>
            <div id="history-list"></div>
        </div>
    </div>
    <script>
        var gateway = `ws://${window.location.hostname}:81/`;
        var websocket;
        var reconnectInterval = null;
        var historyData = [];
        
        const CONFIG = {
            maxHistorySize: 100,
            maxMessageSize: 1024 * 1024,
            dumpChunkSize: 256,
            maxTerminalMessages: 1000,
            reconnectDelay: 2000,
            statsUpdateInterval: 1000,
        };

        const state = {
            isHexView: false,
            autoScroll: true,
            bytesReceived: 0,
            messagesLastSecond: 0,
            lastMessageTime: new Date(),
            isDumping: false,
            dumpStartAddress: 0,
            totalBytesReceived: 0
        };
        
        function addToSendTerminal(message, type) {
            const line = createTerminalLine(message, type);
            const fragment = document.createDocumentFragment();
            fragment.appendChild(line);
            domElements.terminal.appendChild(fragment);
            
            if (state.autoScroll) {
                domElements.terminal.scrollTop = domElements.terminal.scrollHeight;
            }
            
            while (domElements.terminal.children.length > CONFIG.maxTerminalMessages) {
                domElements.terminal.removeChild(domElements.terminal.firstChild);
            }
        }

        function changeBaudRate() {
            const baudrateSelect = document.getElementById('baudrate');
            const customBaudrateInput = document.getElementById('customBaudrate');
            
            if (baudrateSelect.value === 'custom') {
                customBaudrateInput.style.display = 'inline-block';
                customBaudrateInput.focus();
                return;
            } else {
                customBaudrateInput.style.display = 'none';
            }
            
            const baudrate = baudrateSelect.value;
            // Armazena a configuração localmente
            localStorage.setItem('lastBaudrate', baudrate);
            
            // Tenta enviar se houver conexão
            if (websocket && websocket.readyState === WebSocket.OPEN) {
                websocket.send(`@BAUD=${baudrate}`);
            }
            
            addToSendTerminal(`Alterando baudrate para ${baudrate}...`, 'system');
        }

        // Função para lidar com baudrate personalizado
        document.addEventListener('DOMContentLoaded', function() {
            const customBaudrateInput = document.getElementById('customBaudrate');
            customBaudrateInput.addEventListener('change', function() {
                const baudrate = this.value;
                if (baudrate >= 1200 && baudrate <= 2000000) {
                    // Armazena a configuração localmente
                    localStorage.setItem('lastBaudrate', baudrate);
                    
                    // Tenta enviar se houver conexão
                    if (websocket && websocket.readyState === WebSocket.OPEN) {
                        websocket.send(`@BAUD=${baudrate}`);
                    }
                    addToSendTerminal(`Alterando baudrate para ${baudrate}...`, 'system');
                } else {
                    addToSendTerminal('Baudrate inválido. Use valores entre 1200 e 2000000', 'system');
                }
            });
        });

        function changeLineEnding() {
            const ending = document.getElementById('lineEnding').value;
            websocket.send(`@ENDING=${ending}`);
            addToTerminal(`Final de linha alterado para ${ending}`, 'system');
        }

        function changeBufferTimeout() {
            const timeout = document.getElementById('bufferTimeout').value;
            websocket.send(`@TIMEOUT=${timeout}`);
            addToSendTerminal(`Timeout do buffer alterado para ${timeout}ms`, 'system');
        }

        window.addEventListener('load', onLoad);

        function onLoad(event) {
            initDOMCache();
            initWebSocket();
            startStatistics();
            
            // Carrega o último baud rate salvo
            const lastBaudrate = localStorage.getItem('lastBaudrate');
            if (lastBaudrate) {
                const baudrateSelect = document.getElementById('baudrate');
                if (Array.from(baudrateSelect.options).some(opt => opt.value === lastBaudrate)) {
                    baudrateSelect.value = lastBaudrate;
                } else {
                    baudrateSelect.value = 'custom';
                    document.getElementById('customBaudrate').value = lastBaudrate;
                    document.getElementById('customBaudrate').style.display = 'inline-block';
                }
            }
            
            document.getElementById('commandInput').addEventListener('keypress', function(e) {
                if (e.key === 'Enter') {
                    sendCommand();
                }
            });
        }

        function updateConnectionStatus(connected) {
            const status = document.getElementById('connection-status');
            if (connected) {
                status.textContent = 'Conectado';
                status.className = 'status connected';
            } else {
                status.textContent = 'Desconectado';
                status.className = 'status disconnected';
            }
        }

        function initWebSocket() {
            console.log('Tentando abrir conexão WebSocket...');
            websocket = new WebSocket(gateway);
            websocket.onopen    = onOpen;
            websocket.onclose   = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            console.log('Conexão estabelecida');
            updateConnectionStatus(true);
            if (reconnectInterval) {
                clearInterval(reconnectInterval);
                reconnectInterval = null;
            }
        }

        function onClose(event) {
            console.log('Conexão fechada');
            updateConnectionStatus(false);
            if (!reconnectInterval) {
                reconnectInterval = setInterval(initWebSocket, 2000);
            }
        }

        function handleConfig(message) {
            const [cmd, value] = message.split('=');
            switch(cmd) {
                case '@CONFIG:BAUD':
                    document.getElementById('baudrate').value = value;
                    if (!document.getElementById('baudrate').value) {
                        document.getElementById('baudrate').value = 'custom';
                        document.getElementById('customBaudrate').value = value;
                        document.getElementById('customBaudrate').style.display = 'inline-block';
                    }
                    break;
                case '@CONFIG:ENDING':
                    document.getElementById('lineEnding').value = value;
                    break;
                case '@CONFIG:TIMEOUT':
                    document.getElementById('bufferTimeout').value = value;
                    break;
            }
        }

        function onMessage(event) {
            // Verifica se é uma mensagem de configuração
            if (event.data.startsWith('@CONFIG:')) {
                handleConfig(event.data);
                return;
            }
            
            // Atualiza estatísticas
            updateStats(event.data);
            
            // Adiciona ao terminal de eco (retorno)
            const line = createTerminalLine(event.data, 'received');
            const fragment = document.createDocumentFragment();
            fragment.appendChild(line);
            domElements.terminalEcho.appendChild(fragment);
            
            // Auto-scroll para o terminal de eco
            if (state.autoScroll) {
                domElements.terminalEcho.scrollTop = domElements.terminalEcho.scrollHeight;
            }
            
            // Limita o número de mensagens no terminal de eco
            while (domElements.terminalEcho.children.length > CONFIG.maxTerminalMessages) {
                domElements.terminalEcho.removeChild(domElements.terminalEcho.firstChild);
            }
            
            addToHistory(event.data);
        }

        function addToHistory(message) {
            const timestamp = new Date().toLocaleTimeString();
            historyData.unshift({ timestamp, message });
            
            if (historyData.length > CONFIG.maxHistorySize) {
                historyData.pop();
            }
            
            updateHistoryDisplay();
        }

        function updateHistoryDisplay() {
            const fragment = document.createDocumentFragment();
            
            historyData.forEach(item => {
                const div = document.createElement('div');
                div.className = 'history-item';
                div.innerHTML = `<span class="history-timestamp">[${item.timestamp}]</span> ${item.message}`;
                div.onclick = () => {
                    domElements.commandInput.value = item.message;
                };
                fragment.appendChild(div);
            });
            
            domElements.historyList.innerHTML = '';
            domElements.historyList.appendChild(fragment);
        }

        function clearHistory() {
            historyData = [];
            updateHistoryDisplay();
        }

        function sendCommand() {
            var cmd = document.getElementById('commandInput').value;
            if (cmd) {
                // Permite enviar comandos independente do estado da conexão
                try {
                    websocket.send(cmd);
                    const line = createTerminalLine('> ' + cmd, 'sent');
                    const fragment = document.createDocumentFragment();
                    fragment.appendChild(line);
                    domElements.terminal.appendChild(fragment);
                    
                    if (state.autoScroll) {
                        domElements.terminal.scrollTop = domElements.terminal.scrollHeight;
                    }
                    
                    while (domElements.terminal.children.length > CONFIG.maxTerminalMessages) {
                        domElements.terminal.removeChild(domElements.terminal.firstChild);
                    }
                } catch (e) {
                    const line = createTerminalLine('Erro ao enviar comando: ' + e, 'system');
                    const fragment = document.createDocumentFragment();
                    fragment.appendChild(line);
                    domElements.terminal.appendChild(fragment);
                }
                document.getElementById('commandInput').value = '';
            }
        }

        const domElements = {
            terminal: null,
            terminalEcho: null,
            bytesReceived: null,
            lastMessageTime: null,
            messagesPerSecond: null,
            commandInput: null,
            historyList: null
        };

        function initDOMCache() {
            domElements.terminal = document.getElementById('terminal');
            domElements.terminalEcho = document.getElementById('terminalEcho');
            domElements.bytesReceived = document.getElementById('bytesReceived');
            domElements.lastMessageTime = document.getElementById('lastMessageTime');
            domElements.messagesPerSecond = document.getElementById('messagesPerSecond');
            domElements.commandInput = document.getElementById('commandInput');
            domElements.historyList = document.getElementById('history-list');
        }

        function startStatistics() {
            setInterval(() => {
                domElements.messagesPerSecond.textContent = state.messagesLastSecond;
                state.messagesLastSecond = 0;
            }, CONFIG.statsUpdateInterval);
        }

        function toggleHexView() {
            state.isHexView = !state.isHexView;
            document.getElementById('hexButton').classList.toggle('active');
            
            // Atualiza terminal de envio
            const fragmentSend = document.createDocumentFragment();
            Array.from(domElements.terminal.children).forEach(msg => {
                const newLine = createTerminalLine(msg.textContent, msg.className);
                fragmentSend.appendChild(newLine);
            });
            domElements.terminal.innerHTML = '';
            domElements.terminal.appendChild(fragmentSend);
            
            // Atualiza terminal de retorno
            const fragmentEcho = document.createDocumentFragment();
            Array.from(domElements.terminalEcho.children).forEach(msg => {
                const newLine = createTerminalLine(msg.textContent, msg.className);
                fragmentEcho.appendChild(newLine);
            });
            domElements.terminalEcho.innerHTML = '';
            domElements.terminalEcho.appendChild(fragmentEcho);
        }

        function toggleAutoscroll() {
            state.autoScroll = !state.autoScroll;
            document.getElementById('scrollButton').classList.toggle('active');
        }

        function exportData() {
            const data = historyData.map(item => 
                `[${item.timestamp}] ${item.message}`
            ).join('\n');
            
            const blob = new Blob([data], { type: 'text/plain' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `terminal-log-${new Date().toISOString().slice(0,10)}.txt`;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
        }

        let firmwareChunks = [];
        let isDumping = false;
        let dumpStartAddress = 0;
        let totalBytesReceived = 0;

        function startFirmwareDump() {
            if (isDumping) {
                addToSendTerminal('Dump já em andamento...', 'system');
                return;
            }

            const dumpButton = document.getElementById('dumpButton');
            dumpButton.classList.add('active');
            isDumping = true;
            firmwareChunks = [];
            dumpStartAddress = 0;
            totalBytesReceived = 0;

            addToSendTerminal('Iniciando dump do firmware...', 'system');
            requestNextChunk();
        }

        function requestNextChunk() {
            if (!isDumping) return;
            
            websocket.send(`@DUMP=${dumpStartAddress.toString(16)},${CONFIG.dumpChunkSize}`);
            addToTerminal(`Solicitando chunk de memória: 0x${dumpStartAddress.toString(16)}`, 'system');
        }

        function processDumpResponse(data) {
            const parts = data.split(':');
            if (parts.length !== 4 || parts[0] !== '@DUMP') {
                return false;
            }

            const addr = parseInt(parts[1], 16);
            const size = parseInt(parts[2]);
            const hexData = parts[3];

            const bytes = new Uint8Array(hexData.match(/.{1,2}/g).map(byte => parseInt(byte, 16)));
            firmwareChunks.push(bytes);
            totalBytesReceived += bytes.length;

            addToTerminal(`Recebido chunk de 0x${addr.toString(16)} (${bytes.length} bytes)`, 'system');

            if (bytes.length === 0 || totalBytesReceived >= 1024*1024) {
                finishDump();
            } else {
                dumpStartAddress += bytes.length;
                setTimeout(requestNextChunk, 100);
            }

            return true;
        }

        function finishDump() {
            isDumping = false;
            document.getElementById('dumpButton').classList.remove('active');

            const totalLength = firmwareChunks.reduce((sum, chunk) => sum + chunk.length, 0);
            const fullArray = new Uint8Array(totalLength);
            let offset = 0;
            for (const chunk of firmwareChunks) {
                fullArray.set(chunk, offset);
                offset += chunk.length;
            }

            const blob = new Blob([fullArray], { type: 'application/octet-stream' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `firmware-dump-${new Date().toISOString().slice(0,10)}.bin`;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);

            addToTerminal(`Dump completo! Total de ${totalBytesReceived} bytes salvos.`, 'system');
        }

        function toHex(str) {
            let hex = '';
            let ascii = '';
            for (let i = 0; i < str.length; i++) {
                const charCode = str.charCodeAt(i);
                hex += charCode.toString(16).padStart(2, '0') + ' ';
                ascii += (charCode >= 32 && charCode <= 126) ? str[i] : '.';
            }
            return `<span class="hex">${hex}</span><span class="ascii">${ascii}</span>`;
        }

        function createTerminalLine(message, type) {
            const line = document.createElement('div');
            line.className = type;
            
            if (type === 'received') {
                if (state.isHexView) {
                    line.className += ' hex-view';
                    line.innerHTML = toHex(message);
                } else {
                    line.textContent = message;
                }
            } else {
                line.textContent = message;
            }
            
            return line;
        }

        function updateStats(message) {
            state.bytesReceived += message.length;
            state.totalBytesReceived += message.length;
            state.lastMessageTime = new Date();
            state.messagesLastSecond++;
            domElements.bytesReceived.textContent = state.bytesReceived;
            domElements.lastMessageTime.textContent = state.lastMessageTime.toLocaleTimeString();
        }
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send_P(200, "text/html", MAIN_page);
}

void connectToWiFiOrAP(unsigned long timeout_ms = 10000) {
  // Tenta conectar como station
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_password);

  Serial.print("Tentando conectar ao WiFi ");
  Serial.print(sta_ssid);
  Serial.print(" ... ");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    Serial.println();
    Serial.println("Conectado ao WiFi!");
    reportNetworkInfo();
  } else {
    // Falhou -> ativa Access Point
    Serial.println();
    Serial.println("Falha ao conectar como STA. Iniciando Access Point (AP)...");
    WiFi.mode(WIFI_AP);

    // Define IP do AP (opcional — pode mudar se quiser)
    IPAddress apIP(192,168,4,1);
    IPAddress apGateway = apIP;
    IPAddress apNetmask(255,255,255,0);
    WiFi.softAPConfig(apIP, apGateway, apNetmask);

    WiFi.softAP(ap_ssid, ap_password);
    apMode = true;

    Serial.print("AP ativo. SSID: ");
    Serial.println(ap_ssid);
    reportNetworkInfo();
  }
}

void setup() {
  // Configuração dos pinos
  pinMode(TX_PIN, OUTPUT);
  pinMode(RX_PIN, INPUT);
  
  // Pull-down via software no TX
  digitalWrite(TX_PIN, LOW);
  digitalWrite(RX_PIN, LOW);
  delay(100);
  
  // Inicializa Serial com buffer maior e parâmetros específicos
  Serial.begin(115200, SERIAL_8N1);
  Serial.setRxBufferSize(4096);
  Serial.setTimeout(1);
  Serial.flush();
  delay(100);
  Serial.println("Debug serial OK");
  
  // Configura e inicia a Serial por Software
  mySerial.begin(baud);
  mySerial.flush();
  mySerial.setTimeout(1);
  
  // Testa a comunicação
  delay(100);
  serialActive = true;
  mySerial.println("Serial extra OK");
  Serial.println("Serial extra iniciada");

  // tenta conectar; se falhar, habilita AP automaticamente
  connectToWiFiOrAP(10000); // timeout em ms

  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

    mySerial.println("Servidor web iniciado");
  if (!apMode) {
    mySerial.print("Acesse: http://");
    mySerial.println(WiFi.localIP());
  } else {
    mySerial.print("Acesse: http://");
    mySerial.println(WiFi.softAPIP());
  }
}

void reportNetworkInfo() {
  if (apMode) {
    IPAddress apIP = WiFi.softAPIP();
    Serial.println("== Network info (AP mode) ==");
    Serial.print("AP IP: ");
    Serial.println(apIP);
    Serial.print("AP Gateway: ");
    Serial.println(WiFi.gatewayIP()); // normalmente o próprio AP
    Serial.print("AP Netmask: ");
    Serial.println(WiFi.subnetMask());
    Serial.println("=============================");
    return;
  }

  // STA mode
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("== Network info (STA mode) ==");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Netmask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.println("=============================");
  } else {
    Serial.println("Sem conexão WiFi no momento.");
  }
}

void loop() {
  server.handleClient();
  webSocket.loop();

  int st = WiFi.status();
  if (st != lastWiFiStatus) {
    lastWiFiStatus = st;
    reportNetworkInfo();
  }

  // Handle Hardware Serial
  if (millis() - lastSerialRead >= serialReadTimeout) {
    while (Serial.available()) {
      // Lê em chunks pequenos para evitar bloqueio
      int bytesToRead = min(Serial.available(), READ_CHUNK_SIZE);
      for (int i = 0; i < bytesToRead; i++) {
        hwSerialBuffer[hwSerialHead] = Serial.read();
        hwSerialHead = (hwSerialHead + 1) % BUFFER_SIZE;
        
        // Se o buffer está cheio, envia o que tiver
        if (hwSerialHead == hwSerialTail) {
          String data = "";
          while (hwSerialTail != hwSerialHead) {
            data += (char)hwSerialBuffer[hwSerialTail];
            hwSerialTail = (hwSerialTail + 1) % BUFFER_SIZE;
          }
          if (data.length() > 0) {
            sendFormattedData(data, "HW->USB");
          }
        }
      }
      yield();
    }
    
    // Se há dados no buffer, envia
    if (hwSerialHead != hwSerialTail) {
      String data = "";
      while (hwSerialTail != hwSerialHead) {
        data += (char)hwSerialBuffer[hwSerialTail];
        hwSerialTail = (hwSerialTail + 1) % BUFFER_SIZE;
      }
      if (data.length() > 0) {
        sendFormattedData(data, "HW->USB");
      }
    }
    lastSerialRead = millis();
  }

  // Handle Software Serial
  if (millis() - lastMySerialRead >= mySerialReadTimeout) {
    while (mySerial.available()) {
      // Lê em chunks pequenos para evitar bloqueio
      int bytesToRead = min(mySerial.available(), READ_CHUNK_SIZE);
      for (int i = 0; i < bytesToRead; i++) {
        swSerialBuffer[swSerialHead] = mySerial.read();
        swSerialHead = (swSerialHead + 1) % BUFFER_SIZE;
        
        // Se o buffer está cheio, envia o que tiver
        if (swSerialHead == swSerialTail) {
          String data = "";
          while (swSerialTail != swSerialHead) {
            data += (char)swSerialBuffer[swSerialTail];
            swSerialTail = (swSerialTail + 1) % BUFFER_SIZE;
          }
          if (data.length() > 0) {
            sendFormattedData(data, "TX/RX");
          }
        }
      }
      yield();
    }
    
    // Se há dados no buffer, envia
    if (swSerialHead != swSerialTail) {
      String data = "";
      while (swSerialTail != swSerialHead) {
        data += (char)swSerialBuffer[swSerialTail];
        swSerialTail = (swSerialTail + 1) % BUFFER_SIZE;
      }
      if (data.length() > 0) {
        sendFormattedData(data, "TX/RX");
      }
    }
    lastMySerialRead = millis();
  }
}

// Função para enviar dados formatados para o WebSocket
void sendFormattedData(const String& data, const String& source) {
    String formattedData = "[" + source + "] " + data;
    webSocket.broadcastTXT(formattedData);
}

// Função para enviar configurações atuais para um cliente
void sendCurrentConfig(uint8_t clientNum) {
    // Envia baudrate atual
    String baudConfig = "@CONFIG:BAUD=" + String(currentConfig.baudrate);
    webSocket.sendTXT(clientNum, baudConfig);
    
    // Envia configuração de line ending
    String endingConfig = "@CONFIG:ENDING=";
    if (currentConfig.lineEnding == "\r\n") endingConfig += "CRLF";
    else if (currentConfig.lineEnding == "\r") endingConfig += "CR";
    else if (currentConfig.lineEnding == "\n") endingConfig += "LF";
    else endingConfig += "NONE";
    webSocket.sendTXT(clientNum, endingConfig);
    
    // Envia timeout do buffer
    String timeoutConfig = "@CONFIG:TIMEOUT=" + String(currentConfig.bufferTimeout);
    webSocket.sendTXT(clientNum, timeoutConfig);
}

// WebSocket <-> UART e comandos especiais
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    // Cliente conectado - envia configurações atuais
    sendCurrentConfig(num);
  }
  else if (type == WStype_TEXT) {
    String msg = String((char*)payload);

    // Muda baudrate
    if (msg.startsWith("@BAUD=")) {
      long newBaud = msg.substring(6).toInt();
      if (newBaud > 0) {
        currentConfig.baudrate = newBaud;
        baud = newBaud;
        // Configura ambas as seriais
        Serial.flush();
        mySerial.flush();
        
        // Reconfigura a Serial hardware
        Serial.begin(baud);
        Serial.setRxBufferSize(4096);
        
        // Reconfigura a Serial por Software
        mySerial.end();  // Desativa antes de reconfigurar
        digitalWrite(TX_PIN, LOW);  // Garante nível baixo
        delay(10);
        mySerial.begin(baud);
        serialActive = true;
        
        String response = "Baudrate alterado para " + String(baud);
        webSocket.broadcastTXT(response);
      }
    }
    // Muda final de linha
    else if (msg.startsWith("@ENDING=")) {
      String newEnding = msg.substring(8);
      if (newEnding == "CRLF") currentConfig.lineEnding = "\r\n";
      else if (newEnding == "CR") currentConfig.lineEnding = "\r";
      else if (newEnding == "LF") currentConfig.lineEnding = "\n";
      else if (newEnding == "NONE") currentConfig.lineEnding = "";
      lineEnding = currentConfig.lineEnding;
      String response = "Final de linha alterado para: " + newEnding;
      webSocket.sendTXT(num, response);
    }
    // Muda timeout do buffer
    else if (msg.startsWith("@TIMEOUT=")) {
      int newTimeout = msg.substring(9).toInt();
      if (newTimeout >= 10 && newTimeout <= 500) {
        currentConfig.bufferTimeout = newTimeout;
        serialReadTimeout = newTimeout;
        mySerialReadTimeout = newTimeout;
        String response = "Timeout do buffer alterado para: " + String(newTimeout) + "ms";
        webSocket.sendTXT(num, response);
      }
    }
    // Simula dump de firmware
    else if (msg.startsWith("@DUMP=")) {
      int sep = msg.indexOf(',');
      if (sep > 0) {
        String addrStr = msg.substring(6, sep);
        String sizeStr = msg.substring(sep + 1);
        long addr = strtol(addrStr.c_str(), NULL, 16);
        int size = sizeStr.toInt();
        String hexdata = "";
        for (int i = 0; i < size; i++) hexdata += "00";
        String resp = "@DUMP:" + String(addr, HEX) + ":" + String(size) + ":" + hexdata;
        webSocket.sendTXT(num, resp);
      } else {
        webSocket.sendTXT(num, "@DUMP:ERRO");
      }
    }
    // Comando normal: envia para ambas UARTs
    else {
      sendFormattedData(msg, "WEB->USB");  // Indica que o comando veio da web para a USB
      Serial.print(msg + lineEnding);
      
      sendFormattedData(msg, "WEB->TX/RX");  // Indica que o comando veio da web para TX/RX
      mySerial.print(msg + lineEnding);
    }
  }
}

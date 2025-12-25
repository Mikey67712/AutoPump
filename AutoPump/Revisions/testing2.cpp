#include <WiFi.h>
#include <WebServer.h>
#include "esp_system.h"
#include <Adafruit_ADS1X15.h>
#include <string>
#include <sstream>
#include <iomanip>

#define ADS_SDA 19
#define ADS_SCL 18

Adafruit_ADS1115 ads;

// Wifi LAN hosted by ESP32
const char *ssid = "AutoPump";
const char *password = "12345678";
const IPAddress local_IP(10, 0, 0, 1);
const IPAddress gateway(10, 0, 0, 1);
const IPAddress subnet(255, 255, 255, 0);
//void setupWiFi()
//{
//    WiFi.disconnect(true);
//    WiFi.mode(WIFI_AP);
//    delay(500);
//    WiFi.softAPConfig(local_IP, gateway, subnet);
//    WiFi.softAP(ssid, password);
//    Serial.println("Access Point Started");
//    Serial.print("IP Address: ");
//    Serial.println(WiFi.softAPIP());
//}

void setupWiFi() {
    WiFi.disconnect(true);       // Clear any previous Wi-Fi state
    WiFi.mode(WIFI_AP);          // Force Access Point mode

    const int maxRetries = 5;    // Number of attempts
    int attempt = 0;
    bool apStarted = false;

    while (!apStarted && attempt < maxRetries) {
        delay(500); // Allow hardware to stabilize

        // Configure static IP
        if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
            Serial.println("Failed to configure AP IP, retrying...");
            attempt++;
            continue;
        }

        // Start AP
        if (WiFi.softAP(ssid, password)) {
            apStarted = true;
            Serial.println("Access Point Started Successfully!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.softAPIP());
        } else {
            Serial.println("Failed to start AP, retrying...");
            attempt++;
        }
    }

    if (!apStarted) {
        Serial.println("ERROR: Could not start AP after multiple attempts!");
        // Optional: reset ESP32 if AP fails to start
        // ESP.restart();
    }
}



WebServer server(80);

// Variables
// State variables
bool S0 = true;
bool S0a = false;
bool S1a = false;
bool S1aa = false;
bool S1ab = false;
bool S1ac = false;
bool S1ad = false;
bool S1ae = false;
bool S1af = false;
bool S1ag = false;
bool S1ah = false;
bool S1b = false;
bool S1ba = false;
bool S1bb = false;
bool S1bc = false;
bool S1bd = false;
bool S1be = false;
bool S1bf = false;
bool S1bg = false;
bool S1bh = false;
bool S1c = false;
bool S1ca = false;
bool S1cb = false;
bool S2 = false;
bool S2a = false;
bool S2b = false;

// Transition variables
bool ToS0 = false;
bool ToS0a = false;
bool ToS1a = false;
bool ToS1aa = false;
bool ToS1ab = false;
bool ToS1ac = false;
bool ToS1ad = false;
bool ToS1ae = false;
bool ToS1af = false;
bool ToS1ag = false;
bool ToS1ah = false;
bool ToS1b = false;
bool ToS1ba = false;
bool ToS1bb = false;
bool ToS1bc = false;
bool ToS1bd = false;
bool ToS1be = false;
bool ToS1bf = false;
bool ToS1bg = false;
bool ToS1bh = false;
bool ToS1c = false;
bool ToS1ca = false;
bool ToS1cb = false;
bool ToS2 = false;
bool ToS2a = false;
bool ToS2b = false;

// Output affecting variables
bool PumpON = false;
bool SolenoidON = false;
bool GoButtonActive = false;
bool AUTO = false;
bool MANUAL = false;
bool ManualInflate = false;
bool ManualDeflate = false;

// Constants
const unsigned long SettleTm = 5000;       // milliseconds
const unsigned long CalibrationTm = 10000; // milliseconds
const double PumpFlow = 160 / 60;          // Litres per second
const double AutoPumpOnMaxTm = 120;            // Seconds
const double AutoPumpOnMinTm = 10;             // Seconds
const double AutoSolOnMaxTime = 120;        // Seconds
const double AutoSolOnMinTime = 1;          // Seconds
const double ManualPumpOnMinTm = 0.5;             // Seconds
const double ManualSolOnMinTm = 0.5;          // Seconds
const int InflatePin = 2;
const int DeflatePin = 33;
const unsigned long GraphPlotDelay = 250; // milliseconds
const double PresBuffer = 0.3;            // psi

// State start times
unsigned long S1aSttTm = 0;
unsigned long S1abSttTm = 0;
unsigned long S1acSttTm = 0;
unsigned long S1afSttTm = 0;
unsigned long S1agSttTm = 0;
unsigned long S1bSttTm = 0;
unsigned long S1bbSttTm = 0;
unsigned long S1bcSttTm = 0;
unsigned long S1bfSttTm = 0;
unsigned long S1bgSttTm = 0;
unsigned long S1cSttTm = 0;
unsigned long S1caSttTm = 0;
unsigned long S2aSttTm = 0;
unsigned long S2bSttTm = 0;

// String variables
std::string Unit;
std::string StateMessage;

// Misc variables
double P0 = 0.00;
double Psp_psi = 0.00;
double Pc1 = 0.00;
double Pc2 = 0.00;
double m = 0.00; // Inflation gradient
double InflTm = 0.00;
double DeflTm = 0.00;
double DeltaTm = 0.00;
double pressure_psi = 0.0;
bool adsPresent = false;

// Incoming button request word from HMI
uint16_t HMIButtonRequestWord = 0;
// Outgoing ESP32 response word to HMI
uint16_t ESPButtonResponseWord = 0;
// Incoming unit request word from HMI
uint16_t HMIUnitRequestWord = 0;

// Function declarations
void handleRoot();
void Escape();
void handleHMIButtonRequest();
void handleESPButtonResponse();
void handlePresSetpoint();

void setupWebServer()
{
    server.on("/", handleRoot);

    // JSON endpoint for HMI
    server.on("/hmi", HTTP_GET, []() {
        // Process any incoming setpoint request BEFORE generating JSON response
        if (server.hasArg("PspHMI")) {
            handlePresSetpoint();
        }

        // Process button requests
        if (server.hasArg("HMIButtonRequestWord")) {
            handleHMIButtonRequest();
        }

        handleESPButtonResponse(); // Update response word

        // Now build the JSON response
        double Psp_display = 0.0;
        double Pressure_display = 0.0;

        if (Unit == "kPa") {
            Psp_display = Psp_psi * 6.89476;
            Pressure_display = pressure_psi * 6.89476;
        } else if (Unit == "bar") {
            Psp_display = Psp_psi * 0.0689476;
            Pressure_display = pressure_psi * 0.0689476;
        } else if (Unit == "kg/cm2") {
            Psp_display = Psp_psi * 0.0703069;
            Pressure_display = pressure_psi * 0.0703069;
        } else if (Unit == "atm") {
            Psp_display = Psp_psi * 0.0680459;
            Pressure_display = pressure_psi * 0.0680459;
        } else {
            Psp_display = Psp_psi;
            Pressure_display = pressure_psi;
        }

        String json = "{";
        json += "\"Psp\":" + String(Psp_display, 2);
        json += ",\"Pressure\":" + String(Pressure_display, 2);
        json += ",\"unit\":\"" + String(Unit.c_str()) + "\"";
        json += ",\"StateMessage\":\"" + String(StateMessage.c_str()) + "\"";
        json += ",\"ESPButtonResponseWord\":" + String(ESPButtonResponseWord);
        json += "}";

        server.send(200, "application/json", json);
    });



    server.on("/setUnit", HTTP_GET, []() {
        if (server.hasArg("unit")) {
            String newUnit = server.arg("unit");
            if (newUnit == "psi" || newUnit == "kPa" || newUnit == "bar" || newUnit == "kg/cm2" || newUnit == "atm") {
                Unit = newUnit.c_str();
            } else {
                server.send(400, "text/plain", "Invalid unit");
            }
        } else {
            server.send(400, "text/plain", "Missing unit parameter");
        }
    });
    server.begin();
}

// Add this after canvas id: <script src="/static/chart.umd.min.js"></script>
void handleRoot() {
    String page = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0 minimum-scale=1.0">
        <title>AutoPump HMI</title>
        <style>
            :root {
                --white: #ffffff;
                --grey-1: #f0f0f0;
                --grey-2: #d0d0d0;
                --grey-3: #a0a0a0;
                --grey-4: #707070;
                --grey-5: #404040;
                --grey-6: #202020;
                --grey-7: #101010;
                --red: #881f1f;
            }
            *, *::before, *::after {
                box-sizing: border-box !important;
                margin: 0;
                padding: 0;
            }
        body { 
            font-family: sans-serif; 
            background: var(--grey-2);
            display: flex;
            flex-direction: column;
            height: 100vh;
        }

        h1 {
            font-size: 2rem;
            position: sticky;
            top: 0;
            padding: 10px 20px;
            background-color: var(--grey-2);
            border-bottom: 1px solid var(--grey-3);
        }

        main {
            padding: 40px 20px;
            background: var(--grey-1);
            flex-grow: 1;
        }
        .btn {
            padding: 10px 20px; 
            border: none;
            border-radius: 10px; 
            cursor: pointer;
            background: var(--grey-2); 
            border: 1px solid var(--grey-3);
            font-size: 1rem;
            font-weight: bolder;
            color: var(--grey-6);
            align-self: flex-start;
        }
        /* Prevent text selection on buttons */
        .btn {
            user-select: none; /* Standard */
            -webkit-user-select: none; /* Safari */
            -ms-user-select: none; /* IE10+ */
            -moz-user-select: none; /* Firefox */
            -webkit-touch-callout: none; /* Disable callout menu on iOS */
        }
        .btn.active { 
            background: var(--grey-5);
            color: var(--white); 
            border: none;
        }
        #GOBtn {
            width: 100%;
            margin-top: 40px;
            font-size: 1.5rem;
        }
        #GOBtn.active {
            background: var(--red);
            color: var(--white);
            border: 1px solid var(--red);
        }
        #AUTOBtn.active, #MANUALBtn.active {
            padding-bottom: 20px;
            border-radius: 10px 10px 0 0;
        }
        .btn-small {
            font-size: 1rem;
            padding: 5px 15px;
            border-radius: 100px;
            color: var(--grey-5);
        }

        .auto-manual-toggle {
            display: flex;
            gap: 10px;
        }
        .auto-manual-toggle .btn {
            flex: 1;
        }
        .auto-and-manual {
            border: 2px solid var(--grey-3);
            padding: 10px;
            border-radius: 10px;
            margin-bottom: 40px;
            background: var(--grey-1);
        }
        label {
            font-weight: bolder;
            color: var(--grey-4);
            text-transform: uppercase;
            font-size: 0.75rem;
        }
        .auto-only, .manual-only {
            display: none;
        }
        .PspInputContainer {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        #PspInput {
            background: none;
            border: 1px solid var(--grey-3);
            height: 30px;
            flex-grow: 1;
            border-radius: 100px;
            padding-inline: 10px;
        }

        main:has(#AUTOBtn.active) .manual-only {
            display: none;
        }
        main:has(#AUTOBtn.active) .auto-only {
            display: block;
            border: 2px solid var(--grey-5);
            padding: 20px 10px;
            border-radius: 0px 10px 10px 10px;
        }
        main:has(#MANUALBtn.active) .auto-only {
            display: none;
        }
        main:has(#MANUALBtn.active) .manual-only {
            display: block;
            border: 2px solid var(--grey-5);
            padding: 20px 10px;
            border-radius: 10px 0px 10px 10px;
        }
        .manual-btns {
            display: flex;
            gap: 10px;
        }
        .manual-btns .btn {
            flex: 1;
        }
        #StateMessage, #PressureDisplay {
            font-size: 1rem !important;
            font-weight: normal !important;

        }
        </style>
        </head>

        
        <body>

        <h1>AutoPump</h1>

        <main>
            
            <div class="auto-and-manual">
                <label for="unitSelect">Pressure Units:</label>
                <select id="unitSelect" onchange="requestUnitChange()">
                    <option value="kPa">kPa</option>
                    <option value="psi">psi</option>
                    <option value="bar">bar</option>
                    <option value="kg/cm2">kg/cm²</option>
                    <option value="atm">atm</option>
                </select>
                
                <div style="margin-top:10px;">
                    <label>Live Pressure:</label>
                    <span id="PressureDisplay" style="font-size:2em; font-weight:bold;"></span>
                </div>
                
                <div style="margin-top:10px;">
                    <label>State Message:</label>
                    <span id="StateMessage" style="font-size:2em; font-weight:bold;"></span>
                </div>
            </div>
            
            <div class="auto-manual-toggle">
                <button id="AUTOBtn" class="btn">Auto</button>
                <button id="MANUALBtn" class="btn">Manual</button>
            </div>
            
            <div class="manual-only">
                <div class="manual-btns">
                    
                    <button id="ManualInflateBtn" class="btn btn-small">Inflate</button>
                    <button id="ManualDeflateBtn" class="btn btn-small">Deflate</button>
                </div>
            </div>
            
            <div class="auto-only">
                <label for="PspInput" class="PspInputContainer">Pressure Setpoint:
                    <input type="number" id="PspInput" min="1" max="50" step="0.1">
                    
                </label>
                <span id="PspDisplay"></span>
            </div>
            <button id="GOBtn" class="btn">GO</button>
    
        </main>




        
        <script>

            // const autoBtn = document.getElementById('AUTOBtn');
            // autoBtn.addEventListener('click', () => {
            //     document.getElementById('MANUALBtn').classList.remove('active');
            //     autoBtn.classList.add('active');
            // });
            // const manualBtn = document.getElementById('MANUALBtn');
            // manualBtn.addEventListener('click', () => {
            //     document.getElementById('AUTOBtn').classList.remove('active');
            //     manualBtn.classList.add('active');
            // });
        // --- Bit Definitions ---
        const Bit00 = 1 << 0; // GOBtn (Bit 2^0)
        const Bit01 = 1 << 1; // AUTO (Bit 2^1)
        const Bit02 = 1 << 2; // MANUAL (Bit 2^2)
        const Bit03 = 1 << 3; // ManualInflate (Bit 2^3)
        const Bit04 = 1 << 4; // ManualDeflate (Bit 2^4)
        const Bit05 = 1 << 5; // Unused
        const Bit06 = 1 << 6; // Unused
        const Bit07 = 1 << 7; // Unused
        const Bit08 = 1 << 8; // Unused
        const Bit09 = 1 << 9; // Unused
        const Bit10 = 1 << 10; // Unused
        const Bit11 = 1 << 11; // Unused
        const Bit12 = 1 << 12; // Unused
        const Bit13 = 1 << 13; // Unused
        const Bit14 = 1 << 14; // Unused
        const Bit15 = 1 << 15; // Unused

        // Disable long-press text selection on buttons (especially for mobile)
        document.querySelectorAll('.btn').forEach(btn => {
            btn.addEventListener('contextmenu', (e) => {
                e.preventDefault(); // Prevents right-click or long-press menu
            });
        });



        // Initial unit selection
        let selectedUnit = document.getElementById('unitSelect').value; // grab initial dropdown value

        // Backend-authoritative UI state
        let uiState = {
            GOBtn: false,
            AUTOBtn: false,
            MANUALBtn: false,
            ManualInflateBtn: false,
            ManualDeflateBtn: false
        };

        // Build bitmask from any given state object
        function buildButtonWordFromState(ButtonState) {
            let word = 0;
            if (ButtonState.GOBtn) word |= Bit00;
            if (ButtonState.AUTOBtn) word |= Bit01;
            if (ButtonState.MANUALBtn) word |= Bit02;
            if (ButtonState.ManualInflateBtn) word |= Bit03;
            if (ButtonState.ManualDeflateBtn) word |= Bit04;
            return word;
        }

        // Build bitmask for unit request
        function buildHMIUnitRequestWord(){
            let word = 0;
            let unitRequest = document.getElementById('unitSelect').value;
            if (unitRequest == "kPa") word |= Bit00;
            if (unitRequest == "psi") word |= Bit01;
            if (unitRequest == "bar") word |= Bit02;
            if (unitRequest == "kg/cm2") word |= Bit03;
            if (unitRequest == "atm") word |= Bit04;
            return word;
        }

        // Apply backend-authoritative button states to UI
        function applyBackendButtonStates(word) {
            // Decode bits
            let GOResponse = !!(word & Bit00);
            let AUTOResponse = !!(word & Bit01);
            let MANUALResponse = !!(word & Bit02);
            let ManualInflateResponse = !!(word & Bit03);
            let ManualDeflateResponse = !!(word & Bit04);

            // Update uiState
            uiState.GOBtn = GOResponse;
            uiState.AUTOBtn = AUTOResponse;
            uiState.MANUALBtn = MANUALResponse;
            uiState.ManualInflateBtn = ManualInflateResponse;
            uiState.ManualDeflateBtn = ManualDeflateResponse;

            // Update button visuals
            document.getElementById('GOBtn').classList.toggle('active', GOResponse);
            document.getElementById('AUTOBtn').classList.toggle('active', AUTOResponse);
            document.getElementById('MANUALBtn').classList.toggle('active', MANUALResponse);
            document.getElementById('ManualInflateBtn').classList.toggle('active', ManualInflateResponse);
            document.getElementById('ManualDeflateBtn').classList.toggle('active', ManualDeflateResponse);

            // Update select dropdown to match backend state (if needed)
            const unitSelect = document.getElementById('unitSelect');
        }

        // Send a request for desired states to backend & update UI ONLY after backend responds
        function requestStateChange(desiredStates) {

                let buttonRequestWord = buildButtonWordFromState(desiredStates);
                fetch(`/hmi?HMIButtonRequestWord=${buttonRequestWord}`)

                    .then(res => res.json())
                    .then(data => {
                        let backendButtonResponse = data.ESPButtonResponseWord;
                        applyBackendButtonStates(backendButtonResponse); // backend decides final state
                    })
                    .catch(err => console.error("HMI request failed:", err));                    
        }

        function requestUnitChange() {
            const newUnit = document.getElementById('unitSelect').value;

            fetch(`/setUnit?unit=${newUnit}`)
                .then(response => response.json())
                .then(data => {
                    selectedUnit = data.unit;                    
                    document.getElementById('unitSelect').value = selectedUnit;
                })
                .catch(error => console.error('Error:', error));
        }

        // --- Pressure Setpoint Handling ---
        function requestSetpointChange(newSetpoint) {
            fetch(`/hmi?PspHMI=${newSetpoint}`)
                .then(res => res.json())
                .then(data => {
                    // Backend returns authoritative setpoint
                    if (data.Psp !== undefined) {
                        if (document.activeElement !== document.getElementById('PspInput')) {
                            document.getElementById('PspInput').value = data.Psp;
                        }
                        document.getElementById('PspDisplay').textContent = `Setpoint: ${data.Psp} psi`;
                    }
                    // Also update button states if present
                    if (data.ESPButtonResponseWord !== undefined) {
                        applyBackendButtonStates(data.ESPButtonResponseWord);
                    }
                })
                .catch(err => console.error("Setpoint request failed:", err));
        }

        document.getElementById('PspInput').addEventListener('blur', () => {
            let val = parseFloat(document.getElementById('PspInput').value);
            if (!isNaN(val)) {
                requestSetpointChange(val);
            }
        });

        document.getElementById('PspInput').addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                let val = parseFloat(document.getElementById('PspInput').value);
                if (!isNaN(val)) {
                    requestSetpointChange(val);
                    document.getElementById('PspInput').blur(); // Optionally close keyboard on mobile
                }
            }
        });

        // Click handlers: build a desired state, but don't apply locally
        document.querySelectorAll('.btn').forEach(btn => {
            btn.addEventListener('click', () => {
                let id = btn.id;
                let requestedStates = { ...uiState }; // copy current backend-authoritative state

                if (id === 'GOBtn') requestedStates.GOBtn = !uiState.GOBtn;
                if (id === 'AUTOBtn') requestedStates.AUTOBtn = !uiState.AUTOBtn;
                if (id === 'MANUALBtn') requestedStates.MANUALBtn = !uiState.MANUALBtn;

                // Send to backend → wait for confirmation → update UI
                requestStateChange(requestedStates);
            });
        });

        // manual buttons push and hold
        const inflateBtn = document.getElementById('ManualInflateBtn');
        const deflateBtn = document.getElementById('ManualDeflateBtn');

        // Prevent click from firing after touch
        deflateBtn.addEventListener('click', (e) => e.preventDefault());
        inflateBtn.addEventListener('click', (e) => e.preventDefault());

        // Deflate Button
        deflateBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            deflateBtn.setPointerCapture(e.pointerId);
            setManualDeflate(true);
        });

        deflateBtn.addEventListener('pointerup', (e) => {
            e.preventDefault();
            setManualDeflate(false);
            deflateBtn.releasePointerCapture(e.pointerId);
        });

        // Inflate Button
        inflateBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            inflateBtn.setPointerCapture(e.pointerId);
            setManualInflate(true);
        });

        inflateBtn.addEventListener('pointerup', (e) => {
            e.preventDefault();
            setManualInflate(false);
            inflateBtn.releasePointerCapture(e.pointerId);
        });

        // State change functions
        function setManualDeflate(value) {
            requestStateChange({ ...uiState, ManualDeflateBtn: value });
        }

        function setManualInflate(value) {
            requestStateChange({ ...uiState, ManualInflateBtn: value });
        }

        // Poll backend periodically to keep in sync (buttons + setpoint)
        setInterval(() => {
            fetch('/hmi')
                .then(res => res.json())
                .then(data => {

                    // --- UI update ---
                    if (data.ESPButtonResponseWord !== undefined) {
                        applyBackendButtonStates(data.ESPButtonResponseWord);
                    }
                    if (data.Psp !== undefined) {
                        if (document.activeElement !== document.getElementById('PspInput')) {
                            document.getElementById('PspInput').value = data.Psp;
                        }
                        document.getElementById('PspDisplay').textContent = `Setpoint: ${data.Psp} ${data.unit}`;
                    }
                    if (data.Pressure !== undefined) {
                        document.getElementById('PressureDisplay').textContent = `${data.Pressure.toFixed(2)} ${data.unit}`;
                    }
                    if (data.StateMessage !== undefined) {
                        document.getElementById('StateMessage').textContent = data.StateMessage;
                    }
                    if (data.unit !== undefined) {
                        document.getElementById('unitSelect').value = data.unit;
                    }
                })
                .catch(err => console.warn("Polling failed:", err));
        }, 250); // 4 times per second

        </script>
        </body>
        </html>
    )rawliteral";
    server.send(200, "text/html", page);
}

// Reads pressure from ADS1115 ADC module
int16_t adc3 = 0;
int16_t adc1 = 0;
double volts3 = 0.0;
double volts1 = 0.0;
double pressure = 0.0;
double readPressure()
{
    if (!adsPresent) {
        pressure_psi = -1;  // Or keep last valid reading
        return pressure_psi;
    }
    adc3 = ads.readADC_SingleEnded(3);
    adc1 = ads.readADC_SingleEnded(1);
    if (adc3 == -1 || adc1 == -1) { // ADS1115 returns -1 on error
        pressure = -1;
    }
    else
    {
        volts3 = ads.computeVolts(adc3);
        volts1 = ads.computeVolts(adc1);
        pressure_psi = ((150.0 / 4.0) * volts1) - (150.0 / 8.0);
        pressure = pressure_psi;
    }
    return pressure;
}

// Escape to S0 (safe state)
void Escape()
{
    // Write all outputs to FALSE
    PumpON = false;
    SolenoidON = false;
    digitalWrite(InflatePin, PumpON ? HIGH : LOW);
    digitalWrite(DeflatePin, SolenoidON ? HIGH : LOW);

    // Default unit to kPa
    Unit = "kPa";

    // Wipe setpoint
    Psp_psi = 0;

    // Set buttons to safe state
    GoButtonActive = false;
    AUTO = false;
    MANUAL = false;
    ManualInflate = false;
    ManualDeflate = false;

    // Write all state variables to false
    S0 = false;
    S0a = false;
    S1a = false;
    S1aa = false;
    S1ab = false;
    S1ac = false;
    S1ad = false;
    S1ae = false;
    S1af = false;
    S1ag = false;
    S1ah = false;
    S1b = false;
    S1ba = false;
    S1bb = false;
    S1bc = false;
    S1bd = false;
    S1be = false;
    S1bf = false;
    S1bg = false;
    S1bh = false;
    S1c = false;
    S1ca = false;
    S1cb = false;
    S2 = false;
    S2a = false;
    S2b = false;

    // Write all transition variables to FALSE
    ToS0 = false;
    ToS0a = false;
    ToS1a = false;
    ToS1aa = false;
    ToS1ab = false;
    ToS1ac = false;
    ToS1ad = false;
    ToS1ae = false;
    ToS1af = false;
    ToS1ag = false;
    ToS1ah = false;
    ToS1b = false;
    ToS1ba = false;
    ToS1bb = false;
    ToS1bc = false;
    ToS1bd = false;
    ToS1be = false;
    ToS1bf = false;
    ToS1bg = false;
    ToS1bh = false;
    ToS1c = false;
    ToS1ca = false;
    ToS1cb = false;
    ToS2 = false;
    ToS2a = false;
    ToS2b = false;

    // Go to S0 (safe state)
    ToS0 = true;
}

// Limit inflation time to given range
double InflBounds(double x)
{
    if (x > AutoPumpOnMaxTm)
    {
        return AutoPumpOnMaxTm;
    }
    else if (x < AutoPumpOnMinTm)
    {
        return AutoPumpOnMinTm;
    }
    else
    {
        return x;
    }
}

// Limit deflation time to given range
double DeflBounds(double x)
{
    if (x > AutoSolOnMaxTime)
    {
        return AutoSolOnMaxTime;
    }
    else if (x < AutoSolOnMinTime)
    {
        return AutoSolOnMinTime;
    }
    else
    {
        return x;
    }
}

// Helper to check for multiple active states/transitions
void checkMultipleActiveStates()
{
    int stateCount = 0;
    int transCount = 0;

    // Count active state variables
    if (S0) transCount++;
    if (S0a) transCount++;
    if (S1a) transCount++;
    if (S1aa) transCount++;
    if (S1ab) transCount++;
    if (S1ac) transCount++;
    if (S1ad) transCount++;
    if (S1ae) transCount++;
    if (S1af) transCount++;
    if (S1ag) transCount++;
    if (S1ah) transCount++;
    if (S1b) transCount++;
    if (S1ba) transCount++;
    if (S1bb) transCount++;
    if (S1bc) transCount++;
    if (S1bd) transCount++;
    if (S1be) transCount++;
    if (S1bf) transCount++;
    if (S1bg) transCount++;
    if (S1bh) transCount++;
    if (S1c) transCount++;
    if (S1ca) transCount++;
    if (S1cb) transCount++;
    if (S2) transCount++;
    if (S2a) transCount++;
    if (S2b) transCount++;

    // Count active transition variables
    if (ToS0) transCount++;
    if (ToS0a) transCount++;
    if (ToS1a) transCount++;
    if (ToS1aa) transCount++;
    if (ToS1ab) transCount++;
    if (ToS1ac) transCount++;
    if (ToS1ad) transCount++;
    if (ToS1ae) transCount++;
    if (ToS1af) transCount++;
    if (ToS1ag) transCount++;
    if (ToS1ah) transCount++;
    if (ToS1b) transCount++;
    if (ToS1ba) transCount++;
    if (ToS1bb) transCount++;
    if (ToS1bc) transCount++;
    if (ToS1bd) transCount++;
    if (ToS1be) transCount++;
    if (ToS1bf) transCount++;
    if (ToS1bg) transCount++;
    if (ToS1bh) transCount++;
    if (ToS1c) transCount++;
    if (ToS1ca) transCount++;
    if (ToS1cb) transCount++;
    if (ToS2) transCount++;
    if (ToS2a) transCount++;
    if (ToS2b) transCount++;

    if (stateCount > 1 || transCount > 1) {
        Serial.println("ERROR: Multiple state or transition variables are active!");
        // Optionally call Escape() or handle error here
        Escape();
    }
}

// Bit masks
#define Bit00 (1 << 00)   // GOBtn (Bit 2^0)
#define Bit01 (1 << 01)   // AUTO (Bit 2^1)
#define Bit02 (1 << 02)   // MANUAL (Bit 2^2)
#define Bit03 (1 << 03)   // ManualInflate (Bit 2^3)
#define Bit04 (1 << 04)   // ManualDeflate (Bit 2^4)
#define Bit05 (1 << 05)   // Unused
#define Bit06 (1 << 06)   // Unused
#define Bit07 (1 << 07)   // Unused
#define Bit08 (1 << 08)   // Unused
#define Bit09 (1 << 09)   // Unused
#define Bit10 (1 << 10) // Unused
#define Bit11 (1 << 11) // Unused
#define Bit12 (1 << 12) // Unused
#define Bit13 (1 << 13) // Unused
#define Bit14 (1 << 14) // Unused
#define Bit15 (1 << 15) // Unused

void handleHMIButtonRequest()
{
    // If frontend sent a new desired bitfield, parse it
    if (server.hasArg("HMIButtonRequestWord"))
    {
        uint16_t requested = (uint16_t)server.arg("HMIButtonRequestWord").toInt();

        // Accept request
        HMIButtonRequestWord = requested;

        // Unpack word | ------------------------------------------------------------------------------------------------------
        // GoButtonActive
        GoButtonActive = HMIButtonRequestWord & Bit00;
        // AUTO or MANUAL mode selection
        bool ReqAUTO = HMIButtonRequestWord & Bit01;
        bool ReqMANUAL = HMIButtonRequestWord & Bit02;
        if (ReqAUTO && ReqMANUAL)
        { // Don't select AUTO and MANUAL simultaneously
            Escape();
        }
        else if (ReqAUTO)
        { // AUTO requested
            MANUAL = false;
            AUTO = true;
        }
        else if (ReqMANUAL)
        { // MANUAL requested
            AUTO = false;
            MANUAL = true;
        }
        else
        {
            Escape(); // No operating mode requested
        }
        // ManualInflate AND/OR ManualDeflate
        if (MANUAL && !AUTO)
        {
            ManualInflate = HMIButtonRequestWord & Bit03;
            ManualDeflate = HMIButtonRequestWord & Bit04;
        }
    }
}

void handleESPButtonResponse()
{
    ESPButtonResponseWord = 0;
    if (GoButtonActive)
        ESPButtonResponseWord |= Bit00;
    if (AUTO)
        ESPButtonResponseWord |= Bit01;
    if (MANUAL)
        ESPButtonResponseWord |= Bit02;
    if (ManualInflate)
        ESPButtonResponseWord |= Bit03;
    if (ManualDeflate)
        ESPButtonResponseWord |= Bit04;
}

// Incoming pressure setpoint Psp_psi request from HMI
void handlePresSetpoint()
{
    if (server.hasArg("PspHMI")) {
        double input = server.arg("PspHMI").toDouble();

        if (S0 || ToS0 || S0a || ToS0a || S2 || ToS2) {
            if (Unit == "kPa") Psp_psi = input / 6.89476;
            else if (Unit == "bar") Psp_psi = input / 0.0689476;
            else if (Unit == "kg/cm2") Psp_psi = input / 0.0703069;
            else if (Unit == "atm") Psp_psi = input / 0.0680459;
            else Psp_psi = input;  // psi
        }

        // Now apply limits in psi
        if (Psp_psi < 1) Psp_psi = 1;
        if (Psp_psi > 50) Psp_psi = 50;
    }
}

void setup()
{
    setCpuFrequencyMhz(240);
    Serial.begin(115200);

    Wire.begin(ADS_SDA, ADS_SCL);
    Wire.beginTransmission(0x48);
    if (Wire.endTransmission() == 0) {
        if (ads.begin()) {
            adsPresent = true;
            Serial.println("ADS1115 detected.");
        } else {
            adsPresent = false;
            Serial.println("ADS1115 init failed.");
        }
    } else {
        adsPresent = false;
        Serial.println("ADS1115 NOT detected. Skipping ads.begin().");
    }

    pinMode(InflatePin, OUTPUT);
    pinMode(DeflatePin, OUTPUT);

    setupWiFi();
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    setupWebServer();

    // This function brings the system to a S0 (safe state)
    Escape();
}


unsigned long lastSerialMonitorOutput = 0;
// Main loop of code

void loop()
{
    pressure = readPressure();
    if (millis() - lastSerialMonitorOutput >= GraphPlotDelay) {
        lastSerialMonitorOutput = millis();
        Serial.println(StateMessage.c_str());
        Serial.printf("ADC1 Pressure: %.4f\n\n",  pressure);
    }
    server.handleClient();
    checkMultipleActiveStates();

    // State 0 | Return system to safe operating conditions and move to active states | ---------------------------------------
    // S0 | Safe state | single frame |--------------------------------------------------------------------------------------------------
    if (S0 || ToS0)
    {
        // Initialise state
        ToS0 = false;
        S0 = true;

        // Write outputs buttons to FALSE
        PumpON = false;
        SolenoidON = false;
        GoButtonActive = false;
        AUTO = false;
        MANUAL = false;
        ManualInflate = false;
        ManualDeflate = false;

        // Transition Onward Code
        S0 = false;
        ToS0a = true;
    }

    // S0a | Move to operating state | multi-frame | -------------------------------------------------------------------------
    if (S0a || ToS0a)
    {
        // Initialise state
        ToS0a = false;
        S0a = true;

        // Update state variable message
        StateMessage = "S0a | Move to operating state";

        // Pressure reading to compare with setpoint
        pressure = readPressure();
        P0 = pressure;

        // Transition Onward Code
        if (GoButtonActive)
        {
            // User selected MANUAL MODE
            if (MANUAL)
            {
                S0a = false;
                ToS2 = true;
            }
            // User selected AUTO MODE
            else if (AUTO)
            {
                // Need to inflate
                if (P0 < Psp_psi - PresBuffer)
                {
                    S0a = false;
                    ToS1a = true;
                }
                // Need to deflate
                else if (P0 > Psp_psi + PresBuffer)
                {
                    S0a = false;
                    ToS1b = true;
                }
            }
        }
    }

    // State 1 | Automated inflation or deflation |-------------------------------------------------------------------------------------------------
    // S1ax | Smart inflation | -------------------------------------------------------------------------------------------------------
    // S1a | Let pressure settle | multi-frame |---------------------------------------------------------------------------------------------------
    if (S1a || ToS1a)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1a | Let pressure settle";

        // Initialise state
        if (ToS1a)
        {
            ToS1a = false;
            S1a = true;

            S1aSttTm = millis();
        }

        // Transition Onward Code
        if (S1aSttTm != 0.00 && (millis() - S1aSttTm >= SettleTm))
        {
            S1aSttTm = 0.00;

            S1a = false;
            ToS1aa = true;
        }
    }

    // S1aa | Measure calibration pressure Pc1 | single frame | -----------------------------------------------------------------
    if (S1aa || ToS1aa)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1aa = false;
        S1aa = true;

        pressure = readPressure();
        Pc1 = pressure;

        // Transition Onward Code
        S1aa = false;
        ToS1ab = true;
    }

    // S1ab | Inflate for CalibrationTm seconds | multi-frame | ------------------------------------------------------------------------------------
    if (S1ab || ToS1ab)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message        
        std::ostringstream oss;
        oss << "S1ab | Calibration Inflation | Inflate for " << std::fixed << std::setprecision(2) << (CalibrationTm / 1000.0) << " seconds";
        StateMessage = oss.str();

        // Initialise state
        if (ToS1ab)
        {
            ToS1ab = false;
            S1ab = true;

            S1abSttTm = millis();
            PumpON = true;
        }

        // Transition Onward Code
        if (S1abSttTm != 0.00 && (millis() - S1abSttTm >= CalibrationTm))
        {
            PumpON = false;
            S1abSttTm = 0.00;

            S1ab = false;
            ToS1ac = true;
        }
    }

    // S1ac | Let pressure settle | multi-frame | --------------------------------------------------------------------------------
    if (S1ac || ToS1ac)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1ac | Let pressure settle";

        // Initialise state
        if (ToS1ac)
        {
            ToS1ac = false;
            S1ac = true;

            S1acSttTm = millis();
        }

        // Transition Onward Code
        if (S1acSttTm != 0.00 && (millis() - S1acSttTm >= SettleTm))
        {
            S1acSttTm = 0.00;

            S1ac = false;
            ToS1ad = true;
        }
    }

    // S1ad | Measure calibration pressure Pc2 | single frame | --------------------------------------------------------------------
    if (S1ad || ToS1ad)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1ad = false;
        S1ad = true;

        pressure = readPressure();
        Pc2 = pressure;
        DeltaTm = 10.00;

        // Transition Onward Code
        if (Pc2 <= Psp_psi + PresBuffer)
        { // Still under-pressure
            S1ad = false;
            ToS1ae = true;
        }
        else if (Psp_psi - PresBuffer < Pc2 && Pc2 < Psp_psi + PresBuffer)
        { // Calibration clutched it
            S1ad = false;
            ToS0 = true;
        }
        else if (Pc2 <= Psp_psi - PresBuffer)
        { // Calibration overshot
            S1ad = false;
            ToS1c = true;
        }
    }

    // S1ae | Calculate inflation time InflTm | single frame | ----------------------------------------------------------------
    if (S1ae || ToS1ae)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1ae = false;
        S1ae = true;

        // Calcualte inflation time (seconds)
        m = (Pc2 - Pc1)/DeltaTm;
        InflTm = (Psp_psi - Pc2) / m;
        InflTm = InflBounds(InflTm);

        // Transition Onward Code
        DeltaTm = InflTm;
        S1ae = false;
        ToS1af = true;
    }

    // S1af | Inflate for InflTm seconds | multi-frame | --------------------------------------------------------------------------
    if (S1af || ToS1af)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        std::ostringstream oss;
        oss << "S1af | Smart Inflation | Inflate for " << std::fixed << std::setprecision(2) << InflTm << " seconds";
        StateMessage = oss.str();

        // Initialise state
        if (ToS1af)
        {
            ToS1af = false;
            S1af = true;

            S1afSttTm = millis();
            PumpON = true;
        }

        // Transition onward code
        if (S1afSttTm != 0.00 && (millis() - S1afSttTm >= InflTm * 1000))
        {
            PumpON = false;
            InflTm = 0.00;
            S1afSttTm = 0.00;

            S1af = false;
            ToS1ag = true;
        }
    }

    // S1ag | Let pressure settle | multi-frame | ------------------------------------------------------------------------
    if (S1ag || ToS1ag)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1ag | Let pressure settle";

        // Initialise state
        if (ToS1ag)
        {
            ToS1ag = false;
            S1ag = true;

            S1agSttTm = millis();
        }

        // Transition onward code
        if (S1agSttTm != 0.00 && (millis() - S1agSttTm >= SettleTm))
        {
            S1agSttTm = 0.00;

            S1ag = false;
            ToS1ah = true;
        }
    }

    // S1ah | Check pressure and transition to appropriate state | single frame | -----------------------------------------------------
    if (S1ah || ToS1ah)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1ah = false;
        S1ah = true;

        Pc1 = Pc2;
        pressure = readPressure();
        Pc2 = pressure;

        if (Pc2 < Psp_psi - PresBuffer)
        { // Under-pressure
            S1ah = false;
            ToS1ae = true;
        }
        else if (Psp_psi - PresBuffer <= Pc2 && Pc2 <= Psp_psi + PresBuffer)
        { // Adequate pressure
            S1ah = false;
            ToS0 = true;
        }
        else if (Pc2 > Psp_psi + PresBuffer)
        { // Over-pressure
            S1ah = false;
            ToS1c = true;
        }
    }

    // S1bx | Smart deflation | ----------------------------------------------------------------------------------------------------------
    // S1b | Let pressure settle | multi-frame | ---------------------------------------------------------------------------------------------
    if (S1b || ToS1b)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1b | Let pressure settle";

        // Initialise state
        if (ToS1b)
        {
            ToS1b = false;
            S1b = true;

            S1bSttTm = millis();
        }

        // Transition Onward Code
        if (S1bSttTm != 0.00 && (millis() - S1bSttTm >= SettleTm))
        {
            S1bSttTm = 0.00;

            S1b = false;
            ToS1ba = true;
        }
    }

    // S1ba | Measure calibration pressure Pc1 | single frame | ----------------------------------------------------------------------------
    if (S1ba || ToS1ba)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1ba = false;
        S1ba = true;

        pressure = readPressure();
        Pc1 = pressure;

        // Transition Onward Code
        S1ba = false;
        ToS1bb = true;
    }

    // S1bb | Deflate for CalibrationTm seconds | multi-frame | ------------------------------------------------------------------------------------
    if (S1bb || ToS1bb)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        std::ostringstream oss;
        oss << "S1bb | Calibration Deflation | Deflate for " << std::fixed << std::setprecision(2) << (CalibrationTm / 1000.0) << " seconds";
        StateMessage = oss.str();

        // Initialise state
        if (ToS1bb)
        {
            ToS1bb = false;
            S1bb = true;

            S1bbSttTm = millis();
            SolenoidON = true;
        }

        // Transition Onward Code
        if (S1bbSttTm != 0.00 && (millis() - S1bbSttTm >= CalibrationTm))
        {
            SolenoidON = false;
            S1bbSttTm = 0.00;

            S1bb = false;
            ToS1bc = true;
        }
    }

    // S1bc | Let pressure settle | multi-frame | --------------------------------------------------------------------------------
    if (S1bc || ToS1bc)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1bc | Let pressure settle";

        // Initialise state
        if (ToS1bc)
        {
            ToS1bc = false;
            S1bc = true;

            S1bcSttTm = millis();
        }

        // Transition Onward Code
        if (S1bcSttTm != 0.00 && (millis() - S1bcSttTm >= SettleTm))
        {
            S1bcSttTm = 0.00;

            S1bc = false;
            ToS1bd = true;
        }
    }

    // S1bd | Measure calibration pressure Pc2 | single frame | --------------------------------------------------------------------
    if (S1bd || ToS1bd)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1bd = false;
        S1bd = true;

        pressure = readPressure();
        Pc2 = pressure;
        DeltaTm = 10.00;

        // Transition Onward Code
        S1bd = false;
        ToS1be = true;
    }

    // S1be | Calculate deflation time DeflTm | single frame | ----------------------------------------------------------------
    if (S1be || ToS1be)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1be = false;
        S1be = true;

        // Calcualte deflation time (seconds)
        DeflTm = (DeltaTm * (Psp_psi - Pc2)) / (Pc2 - Pc1);
        DeflTm = DeflBounds(DeflTm);

        // Transition Onward Code
        DeltaTm = DeflTm;
        S1be = false;
        ToS1bf = true;
    }

    // S1bf | Deflate for DeflTm seconds | multi-frame | --------------------------------------------------------------------------
    if (S1bf || ToS1bf)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        std::ostringstream oss;
        oss << "S1bf | Smart Deflation | Deflate for " << std::fixed << std::setprecision(2) << (DeflTm) << " seconds";
        StateMessage = oss.str();

        // Initialise state
        if (ToS1bf)
        {
            ToS1bf = false;
            S1bf = true;

            S1bfSttTm = millis();
            SolenoidON = true;
        }

        // Transition onward code
        if (S1bfSttTm != 0.00 && (millis() - S1bfSttTm >= DeflTm * 1000))
        {
            SolenoidON = false;
            DeflTm = 0.00;
            S1bfSttTm = 0.00;

            S1bf = false;
            ToS1bg = true;
        }
    }

    // S1bg | Let pressure settle | multi-frame | ------------------------------------------------------------------------
    if (S1bg || ToS1bg)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1bg | Let pressure settle";

        // Initialise state
        if (ToS1bg)
        {
            ToS1bg = false;
            S1bg = true;

            S1bgSttTm = millis();
        }

        // Transition onward code
        if (S1bgSttTm != 0.00 && (millis() - S1bgSttTm >= SettleTm))
        {
            S1bgSttTm = 0.00;

            S1bg = false;
            ToS1bh = true;
        }
    }

    // S1bh | Check pressure and transition to appropriate state | single frame | -----------------------------------------------------
    if (S1bh || ToS1bh)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1bh = false;
        S1bh = true;

        Pc1 = Pc2;
        pressure = readPressure();
        Pc2 = pressure;

        if (Pc2 < Psp_psi - PresBuffer)
        { // Under-pressure
            S1bh = false;
            ToS0 = true;
        }
        else if (Psp_psi - PresBuffer <= Pc2 && Pc2 <= Psp_psi + PresBuffer)
        { // Adequate pressure
            S1bh = false;
            ToS0 = true;
        }
        else if (Pc2 > Psp_psi + PresBuffer)
        { // Over-pressure
            S1bh = false;
            ToS1be = true;
        }
    }

    // S1cx | Dumb deflation | ------------------------------------------------------------------------------------------------
    // S1c | Deflate for AutoSolOnMinTime | multi-frame | -----------------------------------------------------------------------------------
    if (S1c || ToS1c)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1c | Dumb Deflation | Deflate for " + std::to_string(AutoSolOnMinTime/1000) + " seconds";

        // Initialise state
        if (ToS1c)
        {
            ToS1c = false;
            S1c = true;

            S1cSttTm = millis();
            SolenoidON = true;
        }

        // Transition onward code
        if (S1cSttTm != 0.00 && (millis() - S1cSttTm >= AutoSolOnMinTime * 1000))
        {
            SolenoidON = false;
            S1cSttTm = 0.00;

            S1c = false;
            ToS1ca = true;
        }
    }

    // S1ca | Let pressure settle | Multi-frame | ----------------------------------------------------------------------------
    if (S1ca || ToS1ca)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1ca | Let pressure settle";

        // Initialise state
        if (ToS1ca)
        {
            ToS1ca = false;
            S1ca = true;
            S1caSttTm = millis();
        }

        // Transition onward code
        if (S1caSttTm != 0.00 && (millis() - S1caSttTm >= SettleTm))
        {
            SolenoidON = false;
            S1caSttTm = 0.00;

            S1ca = false;
            ToS1cb = true;
        }
    }

    // S1cb | Measure pressure | single frame | --------------------------------------------------------------------------
    if (S1cb || ToS1cb)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Initialise state
        ToS1cb = false;
        S1cb = true;

        pressure = readPressure();
        Pc2 = pressure;

        // Transition Onward Code
        if (Pc2 > Psp_psi + PresBuffer)
        { // Pressure still too high, repeat dumb deflation cycle
            S1cb = false;
            ToS1c = true;
        }
        else if (Psp_psi - PresBuffer <= Pc2 && Pc2 <= Psp_psi + PresBuffer)
        { // Pressure in range, end automation
            S1cb = false;
            ToS0 = true;
        }
        else if (Pc2 < Psp_psi - PresBuffer)
        { // Pressure too low, end automation
            S1cb = false;
            ToS0 = true;
        }
    }

    // State 2 | Manual inflation or deflation |-------------------------------------------------------------------------------------------------
    // S2 | Manual inflation or deflation | --------------------------------------------------------------------------------
    if (S2 || ToS2)
    {
        // Escape condition
        if (!GoButtonActive || !MANUAL)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S2 | Manual Inflation or Deflation";

        // Initilise state
        ToS2 = false;
        S2 = true;

        // Manual inflate
        if (ManualInflate)
        {
            S2 = false;
            ToS2a = true;
        }

        // Manual deflate
        if (ManualDeflate)
        {
            S2 = false;
            ToS2b = true;
        }

        // Transition Onward Code | Added for consistency, redundant as functionality is captured in escape condition
        // if (!GoButtonActive || !MANUAL){
        //     S2 = false;
        //     ToS0 = true;
        // }
    }

    // S2a | Manual inflation | multi-frame | --------------------------------------------------------------------------------
    if (S2a || ToS2a)
    {
        // Escape condition
        if (!GoButtonActive || !MANUAL)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S2a | Manually Inflating";

        // Initialise state
        if (ToS2a)
        {
            ToS2a = false;
            S2a = true;
            S2aSttTm = millis();
        }

        if (ManualInflate && millis() - S2aSttTm >= ManualPumpOnMinTm * 1000)
        {
            PumpON = true;
        }
        else if (ManualInflate && millis() - S2aSttTm < ManualPumpOnMinTm * 1000)
        {
            PumpON = false;
        }
        else
        {
            PumpON = false;
            S2aSttTm = 0.00;
            S2a = false;
            ToS2 = true;
        }
    }

    // S2b | Manual deflation | multi-frame | --------------------------------------------------------------------------------
    if (S2b || ToS2b)
    {
        // Escape condition
        if (!GoButtonActive || !MANUAL)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S2b | Manually Deflating";

        // Initialise state
        if (ToS2b)
        {
            ToS2b = false;
            S2b = true;
            S2bSttTm = millis();
        }

        if (ManualDeflate && millis() - S2bSttTm >= ManualSolOnMinTm * 1000)
        {
            SolenoidON = true;
        }
        else if (ManualDeflate && millis() - S2bSttTm < ManualSolOnMinTm * 1000)
        {
            SolenoidON = false;
        }
        else
        {
            SolenoidON = false;
            S2bSttTm = 0.00;
            S2b = false;
            ToS2 = true;
        }
    }

    handleESPButtonResponse();
    digitalWrite(InflatePin, PumpON ? HIGH : LOW);
    digitalWrite(DeflatePin, SolenoidON ? HIGH : LOW);
}
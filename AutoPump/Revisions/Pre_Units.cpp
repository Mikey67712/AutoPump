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
void setupWiFi()
{
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
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
std::string StateMessage;

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
const double PumpOnMaxTm = 120;            // Seconds
const double PumpOnMinTm = 10;             // Seconds
const double SolenoidOnMaxTm = 120;        // Seconds
const double SolenoidOnMinTm = 1;          // Seconds
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

// Misc variables
double P0 = 0.00;
double Psp = 0.00;
double Pc1 = 0.00;
double Pc2 = 0.00;
double m = 0.00; // Infltion gradient
double InflTm = 0.00;
double DeflTm = 0.00;
double DeltaTm = 0.00;
double Pressure = 0.00;

// Incoming button request word from HMI
uint16_t HMIRequestWord = 0;
// Outgoing ESP32 response word to HMI
uint16_t ESPButtonResponseWord = 0;

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
    server.on("/hmi", HTTP_GET, []()
              {
        handleHMIButtonRequest();   // parse button request bits
        handlePresSetpoint();       // parse setpoint request
        handleESPButtonResponse();  // build response bits

        String json = "{";
        json += "\"ESPButtonResponseWord\":" + String(ESPButtonResponseWord);
        json += ",\"Psp\":" + String(Psp, 2);
        json += ",\"Pressure\":" + String(Pressure, 2);
        json += ",\"StateMessage\":\"" + String(StateMessage.c_str()) + "\"";
        json += "}";

        server.send(200, "application/json", json); });

    server.begin();
}

// Add this after canvas id: <script src="/static/chart.umd.min.js"></script>
void handleRoot() {
    String page = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
        <meta charset="UTF-8">
        <style>
        body { font-family: sans-serif; text-align: center; background: #f0f0f0; }
        .btn {
            display: inline-block; padding: 10px 20px; margin: 5px;
            border: 2px solid #333; border-radius: 8px; cursor: pointer;
            background: #ddd; font-size: 18px;
        }
        .btn.active { background: #4CAF50; color: white; }
        </style>
        </head>





        <body>

        <h1>AutoPump</h1>

        <div>
            <button id="GOBtn" class="btn">GO</button>
            <button id="AUTOBtn" class="btn">AUTO</button>
            <button id="MANUALBtn" class="btn">MANUAL</button>
            <button id="ManualInflateBtn" class="btn">Inflate</button>
            <button id="ManualDeflateBtn" class="btn">Deflate</button>
        </div>




        <div style="margin-top:20px;">
            <label for="PspInput">Pressure Setpoint (psi):</label>
            <input type="number" id="PspInput" min="1" max="50" step="0.1" style="width:80px;">
            <span id="PspDisplay"></span>
        </div>

        
        
        <div style="margin-top:10px;">
            <label>Live Pressure:</label>
            <span id="PressureDisplay" style="font-size:2em; font-weight:bold;"></span>
        </div>



        <div style="margin-top:10px;">
            <label>State Message:</label>
            <span id="StateMessage" style="font-size:2em; font-weight:bold;"></span>
        </div>



        
        <script>
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

        // Apply backend-authoritative state to UI
        function applyBackendState(word) {
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
        }

        // Send a request for desired states to backend & update UI ONLY after backend responds
        function requestStateChange(desiredStates) {
            let requestWord = buildButtonWordFromState(desiredStates);

            fetch(`/hmi?HMIButtonRequestWord=${requestWord}`)
                .then(res => res.json())
                .then(data => {
                    let backendResponse = data.ESPButtonResponseWord;
                    applyBackendState(backendResponse); // backend decides final state
                })
                .catch(err => console.error("HMI request failed:", err));
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
                        applyBackendState(data.ESPButtonResponseWord);
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

        // Push-and-hold logic for Inflate button
        const inflateBtn = document.getElementById('ManualInflateBtn');

        inflateBtn.addEventListener('mousedown', () => {
            let requestedStates = { ...uiState, ManualInflateBtn: true };
            requestStateChange(requestedStates);
        });
        inflateBtn.addEventListener('touchstart', () => {
            let requestedStates = { ...uiState, ManualInflateBtn: true };
            requestStateChange(requestedStates);
        });

        inflateBtn.addEventListener('mouseup', () => {
            let requestedStates = { ...uiState, ManualInflateBtn: false };
            requestStateChange(requestedStates);
        });
        inflateBtn.addEventListener('mouseleave', () => {
            let requestedStates = { ...uiState, ManualInflateBtn: false };
            requestStateChange(requestedStates);
        });
        inflateBtn.addEventListener('touchend', () => {
            let requestedStates = { ...uiState, ManualInflateBtn: false };
            requestStateChange(requestedStates);
        });

        // Push-and-hold logic for Deflate button
        const deflateBtn = document.getElementById('ManualDeflateBtn');

        deflateBtn.addEventListener('mousedown', () => {
            let requestedStates = { ...uiState, ManualDeflateBtn: true };
            requestStateChange(requestedStates);
        });
        deflateBtn.addEventListener('touchstart', () => {
            let requestedStates = { ...uiState, ManualDeflateBtn: true };
            requestStateChange(requestedStates);
        });

        deflateBtn.addEventListener('mouseup', () => {
            let requestedStates = { ...uiState, ManualDeflateBtn: false };
            requestStateChange(requestedStates);
        });
        deflateBtn.addEventListener('mouseleave', () => {
            let requestedStates = { ...uiState, ManualDeflateBtn: false };
            requestStateChange(requestedStates);
        });
        deflateBtn.addEventListener('touchend', () => {
            let requestedStates = { ...uiState, ManualDeflateBtn: false };
            requestStateChange(requestedStates);
        });

        // Poll backend periodically to keep in sync (buttons + setpoint)
        setInterval(() => {
            fetch('/hmi')
                .then(res => res.json())
                .then(data => {

                    // --- UI update ---
                    if (data.ESPButtonResponseWord !== undefined) {
                        applyBackendState(data.ESPButtonResponseWord);
                    }
                    if (data.Psp !== undefined) {
                        if (document.activeElement !== document.getElementById('PspInput')) {
                            document.getElementById('PspInput').value = data.Psp;
                        }
                        document.getElementById('PspDisplay').textContent = `Setpoint: ${data.Psp} psi`;
                    }
                    if (data.Pressure !== undefined) {
                        document.getElementById('PressureDisplay').textContent = `${data.Pressure.toFixed(2)} psi`;
                    }
                    if (data.StateMessage !== undefined) {
                        document.getElementById('StateMessage').textContent = data.StateMessage;
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
int16_t adc1 = 0;
double volts0 = 0.0;
double pressure = 0.0;
double pressure_psi = 0.0;
double readPressure()
{
    adc1 = ads.readADC_SingleEnded(1);
    if (adc1 == -1) { // ADS1115 returns -1 on error
        pressure = -1;
        return pressure;
    }
    volts0 = ads.computeVolts(adc1);
    pressure_psi = ((150.0 / 4.0) * volts0) - (150.0 / 8.0);
    pressure = pressure_psi;
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

    // Go to S0 (safe state)
    ToS0 = true;
}

// Limit inflation time to given range
double InflBounds(double x)
{
    if (x > PumpOnMaxTm)
    {
        return PumpOnMaxTm;
    }
    else if (x < PumpOnMinTm)
    {
        return PumpOnMinTm;
    }
    else
    {
        return x;
    }
}

// Limit deflation time to given range
double DeflBounds(double x)
{
    if (x > SolenoidOnMaxTm)
    {
        return SolenoidOnMaxTm;
    }
    else if (x < SolenoidOnMinTm)
    {
        return SolenoidOnMinTm;
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
    if (S0) stateCount++;
    if (S0a) stateCount++;
    if (S1a) stateCount++;
    if (S1aa) stateCount++;
    if (S1ab) stateCount++;
    if (S1ac) stateCount++;
    if (S1ad) stateCount++;
    if (S1ae) stateCount++;
    if (S1af) stateCount++;
    if (S1ag) stateCount++;
    if (S1ah) stateCount++;
    if (S1b) stateCount++;
    if (S1ba) stateCount++;
    if (S1bb) stateCount++;
    if (S1bc) stateCount++;
    if (S1bd) stateCount++;
    if (S1be) stateCount++;
    if (S1bf) stateCount++;
    if (S1bg) stateCount++;
    if (S1bh) stateCount++;
    if (S1c) stateCount++;
    if (S1ca) stateCount++;
    if (S1cb) stateCount++;
    if (S2) stateCount++;

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
        HMIRequestWord = requested;

        // Unpack word | ------------------------------------------------------------------------------------------------------
        // GoButtonActive
        GoButtonActive = HMIRequestWord & Bit00;
        // AUTO or MANUAL mode selection
        bool ReqAUTO = HMIRequestWord & Bit01;
        bool ReqMANUAL = HMIRequestWord & Bit02;
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
            ManualInflate = HMIRequestWord & Bit03;
            ManualDeflate = HMIRequestWord & Bit04;
        }
    }
}

// Incoming pressure setpoint Psp request from HMI
void handlePresSetpoint()
{
    if (server.hasArg("PspHMI"))
    {
        if (S0 || ToS0 || S0a || ToS0a || S2 || ToS2)
        {
            Psp = server.arg("PspHMI").toDouble();
        }

        if (Psp < 1)
        {
            Psp = 1;
        }
        else if (Psp > 50)
        {
            Psp = 50;
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

void setup()
{
    setCpuFrequencyMhz(240);
    Serial.begin(115200);

    Wire.begin(ADS_SDA, ADS_SCL);
    ads.begin();

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
    Pressure = readPressure();
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
        Pressure = readPressure();
        P0 = Pressure;

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
                if (P0 < Psp - PresBuffer)
                {
                    S0a = false;
                    ToS1a = true;
                }
                // Need to deflate
                else if (P0 > Psp + PresBuffer)
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

        // Time at ~start of frame
        if (ToS1a)
        {
            // Initialise state
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

        Pressure = readPressure();
        Pc1 = Pressure;

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

        // Time at ~start of frame
        if (ToS1ab)
        {
            // Initialise state
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

        // Time at ~start of frame
        if (ToS1ac)
        {
            // Initialise state
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

        Pressure = readPressure();
        Pc2 = Pressure;
        DeltaTm = 10.00;

        // Transition Onward Code
        if (Pc2 <= Psp + PresBuffer)
        { // Still under-pressure
            S1ad = false;
            ToS1ae = true;
        }
        else if (Psp - PresBuffer < Pc2 && Pc2 < Psp + PresBuffer)
        { // Calibration clutched it
            S1ad = false;
            ToS0 = true;
        }
        else if (Pc2 <= Psp - PresBuffer)
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
        InflTm = (Psp - Pc2) / m;
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
        Pressure = readPressure();
        Pc2 = Pressure;

        if (Pc2 < Psp - PresBuffer)
        { // Under-pressure
            S1ah = false;
            ToS1ae = true;
        }
        else if (Psp - PresBuffer <= Pc2 && Pc2 <= Psp + PresBuffer)
        { // Adequate pressure
            S1ah = false;
            ToS0 = true;
        }
        else if (Pc2 > Psp + PresBuffer)
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

        // Time at ~start of frame
        if (ToS1b)
        {
            // Initialise state
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

        Pressure = readPressure();
        Pc1 = Pressure;

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

        // Time at ~start of frame
        if (ToS1bb)
        {
            // Initialise state
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

        // Time at ~start of frame
        if (ToS1bc)
        {
            // Initialise state
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

        Pressure = readPressure();
        Pc2 = Pressure;
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
        DeflTm = (DeltaTm * (Psp - Pc2)) / (Pc2 - Pc1);
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
        Pressure = readPressure();
        Pc2 = Pressure;

        if (Pc2 < Psp - PresBuffer)
        { // Under-pressure
            S1bh = false;
            ToS0 = true;
        }
        else if (Psp - PresBuffer <= Pc2 && Pc2 <= Psp + PresBuffer)
        { // Adequate pressure
            S1bh = false;
            ToS0 = true;
        }
        else if (Pc2 > Psp + PresBuffer)
        { // Over-pressure
            S1bh = false;
            ToS1be = true;
        }
    }

    // S1cx | Dumb deflation | ------------------------------------------------------------------------------------------------
    // S1c | Deflate for SolenoidOnMinTm | multi-frame | -----------------------------------------------------------------------------------
    if (S1c || ToS1c)
    {
        // Escape condition
        if (!GoButtonActive || !AUTO)
        {
            Escape();
        }

        // Update state variable message
        StateMessage = "S1c | Dumb Deflation | Deflate for " + std::to_string(SolenoidOnMinTm/1000) + " seconds";

        // Initialise state
        if (ToS1c)
        {
            ToS1c = false;
            S1c = true;

            S1cSttTm = millis();
            SolenoidON = true;
        }

        // Transition onward code
        if (S1cSttTm != 0.00 && (millis() - S1cSttTm >= SolenoidOnMinTm * 1000))
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

        Pressure = readPressure();
        Pc2 = Pressure;

        // Transition Onward Code
        if (Pc2 > Psp + PresBuffer)
        { // Pressure still too high, repeat dumb deflation cycle
            S1cb = false;
            ToS1c = true;
        }
        else if (Psp - PresBuffer <= Pc2 && Pc2 <= Psp + PresBuffer)
        { // Pressure in range, end automation
            S1cb = false;
            ToS0 = true;
        }
        else if (Pc2 < Psp - PresBuffer)
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

        // Initilise state
        ToS2 = false;
        S2 = true;

        // Manual inflate
        if (ManualInflate)
        {
            PumpON = true;
        }
        else
        {
            PumpON = false;
        }

        // Manual deflate
        if (ManualDeflate)
        {
            SolenoidON = true;
        }
        else
        {
            SolenoidON = false;
        }

        // Transition Onward Code | Added for consistency, redundant as functionality is captured in escape condition
        // if (!GoButtonActive || !MANUAL){
        //     S2 = false;
        //     ToS0 = true;
        // }
    }

    handleESPButtonResponse();
    digitalWrite(InflatePin, PumpON ? HIGH : LOW);
    digitalWrite(DeflatePin, SolenoidON ? HIGH : LOW);
}
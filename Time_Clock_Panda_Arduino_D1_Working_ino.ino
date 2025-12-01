#include <ESP8266WiFi.h>  // Needed for WiFi functionality
#include <ESP8266HTTPClient.h> // Needed to perform HTTP requests (download/upload)
#include <WiFiClient.h>   // Needed for TCP/IP connections

// --- WI-FI AND ADDRESS SETTINGS (ADJUST HERE!) ---
const char* ssid = "[YOUR_WIFI_NAME]";     // YOUR Wi-Fi network name
const char* password = "[YOUR_WIFI_PASSWORD]"; // YOUR Wi-Fi password

// Static IP Arduino D1 adjust if necessary 
IPAddress local_IP(192,168,16,80);  // Fixed IP address of the ESP (Wemos D1 Mini)
IPAddress gateway(192,168,16,1);   // The IP address of your router (gateway)
IPAddress subnet(255,255,255,0);   // Standard subnet mask

// DNS for reliable name resolution
IPAddress primaryDNS(192, 168, 16, 1);  // Primary DNS (often the router)
IPAddress secondaryDNS(8, 8, 8, 8); // Secondary DNS (Google DNS)

// Panda and GIF URL
const char* pandaOTA = "http://[PANDA'S_LOCAL_IP]/ota"; // The Panda's local IP address! (Target for the GIF)
const char* gifUrl = "http://[YOUR_DOMAIN_NAME]/current_time.gif"; // The URL of the GIF on the Strato server

// Required upload header for the Panda
const char* otaTypeHeader = "standby"; // Specific header for Panda

// Interval (1 minute = 60000 milliseconds)
const unsigned long interval = 60000; // Update every 60 seconds
unsigned long previousMillis = 0; // Time of the last update

// --- FUNCTION DECLARATION ---
void uploadTimeGIF(); 

// ----------------------------------------------------
// --- SETUP & LOOP ---
// ----------------------------------------------------

void setup() {
    Serial.begin(115200); // Start Serial Communication

    WiFi.mode(WIFI_STA); // Set the ESP to Client Mode
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS); // Configure static IP
    
    WiFi.begin(ssid, password); // Connect to Wi-Fi

    Serial.print("Connecting to Wi-Fi"); // Start connection log
    while (WiFi.status() != WL_CONNECTED) { // Wait until connected
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!"); // Log success
    Serial.print("ESP IP address: "); Serial.println(WiFi.localIP()); // Show local IP
}

void loop() {
    unsigned long currentMillis = millis(); // Current runtime in milliseconds
    if(currentMillis - previousMillis >= interval || previousMillis == 0){ // Check if interval has passed
        previousMillis = currentMillis; // Reset the timer
        uploadTimeGIF(); // Execute the download/upload
    }
}

// ----------------------------------------------------
// --- MAIN FUNCTION: DOWNLOAD & UPLOAD ---
// ----------------------------------------------------

void uploadTimeGIF() {
    if(WiFi.status() != WL_CONNECTED){ // Double-check connection
        Serial.println("Wi-Fi not connected."); // Log error
        return;
    }

    // --- 1. DOWNLOAD THE GIF DATA ---
    Serial.println("--- Start: Downloading GIF..."); // Log download start
    WiFiClient clientDownload; // Client for download
    HTTPClient http;          // HTTP object for download
    
    http.begin(clientDownload, gifUrl); // Start connection to the GIF URL
    int httpCode = http.GET(); // Execute the GET request
    
    uint8_t* buf = NULL; // Buffer to store GIF data
    int gifSize = 0;     // Size of the GIF data

    if(httpCode == HTTP_CODE_OK){ // If HTTP code is 200
        // Use Content-Length to determine exact size
        int totalSize = http.getSize(); 
        
        if (totalSize <= 0) { // Check for valid size
            Serial.println("[DOWNLOAD] Error: Server did not provide a valid Content-Length.");
            http.end();
            return;
        }

        // Max size check: max 1.5MB (Panda limit)
        if (totalSize > 1536 * 1024) { 
            Serial.printf("[DOWNLOAD] Error: GIF is too large (%d bytes).", totalSize);
            http.end();
            return;
        }

        WiFiClient* stream = http.getStreamPtr(); // Stream pointer to the data
        
        // Allocate buffer for the full GIF size
        buf = new uint8_t[totalSize];
        
        // Read the exact number of bytes
        int bytesRead = stream->readBytes((char*)buf, totalSize);
        gifSize = bytesRead;

        if(gifSize != totalSize){ // Check if everything was received
            Serial.printf("[DOWNLOAD] Error: Incomplete download. Expected %d bytes, received %d bytes.\n", totalSize, gifSize);
            delete[] buf;
            http.end();
            return;
        }

        Serial.printf("[DOWNLOAD] GIF successfully downloaded (%d bytes).\n", gifSize); // Log success
    } else {
        Serial.printf("[DOWNLOAD] Error downloading GIF, HTTP code: %d\n", httpCode); // Log HTTP error
        http.end();
        return;
    }
    http.end(); // Close download connection


    // --- 2. POST TO PANDA (with Content-Type fix) ---
    Serial.println("--- Start: Uploading to Panda..."); // Log upload start
    WiFiClient clientPanda; // Client for upload
    HTTPClient panda;       // HTTP object for upload
    panda.begin(clientPanda, pandaOTA); // Start connection to the Panda IP

    panda.addHeader("ota-type", otaTypeHeader); // Add Panda specific header
    panda.addHeader("Content-Type", "image/gif"); // Force Content-Type

    int uploadCode = panda.POST(buf, gifSize); // Execute POST request with the data
    
    // Read the response and close
    Serial.printf("[UPLOAD] Sent GIF to Panda, HTTP code: %d\n", uploadCode); // Log code
    if(uploadCode == HTTP_CODE_OK){
        Serial.println("[UPLOAD] Success! Panda has updated the GIF."); // Log success
    } else {
        Serial.printf("[UPLOAD] Error uploading to Panda (Code: %d).\n", uploadCode); // Log upload error
    }
    
    panda.end(); // Close upload connection
    delete[] buf; // Release memory
    Serial.println("--- End of Update Cycle ---\n"); // Log end of cycle
}
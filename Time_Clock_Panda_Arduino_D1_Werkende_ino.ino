#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// --- WI-FI EN ADRES INSTELLINGEN (PAS HIER AAN!) ---
const char* ssid = "[JOUW_WIFI_NAAM]";     // JOUW Wi-Fi netwerknaam
const char* password = "[JOUW_WIFI_WACHTWOORD]"; // JOUW Wi-Fi wachtwoord

// Vast IP Arduino D1 (Wemos)
IPAddress local_IP(192,168,16,80);  // Vast IP-adres van de ESP (Wemos D1 Mini) je kan dit veranderen als nodig
IPAddress gateway(192,168,16,1);   // Het IP-adres van jouw router (gateway)
IPAddress subnet(255,255,255,0);   // Standaard subnetmasker

// DNS voor betrouwbare naamresolutie
IPAddress primaryDNS(192, 168, 16, 1);  // Primair DNS (vaak de router)
IPAddress secondaryDNS(8, 8, 8, 8); // Secundair DNS (Google DNS)

// Panda en GIF URL
const char* pandaOTA = "http://[PANDA'S_LOKALE_IP]/ota"; // Het lokale IP-adres van de Panda (Doel voor de GIF)
const char* gifUrl = "http://[JOUW_DOMEINNAAM]/current_time.gif"; // De URL van de GIF op de Strato server

// Vereiste upload header voor de Panda
const char* otaTypeHeader = "standby"; // Specifieke header voor Panda

// Interval (1 minuut = 60000 milliseconden)
const unsigned long interval = 60000; // Update elke 60 seconden
unsigned long previousMillis = 0; // Tijdstip van de laatste update

// --- FUNCTIE DECLARATIE ---
void uploadTimeGIF(); 

// ----------------------------------------------------
// --- SETUP & LOOP ---
// ----------------------------------------------------

void setup() {
    Serial.begin(115200); // Start Seriële Communicatie

    WiFi.mode(WIFI_STA); // Zet de ESP in Client Modus
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS); // Configureer vast IP
    
    WiFi.begin(ssid, password); // Maak verbinding met Wi-Fi

    Serial.print("Verbinden met Wi-Fi"); // Start verbindingslog
    while (WiFi.status() != WL_CONNECTED) { // Wacht tot verbinding is gemaakt
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nVerbonden!"); // Log succes
    Serial.print("ESP IP-adres: "); Serial.println(WiFi.localIP()); // Toon lokaal IP
}

void loop() {
    unsigned long currentMillis = millis(); // Huidige looptijd in milliseconden
    if(currentMillis - previousMillis >= interval || previousMillis == 0){ // Controleer of interval is verstreken
        previousMillis = currentMillis; // Reset de timer
        uploadTimeGIF(); // Voer de download/upload uit
    }
}

// ----------------------------------------------------
// --- HOOFD FUNCTIE: DOWNLOAD & UPLOAD ---
// ----------------------------------------------------

void uploadTimeGIF() {
    if(WiFi.status() != WL_CONNECTED){ // Dubbelcheck de verbinding
        Serial.println("Wi-Fi niet verbonden."); // Log fout
        return;
    }

    // --- 1. DOWNLOAD DE GIF DATA ---
    Serial.println("--- Start: Downloaden van GIF..."); // Log start download
    WiFiClient clientDownload; // Client voor download
    HTTPClient http;          // HTTP Object voor download
    
    http.begin(clientDownload, gifUrl); // Start de verbinding met de GIF URL
    int httpCode = http.GET(); // Voer de GET request uit
    
    uint8_t* buf = NULL; // Buffer om GIF data op te slaan
    int gifSize = 0;     // Grootte van de GIF data

    if(httpCode == HTTP_CODE_OK){ // Als de HTTP code 200 is
        // Gebruik de Content-Length van de server om de exacte grootte te bepalen
        int totalSize = http.getSize(); 
        
        if (totalSize <= 0) { // Controleer op geldige grootte
            Serial.println("[DOWNLOAD] Fout: Server gaf geen geldige Content-Length.");
            http.end();
            return;
        }

        // Maximale grootte check: max 1.5MB (Panda limiet)
        if (totalSize > 1536 * 1024) { 
            Serial.printf("[DOWNLOAD] Fout: GIF is te groot (%d bytes).", totalSize);
            http.end();
            return;
        }

        WiFiClient* stream = http.getStreamPtr(); // Stream pointer naar de data
        
        // Allokeer buffer voor de volledige GIF-grootte
        buf = new uint8_t[totalSize];
        
        // Lees het exacte aantal bytes
        int bytesRead = stream->readBytes((char*)buf, totalSize);
        gifSize = bytesRead;

        if(gifSize != totalSize){ // Controleer of alles is binnengekomen
            Serial.printf("[DOWNLOAD] Fout: Onvolledige download. Verwacht %d bytes, ontvangen %d bytes.\n", totalSize, gifSize);
            delete[] buf;
            http.end();
            return;
        }

        Serial.printf("[DOWNLOAD] GIF succesvol gedownload (%d bytes).\n", gifSize); // Log succes
    } else {
        Serial.printf("[DOWNLOAD] Fout bij downloaden GIF, HTTP code: %d\n", httpCode); // Log HTTP fout
        http.end();
        return;
    }
    http.end(); // Sluit de download connectie


    // --- 2. POST NAAR PANDA (met Content-Type fix) ---
    Serial.println("--- Start: Uploaden naar Panda..."); // Log start upload
    WiFiClient clientPanda; // Client voor upload
    HTTPClient panda;       // HTTP Object voor upload
    panda.begin(clientPanda, pandaOTA); // Start de verbinding met het Panda IP

    panda.addHeader("ota-type", otaTypeHeader); // Voeg Panda specifieke header toe
    panda.addHeader("Content-Type", "image/gif"); // Dwing Content-Type af

    int uploadCode = panda.POST(buf, gifSize); // Voer de POST request uit met de data
    
    // Lees het antwoord en sluit
    Serial.printf("[UPLOAD] GIF naar Panda gestuurd, HTTP code: %d\n", uploadCode); // Log code
    if(uploadCode == HTTP_CODE_OK){
        Serial.println("[UPLOAD] Succes! Panda heeft de GIF bijgewerkt."); // Log succes
    } else {
        Serial.printf("[UPLOAD] Fout bij uploaden naar Panda (Code: %d).\n", uploadCode); // Log upload fout
    }
    
    panda.end(); // Sluit de upload connectie
    delete[] buf; // Vrijgegeven van het geheugen
    Serial.println("--- Einde Update Cyclus ---\n"); // Log einde cyclus
}
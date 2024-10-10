#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
 
const char* ssid = "wifi"; // Your WiFi SSID
const char* password = "password"; // Your WiFi password
const char* serverUrlV1 = "https://vaishnavp45.github.io/update_FOTA/V1_new_001.hex"; // URL for new firmware
 
// Forward declarations
void checkAndUpdateFirmware();
void downloadHexFile(const char* url, const char* path);
size_t getFileSize(const char* path);
size_t getFirmwareSize(const char* url);
String getFirmwareName(const char* url);
void printPartitionInfo(const char* partitionPath);
 
void setup() {
    Serial.begin(115200);
 
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }
 
    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    Serial.println("Connected to WiFi");
 
    // Start the update check and process
    checkAndUpdateFirmware();
}
void checkAndUpdateFirmware() {
    // Check if partition 1 has any firmware (first update scenario)
    if (!SPIFFS.exists("/partition1/V1_new_001.hex")) { 
        // First time, just download the new firmware
        Serial.println("\nNo firmware in partition 1, downloading new firmware...\n");
        downloadHexFile(serverUrlV1, "/partition1/V1_new_001.hex"); // Store the new firmware partition-1
        downloadHexFile(serverUrlV1, "/partition2/V1_new_001.hex"); // Store the new firmware partition-2
    } else {
        // Compare new firmware name and size with existing firmware in partition 1
        String newFirmwareName = getFirmwareName(serverUrlV1);
        size_t newFirmwareSize = getFirmwareSize(serverUrlV1);
      
        size_t oldFirmwareSize = getFileSize("/partition1/V1_new_001.hex");
        String oldFirmwareName = getFirmwareName("/partition1/V1_new_001.hex"); // Get the name of the current firmware
       
        if (newFirmwareSize != oldFirmwareSize || newFirmwareName != oldFirmwareName) {
            // Firmware is different, update it
            Serial.println("\nNew firmware is different, updating partitions...\n");
 
            // Move old firmware from partition 1 to partition 2
            SPIFFS.remove("/partition2/V1_old.hex"); // Remove the old firmware if it exists
            SPIFFS.rename("/partition1/V1_new_001.hex", "/partition2/V1_old.hex"); // Move current firmware to partition 2
            // Erase partition 1 and download the new firmware
            SPIFFS.remove("/partition1/V1_new_001.hex"); // Remove old firmware in partition 1
            downloadHexFile(serverUrlV1, "/partition1/V1_new_001.hex"); // Download new firmware to partition 1
        } else {
            // Firmware is the same, no update needed
            Serial.println("\nNew firmware is the same as the old one. No update needed..\n");
        }
    }
 
    // Print both partition contents and sizes
    printPartitionInfo("/partition1/V1_new_001.hex"); // Updated firmware in partition 1
    printPartitionInfo("/partition2/V1_old.hex"); // Old firmware in partition 2
}
void downloadHexFile(const char* url, const char* path) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        File file = SPIFFS.open(path, "w");
        if (!file) {
            Serial.println("Failed to open file for writing");
            return;
        }
        http.writeToStream(&file);
        file.close();
        Serial.printf("File downloaded and saved to SPIFFS at %s\n", path);
    } else {
        Serial.printf("HTTP GET failed: %s", http.errorToString(httpCode).c_str());
    }
    http.end();
}
 
size_t getFileSize(const char* path) {
    File file = SPIFFS.open(path, "r");
    if (!file) {
        Serial.println("Failed to open file for size checking");
        return 0;
    }
    size_t fileSize = file.size();
    file.close();
    return fileSize;
}
 
size_t getFirmwareSize(const char* url) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    size_t fileSize = 0;
    if (httpCode == HTTP_CODE_OK) {
        fileSize = http.getSize();
    }
    http.end();
    return fileSize;
}
 
String getFirmwareName(const char* url) {
    String urlStr = String(url);
    int index = urlStr.lastIndexOf('/');
    return urlStr.substring(index + 1);
}
 
void printPartitionInfo(const char* partitionPath) {
    if (!SPIFFS.exists(partitionPath)) {
        Serial.printf("Partition %s does not exist.\n", partitionPath);
        return;
    }
 
    File file = SPIFFS.open(partitionPath, "r");
    if (!file) {
        Serial.printf("Failed to open partition %s for reading\n", partitionPath);
        return;
    }
 
    Serial.printf("\nContents of %s (size: %d bytes):\n", partitionPath, file.size());
    while (file.available()) {
        Serial.write(file.read()); // Read and print byte by byte
    }
    Serial.printf("\n(size: %d bytes)",file.size());
    file.close();
    Serial.printf("\nEnd of partition %s\n", partitionPath);
}
 
void loop() {
    // Nothing to do in the loop for now
}

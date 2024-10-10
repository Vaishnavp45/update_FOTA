/*This code will interrupt the trigger when wire is connected to GND but if we connect the wire to GND while 
data is printing it will not check that and also it will not trigger after finishing of printing of data*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
 
const char* ssid = "POCO C65"; // Your WiFi SSID
const char* password = "00000000"; // Your WiFi password
const char* serverUrlV1 = "https://vaishnavp45.github.io/update_FOTA/V1_new_001.hex"; // URL for new firmware
 
#define INTERRUPT_PIN_13 13 // GPIO pin for initial interrupt
#define INTERRUPT_PIN_14 14 // GPIO pin for partition swap interrupt
 
volatile bool updateFirmwareFlag = false; // Flag to indicate firmware update request
volatile bool swapPartitionFlag = false;  // Flag to indicate partition swap request
 
bool isWireConnected_13 = false; // Flag to track wire connection for GPIO 13
bool isWireConnected_14 = false; // Flag to track wire connection for GPIO 14
bool isUpdating = false;         // Flag to indicate if update is in progress
bool isPrintingData = false;     // Flag to track if data is being printed
 
// Forward declarations
void checkAndUpdateFirmware();
void swapPartitions();
void downloadHexFile(const char* url, const char* path);
size_t getFileSize(const char* path);
size_t getFirmwareSize(const char* url);
String getFirmwareName(const char* url);
void printPartitionInfo(const char* partitionPath);
 
// Debounce delay
const unsigned long debounceDelay = 10; // 10 ms debounce time
unsigned long lastDebounceTime_13 = 0;
unsigned long lastDebounceTime_14 = 0;
 
bool lastWireState_13 = HIGH; // Last stable state of GPIO 13 wire
bool lastWireState_14 = HIGH; // Last stable state of GPIO 14 wire
 
// Check GPIO 13 wire connection state
void checkWireConnection_13() {
    bool currentWireState_13 = digitalRead(INTERRUPT_PIN_13) == LOW;
    
    if (currentWireState_13 != lastWireState_13) {
        lastDebounceTime_13 = millis();
    }
 
    if ((millis() - lastDebounceTime_13) > debounceDelay) {
        if (currentWireState_13 != isWireConnected_13) {
            isWireConnected_13 = currentWireState_13;
            if (isWireConnected_13 && !isUpdating && !isPrintingData) {
                updateFirmwareFlag = true;
            }
        }
    }
    lastWireState_13 = currentWireState_13;
}
 
// Check GPIO 14 wire connection state
void checkWireConnection_14() {
    bool currentWireState_14 = digitalRead(INTERRUPT_PIN_14) == LOW;
 
    if (currentWireState_14 != lastWireState_14) {
        lastDebounceTime_14 = millis();
    }
 
    if ((millis() - lastDebounceTime_14) > debounceDelay) {
        if (currentWireState_14 != isWireConnected_14) {
            isWireConnected_14 = currentWireState_14;
            if (isWireConnected_14 && !isUpdating && !isPrintingData) {
                swapPartitionFlag = true;
            }
        }
    }
    lastWireState_14 = currentWireState_14;
}
 
void setup() {
    Serial.begin(115200);
 
    pinMode(INTERRUPT_PIN_13, INPUT_PULLUP);
    pinMode(INTERRUPT_PIN_14, INPUT_PULLUP);
 
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }
 
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    Serial.println("Connected to WiFi");
}
 
void loop() {
    checkWireConnection_13();
    checkWireConnection_14();
 
    if (updateFirmwareFlag) {
        updateFirmwareFlag = false;
        Serial.println("Interrupt triggered, updating firmware...");
        checkAndUpdateFirmware();
    }
 
    if (swapPartitionFlag) {
        swapPartitionFlag = false;
        Serial.println("Interrupt triggered, swapping partitions...");
        swapPartitions();
    }
 
    delay(50); // Reduce loop frequency
}
 
void checkAndUpdateFirmware() {
    isUpdating = true;
 
    if (!SPIFFS.exists("/partition1/V1_new_001.hex")) {
        Serial.println("\nNo firmware in partition 1, downloading new firmware...\n");
        downloadHexFile(serverUrlV1, "/partition1/V1_new_001.hex");
        downloadHexFile(serverUrlV1, "/partition2/V1_new_001.hex");
    } else {
        String newFirmwareName = getFirmwareName(serverUrlV1);
        size_t newFirmwareSize = getFirmwareSize(serverUrlV1);
        size_t oldFirmwareSize = getFileSize("/partition1/V1_new_001.hex");
        String oldFirmwareName = getFirmwareName("/partition1/V1_new_001.hex");
 
        if (newFirmwareSize != oldFirmwareSize || newFirmwareName != oldFirmwareName) {
            Serial.println("\nNew firmware is different, updating partitions...\n");
            SPIFFS.remove("/partition2/V1_old.hex");
            SPIFFS.rename("/partition1/V1_new_001.hex", "/partition2/V1_old.hex");
            SPIFFS.remove("/partition1/V1_new_001.hex");
            downloadHexFile(serverUrlV1, "/partition1/V1_new_001.hex");
        } else {
            Serial.println("\nNew firmware is the same as the old one. No update needed.\n");
            delay(4000);
        }
    }
 
    printPartitionInfo("/partition1/V1_new_001.hex");
    printPartitionInfo("/partition2/V1_old.hex");
    delay(5000);
    isUpdating = false;
}
 
void swapPartitions() {
    Serial.println("Copying firmware from partition 2 to partition 1...");
    const char* sourcePath = "/partition2/V1_old.hex";
    const char* destinationPath = "/partition1/V1_new_001.hex";
    delay(4000);

    if (SPIFFS.exists(sourcePath)) {
        File sourceFile = SPIFFS.open(sourcePath, "r");
        if (!sourceFile) {
            Serial.println("Failed to open source file for reading");
            return;
        }
 
        File destFile = SPIFFS.open(destinationPath, "w");
        if (!destFile) {
            Serial.println("Failed to open destination file for writing");
            sourceFile.close();
            return;
        }
 
        // Copy contents
        while (sourceFile.available()) {
            destFile.write(sourceFile.read());
        }
 
        sourceFile.close();
        destFile.close();
        printPartitionInfo("/partition1/V1_new_001.hex");
        printPartitionInfo("/partition2/V1_old.hex");
        Serial.println("\nFirmware copied successfully.");
    } else {
        Serial.println("Source firmware file does not exist.");
    }
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
    isPrintingData = true; // Indicate that we are in the process of printing data
    if (!SPIFFS.exists(partitionPath)) {
        Serial.printf("Partition %s does not exist.\n", partitionPath);
        isPrintingData = false; // Reset the printing flag
        return;
    }
 
    File file = SPIFFS.open(partitionPath, "r");
    if (!file) {
        Serial.printf("Failed to open partition %s for reading\n", partitionPath);
        isPrintingData = false; // Reset the printing flag
        return;
    }
 
    Serial.printf("\nContents of %s (size: %d bytes):\n", partitionPath, file.size());
    while (file.available()) {
        Serial.write(file.read()); // Read and print byte by byte
    }
    Serial.printf("\n(size: %d bytes)", file.size());
    file.close();
    Serial.printf("\nEnd of partition %s\n", partitionPath);
    isPrintingData = false; // Reset the printing flag after printing is done
}
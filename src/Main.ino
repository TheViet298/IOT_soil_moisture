#define BLYNK_TEMPLATE_ID "TMPLAnBpUb8X"
#define BLYNK_DEVICE_NAME "TUOI CAY TU DONG"
#define BLYNK_FIRMWARE_VERSION "0.1.0"
#define BLYNK_PRINT Serial
#define APP_DEBUG

#include <WiFi.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <FirebaseESP32.h>

#define MQTT_SERVER "e644ce5e89354e76856d7f082f98aff4.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "NTV_N5_IOT"
#define MQTT_PASSWORD "Viet0000@"
#define MQTT_TOPIC "tuoi_cay/soil_moisture"

// Firebase
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Cảm biến độ ẩm đất & Relay
const int soilMoisturePin = 35;
const int relayPin = 27;
unsigned long lastMillis = 0;
const int interval = 5000; 

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// Gửi dữ liệu lên Firebase (luôn gửi)
void sendToFirebase(int soilMoisture) {
    String path = "/sensor_data/soil_moisture";
    if (Firebase.setInt(firebaseData, path, soilMoisture)) {
        Serial.println(" Đã gửi dữ liệu lên Firebase!");
    } else {
        Serial.print(" Lỗi Firebase: ");
        Serial.println(firebaseData.errorReason());
    }
}

// Nhận dữ liệu từ MQTT và gửi lên Firebase
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
    Serial.print(" Nhận dữ liệu từ MQTT: ");
    Serial.println(topic);

    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print(" Độ ẩm đất từ MQTT: ");
    Serial.println(message);

    int soilMoisture = message.toInt();
    sendToFirebase(soilMoisture); // Gửi dữ liệu lên Firebase
}

// Kết nối MQTT
void connectMQTT() {
    espClient.setInsecure();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(callbackMQTT);
    mqttClient.setBufferSize(512);

    while (!mqttClient.connected()) {
        Serial.print(" Kết nối MQTT...");
        if (mqttClient.connect("ESP32_Client", MQTT_USER, MQTT_PASSWORD)) {
            Serial.println(" MQTT kết nối thành công!");
            mqttClient.subscribe(MQTT_TOPIC);
        } else {
            Serial.print(" Lỗi MQTT, mã: ");
            Serial.println(mqttClient.state());
            delay(5000);
        }
    }
}

// Kết nối WiFi
void connectWiFi() {
    WiFi.begin("TP-Link_3F46", "20596811");
    Serial.print(" Đang kết nối WiFi...");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        attempt++;
        if (attempt > 30) {  // Nếu kết nối quá 15s thì reset ESP32
            Serial.println("\n Không kết nối được WiFi! Đang reset...");
            ESP.restart();
        }
    }
    Serial.println("\n WiFi kết nối thành công!");
}

void setup() {
    Serial.begin(115200);
    delay(100);
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, HIGH); // Relay tắt khi bắt đầu

    connectWiFi();

    // Kết nối Firebase
    config.host = "nhom5-iot-9b213-default-rtdb.asia-southeast1.firebasedatabase.app";
    config.signer.tokens.legacy_token = "krTZyZmVADrnpIB3h1R4ztoRpic61RM5lc0UnS8k";
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    connectMQTT();
}

void loop() {
    mqttClient.loop();

    // Nếu mất kết nối MQTT, thử kết nối lại
    if (!mqttClient.connected()) {
        connectMQTT();
    }

    if (millis() - lastMillis > interval) {
        int soilMoisture = analogRead(soilMoisturePin);
        soilMoisture = map(soilMoisture, 0, 4095, 100, 0);  // Chuyển đổi sang %

        Serial.print("Độ ẩm đất: ");
        Serial.print(soilMoisture);
        Serial.println("%");

        // Điều khiển máy bơm
        if (soilMoisture < 30 && digitalRead(relayPin) == HIGH) {
            Serial.println("Độ ẩm thấp, bật máy bơm...");
            digitalWrite(relayPin, LOW);
        } else if (soilMoisture >= 30 && digitalRead(relayPin) == LOW) {
            Serial.println("Độ ẩm đủ, tắt máy bơm...");
            digitalWrite(relayPin, HIGH);
        }

        // Gửi dữ liệu lên MQTT
        if (mqttClient.publish(MQTT_TOPIC, String(soilMoisture).c_str())) {
            Serial.println("Đã gửi dữ liệu lên MQTT!");
        } else {
            Serial.println("Lỗi gửi MQTT!");
        }

        // Gửi Firebase ngay sau MQTT
        sendToFirebase(soilMoisture);

        lastMillis = millis();
    }
}

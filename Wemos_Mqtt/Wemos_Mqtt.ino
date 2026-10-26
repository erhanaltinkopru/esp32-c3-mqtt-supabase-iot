#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include "DHT.h"

// Gizli bilgiler aynı password.h dosyasından çekiliyor
#include "../Mqtt/password.h"

// --- WEMOS D1 MINİ (2. CİHAZ) TANIMLARI ---
// Aynı tabloya yazarken 1. cihazla karışmaması için cihaz ID'si
const char* WEMOS_DEVICE_KEY = "sensor-002"; 
const char* device_type = "climate";

// --- DHT11 SENSÖR AYARLARI (Wemos D1 Mini) ---
// Wemos D1 Mini pin eşleşmesi: D2 pini = GPIO 4'e denk gelir.
#define DHTPIN D2      
#define DHTTYPE DHT11  

DHT dht(DHTPIN, DHTTYPE);

// ESP8266 için güvenli SSL istemcisi
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Zamanlayıcı ve Değişim Takip Değişkenleri
unsigned long lastMqttMsg = 0;
unsigned long lastSupabaseMsg = 0;
bool firstBootSend = true;

// Web grafik ve raporlama için periyot: 2 dakikada bir (120.000 ms)
const unsigned long SUPABASE_HEARTBEAT_INTERVAL = 2 * 60 * 1000UL; 

// Eşik değerler (Sıcaklık 0.5°C veya Nem %2 değişirse anında gönder)
const float TEMP_THRESHOLD = 0.5;
const float HUM_THRESHOLD = 2.0;

// Hızlı dalgalanmalarda ardı ardına Supabase'e yüklenmemek için min bekleme (30 sn)
const unsigned long MIN_SUPABASE_INTERVAL = 30 * 1000UL; 

float lastSentTemp = -999.0;
float lastSentHum = -999.0;

void setup_wifi() {
  delay(100);
  Serial.println();
  Serial.print("Wi-Fi'ye baglaniyor: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    timeout++;
    if (timeout > 50) {
      Serial.println("\nBaglanti kurulamadi, kart yeniden baslatiliyor...");
      ESP.restart();
    }
  }

  Serial.println("\n[Wi-Fi BAGLANDI]");
  Serial.print("Wemos IP: ");
  Serial.println(WiFi.localIP());

  // ESP8266 RAM Tasarrufu için SSL Buffer boyutunu sınırla ve doğrulamayı atla
  espClient.setInsecure();
  espClient.setBufferSizes(1024, 1024);
}

void reconnect_mqtt() {
  if (!client.connected()) {
    Serial.print("IoTMer Bulut'a baglaniliyor (Wemos)...");
    if (client.connect(WEMOS_DEVICE_KEY, MQTT_USER, DEVICE_PASS)) {
      Serial.println("BASARIYLA BAGLANDI!");
    } else {
      Serial.print("BASARISIZ! MQTT hata kodu (rc=");
      Serial.print(client.state());
      Serial.println(") - sonraki cevrimde tekrar denenecek");
    }
  }
}

// Supabase'e Evrensel Telemetri Verisi Gönder (ESP8266 Versiyonu)
bool send_to_supabase(float temp, float hum, const char* noteText) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Supabase] HATA: Wi-Fi bagli degil!");
    return false;
  }

  // ESP8266 BearSSL istemcisi
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  secureClient.setBufferSizes(1024, 1024); // 80KB RAM'i korumak için kritik!

  HTTPClient http;
  if (!http.begin(secureClient, SUPABASE_URL)) {
    Serial.println("[Supabase] HTTP baglantisi baslatilamadi!");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Prefer", "return=minimal");

  // Evrensel JSON şemamız: device_id, device_type, data (jsonb), note
  String payload = "{";
  payload += "\"device_id\":\"" + String(WEMOS_DEVICE_KEY) + "\",";
  payload += "\"device_type\":\"" + String(device_type) + "\",";
  payload += "\"data\":{\"temp\":" + String(temp, 1) + ",\"hum\":" + String(hum, 1) + "},";
  payload += "\"note\":\"" + String(noteText) + "\"";
  payload += "}";

  Serial.print("[Supabase] Gonderiliyor -> ");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  if (httpCode >= 200 && httpCode < 300) {
    Serial.printf("[Supabase] BASARILI! (HTTP %d)\n", httpCode);
    http.end();
    return true;
  } else {
    Serial.printf("[Supabase] HATA! HTTP Kodu: %d, Yanit: %s\n", httpCode, http.getString().c_str());
    http.end();
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  dht.begin(); 
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi koptu, yeniden baglaniliyor...");
    setup_wifi();
  }

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop(); 

  unsigned long now = millis();

  // Her 5 saniyede bir sensör oku ve MQTT'ye bas
  if (now - lastMqttMsg > 5000) {
    lastMqttMsg = now;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("DHT11'den veri okunamadi!");
      return;
    }

    // --- 1. MQTT GÖNDERİMİ ---
    Serial.print("[Wemos] Sicaklik: ");
    Serial.print(t);
    Serial.print(" C  | Nem: ");
    Serial.print(h);
    Serial.println(" %");

    String mqttPayload = "{\"temperature\": " + String(t, 1) + ", \"humidity\": " + String(h, 1) + "}";
    String topic = "erhev/" + String(WEMOS_DEVICE_KEY) + "/state";
    bool gonderildi = client.publish(topic.c_str(), mqttPayload.c_str());
    if (gonderildi) {
      Serial.print("MQTT Gonderildi -> " + topic + " : ");
      Serial.println(mqttPayload);
    } else {
      Serial.println("HATA: MQTT Gonderilemedi!");
    }

    // --- 2. SUPABASE GÖNDERİM STRATEJİSİ ---
    bool shouldSendSupabase = false;
    String note = "";

    if (firstBootSend) {
      shouldSendSupabase = true;
      note = "Wemos Ilk acilis (Boot)";
    } 
    else if (now - lastSupabaseMsg >= SUPABASE_HEARTBEAT_INTERVAL) {
      shouldSendSupabase = true;
      note = "Periyodik guncelleme (2 dk)";
    } 
    else if ((now - lastSupabaseMsg >= MIN_SUPABASE_INTERVAL) &&
             (abs(t - lastSentTemp) >= TEMP_THRESHOLD || abs(h - lastSentHum) >= HUM_THRESHOLD)) {
      shouldSendSupabase = true;
      note = "Deger degisimi tetikledi";
    }

    if (shouldSendSupabase) {
      if (send_to_supabase(t, h, note.c_str())) {
        lastSentTemp = t;
        lastSentHum = h;
        lastSupabaseMsg = now;
        firstBootSend = false;
      }
    }
  }
}

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "esp_wifi.h"

// Varsayılan ayarlar ve HTML web paneli
#include "password.h"
#include "relay_webpanel.h"

// --- DONANIM PİN TANIMLARI (Waveshare ESP32-S3 Relay-6CH) ---
const int RELAY_PINS[6] = { 1, 2, 41, 42, 45, 46 }; // 6 adet izole röle
const int BUZZER_PIN    = 21;                        // Dahili pasif buzzer
const int RGB_LED_PIN   = 38;                        // Dahili WS2812 RGB LED (GPIO 38)
const int BOOT_BTN_PIN  = 0;                         // Kart üzerindeki BOOT butonu

// Röle durumları
bool relayStates[6] = { false, false, false, false, false, false };

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// --- DİNAMİK AYARLAR (NVS Flash Hafızadan Yüklenir) ---
String wifi_ssid;
String wifi_password;

String mqtt_server;
int    mqtt_port;
String mqtt_user;
String mqtt_device_key;
String mqtt_device_pass;
String base_topic;

bool   buzzer_enabled;

bool   supabase_enabled;
String supabase_url;
String supabase_anon_key;

// Durum Takip
bool inConfigMode = false;
bool pendingPublishAll = false; // Çökmeyi önleyen güvenli yayın bayrağı
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 30000;

// --- RGB LED VE SESLİ GERİ BİLDİRİM ---
void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  // Waveshare WS2812 çipinde R ve G kanallari yer degistirerek gercek renkler saglanir
  neopixelWrite(RGB_LED_PIN, g, r, b);
}

// Röle durumuna göre LED rengini otomatik ayarla
void updateStatusLed() {
  if (inConfigMode) {
    setRgbColor(255, 120, 0); // Turuncu: Ayar Modunda (Web Paneli Açık)
    return;
  }
  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
    setRgbColor(0, 0, 150); // Mavi: Bağlantı Bekleniyor
    return;
  }
  
  bool anyRelayOn = false;
  for (int i = 0; i < 6; i++) {
    if (relayStates[i]) { anyRelayOn = true; break; }
  }

  if (anyRelayOn) {
    setRgbColor(200, 0, 200); // Parlak Mor / Magenta: En az 1 röle AÇIK!
  } else {
    setRgbColor(0, 150, 0);   // Saf Yeşil: Sistem Çevrimiçi & Boşta (Hazır)
  }
}

void beep(int ms) {
  if (!buzzer_enabled) return;
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

// --- AYARLARI FLASH (NVS) HAFIZADAN YÜKLE ---
void loadSettings() {
  prefs.begin("relay_cfg", false);

  wifi_ssid        = prefs.getString("ssid", DEFAULT_WIFI_SSID);
  wifi_password    = prefs.getString("pass", DEFAULT_WIFI_PASSWORD);

  mqtt_server      = prefs.getString("mq_srv", DEFAULT_MQTT_SERVER);
  mqtt_port        = prefs.getInt("mq_port", DEFAULT_MQTT_PORT);
  mqtt_user        = prefs.getString("mq_usr", DEFAULT_MQTT_USER);
  mqtt_device_key  = prefs.getString("mq_key", DEFAULT_DEVICE_KEY);
  mqtt_device_pass = prefs.getString("mq_pass", DEFAULT_DEVICE_PASS);
  base_topic       = prefs.getString("mq_top", DEFAULT_BASE_TOPIC);

  buzzer_enabled   = prefs.getBool("bz_en", DEFAULT_BUZZER_ENABLE);

  supabase_enabled = prefs.getBool("sb_en", DEFAULT_SUPABASE_ENABLE);
  supabase_url     = prefs.getString("sb_url", DEFAULT_SUPABASE_URL);
  supabase_anon_key= prefs.getString("sb_key", DEFAULT_SUPABASE_ANON_KEY);

  prefs.end();
  Serial.println("[Ayar] Flash hafizadan ayarlar basariyla yuklendi.");
}

// --- AYARLARI FLASH (NVS) HAFIZAYA KAYDET ---
void saveSettings() {
  prefs.begin("relay_cfg", false);

  prefs.putString("ssid", wifi_ssid);
  prefs.putString("pass", wifi_password);

  prefs.putString("mq_srv", mqtt_server);
  prefs.putInt("mq_port", mqtt_port);
  prefs.putString("mq_usr", mqtt_user);
  prefs.putString("mq_key", mqtt_device_key);
  prefs.putString("mq_pass", mqtt_device_pass);
  prefs.putString("mq_top", base_topic);

  prefs.putBool("bz_en", buzzer_enabled);

  prefs.putBool("sb_en", supabase_enabled);
  prefs.putString("sb_url", supabase_url);
  prefs.putString("sb_key", supabase_anon_key);

  prefs.end();
  Serial.println("[Ayar] Yeni ayarlar Flash hafizaya kaydedildi!");
}

// --- SUPABASE OLAY KAYDI (OPSİYONEL) ---
void logToSupabase(int index, bool state) {
  if (!supabase_enabled || WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure sec;
  sec.setInsecure();
  HTTPClient http;
  if (!http.begin(sec, supabase_url)) return;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabase_anon_key);
  http.addHeader("Authorization", String("Bearer ") + supabase_anon_key);
  http.addHeader("Prefer", "return=minimal");

  char devKey[16];
  snprintf(devKey, sizeof(devKey), "relay-%03d", index + 1);

  String payload = "{";
  payload += "\"device_id\":\"" + String(devKey) + "\",";
  payload += "\"device_type\":\"relay\",";
  payload += "\"data\":{\"state\":" + String(state ? 1 : 0) + "},";
  payload += "\"note\":\"Role " + String(index + 1) + (state ? " ACILDI" : " KAPATILDI") + "\"";
  payload += "}";

  http.POST(payload);
  http.end();
}

// --- RÖLE KONTROL FONKSİYONU ---
void setRelay(int index, bool state, bool publishMqtt = false) {
  if (index < 0 || index >= 6) return;

  // Eger zaten o durumdaysa hicbir sey yapma
  if (relayStates[index] == state) return;

  relayStates[index] = state;
  digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
  updateStatusLed(); // LED rengini anında güncelle!

  char devKey[16];
  snprintf(devKey, sizeof(devKey), "relay-%03d", index + 1);
  Serial.printf("[%s] -> %s\n", devKey, state ? "ACIK (ON)" : "KAPALI (OFF)");

  // Sadece lokal basilan (fiziksel buton veya web panel) islemlerde MQTT bas
  if (publishMqtt && mqttClient.connected()) {
    String topic = base_topic + "/" + String(devKey) + "/state";
    mqttClient.publish(topic.c_str(), state ? "1" : "0", true);
  }

  // Opsiyonel Supabase geçmiş kaydı
  logToSupabase(index, state);
}

// --- TÜM RÖLELERİN DURUMUNU YAYINLA ---
void publishAllStates() {
  for (int i = 0; i < 6; i++) {
    char devKey[16];
    snprintf(devKey, sizeof(devKey), "relay-%03d", i + 1);
    String topic = base_topic + "/" + String(devKey) + "/state";
    mqttClient.publish(topic.c_str(), relayStates[i] ? "1" : "0", true);
  }
}

// --- WEB PANEL SAYFASI (relay_webpanel.h) ---
void handleRoot() {
  String html = getRelayWebPanelHtml(
    relayStates,
    wifi_ssid, wifi_password,
    mqtt_server, mqtt_port, mqtt_user,
    mqtt_device_key, mqtt_device_pass,
    base_topic, buzzer_enabled,
    supabase_enabled, supabase_url, supabase_anon_key
  );
  server.send(200, "text/html", html);
}

// --- WEB ÜZERİNDEN CANLI RÖLE TESTİ ---
void handleToggle() {
  if (server.hasArg("r")) {
    int r = server.arg("r").toInt();
    if (r >= 0 && r < 6) {
      setRelay(r, !relayStates[r], true);
      beep(40);
    }
  }
  handleRoot();
}

void handleAll() {
  if (server.hasArg("state")) {
    bool st = (server.arg("state") == "1");
    for (int i = 0; i < 6; i++) {
      setRelay(i, st, false);
    }
    updateStatusLed();
    publishAllStates();
    beep(80);
  }
  handleRoot();
}

// --- CAPTIVE PORTAL OTOMATİK YÖNLENDİRME ---
void handleRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// --- FORM KAYDET VE YENİDEN BAŞLAT ---
void handleSave() {
  if (server.hasArg("ssid")) wifi_ssid = server.arg("ssid");
  if (server.hasArg("pass")) wifi_password = server.arg("pass");

  if (server.hasArg("mq_srv")) mqtt_server = server.arg("mq_srv");
  if (server.hasArg("mq_port")) mqtt_port = server.arg("mq_port").toInt();
  if (server.hasArg("mq_usr")) mqtt_user = server.arg("mq_usr");
  if (server.hasArg("mq_key")) mqtt_device_key = server.arg("mq_key");
  if (server.hasArg("mq_pass")) mqtt_device_pass = server.arg("mq_pass");
  if (server.hasArg("mq_top")) base_topic = server.arg("mq_top");

  buzzer_enabled = server.hasArg("bz_en");

  supabase_enabled = server.hasArg("sb_en");
  if (server.hasArg("sb_url")) supabase_url = server.arg("sb_url");
  if (server.hasArg("sb_key")) supabase_anon_key = server.arg("sb_key");

  saveSettings();

  String resp = R"rawliteral(
  <!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>body{background:#090d16;color:#fff;font-family:sans-serif;text-align:center;padding:40px;}</style></head>
  <body><h2>✅ Ayarlar Başarıyla Kaydedildi!</h2><p>Cihaz yeniden başlatılıyor... Lütfen birkaç saniye sonra yeni ağ üzerinden bağlanın.</p></body></html>
  )rawliteral";
  server.send(200, "text/html", resp);

  delay(2000);
  ESP.restart();
}

// --- AP (ACCESS POINT / AYAR PORTALI) BAŞLAT ---
void startConfigPortal() {
  inConfigMode = true;
  updateStatusLed(); // Turuncu LED yansın!
  Serial.println("\n=================================");
  Serial.println("  AYAR MODU (CAPTIVE PORTAL) AKTIF ");
  Serial.println("=================================");

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  
  String apName = "Waveshare-Relay-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName.c_str());

  dnsServer.start(DNS_PORT, "*", apIP);

  Serial.print("Wi-Fi Ağı Açıldı  : ");
  Serial.println(apName);
  Serial.print("Web Panel IP       : ");
  Serial.println(WiFi.softAPIP()); // 192.168.4.1
  Serial.println("=================================");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/all", HTTP_GET, handleAll);

  // Captive Portal tespiti
  server.on("/generate_204", handleRedirect); // Android
  server.on("/fwlink", handleRedirect);       // Windows
  server.on("/hotspot-detect.html", handleRedirect); // Apple iOS

  server.onNotFound(handleRedirect);
  server.begin();

  beep(200);
}

// --- MQTT'DEN GELEN KOMUTLARI İŞLEYEN CALLBACK ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();

  String topicStr = String(topic);
  Serial.printf("[MQTT Girdi] %s -> %s\n", topic, message.c_str());

  // 1. TEKİL RÖLE KONTROLLERİ: {base_topic}/relay-001/state .. relay-006/state
  for (int i = 0; i < 6; i++) {
    char devKey[16];
    snprintf(devKey, sizeof(devKey), "relay-%03d", i + 1);
    String expectedStateTopic = base_topic + "/" + String(devKey) + "/state";

    if (topicStr == expectedStateTopic) {
      bool targetState = relayStates[i];

      if (message == "1" || message.equalsIgnoreCase("on") || message.equalsIgnoreCase("true") || 
          message.indexOf("\"state\":1") >= 0 || message.indexOf("\"state\": 1") >= 0) {
        targetState = true;
      } else if (message == "0" || message.equalsIgnoreCase("off") || message.equalsIgnoreCase("false") || 
                 message.indexOf("\"state\":0") >= 0 || message.indexOf("\"state\": 0") >= 0) {
        targetState = false;
      }

      // Sadece durum gerçekten değiştiğinde çek ve bip sesi ver!
      if (relayStates[i] != targetState) {
        setRelay(i, targetState, false);
        beep(targetState ? 40 : 20);
      }
      return;
    }
  }

  // 2. MASTER KONTROL: {base_topic}/relay-all/state
  String allTopic = base_topic + "/relay-all/state";
  if (topicStr == allTopic) {
    bool turnOn = (message.indexOf('1') >= 0 || message.equalsIgnoreCase("true") || message.equalsIgnoreCase("on"));
    Serial.printf("[MASTER KONTROL] Tum roleler %s yapiliyor...\n", turnOn ? "ACIK" : "KAPALI");
    
    // Röleleri fiziksel olarak çek
    for (int i = 0; i < 6; i++) {
      relayStates[i] = turnOn;
      digitalWrite(RELAY_PINS[i], turnOn ? HIGH : LOW);
    }
    updateStatusLed(); // LED rengini anında senkronize et!
    beep(turnOn ? 100 : 50);

    // Çökmeyi önlemek için yayını callback bittikten sonraya (loop'a) bırak
    pendingPublishAll = true;
    return;
  }
}

// --- WI-FI BAĞLANTISI ---
void setup_wifi() {
  if (wifi_ssid == "" || wifi_ssid == "***") {
    Serial.println("[Wi-Fi] Kayitli ag yok! Ayar Modu baslatiliyor...");
    startConfigPortal();
    return;
  }

  delay(100);
  Serial.println();
  Serial.print("Wi-Fi'ye baglaniyor: ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    setRgbColor(0, 0, 100); // Mavi yanıp sönsün
    delay(250);
    setRgbColor(0, 0, 0);
    delay(250);
    Serial.print(".");
    timeout++;
    if (timeout > 40) { // 20 saniyede bağlanamazsa kartı sıfırla, tekrar dene!
      setRgbColor(255, 0, 0); // Kırmızı flaş
      Serial.println("\n[Wi-Fi] Baglanti kurulamadi, kart yeniden baslatiliyor (Reset)...");
      delay(1000);
      ESP.restart();
    }
  }

  Serial.println("\n[Wi-Fi BAGLANDI]");
  Serial.print("IP Adresi : ");
  Serial.println(WiFi.localIP());
  espClient.setInsecure();
}

// --- MQTT BAĞLANTI & ABONELİK ---
void reconnect_mqtt() {
  if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("IoTMer MQTT'ye baglaniliyor...");
    String clientId = "board-" + mqtt_device_key + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_device_pass.c_str())) {
      Serial.println("BASARIYLA BAGLANDI!");

      // 6 röleye abone ol
      for (int i = 1; i <= 6; i++) {
        char devKey[16];
        snprintf(devKey, sizeof(devKey), "relay-%03d", i);
        String subTopic = base_topic + "/" + String(devKey) + "/state";
        mqttClient.subscribe(subTopic.c_str());
      }

      // Master kontrole abone ol
      String allSubTopic = base_topic + "/relay-all/state";
      mqttClient.subscribe(allSubTopic.c_str());

      Serial.println("Abone Olundu: 6 Röle + Master Konusu");
      updateStatusLed();
      beep(150);
    } else {
      Serial.printf("BASARISIZ! (rc=%d) - sonraki cevrimde tekrar denenecek\n", mqttClient.state());
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // USB CDC Seri Port Tıkanma Koruması:
  // IDE kapalıyken veya kart prizdeyken Serial.print kartı dondurmasın (Timeout = 0)
  Serial.setTxTimeoutMs(0);

  delay(1000);
  Serial.println("\n=================================");
  Serial.println(" Waveshare ESP32-S3 Relay-6CH ");
  Serial.println("=================================");

  // Pin modları ayarla
  for (int i = 0; i < 6; i++) {
    digitalWrite(RELAY_PINS[i], LOW);
    pinMode(RELAY_PINS[i], OUTPUT);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  loadSettings();
  beep(50);

  setup_wifi();

  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
}

void loop() {
  // --- 1. FİZİKSEL BOOT BUTONUNA 3 SANİYE BASILIRSA AYAR MODUNA GİR ---
  if (digitalRead(BOOT_BTN_PIN) == LOW) {
    unsigned long pressStart = millis();
    Serial.println("[BOOT Butonu] Basildi, 3 saniye bekleniyor...");
    while (digitalRead(BOOT_BTN_PIN) == LOW) {
      if (millis() - pressStart >= 3000) {
        Serial.println("-> 3 SANIYE DOLDU! Ayar Modu (AP) Baslatiliyor...");
        startConfigPortal();
        while (digitalRead(BOOT_BTN_PIN) == LOW) { delay(10); }
        break;
      }
      delay(50);
    }
  }

  // --- 2. AYAR MODUNDAYSA (WEB SERVER VE DNS ÇALIŞTIR) ---
  if (inConfigMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }

  // --- 3. NORMAL MQTT DÖNGÜSÜ ---
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long wifiLostTime = 0;
    if (wifiLostTime == 0) wifiLostTime = millis();

    // Wi-Fi koptuysa ve 30 saniye içinde geri gelmediyse sistemi sıfırla
    if (millis() - wifiLostTime > 30000) {
      Serial.println("[Wi-Fi] Baglanti uzun sure koptu, kart yeniden baslatiliyor...");
      delay(1000);
      ESP.restart();
    }
  } else {
    if (!mqttClient.connected()) {
      reconnect_mqtt();
    }
    mqttClient.loop();

    // Callback dışında güvenli bir anda durumları yayınla (Çökmeyi %100 engeller)
    if (pendingPublishAll) {
      pendingPublishAll = false;
      publishAllStates();
    }
  }

  // Periyodik kalp atışı (30 sn)
  unsigned long now = millis();
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    if (mqttClient.connected()) {
      publishAllStates();
    }
  }
}

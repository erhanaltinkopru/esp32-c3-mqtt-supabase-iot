#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include <time.h>
#include "DHT.h"

// Varsayılan fabrika ayarları ve Şık Web Paneli Arayüzü
#include "password.h"
#include "webpanel.h"

// --- PİN TANIMLARI ---
#define DHTPIN 4        // DHT11 Sensör Pini (ESP32-C3 GPIO 4)
#define DHTTYPE DHT11
#define BOOT_PIN 9      // ESP32-C3 Dahili BOOT Butonu (GPIO 9)

DHT dht(DHTPIN, DHTTYPE);
Preferences prefs;
WebServer server(80);
DNSServer dnsServer;

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// --- DİNAMİK ÇALIŞMA AYARLARI (Flash Hafızadan Çekilir) ---
String ap_name;
String wifi_ssid;
String wifi_password;
int    wifi_tx_power;   // dBm cinsinden Wi-Fi gucu (ESP32-C3 icin 15-17 onerilir)

String mqtt_server;
int    mqtt_port;
String mqtt_user;
String mqtt_device_key;
String mqtt_device_pass;
String mqtt_topic;

bool   supabase_enabled;
String supabase_url;
String supabase_anon_key;

int    relay_mode;      // 0: Kapalı, 1: Manuel, 2: Isıtma (Termostat), 3: Soğutma (Klima)
int    relay_pin;
float  target_temp;
float  hysteresis;

bool   led_enabled;     // WS2812 Bildirim Ledi Açık/Kapalı
int    led_pin;         // WS2812 GPIO Pini

int    mqtt_interval;       // saniye
int    supabase_interval;   // dakika
float  temp_threshold;
float  hum_threshold;

// --- ÇALIŞMA ZAMANLAYICILARI VE DURUMLAR ---
bool inConfigMode = false;
bool relayState = false;

unsigned long lastMqttMsg = 0;
unsigned long lastSupabaseMsg = 0;
bool firstBootSend = true;
int  supabaseFailCount = 0; // Üst üste başarısız Supabase gönderim sayacı

// Wi-Fi guc seviyesini wifi_power_t enum degerine cevir
wifi_power_t getTxPowerEnum(int dbm) {
  if (dbm <= 8)  return WIFI_POWER_8_5dBm;
  if (dbm <= 11) return WIFI_POWER_11dBm;
  if (dbm <= 13) return WIFI_POWER_13dBm;
  if (dbm <= 15) return WIFI_POWER_15dBm;
  if (dbm <= 17) return WIFI_POWER_17dBm;
  return WIFI_POWER_19_5dBm;
}
String getUptimeStr() {
  unsigned long sec = millis() / 1000;
  unsigned long min = sec / 60;
  unsigned long hr  = min / 60;
  char buf[24];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hr, min % 60, sec % 60);
  return String(buf);
}

// İnternetten (NTP) güncel saat ve Unix Epoch zamanı al
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) {
    return "--:--:--";
  }
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

unsigned long getEpochTime() {
  time_t now;
  time(&now);
  return (unsigned long)now;
}

// Wi-Fi sinyalini gerçeğe uygun hassas yüzdeye (%0-%100) çevir
int getWifiSignalPercent() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -50) return 100;
  if (rssi <= -95) return 0;
  if (rssi > -65) {
    return map(rssi, -65, -50, 75, 100);
  } else if (rssi > -75) {
    return map(rssi, -75, -65, 40, 75);
  } else if (rssi > -85) {
    return map(rssi, -85, -75, 15, 40);
  } else {
    return map(rssi, -95, -85, 1, 15);
  }
}

// ESP donanım sağlığı özeti: RAM (Kullanım/Toplam), Çip Sıcaklığı, Uptime
String getEspStatusStr() {
  uint32_t freeKb = ESP.getFreeHeap() / 1024;
  uint32_t totalKb = ESP.getHeapSize() / 1024;
  uint32_t usedPct = totalKb > 0 ? (100 - (ESP.getFreeHeap() * 100 / ESP.getHeapSize())) : 0;
  float chipTemp = temperatureRead();
  char buf[96];
  snprintf(buf, sizeof(buf), "RAM: %luKB/%luKB (%%%lu) | Cip: %.1fC | Uptime: %s",
           (unsigned long)freeKb, (unsigned long)totalKb, (unsigned long)usedPct,
           chipTemp, getUptimeStr().c_str());
  return String(buf);
}

float lastSentTemp = -999.0;
float lastSentHum = -999.0;
float currentTemp = 0.0;
float currentHum = 0.0;

// --- RGB LED (WS2812) BİLDİRİM FONKSİYONU ---
void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!led_enabled || led_pin < 0) return;
  // Waveshare / WS2812 mantığı: G, R, B
  neopixelWrite(led_pin, g, r, b);
}

void updateStatusLed() {
  if (!led_enabled || led_pin < 0) return;

  if (inConfigMode) {
    setRgbColor(255, 120, 0); // Turuncu: Ayar Modunda (Web Paneli Açık)
    return;
  }
  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
    setRgbColor(0, 0, 150); // Mavi: Bağlantı Bekleniyor
    return;
  }
  
  if (relayState) {
    setRgbColor(200, 0, 200); // Parlak Mor / Magenta: Röle AÇIK!
  } else {
    setRgbColor(0, 150, 0);   // Saf Yeşil: Sistem Çevrimiçi & Boşta (Hazır)
  }
}

// --- AYARLARI FLASH (NVS) HAFIZADAN YÜKLE ---
void loadSettings() {
  prefs.begin("iot_cfg", false);

  String defaultAp = String(DEFAULT_AP_NAME) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  ap_name          = prefs.getString("ap_nm", defaultAp);

  wifi_ssid        = prefs.getString("ssid", DEFAULT_WIFI_SSID);
  wifi_password    = prefs.getString("pass", DEFAULT_WIFI_PASSWORD);
  wifi_tx_power    = prefs.getInt("tx_pwr", DEFAULT_WIFI_POWER);

  mqtt_server      = prefs.getString("mq_srv", DEFAULT_MQTT_SERVER);
  mqtt_port        = prefs.getInt("mq_port", DEFAULT_MQTT_PORT);
  mqtt_user        = prefs.getString("mq_usr", DEFAULT_MQTT_USER);
  mqtt_device_key  = prefs.getString("mq_key", DEFAULT_DEVICE_KEY);
  mqtt_device_pass = prefs.getString("mq_pass", DEFAULT_DEVICE_PASS);
  mqtt_topic       = prefs.getString("mq_top", DEFAULT_MQTT_TOPIC);

  supabase_enabled = prefs.getBool("sb_en", DEFAULT_SUPABASE_ENABLE);
  supabase_url     = prefs.getString("sb_url", DEFAULT_SUPABASE_URL);
  supabase_anon_key= prefs.getString("sb_key", DEFAULT_SUPABASE_ANON_KEY);

  // Boşluk veya hatalı karakter temizliği
  supabase_url.trim();
  supabase_anon_key.trim();
  if (supabase_url.length() < 10) supabase_url = DEFAULT_SUPABASE_URL;
  if (supabase_anon_key.length() < 20) supabase_anon_key = DEFAULT_SUPABASE_ANON_KEY;

  relay_mode       = prefs.getInt("ry_mod", DEFAULT_RELAY_MODE);
  relay_pin        = prefs.getInt("ry_pin", DEFAULT_RELAY_PIN);
  target_temp      = prefs.getFloat("ry_tgt", DEFAULT_TARGET_TEMP);
  hysteresis       = prefs.getFloat("ry_hys", DEFAULT_HYSTERESIS);

  led_enabled      = prefs.getBool("led_en", DEFAULT_LED_ENABLE);
  led_pin          = prefs.getInt("led_pin", DEFAULT_LED_PIN);

  mqtt_interval    = prefs.getInt("int_mq", DEFAULT_MQTT_INTERVAL);
  supabase_interval= prefs.getInt("int_sb", DEFAULT_SUPABASE_INTERVAL);
  temp_threshold   = prefs.getFloat("th_tmp", DEFAULT_TEMP_THRESHOLD);
  hum_threshold    = prefs.getFloat("th_hum", DEFAULT_HUM_THRESHOLD);

  prefs.end();

  Serial.println("[Ayar] Flash hafizadan ayarlar basariyla yuklendi.");
}

// --- AYARLARI FLASH (NVS) HAFIZAYA KAYDET ---
void saveSettings() {
  prefs.begin("iot_cfg", false);

  prefs.putString("ap_nm", ap_name);

  prefs.putString("ssid", wifi_ssid);
  prefs.putString("pass", wifi_password);
  prefs.putInt("tx_pwr", wifi_tx_power);

  prefs.putString("mq_srv", mqtt_server);
  prefs.putInt("mq_port", mqtt_port);
  prefs.putString("mq_usr", mqtt_user);
  prefs.putString("mq_key", mqtt_device_key);
  prefs.putString("mq_pass", mqtt_device_pass);
  prefs.putString("mq_top", mqtt_topic);

  prefs.putBool("sb_en", supabase_enabled);
  prefs.putString("sb_url", supabase_url);
  prefs.putString("sb_key", supabase_anon_key);

  prefs.putInt("ry_mod", relay_mode);
  prefs.putInt("ry_pin", relay_pin);
  prefs.putFloat("ry_tgt", target_temp);
  prefs.putFloat("ry_hys", hysteresis);

  prefs.putBool("led_en", led_enabled);
  prefs.putInt("led_pin", led_pin);

  prefs.putInt("int_mq", mqtt_interval);
  prefs.putInt("int_sb", supabase_interval);
  prefs.putFloat("th_tmp", temp_threshold);
  prefs.putFloat("th_hum", hum_threshold);

  prefs.end();
  Serial.println("[Ayar] Yeni ayarlar Flash hafizaya kaydedildi!");
}

// --- RÖLE KONTROL FONKSİYONU ---
void setRelay(bool state) {
  if (relay_mode == 0) return; // Devre dışı
  relayState = state;
  digitalWrite(relay_pin, relayState ? HIGH : LOW);
  updateStatusLed(); // Röle durumu değişince LED rengini anında güncelle!
  Serial.printf("[Röle GPIO %d] -> %s\n", relay_pin, relayState ? "ACIK (ON)" : "KAPALI (OFF)");
}

// --- AKILLI TERMOSTAT MANTIĞI ---
void runThermostatLogic(float t, float h) {
  if (relay_mode == 0) return;

  if (relay_mode == 2) {
    if (t <= (target_temp - hysteresis) && !relayState) {
      Serial.printf("[Termostat: ISITMA] Sicaklik (%.1f C) dustu. Role ACILIYOR!\n", t);
      setRelay(true);
    } else if (t >= (target_temp + hysteresis) && relayState) {
      Serial.printf("[Termostat: ISITMA] Sicaklik (%.1f C) hedefe ulasti. Role KAPATILIYOR!\n", t);
      setRelay(false);
    }
  } else if (relay_mode == 3) {
    if (t >= (target_temp + hysteresis) && !relayState) {
      Serial.printf("[Termostat: SOGUTMA] Sicaklik (%.1f C) yukseldi. Fan/Klima ACILIYOR!\n", t);
      setRelay(true);
    } else if (t <= (target_temp - hysteresis) && relayState) {
      Serial.printf("[Termostat: SOGUTMA] Sicaklik (%.1f C) hedefe indi. Fan/Klima KAPATILIYOR!\n", t);
      setRelay(false);
    }
  }
}

// --- WEB PANEL SAYFASI (webpanel.h dosyasından çağrılır) ---
void handleRoot() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(t)) currentTemp = t;
  if (!isnan(h)) currentHum = h;

  String html = getWebPanelHtml(
    currentTemp, currentHum, relayState,
    ap_name,
    wifi_ssid, wifi_password, wifi_tx_power,
    mqtt_server, mqtt_port, mqtt_user,
    mqtt_device_key, mqtt_device_pass, mqtt_topic,
    supabase_enabled, supabase_url, supabase_anon_key,
    relay_mode, relay_pin, target_temp, hysteresis,
    led_enabled, led_pin,
    mqtt_interval, supabase_interval, temp_threshold, hum_threshold
  );

  server.send(200, "text/html", html);
}

// --- CAPTIVE PORTAL OTOMATİK YÖNLENDİRME (Android / iPhone / Windows Algılama) ---
void handleRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// --- FORM KAYDET VE YENİDEN BAŞLAT ---
void handleSave() {
  if (server.hasArg("ap_nm")) ap_name = server.arg("ap_nm");

  if (server.hasArg("ssid")) wifi_ssid = server.arg("ssid");
  if (server.hasArg("pass")) wifi_password = server.arg("pass");
  if (server.hasArg("tx_pwr")) wifi_tx_power = server.arg("tx_pwr").toInt();

  if (server.hasArg("mq_srv")) mqtt_server = server.arg("mq_srv");
  if (server.hasArg("mq_port")) mqtt_port = server.arg("mq_port").toInt();
  if (server.hasArg("mq_usr")) mqtt_user = server.arg("mq_usr");
  if (server.hasArg("mq_key")) mqtt_device_key = server.arg("mq_key");
  if (server.hasArg("mq_pass")) mqtt_device_pass = server.arg("mq_pass");
  if (server.hasArg("mq_top")) mqtt_topic = server.arg("mq_top");

  supabase_enabled = server.hasArg("sb_en");
  if (server.hasArg("sb_url")) supabase_url = server.arg("sb_url");
  if (server.hasArg("sb_key")) supabase_anon_key = server.arg("sb_key");

  if (server.hasArg("ry_mod")) relay_mode = server.arg("ry_mod").toInt();
  if (server.hasArg("ry_pin")) relay_pin = server.arg("ry_pin").toInt();
  if (server.hasArg("ry_tgt")) target_temp = server.arg("ry_tgt").toFloat();
  if (server.hasArg("ry_hys")) hysteresis = server.arg("ry_hys").toFloat();

  led_enabled = server.hasArg("led_en");
  if (server.hasArg("led_pin")) led_pin = server.arg("led_pin").toInt();

  if (server.hasArg("int_mq")) mqtt_interval = server.arg("int_mq").toInt();
  if (server.hasArg("int_sb")) supabase_interval = server.arg("int_sb").toInt();
  if (server.hasArg("th_tmp")) temp_threshold = server.arg("th_tmp").toFloat();
  if (server.hasArg("th_hum")) hum_threshold = server.arg("th_hum").toFloat();

  saveSettings();

  String resp = R"rawliteral(
  <!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>body{background:#0b1329;color:#fff;font-family:sans-serif;text-align:center;padding:40px;}</style></head>
  <body><h2>✅ Ayarlar Basariyla Kaydedildi!</h2><p>Cihaz yeniden baslatiliyor... Lutfen birkac saniye bekleyin.</p></body></html>
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
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // ESP32-C3 RF guc dusumu (brownout) ve Wi-Fi sagirligini %100 cozer!
  
  String targetAp = ap_name;
  targetAp.trim();
  if (targetAp.length() == 0) {
    targetAp = "ESP32-SmartSensor-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  }

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(targetAp.c_str());

  dnsServer.start(DNS_PORT, "*", apIP);

  Serial.print("Wi-Fi Ağı Açıldı  : ");
  Serial.println(targetAp);
  Serial.print("Web Panel IP       : ");
  Serial.println(WiFi.softAPIP()); // 192.168.4.1
  Serial.println("=================================");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);

  // Captive Portal tespiti
  server.on("/generate_204", handleRedirect); // Android
  server.on("/fwlink", handleRedirect);       // Windows
  server.on("/hotspot-detect.html", handleRedirect); // Apple iOS

  server.onNotFound(handleRedirect);
  server.begin();
}

// --- YAYIN KONUSUNDAN (mqtt_topic) CİHAZ ID VE RÖLE KONUSU ÇIKARMA ---
// Örn: mqtt_topic = "erhev/sensor-001/state" -> Cihaz ID = "sensor-001"
String extractDeviceIdFromTopic() {
  String top = mqtt_topic;
  top.trim();
  int firstSlash = top.indexOf('/');
  int lastSlash  = top.lastIndexOf('/');
  if (firstSlash >= 0 && lastSlash > firstSlash) {
    return top.substring(firstSlash + 1, lastSlash);
  }
  return mqtt_device_key; // Eğer düz bir konu yazıldıysa fallback olarak Cihaz Key kullan
}

// Örn: mqtt_topic = "erhev/sensor-001/state" -> Röle Komut Konusu = "erhev/sensor-001/cmd/relay"
String getRelayCmdTopic() {
  String top = mqtt_topic;
  top.trim();
  if (top.endsWith("/state")) {
    return top.substring(0, top.length() - 6) + "/cmd/relay";
  }
  return top + "/cmd/relay";
}

// --- SUPABASE TELEMETRİ GÖNDERİMİ ---
bool send_to_supabase(float temp, float hum, const char* noteText) {
  if (!supabase_enabled) return false;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%s] [Supabase HATA] Wi-Fi bagli degil! (Status=%d)\n", getUptimeStr().c_str(), WiFi.status());
    return false;
  }

  Serial.printf("[%s] [Supabase] Gonderim basliyor... (Bos RAM: %u byte | En Buyuk Blok: %u byte | RSSI: %d dBm)\n",
                getUptimeStr().c_str(), ESP.getFreeHeap(), ESP.getMaxAllocHeap(), WiFi.RSSI());

  // ESP32-C3 SSL RAM koruması: MQTT bağlı olsun ya da olmasın espClient soketini tamamen boşalt
  mqttClient.disconnect();
  espClient.stop();
  delay(100);

  HTTPClient http;
  http.setTimeout(8000);
  // İkinci bir WiFiClientSecure yaratmak yerine mevcut espClient nesnesini kullan!
  espClient.setInsecure();
  if (!http.begin(espClient, supabase_url)) {
    Serial.printf("[%s] [Supabase HATA] http.begin() basarisiz! URL: %s\n", getUptimeStr().c_str(), supabase_url.c_str());
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabase_anon_key);
  http.addHeader("Authorization", String("Bearer ") + supabase_anon_key);
  http.addHeader("Prefer", "return=minimal");

  String devId = extractDeviceIdFromTopic();

  String payload = "{";
  payload += "\"device_id\":\"" + devId + "\",";
  payload += "\"device_type\":\"climate\",";
  payload += "\"data\":{\"temp\":" + String(temp, 1) + ",\"hum\":" + String(hum, 1) + ",\"relay\":" + (relayState ? "1" : "0") + "},";
  payload += "\"note\":\"" + String(noteText) + "\"";
  payload += "}";

  int httpCode = http.POST(payload);
  http.end();
  espClient.stop();

  // Supabase isteği biter bitmez MQTT'yi geri bağla
  delay(100);
  reconnect_mqtt();

  if (httpCode >= 200 && httpCode < 300) {
    supabaseFailCount = 0;
    Serial.printf("[%s] [Supabase BASARILI] HTTP %d | Not: %s | Bos RAM: %u\n",
                  getUptimeStr().c_str(), httpCode, noteText, ESP.getFreeHeap());
    return true;
  } else {
    supabaseFailCount++;
    Serial.printf("[%s] [Supabase HATA #%d] HTTP: %d (%s) | Bos RAM: %u | MaxBlok: %u | RSSI: %d dBm\n",
                  getUptimeStr().c_str(), supabaseFailCount, httpCode,
                  http.errorToString(httpCode).c_str(), ESP.getFreeHeap(), ESP.getMaxAllocHeap(), WiFi.RSSI());

    // Üst üste 3 kez başarısız olursa Wi-Fi soketini/bağlantısını tazele (Auto-Heal)
    if (supabaseFailCount >= 3) {
      Serial.printf("[%s] [OTO-ONARIM] Supabase 3 kez basarisiz oldu! Wi-Fi baglantisi tazeleniyor...\n", getUptimeStr().c_str());
      supabaseFailCount = 0;
      WiFi.disconnect();
      delay(200);
      WiFi.reconnect();
    }
    return false;
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
  WiFi.setTxPower(getTxPowerEnum(wifi_tx_power)); // Secilen guc seviyesini uygula
  WiFi.setSleep(false);               // Modemin ESP32'yi uykuya (Power Save) sokmasini %100 engeller!
  WiFi.setAutoReconnect(true);
  Serial.printf("[%s] [Wi-Fi] Baglanti baslatiliyor (TX Gucu: %d dBm)...\n", getUptimeStr().c_str(), wifi_tx_power);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    // Bağlanırken BOOT butonuna basılırsa doğrudan Ayar Modunu (AP) başlat!
    if (digitalRead(BOOT_PIN) == LOW) {
      Serial.println("\n[BOOT Butonu] Wi-Fi aramasinda basildi! Ayar Modu (AP) aciliyor...");
      startConfigPortal();
      while (digitalRead(BOOT_PIN) == LOW) { delay(10); }
      return;
    }

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

  // İnternet saatini senkronize et (Türkiye: UTC+3 = 10800 saniye)
  configTime(3 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.println("[NTP] Saat senkronizasyonu baslatildi (UTC+3 Istanbul).");

  espClient.setInsecure();
  updateStatusLed();
}

// --- MQTT BAĞLANTI & ABONELİK ---
void reconnect_mqtt() {
  if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("IoTMer MQTT'ye baglaniliyor...");
    String clientId = "board-" + mqtt_device_key + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_device_pass.c_str())) {
      Serial.println("BASARIYLA BAGLANDI!");
      updateStatusLed();

      if (relay_mode == 1) {
        String relayCmdTopic = getRelayCmdTopic();
        mqttClient.subscribe(relayCmdTopic.c_str());
        Serial.print("Röle Komutuna Abone Olundu: ");
        Serial.println(relayCmdTopic);
      }
    } else {
      Serial.printf("BASARISIZ! (rc=%d) - sonraki cevrimde tekrar denenecek\n", mqttClient.state());
    }
  }
}

// --- MQTT GELEN MESAJ YAKALAMA ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();

  Serial.printf("[MQTT Mesaj] %s -> %s\n", topic, message.c_str());

  if (relay_mode == 1) {
    if (message == "1" || message.equalsIgnoreCase("ON") || message.indexOf("\"state\":1") >= 0 || message.indexOf("\"state\": 1") >= 0) {
      setRelay(true);
    } else if (message == "0" || message.equalsIgnoreCase("OFF") || message.indexOf("\"state\":0") >= 0 || message.indexOf("\"state\": 0") >= 0) {
      setRelay(false);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0); // Bilgisayar / IDE kapalıyken kart kilitlenmesin
  delay(1000);
  Serial.println("\n=================================");
  Serial.println(" ESP32-C3 SmartSensor & Relay ");
  Serial.println("=================================");

  pinMode(BOOT_PIN, INPUT_PULLUP);

  dht.begin();
  loadSettings();

  if (relay_mode > 0) {
    pinMode(relay_pin, OUTPUT);
    digitalWrite(relay_pin, LOW);
  }

  setup_wifi();

  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
}

void loop() {
  // --- 2. AYAR MODUNDAYSA SADECE WEB SERVER VE DNS ÇALIŞTIR ---
  if (inConfigMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }

  // --- 1. FİZİKSEL BOOT BUTONUNA 3 SANİYE BASILIRSA AYAR MODUNA GİR ---
  if (digitalRead(BOOT_PIN) == LOW) {
    unsigned long pressStart = millis();
    Serial.println("[BOOT Butonu] Basildi, 3 saniye bekleniyor...");
    while (digitalRead(BOOT_PIN) == LOW) {
      if (millis() - pressStart >= 3000) {
        Serial.println("-> 3 SANIYE DOLDU! Ayar Modu (AP) Baslatiliyor...");
        startConfigPortal();
        while (digitalRead(BOOT_PIN) == LOW) { delay(10); }
        return;
      }
      delay(50);
    }
  }

  // --- 3. NORMAL ÇALIŞMA DÖNGÜSÜ ---
  static unsigned long wifiLostTime = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiLostTime == 0) {
      wifiLostTime = millis();
      Serial.printf("[%s] [Wi-Fi UYARI] Baglanti koptu! Yeniden baglaniliyor...\n", getUptimeStr().c_str());
      WiFi.reconnect();
    }

    // Wi-Fi koptuysa ve 30 saniye içinde geri gelmediyse sistemi sıfırla
    if (millis() - wifiLostTime > 30000) {
      Serial.printf("[%s] [Wi-Fi KRITIK] 30 saniyedir baglanti yok, kart yeniden baslatiliyor...\n", getUptimeStr().c_str());
      delay(1000);
      ESP.restart();
    }
  } else {
    if (wifiLostTime != 0) {
      Serial.printf("[%s] [Wi-Fi KURTARILDI] Baglanti geri geldi! IP: %s\n", getUptimeStr().c_str(), WiFi.localIP().toString().c_str());
      wifiLostTime = 0; // Sayacı sıfırla!
    }
    if (!mqttClient.connected()) {
      reconnect_mqtt();
    }
    mqttClient.loop();
  }

  unsigned long now = millis();

  if (now - lastMqttMsg >= (unsigned long)(mqtt_interval * 1000)) {
    lastMqttMsg = now;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("[DHT11] Hata: Veri okunamadi!");
      return;
    }

    currentTemp = t;
    currentHum = h;

    runThermostatLogic(t, h);

    String wifiSignalStr = "%" + String(getWifiSignalPercent()) + " (" + String(WiFi.RSSI()) + " dBm)";
    String espStatusStr  = getEspStatusStr();
    String timeStr       = getFormattedTime();
    unsigned long epochTime = getEpochTime();

    // Android "IoT MQTT Panel" için tek kutuda alt alta şık özet metni
    String summaryStr = "Sicaklik: " + String(t, 1) + " C | Nem: %" + String(h, 1) + "\\n" +
                        "Role: " + (relayState ? "ACIK (ON)" : "KAPALI (OFF)") + "\\n" +
                        "Wi-Fi: " + wifiSignalStr + "\\n" +
                        espStatusStr + "\\n" +
                        "Saat: " + timeStr;

    // MQTT Yayını
    Serial.printf("[Sensör] %.1f C | %.1f %% | Röle: %s | Wi-Fi: %s | %s | Saat: %s\n", 
                  t, h, relayState ? "ON" : "OFF", wifiSignalStr.c_str(), espStatusStr.c_str(), timeStr.c_str());
    String mqttPayload = "{\"temperature\":" + String(t, 1) + 
                         ",\"humidity\":" + String(h, 1) + 
                         ",\"relay\":" + (relayState ? "1" : "0") + 
                         ",\"wifi_signal\":\"" + wifiSignalStr + "\"" +
                         ",\"esp_status\":\"" + espStatusStr + "\"" +
                         ",\"time\":\"" + timeStr + "\"" +
                         ",\"timestamp\":" + String(epochTime) + 
                         ",\"summary\":\"" + summaryStr + "\"}";

    if (mqttClient.connected()) {
      mqttClient.publish(mqtt_topic.c_str(), mqttPayload.c_str());
      Serial.printf("[MQTT] %s -> %s\n", mqtt_topic.c_str(), mqttPayload.c_str());
    }

    // Supabase Yayını
    if (supabase_enabled) {
      bool shouldSendSupabase = false;
      String note = "";
      unsigned long sbIntervalMs = (unsigned long)supabase_interval * 60 * 1000UL;

      if (firstBootSend) {
        shouldSendSupabase = true;
        note = "Ilk acilis kaydi (Boot)";
      } else if (now - lastSupabaseMsg >= sbIntervalMs) {
        shouldSendSupabase = true;
        note = "Periyodik guncelleme";
      } else if ((now - lastSupabaseMsg >= 30000) &&
                 (abs(t - lastSentTemp) >= temp_threshold || abs(h - lastSentHum) >= hum_threshold)) {
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
    } else {
      static unsigned long lastSbWarn = 0;
      if (now - lastSbWarn > 30000) {
        lastSbWarn = now;
        Serial.println("[Supabase] Devre disi! (Web panelden veya ayarlardan aciniz)");
      }
    }
  }
}

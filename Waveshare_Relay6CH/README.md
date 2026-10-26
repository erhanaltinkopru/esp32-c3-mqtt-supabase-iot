# Waveshare ESP32-S3 Relay 6CH - Proje Notları ve Yapılacaklar (TODO)

## 📋 Yapılacak Güncellemeler (TODO)

- [ ] **1. Wi-Fi Uyku Modu Koruması (`WiFi.setSleep(false)`):**
  - Uzun süre röle komutu gelmediğinde modemin ESP32-S3'ü Wi-Fi güç tasarrufu (Power Save / DTIM) uykusuna sokmasını engellemek için `setup_wifi()` fonksiyonuna `WiFi.setSleep(false);` ve `WiFi.setAutoReconnect(true);` eklenecek.

- [ ] **2. Wi-Fi Kopma Sayacı Sıfırlama (`wifiLostTime = 0`):**
  - `loop()` içindeki `if (WiFi.status() != WL_CONNECTED)` bloğunda; Wi-Fi anlık kopup 30 saniye dolmadan geri geldiğinde `else` bloğu içinde `wifiLostTime = 0;` yapılarak sayaç sıfırlanacak ve koptuğu ilk anda `WiFi.reconnect();` çağrılacak.

- [ ] **3. Tek SSL İstemcisi (`espClient`) Paylaşımı (Supabase Aktifse):**
  - Eğer web panelden Supabase olay günlüğü (`supabase_enabled`) açılırsa, `logToSupabase()` içinde ikinci bir `WiFiClientSecure sec;` oluşturmak yerine mevcut `espClient` üzerinden sıralı (senkron) gönderim yapılacak.

- [ ] **4. Hotspot Yayın Adını (SSID) Web Panelden Değiştirilebilir Yapma:**
  - Birden fazla Waveshare kartı kullanıldığında karışmaması için C3 projesinde olduğu gibi Web Paneline "Cihaz Wi-Fi Yayın Adı (Hotspot SSID)" alanı eklenecek.

- [ ] **5. Home Assistant MQTT Auto-Discovery:**
  - Cihaz MQTT'ye bağlandığında 6 röleyi (`relay-001` .. `relay-006`) otomatik olarak Home Assistant'a tanıtacak discovery paketleri eklenecek.

# ESP32 Canlı IoT İklim & Telemetri İstasyonu v1.0
### Production-Grade Dual-Cloud (MQTTS & Supabase) IoT Telemetry Station

🌐 [English](#english-version) | 🇹🇷 [Türkçe Versiyon](#türkçe-versiyon)

---

## English Version

A production-grade, highly resilient **Dual-Cloud IoT Climate & Telemetry Station (v1.0)** developed in C++ (Arduino/ESP-IDF framework) for **ESP32-C3 (RISC-V)** hardware. Engineered to stream non-blocking environmental telemetry (DHT11 Temperature & Humidity) simultaneously to a **Secure MQTT Broker (MQTTS Port 8883 / TLS)** and **Supabase PostgreSQL Cloud Database (HTTPS REST)** with dynamic Wi-Fi Captive Portal configuration, live hardware diagnostics, and deep crash-prevention architecture.

Tested, benchmarked, and verified **17+ hours continuously with 740+ telemetry cycles and 0.00% error rate**.

### 🌟 Key Features

*   **Dual Cloud Telemetry Engine:** Concurrently streams live sensor data to **IoTMer MQTTS (Port 8883 TLS)** for instant dashboard updates and **Supabase (PostgreSQL REST API)** for long-term historical storage and time-series charting.
*   **Universal SoftAP Captive Portal:** Zero hardcoded dependencies. When Wi-Fi disconnects or upon holding the physical BOOT button for 3 seconds, the ESP32 launches an autonomous DNS Captive Portal (`192.168.4.1`) with a responsive HTML5 UI for configuring Wi-Fi SSID, Passwords, TX Power, MQTT topics, and Thermostat thresholds directly in flash (NVS).
*   **Smart Single-Line MQTT Summary (`$summary`):** Specifically optimized for mobile dashboards like the Android **IoT MQTT Panel** app. Packs temperature, humidity, relay state, Wi-Fi signal percentage, raw dBm, free RAM, chip temperature, and uptime into one structured, multiline string so users do not have to clutter their screens with 20 individual widgets.
*   **Live Hardware Diagnostics:** Real-time on-chip temperature reading (`temperatureRead()`), heap allocation and fragmentation tracking (`getFreeHeap / getHeapSize`), and monotonic uptime counter.
*   **Fail-Safe Auto-Healing:** Implements non-blocking watchdog routines: if Wi-Fi drops for more than 30 seconds or Supabase encounters 3 consecutive failures, the socket automatically flushes, reconnects, or safely restarts the MCU to prevent silent field deadlocks.

### 🛠 Engineering Challenges & Production Solutions (The "Battle-Tested" Deep Dive)

1.  **SSL RAM Exhaustion & Heap Fragmentation (`HTTP: -1` Resolution):**
    *   *Problem:* The ESP32-C3 has ~400KB SRAM. Running simultaneous TLS handshakes for both MQTT (8883) and Supabase HTTPS exhausts available heap, leading to fragmented heap buffers (< 25KB largest allocable block) and subsequent socket termination (`HTTP: -1`).
    *   *Solution:* Architected a single-socket shared resource pattern. The firmware temporarily halts the MQTT socket right before executing the Supabase HTTPS POST, flushes the SSL buffers, and immediately re-establishes the lightweight MQTT session, guaranteeing continuous execution with zero memory leaks.
2.  **Modem Power-Save Disconnects (`WiFi.setSleep(false)`):**
    *   *Problem:* Certain enterprise and domestic routers (e.g., Zyxel, Huawei) enforce aggressive 802.11 DTIM sleep modes on ESP32, triggering `wifi:Association refused temporarily` and packet drops.
    *   *Solution:* Explicitly locked Wi-Fi sleep states using `WiFi.setSleep(false)` and calibrated dynamic TX power to **15 dBm** (`WIFI_POWER_15dBm`), achieving rock-solid link stability through walls.
3.  **USB CDC Serial Deadlock (`Serial.setTxTimeoutMs(0)`):**
    *   *Problem:* ESP32-C3 utilizes an internal native USB-JTAG/CDC controller. When unplugged from a PC or when the host terminal closes, standard `Serial.print` blocks indefinitely waiting for an endpoint ACK, freezing the MCU.
    *   *Solution:* Applied `Serial.setTxTimeoutMs(0)` at startup, enabling non-blocking UART logging regardless of host connection state.
4.  **Device "Freeze Doubt" Elimination (NTP Heartbeat):**
    *   *Problem:* In static environments where temperature and humidity remain constant for hours (e.g., 25.0°C / 57%), operators cannot distinguish whether the device is functioning or frozen.
    *   *Solution:* Integrated synchronized internet network time (UTC+3 Istanbul via `configTime`) publishing both human-readable `time` ("17:33:00") and Unix `timestamp` with every packet as a verifiable heartbeat.

---

## Türkçe Versiyon

**ESP32-C3 (RISC-V)** donanımı üzerinde geliştirilmiş; zorlu saha şartlarında sıfır kesintiyle çalışan, **Çift Bulutlu (Dual-Cloud: MQTTS TLS + Supabase HTTPS REST) Canlı IoT İklim & Telemetri İstasyonu v1.0**.

Bu proje; basit bir sensör okuma uygulamasının ötesinde, gömülü sistem geliştiricilerinin ESP32 ailesinde sıklıkla karşılaştığı **SSL bellek tükenmesi, modem kaynaklı uyku kopmaları, USB CDC kilitlenmeleri ve dinamik yapılandırma** problemlerini kökten çözen referans bir mühendislik mimarisidir.

### 🚀 Öne Çıkan Özellikler

*   **Çift Bulutlu Hibrit Mimari (MQTTS + Supabase REST):** Sensör verilerini milisaniyelik hızla **IoTMer Cloud MQTT (Port 8883 Şifreli TLS)** üzerinden canlı panellere basarken, eş zamanlı olarak **Supabase PostgreSQL** veritabanına periyodik ve eşik tetiklemeli (Threshold-based) olarak kaydeder.
*   **Evrensel Web Panel & Captive Portal (SoftAP):** Koda gömülü sabit şifrelere son! Wi-Fi ağı koptuğunda veya kart üzerindeki BOOT butonuna 3 saniye basıldığında ESP32 otomatik olarak bir Wi-Fi Hotspot (`ESP32-SmartSensor`) ve DNS Captive Portal başlatır. Telefon veya bilgisayardan `192.168.4.1` adresine girilerek Wi-Fi, MQTT broker, Supabase anahtarları ve termostat eşikleri flash hafızaya (NVS) kaydedilir.
*   **Android "IoT MQTT Panel" İçin Tek Satırlık Akıllı Özet (`$summary`):** Mobil panellerde her değişken için 20 ayrı kutu açıp ekranı boğmak yerine, tüm telemetriyi (Sıcaklık, Nem, Röle, Wi-Fi % / dBm, RAM, İşlemci Sıcaklığı, Uptime ve Saat) tek bir formatlı metin alanında alt alta sunar.
*   **Canlı Donanım Teşhisi:** ESP32-C3 çipinin dahili sıcaklığı (`chip_temp`), toplam ve boş heap belleği (`free_ram / total_ram`), RF sinyal kalitesi (hesaplanmış % ve ham dBm) ve kesintisiz çalışma sayacı (`uptime`).
*   **Akıllı Termostat Mantığı:** İster manuel (MQTT/Web üzerinden aç-kapa), ister histerezisli otomatik ısıtma/soğutma termostat modu.

---

### 🛠 Karşılaşılan Kronik Sorunlar ve Mühendislik Çözümleri (Endüstriyel Stabilite)

Saha testlerinde ve uzun süreli çalıştırmalarda karşılaşılan kritik problemler şu çözümlerle kalıcı olarak giderilmiştir:

#### 1. SSL RAM Çökmesi ve Heap Parçalanması (`HTTP: -1` Çözümü)
*   **Sorun:** ESP32-C3'ün sınırlı RAM'inde hem TLS MQTT (8883) soketi hem de Supabase HTTPS REST istemcisi aynı anda açık kaldığında bellek parçalanmakta (en büyük blok 25KB altına inmekte) ve `[Supabase] HATA! HTTP: -1 (connection refused)` hatasıyla sistem kilitlenmekteydi.
*   **Çözüm:** İki ayrı SSL nesnesi yerine tek bir paylaşımlı `espClient` nesnesi kullanıldı. Supabase gönderimi tetiklendiğinde MQTT bağlantısı kontrollü olarak durdurulur, SSL tamponu boşaltılıp HTTPS POST tamamlanır ve hemen ardından MQTT oturumu geri bağlanır. Sonuç: **Sıfır bellek sızıntısı ve 17+ saat kesintisiz çalışma!**

#### 2. Wi-Fi Uyku Modu (Power Save) ve Modem DTIM Kopmaları
*   **Sorun:** Özellikle Zyxel ve benzeri modern modemlerde ESP32 güç tasarrufuna geçmeye çalıştığında modem ile AP senkronizasyonu kopmakta, `Association refused temporarily` uyarısıyla cihaz ağdan düşmekteydi.
*   **Çözüm:** `WiFi.setSleep(false)` komutu ile modemin ESP32'yi uyku moduna sokması engellendi. Dinamik Wi-Fi TX çıkış gücü **15 dBm** seviyesine sabitlenerek duvar arkasından bile stabil haberleşme sağlandı.

#### 3. USB-CDC Seri Port Kilitlenmesi (Bilgisayarsız Donma Problemi)
*   **Sorun:** ESP32-C3 dahili USB-JTAG/CDC donanımına sahiptir. Kart bilgisayardan ayrıldığında veya seri monitör kapalıyken standart `Serial.print` komutları buffer boşaltma onayı (ACK) beklediği için işlemci döngüsü bloke olmaktaydı.
*   **Çözüm:** Setup bloğuna `Serial.setTxTimeoutMs(0)` eklenerek seri portun bağımsız ve tamamen non-blocking çalışması garanti altına alındı.

#### 4. İnternet Saati (NTP), Timestamp ve "Donma Şüphesini" Yıkan Heartbeat
*   **Sorun:** Oda sıcaklığı saatlerce 25.0°C seviyesinde sabit kaldığında, uzaktaki kullanıcı cihazın aktif mi çalıştığını yoksa dondu mu olduğunu anlayamamaktadır.
*   **Çözüm:** Wi-Fi bağlantısı kurulduğu an `configTime(3 * 3600, 0, "pool.ntp.org", ...)` ile Türkiye saati (UTC+3) senkronize edilir. Her MQTT paketine anlık saat (`"17:33:00"`) ve Unix `timestamp` eklenerek canlı bir sistem nabzı (heartbeat) oluşturuldu.

#### 5. Donma Önleyici Otonom Bekçi (Auto-Heal & Soft-Reset)
*   **Sorun:** Ağ altyapısındaki anlık kesintilerde sonsuz döngüde kilitlenip kalma riski.
*   **Çözüm:** Wi-Fi koptuğunda `wifiLostTime` sayacı başlar; 30 saniye içinde ağ geri gelmezse kart donanımsal reset atarak taze bağlantıyla ayağa kalkar. Supabase 3 kez üst üste başarısız olursa Wi-Fi soketi otomatik olarak tazelenir.

---

### 📊 Canlı Telemetri & MQTT JSON Formatı

Cihaz tarafından `erhev/sensor-001/state` konusuna yayınlanan optimize edilmiş JSON yükü:

```json
{
  "temperature": 25.0,
  "humidity": 57.0,
  "relay": 1,
  "wifi_signal": "%37 (-76 dBm)",
  "esp_status": "RAM: 147KB/287KB (%49) | Cip: 61.2C | Uptime: 01:37:39",
  "time": "17:33:00",
  "timestamp": 1793025180,
  "summary": "Sicaklik: 25.0 C | Nem: %57.0\nRole: ACIK (ON)\nWi-Fi: %37 (-76 dBm)\nRAM: 147KB/287KB (%49) | Cip: 61.2C | Uptime: 01:37:39\nSaat: 17:33:00"
}
```

#### Android "IoT MQTT Panel" Değişken Eşleşmesi:
*   **`$summary` :** Tek bir **Text** widget'ına atanır. Ekranı kalabalıklaştırmadan tüm sistem durumunu alt alta şık bir kartta gösterir.
*   **`$temperature` / `$humidity` :** Canlı grafik çizdirmek için kullanılır.
*   **`$relay` :** Röle anahtarına (Switch) bağlanır.

---

## 📸 Ekran Görüntüleri / Screenshots

### 1. Bulut Cihaz Durumu (Reported Device State)
*MQTT üzerinden raporlanan canlı `$summary`, donanım sağlığı ve zaman damgası verisi:*
![Reported Device State](https://raw.githubusercontent.com/erhanaltinkopru/esp32-c3-mqtt-supabase-iot/main/screenshots/device_state_reported.png)

### 2. Canlı IoT İklim & Telemetri İstasyonu Web Paneli (24 Saatlik Analiz)
*Supabase PostgreSQL REST API ve Realtime WebSocket ile çalışan 24 saatlik telemetri grafiği:*
![IoT Station Dashboard Chart](https://raw.githubusercontent.com/erhanaltinkopru/esp32-c3-mqtt-supabase-iot/main/screenshots/iot_station_dashboard_chart.png)

### 3. Gündüz/Gece Teşhis & 740+ Ölçüm Doğrulama Raporu
*Gece ve gündüz dilimleri, çiy noktası ve sıfır veri kaybı ile işlenen telemetri paketi dökümü:*
![IoT Station Full Report](https://raw.githubusercontent.com/erhanaltinkopru/esp32-c3-mqtt-supabase-iot/main/screenshots/iot_station_full_report.png)

### 4. Fiziksel Donanım ve Devre Kurulumu (Hardware Breadboard Setup)
*ESP32-C3 SuperMini, DHT11 sensör ve test masası kurulumu:*
![ESP32-C3 Hardware Setup](https://raw.githubusercontent.com/erhanaltinkopru/esp32-c3-mqtt-supabase-iot/main/screenshots/esp32_c3_hardware_breadboard.jpg)

---

## 🚀 Kurulum ve İlk Çalıştırma

1.  Depoyu klonlayın:
    ```bash
    git clone https://github.com/erhanaltinkopru/esp32-c3-mqtt-supabase-iot.git
    ```
2.  `Mqtt/password.h.example` dosyasını `Mqtt/password.h` olarak kopyalayın ve kendi Wi-Fi / MQTT / Supabase bilgilerinizi girin.
3.  Arduino IDE veya PlatformIO üzerinden kartınızı **ESP32C3 Dev Module** olarak seçip derleyin ve karta yükleyin.
4.  Cihaz ilk açıldığında `ESP32-SmartSensor` isimli bir Wi-Fi yayını yapar. Şifresiz bağlanıp açılan Web Panel üzerinden tüm ayarları dinamik olarak yapılandırabilirsiniz.

---
*Erhan Altınköprü — Otomasyon Yazılım & Saha Uzmanı*

#ifndef WEBPANEL_H
#define WEBPANEL_H

#include <Arduino.h>

// Web panelinin HTML/CSS/JS şablonunu oluşturan fonksiyon
inline String getWebPanelHtml(
  float currentTemp, float currentHum, bool relayState,
  const String& ap_name,
  const String& wifi_ssid, const String& wifi_password, int wifi_tx_power,
  const String& mqtt_server, int mqtt_port, const String& mqtt_user,
  const String& mqtt_device_key, const String& mqtt_device_pass, const String& mqtt_topic,
  bool supabase_enabled, const String& supabase_url, const String& supabase_anon_key,
  int relay_mode, int relay_pin, float target_temp, float hysteresis,
  bool led_enabled, int led_pin, int led_order,
  int mqtt_interval, int supabase_interval, float temp_threshold, float hum_threshold
) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SmartSensor IoT Panel</title>
  <style>
    :root {
      --bg: #0b1329; --card: #1e293b; --primary: #38bdf8; --text: #f8fafc;
      --muted: #94a3b8; --border: #334155; --input: #0f172a; --success: #22c55e;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 14px; display: flex; justify-content: center; }
    .container { width: 100%; max-width: 520px; }
    .header { text-align: center; margin-bottom: 16px; }
    .header h1 { font-size: 22px; color: var(--primary); display: flex; align-items: center; justify-content: center; gap: 8px; }
    .header p { color: var(--muted); font-size: 12px; margin-top: 4px; }
    
    .live-card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 12px; margin-bottom: 14px; display: flex; justify-content: space-around; text-align: center; }
    .live-val { font-size: 20px; font-weight: bold; color: var(--primary); }
    .live-lbl { font-size: 11px; color: var(--muted); text-transform: uppercase; margin-top: 2px; }

    .card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 16px; margin-bottom: 14px; }
    .card-title { font-size: 14px; font-weight: 600; color: var(--primary); margin-bottom: 12px; border-bottom: 1px solid var(--border); padding-bottom: 6px; }
    
    .form-group { margin-bottom: 12px; }
    label { display: block; font-size: 12px; color: var(--muted); margin-bottom: 4px; font-weight: 500; }
    input[type="text"], input[type="password"], input[type="number"], select {
      width: 100%; padding: 10px 12px; background: var(--input); border: 1px solid var(--border);
      border-radius: 8px; color: #fff; font-size: 14px; outline: none; transition: 0.2s;
    }
    input:focus, select:focus { border-color: var(--primary); }
    
    .row { display: flex; gap: 10px; }
    .col { flex: 1; }
    
    /* Şifre Göster/Gizle Göz Butonlu Input */
    .pass-wrap { position: relative; display: flex; align-items: center; }
    .pass-wrap input { padding-right: 40px; }
    .eye-btn {
      position: absolute; right: 10px; background: none; border: none;
      color: var(--muted); cursor: pointer; padding: 4px; font-size: 16px;
      display: flex; align-items: center; justify-content: center; outline: none;
    }
    .eye-btn:hover { color: var(--primary); }

    .toggle-wrap { display: flex; align-items: center; justify-content: space-between; padding: 6px 0; }
    .toggle-wrap label { margin-bottom: 0; font-size: 14px; color: var(--text); }
    .switch { position: relative; display: inline-block; width: 44px; height: 24px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: var(--border); transition: .3s; border-radius: 24px; }
    .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--success); }
    input:checked + .slider:before { transform: translateX(20px); }

    .btn-save { width: 100%; padding: 14px; background: #0284c7; color: white; border: none; border-radius: 10px; font-size: 15px; font-weight: bold; cursor: pointer; margin-top: 10px; box-shadow: 0 4px 12px rgba(2, 132, 199, 0.3); }
    .btn-save:hover { background: #0369a1; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>⚡ SmartSensor IoT</h1>
      <p>Cihaz Yapılandırma ve Termostat Paneli</p>
    </div>

    <div class="live-card">
      <div><div class="live-val">)rawliteral" + String(currentTemp, 1) + R"rawliteral( °C</div><div class="live-lbl">Sıcaklık</div></div>
      <div><div class="live-val">)rawliteral" + String(currentHum, 1) + R"rawliteral( %</div><div class="live-lbl">Nem</div></div>
      <div><div class="live-val" style="color:)rawliteral" + (relayState ? "#22c55e" : "#ef4444") + R"rawliteral(">)rawliteral" + (relayState ? "AÇIK" : "KAPALI") + R"rawliteral(</div><div class="live-lbl">Röle</div></div>
    </div>

    <form action="/save" method="POST">
      <!-- 0. CİHAZ YAYIN ADI (HOTSPOT) -->
      <div class="card">
        <div class="card-title">📡 Cihaz & Hotspot Ayarı</div>
        <div class="form-group">
          <label>Cihaz Wi-Fi Yayın Adı (Hotspot SSID)</label>
          <input type="text" name="ap_nm" value=")rawliteral" + ap_name + R"rawliteral(" required placeholder="Örn: Erhan-Kombi-Sensor">
          <small style="color: var(--muted); font-size: 11px;">Ayar moduna geçildiğinde bu Wi-Fi ismi yayınlanır.</small>
        </div>
      </div>

      <!-- 1. WI-FI -->
      <div class="card">
        <div class="card-title">📶 Wi-Fi Bağlantısı</div>
        <div class="form-group">
          <label>Ağ Adı (SSID)</label>
          <input type="text" name="ssid" value=")rawliteral" + wifi_ssid + R"rawliteral(" required>
        </div>
        <div class="form-group">
          <label>Wi-Fi Şifresi</label>
          <div class="pass-wrap">
            <input type="password" id="p_wifi" name="pass" value=")rawliteral" + wifi_password + R"rawliteral(">
            <button type="button" class="eye-btn" onclick="togglePass('p_wifi', this)">👁️</button>
          </div>
        </div>
        <div class="form-group">
          <label>Wi-Fi TX Anten Gücü (dBm)</label>
          <select name="tx_pwr">
            <option value="8" )rawliteral" + (wifi_tx_power == 8 ? "selected" : "") + R"rawliteral(>8.5 dBm (Çok Düşük - Sadece yan yana test için)</option>
            <option value="11" )rawliteral" + (wifi_tx_power == 11 ? "selected" : "") + R"rawliteral(>11.0 dBm (Düşük)</option>
            <option value="13" )rawliteral" + (wifi_tx_power == 13 ? "selected" : "") + R"rawliteral(>13.0 dBm (Orta)</option>
            <option value="15" )rawliteral" + (wifi_tx_power == 15 ? "selected" : "") + R"rawliteral(>15.0 dBm (Tavsiye Edilen - Dengeli)</option>
            <option value="17" )rawliteral" + (wifi_tx_power == 17 ? "selected" : "") + R"rawliteral(>17.0 dBm (Yüksek - Uzak Modemler İçin)</option>
            <option value="19" )rawliteral" + (wifi_tx_power >= 19 ? "selected" : "") + R"rawliteral(>19.5 dBm (Maksimum - C3'te LDO ısınması yapabilir)</option>
          </select>
          <small style="color: var(--muted); font-size: 11px; line-height: 1.4; display: block; margin-top: 4px;">
            ℹ️ <b>ESP32-C3 İçin Öneri:</b> 15.0 dBm veya 17.0 dBm seçiniz.<br>
            8.5 dBm modem uzaktayken sinyali öldürür (-77 dBm); 19.5 dBm ise minik C3 kartlarda RF boğulması ve voltaj çökmesi yapabilir.
          </small>
        </div>
      </div>

      <!-- 2. MQTT -->
      <div class="card">
        <div class="card-title">☁️ IoT (MQTT) Sunucu Ayarları</div>
        <div class="row">
          <div class="col form-group">
            <label>Broker Host</label>
            <input type="text" name="mq_srv" value=")rawliteral" + mqtt_server + R"rawliteral(" required>
          </div>
          <div class="col form-group" style="max-width: 100px;">
            <label>Port</label>
            <input type="number" name="mq_port" value=")rawliteral" + String(mqtt_port) + R"rawliteral(" required>
          </div>
        </div>
        <div class="row">
          <div class="col form-group">
            <label>Kullanıcı Adı</label>
            <input type="text" name="mq_usr" value=")rawliteral" + mqtt_user + R"rawliteral(">
          </div>
          <div class="col form-group">
            <label>Cihaz Key (Client ID)</label>
            <input type="text" name="mq_key" value=")rawliteral" + mqtt_device_key + R"rawliteral(" required>
          </div>
        </div>
        <div class="form-group">
          <label>Cihaz Şifresi</label>
          <div class="pass-wrap">
            <input type="password" id="p_mqtt" name="mq_pass" value=")rawliteral" + mqtt_device_pass + R"rawliteral(">
            <button type="button" class="eye-btn" onclick="togglePass('p_mqtt', this)">👁️</button>
          </div>
        </div>
        <div class="form-group">
          <label>Yayın Konusu (Publish Topic)</label>
          <input type="text" name="mq_top" value=")rawliteral" + mqtt_topic + R"rawliteral(" placeholder="erhev/sensor-001/state" required>
          <p style="font-size:11px; color:var(--muted); margin-top:4px; line-height:1.4;">
            📤 Sensör verisi doğrudan bu konuya basılır (Örn: <b>erhev/sensor-001/state</b>)<br>
            📥 Manuel röle komutu bu konuya göre otomatik dinlenir: <b>erhev/sensor-001/cmd/relay</b> (Payload: <b>1</b> / <b>0</b>)<br>
            ℹ️ <b>Cihaz Key (Client ID)</b> tamamen bağımsızdır; sadece MQTT sunucusuna bağlanırken kullanılan kimliktir.
          </p>
        </div>
      </div>

      <!-- 3. SUPABASE (OPSİYONEL CHECKBOX) -->
      <div class="card">
        <div class="card-title">🗄️ Supabase Bulut Veritabanı</div>
        <div class="toggle-wrap form-group">
          <label>Supabase Entegrasyonunu Etkinleştir</label>
          <label class="switch">
            <input type="checkbox" name="sb_en" )rawliteral" + (supabase_enabled ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
        <div class="form-group">
          <label>Supabase Tablo URL'si</label>
          <input type="text" name="sb_url" value=")rawliteral" + supabase_url + R"rawliteral(">
        </div>
        <div class="form-group">
          <label>Anon Public Key</label>
          <div class="pass-wrap">
            <input type="password" id="p_sbkey" name="sb_key" value=")rawliteral" + supabase_anon_key + R"rawliteral(">
            <button type="button" class="eye-btn" onclick="togglePass('p_sbkey', this)">👁️</button>
          </div>
        </div>
      </div>

      <!-- 4. AKILLI TERMOSTAT & RÖLE -->
      <div class="card">
        <div class="card-title">🔥 Akıllı Termostat & Röle Modu</div>
        <div class="form-group">
          <label>Çalışma Modu</label>
          <select name="ry_mod">
            <option value="0" )rawliteral" + (relay_mode == 0 ? "selected" : "") + R"rawliteral(>0 - Devre Dışı (Sadece Sensör)</option>
            <option value="1" )rawliteral" + (relay_mode == 1 ? "selected" : "") + R"rawliteral(>1 - Manuel Mod (MQTT / Web Kontrollü)</option>
            <option value="2" )rawliteral" + (relay_mode == 2 ? "selected" : "") + R"rawliteral(>2 - Otomatik Termostat (Isıtıcı/Kombi Modu)</option>
            <option value="3" )rawliteral" + (relay_mode == 3 ? "selected" : "") + R"rawliteral(>3 - Otomatik Soğutma (Fan/Klima Modu)</option>
          </select>
        </div>
        <div class="row">
          <div class="col form-group">
            <label>Röle GPIO Pini</label>
            <input type="number" name="ry_pin" value=")rawliteral" + String(relay_pin) + R"rawliteral(">
          </div>
          <div class="col form-group">
            <label>Hedef Sıcaklık (°C)</label>
            <input type="number" step="0.1" name="ry_tgt" value=")rawliteral" + String(target_temp, 1) + R"rawliteral(">
          </div>
          <div class="col form-group">
            <label>Tolerans (°C)</label>
            <input type="number" step="0.1" name="ry_hys" value=")rawliteral" + String(hysteresis, 1) + R"rawliteral(">
          </div>
        </div>
      </div>

      <!-- 5. WS2812 BİLDİRİM LEDİ -->
      <div class="card">
        <div class="card-title">💡 WS2812 RGB Durum Bildirim Ledi</div>
        <div class="toggle-wrap form-group">
          <label>Bildirim Ledini Etkinleştir</label>
          <label class="switch">
            <input type="checkbox" name="led_en" )rawliteral" + (led_enabled ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
        <div class="form-group">
          <label>WS2812 DIN Pini (GPIO)</label>
          <input type="number" name="led_pin" value=")rawliteral" + String(led_pin) + R"rawliteral(">
        </div>
        <div class="form-group">
          <label>LED Renk Sırası (Çip / Şerit Tipi)</label>
          <select name="led_ord">
            <option value="0" )rawliteral" + (led_order == 0 ? "selected" : "") + R"rawliteral(>GRB (Çoğu WS2812B Şerit LED - Varsayılan)</option>
            <option value="1" )rawliteral" + (led_order == 1 ? "selected" : "") + R"rawliteral(>RGB (WS2811 / Standart RGB Şerit)</option>
            <option value="2" )rawliteral" + (led_order == 2 ? "selected" : "") + R"rawliteral(>BRG (Bazı 12V WS2811 Şeritler)</option>
            <option value="3" )rawliteral" + (led_order == 3 ? "selected" : "") + R"rawliteral(>BGR</option>
          </select>
          <small style="color: var(--muted); font-size: 11px;">Renkler yanlış yanıyorsa (ör. yeşil yerine turkuaz/mavi) buradan şeridinize uygun çip sırasını seçiniz.</small>
        </div>
      </div>

      <!-- 6. ZAMANLAMA VE EŞİKLER -->
      <div class="card">
        <div class="card-title">⏱️ Zamanlama ve Eşik Ayarları</div>
        <div class="row">
          <div class="col form-group">
            <label>MQTT Yayın (sn)</label>
            <input type="number" name="int_mq" value=")rawliteral" + String(mqtt_interval) + R"rawliteral(" required>
          </div>
          <div class="col form-group">
            <label>Supabase Kayıt (dk)</label>
            <input type="number" name="int_sb" value=")rawliteral" + String(supabase_interval) + R"rawliteral(" required>
          </div>
        </div>
        <div class="row">
          <div class="col form-group">
            <label>Sıcaklık Fark Eşiği (°C)</label>
            <input type="number" step="0.1" name="th_tmp" value=")rawliteral" + String(temp_threshold, 1) + R"rawliteral(" required>
          </div>
          <div class="col form-group">
            <label>Nem Fark Eşiği (%)</label>
            <input type="number" step="0.1" name="th_hum" value=")rawliteral" + String(hum_threshold, 1) + R"rawliteral(" required>
          </div>
        </div>
      </div>

      <button type="submit" class="btn-save">💾 Ayarları Kaydet ve Başlat</button>
    </form>
    <div style="text-align: center; margin-top: 15px; color: var(--muted); font-size: 11px;">
      ESP32-C3 Smart IoT Controller &copy; 2026
    </div>
  </div>

  <script>
    function togglePass(id, btn) {
      var input = document.getElementById(id);
      if (input.type === "password") {
        input.type = "text";
        btn.textContent = "🔒";
      } else {
        input.type = "password";
        btn.textContent = "👁️";
      }
    }
  </script>
</body>
</html>
)rawliteral";

  return html;
}

#endif // WEBPANEL_H

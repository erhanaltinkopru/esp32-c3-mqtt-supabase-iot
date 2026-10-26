#ifndef RELAY_WEBPANEL_H
#define RELAY_WEBPANEL_H

#include <Arduino.h>

inline String getRelayWebPanelHtml(
  const bool relayStates[6],
  const String& wifi_ssid, const String& wifi_password,
  const String& mqtt_server, int mqtt_port, const String& mqtt_user,
  const String& mqtt_device_key, const String& mqtt_device_pass,
  const String& base_topic, bool buzzer_enabled,
  bool supabase_enabled, const String& supabase_url, const String& supabase_anon_key
) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Waveshare 6-CH Relay Panel</title>
  <style>
    :root {
      --bg: #090d16; --card: #151d30; --primary: #38bdf8; --text: #f8fafc;
      --muted: #94a3b8; --border: #243049; --input: #0b1120; --success: #22c55e; --danger: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 14px; display: flex; justify-content: center; }
    .container { width: 100%; max-width: 520px; }
    .header { text-align: center; margin-bottom: 16px; }
    .header h1 { font-size: 20px; color: var(--primary); display: flex; align-items: center; justify-content: center; gap: 8px; }
    .header p { color: var(--muted); font-size: 12px; margin-top: 3px; }
    
    .card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 16px; margin-bottom: 14px; }
    .card-title { font-size: 14px; font-weight: 600; color: var(--primary); margin-bottom: 12px; border-bottom: 1px solid var(--border); padding-bottom: 6px; }

    /* Canlı Röle Kontrol Butonları Grid */
    .relay-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 10px; }
    .relay-btn {
      padding: 12px 8px; border-radius: 8px; border: 1px solid var(--border); font-size: 13px; font-weight: bold;
      text-align: center; cursor: pointer; transition: 0.2s; text-decoration: none; display: block;
    }
    .r-on { background: rgba(34, 197, 94, 0.15); border-color: var(--success); color: var(--success); }
    .r-off { background: var(--input); color: var(--muted); }
    .r-off:hover { border-color: var(--muted); color: #fff; }

    .master-btn-row { display: flex; gap: 10px; margin-top: 6px; }
    .btn-m { flex: 1; padding: 8px; border-radius: 6px; font-size: 12px; font-weight: 600; text-align: center; cursor: pointer; text-decoration: none; border: 1px solid var(--border); }
    .btn-m-on { background: rgba(34, 197, 94, 0.2); color: var(--success); border-color: var(--success); }
    .btn-m-off { background: rgba(239, 68, 68, 0.2); color: var(--danger); border-color: var(--danger); }

    .form-group { margin-bottom: 12px; }
    label { display: block; font-size: 12px; color: var(--muted); margin-bottom: 4px; font-weight: 500; }
    input[type="text"], input[type="password"], input[type="number"] {
      width: 100%; padding: 10px 12px; background: var(--input); border: 1px solid var(--border);
      border-radius: 8px; color: #fff; font-size: 14px; outline: none; transition: 0.2s;
    }
    input:focus { border-color: var(--primary); }
    
    .row { display: flex; gap: 10px; }
    .col { flex: 1; }

    .pass-wrap { position: relative; display: flex; align-items: center; }
    .pass-wrap input { padding-right: 40px; }
    .eye-btn {
      position: absolute; right: 10px; background: none; border: none;
      color: var(--muted); cursor: pointer; padding: 4px; font-size: 16px;
      display: flex; align-items: center; justify-content: center; outline: none;
    }
    .eye-btn:hover { color: var(--primary); }

    .toggle-wrap { display: flex; align-items: center; justify-content: space-between; padding: 6px 0; }
    .toggle-wrap label { margin-bottom: 0; font-size: 13px; color: var(--text); }
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
      <h1>🔌 Waveshare ESP32-S3</h1>
      <p>6-Kanal Endüstriyel Röle Kontrol ve Ayar Paneli</p>
    </div>

    <!-- CANLI RÖLE TEST BUTONLARI -->
    <div class="card">
      <div class="card-title">⚡ Canlı Röle Testi</div>
      <div class="relay-grid">
)rawliteral";

  for (int i = 0; i < 6; i++) {
    bool state = relayStates[i];
    html += "<a href=\"/toggle?r=" + String(i) + "\" class=\"relay-btn " + (state ? "r-on" : "r-off") + "\">";
    html += "Röle " + String(i + 1) + "<br><span style=\"font-size:11px;\">" + (state ? "AÇIK" : "KAPALI") + "</span></a>";
  }

  html += R"rawliteral(
      </div>
      <div class="master-btn-row">
        <a href="/all?state=1" class="btn-m btn-m-on">Tümünü Aç</a>
        <a href="/all?state=0" class="btn-m btn-m-off">Tümünü Kapat</a>
      </div>
    </div>

    <form action="/save" method="POST">
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
      </div>

      <!-- 2. MQTT SUNUCU VE CİHAZ AYARLARI -->
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
            <label>Ana Cihaz Key</label>
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
          <label>Temel Konu Öneki (Base Topic Prefix)</label>
          <input type="text" name="mq_top" value=")rawliteral" + base_topic + R"rawliteral(" placeholder="erhev" required>
          <p style="font-size:11px; color:var(--muted); margin-top:3px;">Röleler bu önekle dinlenir: <b>{Önek}/relay-001/state</b> vb.</p>
        </div>
        <div class="toggle-wrap form-group" style="border-top:1px solid var(--border); margin-top:8px; padding-top:10px;">
          <label>Dahili Buzzer Sesli Geri Bildirim</label>
          <label class="switch">
            <input type="checkbox" name="bz_en" )rawliteral" + (buzzer_enabled ? "checked" : "") + R"rawliteral(>
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <!-- 3. SUPABASE (OPSİYONEL KAYIT) -->
      <div class="card">
        <div class="card-title">🗄️ Supabase Olay Günlüğü (Opsiyonel)</div>
        <div class="toggle-wrap form-group">
          <label>Röle Geçmişini Supabase'e Kaydet</label>
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

      <button type="submit" class="btn-save">💾 Ayarları Kaydet ve Başlat</button>
    </form>
    <div style="text-align: center; margin-top: 15px; color: var(--muted); font-size: 11px;">
      Waveshare ESP32-S3 Relay-6CH Controller &copy; 2026
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

#endif // RELAY_WEBPANEL_H

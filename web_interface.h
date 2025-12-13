// ============================================================================
// WEB INTERFACE - Phase 2
// Adds http://clock.local configuration interface
// 
// Include this file in main.cpp and call setup_web_server() in setup()
// ============================================================================

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>

// External references to config and functions from main.cpp
extern ClockConfig config;
extern void save_config();
extern void load_config();
extern LilyGo_Class amoled;
extern float battery_voltage;
extern bool is_charging();
extern int sunrise_time, sunset_time;

WebServer web_server(80);

// ============================================================================
// HTML PAGE - Embedded (compressed with gzip would be better, but keeping simple)
// ============================================================================

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Flip Clock Config</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0a0a0a;color:#e0e0e0;padding:20px}
.container{max-width:800px;margin:0 auto}
h1{color:#ff3333;margin-bottom:10px;font-size:28px}
h2{color:#ff6666;margin:25px 0 12px;font-size:18px;border-bottom:1px solid #333;padding-bottom:6px}
.subtitle{color:#888;margin-bottom:25px;font-size:14px}
.status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:25px}
.status-card{background:#1a1a1a;border:1px solid #333;border-radius:6px;padding:12px}
.status-label{color:#888;font-size:11px;margin-bottom:4px}
.status-value{color:#fff;font-size:18px;font-weight:600}
.form-group{margin-bottom:16px}
label{display:block;color:#aaa;margin-bottom:4px;font-size:13px}
input[type="text"],input[type="password"],input[type="number"],select{width:100%;padding:8px;background:#1a1a1a;border:1px solid #333;border-radius:4px;color:#fff;font-size:13px}
input[type="range"]{width:100%;height:5px;background:#333;border-radius:3px}
input[type="checkbox"]{width:18px;height:18px;margin-right:8px;vertical-align:middle}
.slider-val{display:inline-block;background:#ff3333;color:#fff;padding:2px 6px;border-radius:3px;font-size:11px;margin-left:8px;min-width:35px;text-align:center}
button{background:#ff3333;color:#fff;border:none;padding:10px 20px;border-radius:4px;font-size:14px;cursor:pointer;margin-right:8px;margin-bottom:8px}
button:hover{background:#ff5555}
button.sec{background:#333}
button.sec:hover{background:#444}
.color-grid{display:grid;grid-template-columns:repeat(5,1fr);gap:8px;margin-top:8px}
.color-opt{width:100%;height:40px;border:2px solid #333;border-radius:4px;cursor:pointer}
.color-opt.sel{border-color:#ff3333;border-width:3px}
.msg{margin-top:15px;padding:10px;border-radius:4px;font-size:13px}
.msg.ok{background:#1b5e20;color:#4caf50}
.msg.err{background:#b71c1c;color:#f44336}
small{color:#666;font-size:11px}
@media(max-width:600px){body{padding:10px}h1{font-size:22px}.status-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="container">
<h1>⏰ Flip Clock</h1>
<p class="subtitle">Configuration Interface • Phase 2</p>

<h2>📊 Status</h2>
<div class="status-grid">
  <div class="status-card"><div class="status-label">Time</div><div class="status-value" id="time">--:--</div></div>
  <div class="status-card"><div class="status-label">Battery</div><div class="status-value" id="batt">--%</div></div>
  <div class="status-card"><div class="status-label">WiFi</div><div class="status-value" id="wifi">--</div></div>
  <div class="status-card"><div class="status-label">Uptime</div><div class="status-value" id="up">--</div></div>
</div>

<h2>🎨 Display</h2>
<div class="form-group">
  <label><input type="checkbox" id="show_sec"> Show seconds</label>
</div>
<div class="form-group">
  <label><input type="checkbox" id="show_date"> Show date</label>
</div>
<div class="form-group">
  <label>Color Scheme</label>
  <div class="color-grid">
    <div class="color-opt" style="background:#ff3333" data-c="0"></div>
    <div class="color-opt" style="background:#33ff33" data-c="1"></div>
    <div class="color-opt" style="background:#3333ff" data-c="2"></div>
    <div class="color-opt" style="background:#ffffff" data-c="3"></div>
    <div class="color-opt" style="background:#ffaa00" data-c="4"></div>
  </div>
</div>

<h2>💡 Brightness</h2>
<div class="form-group">
  <label><input type="checkbox" id="auto_br"> Auto-brightness (sunrise/sunset)</label>
</div>
<div class="form-group">
  <label>Day <span class="slider-val" id="day_v">200</span></label>
  <input type="range" id="day_br" min="50" max="255" value="200">
</div>
<div class="form-group">
  <label>Night <span class="slider-val" id="night_v">40</span></label>
  <input type="range" id="night_br" min="10" max="150" value="40">
</div>
<div class="form-group">
  <label>Transition (minutes) <span class="slider-val" id="trans_v">30</span></label>
  <input type="range" id="trans" min="5" max="60" value="30">
</div>

<h2>🌍 Location</h2>
<div class="form-group">
  <label>Latitude</label>
  <input type="number" id="lat" step="0.0001" value="39.7555">
  <small>Current sunrise: <span id="rise">--:--</span>, sunset: <span id="set">--:--</span></small>
</div>
<div class="form-group">
  <label>Longitude</label>
  <input type="number" id="lon" step="0.0001" value="-119.8135">
</div>
<div class="form-group">
  <label>Timezone</label>
  <input type="text" id="tz" value="PST8PDT,M3.2.0/2,M11.1.0/2">
</div>

<h2>📡 WiFi</h2>
<div class="form-group">
  <label>SSID</label>
  <input type="text" id="ssid">
</div>
<div class="form-group">
  <label>Password</label>
  <input type="password" id="pass">
  <small>Leave blank to keep current password</small>
</div>

<h2>🌤️ Weather (Optional)</h2>
<div class="form-group">
  <label><input type="checkbox" id="wthr_en"> Enable weather</label>
</div>
<div class="form-group">
  <label>OpenWeatherMap API Key</label>
  <input type="text" id="wthr_key" placeholder="Get free key at openweathermap.org">
  <small>Free: 60 calls/min, 1000/day</small>
</div>

<h2>⚙️ Actions</h2>
<button onclick="save()">💾 Save Config</button>
<button class="sec" onclick="restart()">🔄 Restart</button>
<button class="sec" onclick="exportCfg()">📥 Export</button>
<div id="msg"></div>

</div>

<script>
setInterval(updateStatus,2000);
updateStatus();
loadCfg();

document.getElementById('day_br').oninput=function(){document.getElementById('day_v').textContent=this.value};
document.getElementById('night_br').oninput=function(){document.getElementById('night_v').textContent=this.value};
document.getElementById('trans').oninput=function(){document.getElementById('trans_v').textContent=this.value};

document.querySelectorAll('.color-opt').forEach(e=>{
  e.onclick=function(){
    document.querySelectorAll('.color-opt').forEach(x=>x.classList.remove('sel'));
    this.classList.add('sel');
  };
});

async function updateStatus(){
  try{
    const r=await fetch('/api/status');
    const d=await r.json();
    document.getElementById('time').textContent=d.time;
    document.getElementById('batt').textContent=d.batt+(d.chrg?' ⚡':'');
    document.getElementById('wifi').textContent=d.rssi+' dBm';
    document.getElementById('up').textContent=d.up;
    document.getElementById('rise').textContent=d.rise;
    document.getElementById('set').textContent=d.set;
  }catch(e){}
}

async function loadCfg(){
  try{
    const r=await fetch('/api/config');
    const c=await r.json();
    document.getElementById('show_sec').checked=c.show_sec;
    document.getElementById('show_date').checked=c.show_date;
    document.getElementById('auto_br').checked=c.auto_br;
    document.getElementById('day_br').value=c.day_br;
    document.getElementById('night_br').value=c.night_br;
    document.getElementById('trans').value=c.trans;
    document.getElementById('day_v').textContent=c.day_br;
    document.getElementById('night_v').textContent=c.night_br;
    document.getElementById('trans_v').textContent=c.trans;
    document.getElementById('lat').value=c.lat;
    document.getElementById('lon').value=c.lon;
    document.getElementById('tz').value=c.tz;
    document.getElementById('ssid').value=c.ssid;
    document.getElementById('wthr_en').checked=c.wthr_en;
    document.getElementById('wthr_key').value=c.wthr_key;
    document.querySelectorAll('.color-opt')[c.color].classList.add('sel');
  }catch(e){console.error(e)}
}

async function save(){
  const c={
    show_sec:document.getElementById('show_sec').checked,
    show_date:document.getElementById('show_date').checked,
    auto_br:document.getElementById('auto_br').checked,
    day_br:parseInt(document.getElementById('day_br').value),
    night_br:parseInt(document.getElementById('night_br').value),
    trans:parseInt(document.getElementById('trans').value),
    lat:parseFloat(document.getElementById('lat').value),
    lon:parseFloat(document.getElementById('lon').value),
    tz:document.getElementById('tz').value,
    ssid:document.getElementById('ssid').value,
    pass:document.getElementById('pass').value,
    wthr_en:document.getElementById('wthr_en').checked,
    wthr_key:document.getElementById('wthr_key').value,
    color:parseInt(document.querySelector('.color-opt.sel').dataset.c)
  };
  
  try{
    const r=await fetch('/api/config',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(c)
    });
    
    if(r.ok){
      document.getElementById('msg').innerHTML='<div class="msg ok">✓ Saved! Restart to apply changes.</div>';
    }else{
      document.getElementById('msg').innerHTML='<div class="msg err">✗ Save failed</div>';
    }
  }catch(e){
    document.getElementById('msg').innerHTML='<div class="msg err">✗ Error: '+e+'</div>';
  }
}

async function restart(){
  if(!confirm('Restart clock? Takes ~30 seconds.'))return;
  await fetch('/api/restart',{method:'POST'});
  document.getElementById('msg').innerHTML='<div class="msg ok">Restarting... refresh in 30s</div>';
}

function exportCfg(){
  fetch('/api/config')
    .then(r=>r.json())
    .then(d=>{
      const b=new Blob([JSON.stringify(d,null,2)],{type:'application/json'});
      const u=URL.createObjectURL(b);
      const a=document.createElement('a');
      a.href=u;
      a.download='clock_config.json';
      a.click();
    });
}
</script>
</body>
</html>
)rawliteral";

// ============================================================================
// API HANDLERS
// ============================================================================

void handle_root() {
  web_server.send_P(200, "text/html", HTML_PAGE);
}

void handle_status() {
  StaticJsonDocument<512> doc;
  
  struct tm ti;
  char tbuf[16], upbuf[32], risebuf[8], setbuf[8];
  
  if (getLocalTime(&ti)) {
    strftime(tbuf, sizeof(tbuf), "%I:%M %p", &ti);
  } else {
    strcpy(tbuf, "--:--");
  }
  
  // Uptime
  unsigned long up_sec = millis() / 1000;
  unsigned long up_min = up_sec / 60;
  unsigned long up_hr = up_min / 60;
  unsigned long up_day = up_hr / 24;
  
  if (up_day > 0) {
    snprintf(upbuf, sizeof(upbuf), "%lud %luh", up_day, up_hr % 24);
  } else if (up_hr > 0) {
    snprintf(upbuf, sizeof(upbuf), "%luh %lum", up_hr, up_min % 60);
  } else {
    snprintf(upbuf, sizeof(upbuf), "%lum", up_min);
  }
  
  // Sunrise/sunset
  snprintf(risebuf, sizeof(risebuf), "%02d:%02d", sunrise_time / 60, sunrise_time % 60);
  snprintf(setbuf, sizeof(setbuf), "%02d:%02d", sunset_time / 60, sunset_time % 60);
  
  // Battery
  float batt_v = amoled.getBattVoltage();
  int batt_pct = (int)((batt_v - 3.0) / 1.2 * 100.0);
  batt_pct = constrain(batt_pct, 0, 100);
  
  doc["time"] = tbuf;
  doc["batt"] = batt_pct;
  doc["chrg"] = is_charging();
  doc["rssi"] = WiFi.RSSI();
  doc["up"] = upbuf;
  doc["rise"] = risebuf;
  doc["set"] = setbuf;
  
  String json;
  serializeJson(doc, json);
  web_server.send(200, "application/json", json);
}

void handle_get_config() {
  StaticJsonDocument<1024> doc;
  
  doc["show_sec"] = config.show_seconds;
  doc["show_date"] = config.show_date;
  doc["auto_br"] = config.auto_brightness;
  doc["day_br"] = config.day_brightness;
  doc["night_br"] = config.night_brightness;
  doc["trans"] = config.transition_minutes;
  doc["lat"] = config.latitude;
  doc["lon"] = config.longitude;
  doc["tz"] = config.timezone;
  doc["ssid"] = config.wifi_ssid;
  doc["wthr_en"] = config.weather_enabled;
  doc["wthr_key"] = config.weather_api_key;
  doc["color"] = config.color_scheme;
  
  String json;
  serializeJson(doc, json);
  web_server.send(200, "application/json", json);
}

void handle_post_config() {
  if (!web_server.hasArg("plain")) {
    web_server.send(400, "text/plain", "No data");
    return;
  }
  
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, web_server.arg("plain"));
  
  if (err) {
    web_server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  // Update config
  config.show_seconds = doc["show_sec"] | config.show_seconds;
  config.show_date = doc["show_date"] | config.show_date;
  config.auto_brightness = doc["auto_br"] | config.auto_brightness;
  config.day_brightness = doc["day_br"] | config.day_brightness;
  config.night_brightness = doc["night_br"] | config.night_brightness;
  config.transition_minutes = doc["trans"] | config.transition_minutes;
  config.latitude = doc["lat"] | config.latitude;
  config.longitude = doc["lon"] | config.longitude;
  config.color_scheme = doc["color"] | config.color_scheme;
  
  if (doc.containsKey("tz")) {
    strlcpy(config.timezone, doc["tz"], sizeof(config.timezone));
  }
  if (doc.containsKey("ssid")) {
    strlcpy(config.wifi_ssid, doc["ssid"], sizeof(config.wifi_ssid));
  }
  if (doc.containsKey("pass") && strlen(doc["pass"]) > 0) {
    strlcpy(config.wifi_pass, doc["pass"], sizeof(config.wifi_pass));
  }
  if (doc.containsKey("wthr_key")) {
    strlcpy(config.weather_api_key, doc["wthr_key"], sizeof(config.weather_api_key));
  }
  
  config.weather_enabled = doc["wthr_en"] | config.weather_enabled;
  
  save_config();
  
  web_server.send(200, "text/plain", "OK");
}

void handle_restart() {
  web_server.send(200, "text/plain", "Restarting...");
  delay(500);
  ESP.restart();
}

// ============================================================================
// SETUP FUNCTION - Call this from main.cpp setup()
// ============================================================================

void setup_web_server() {
  // Start mDNS
  if (MDNS.begin("clock")) {
    Serial.println("✓ mDNS started: http://clock.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("✗ mDNS failed");
  }
  
  // Register handlers
  web_server.on("/", HTTP_GET, handle_root);
  web_server.on("/api/status", HTTP_GET, handle_status);
  web_server.on("/api/config", HTTP_GET, handle_get_config);
  web_server.on("/api/config", HTTP_POST, handle_post_config);
  web_server.on("/api/restart", HTTP_POST, handle_restart);
  
  // Start server
  web_server.begin();
  Serial.println("✓ Web server started on port 80");
  Serial.printf("✓ Access at: http://clock.local or http://%s\n", 
                WiFi.localIP().toString().c_str());
}

// Call this from loop()
void handle_web_server() {
  web_server.handleClient();
  MDNS.update();
}

#endif // WEB_INTERFACE_H

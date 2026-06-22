#include <WiFi.h>
#include <WebServer.h>

//     CONFIG                                                
const char* SSID     = "YOUR_WIFI";
const char* PASSWORD = "YOUR-PASSWORD";

const int   ADC_PIN           = 32;
const float ADC_MAX           = 4095.0f;
const float VREF              = 3.3f;
const float DIVIDER_RATIO     = 1.0f;    // Direct connection to capacitor, no divider

const float STEP_THRESHOLD    = 0.35f;   // Actual voltage above this = step detected
const float RELEASE_THRESHOLD = 0.10f;   // Actual voltage below this = step released
const int   MIN_PRESS_TIME    = 70;      // ms   foot must be ON for at least this long
const int   DEBOUNCE_TIME     = 400;     // ms   ignore new steps for this long after one is logged

const int   SAMPLE_COUNT      = 20;      // More oversampling to reduce noise
const int   READ_INTERVAL     = 30;      // ms   fast polling for step detection

WebServer server(80);

//     STEP LOG                                              
struct StepRecord {
  int           stepNum;
  float         pressVoltage;    // voltage when foot first landed
  float         peakVoltage;     // max voltage during press
  float         releaseVoltage;  // voltage at moment of release
  unsigned long pressTime;
  unsigned long releaseTime;
};

const int MAX_STEPS = 100;
StepRecord steps[MAX_STEPS];
int        stepCount = 0;

//     STATE                                                 
bool          footOn           = false;
float         peakThisStep     = 0.0f;
float         pressVoltage     = 0.0f;
unsigned long pressedAt        = 0;
unsigned long lastStepLoggedAt = 0;
float         currentVoltage   = 0.0f;
float         lastVoltage       = 0.0f;  // previous reading, used to capture release voltage
unsigned long lastReadTime      = 0;

//     ADC READ WITH OVERSAMPLING                            
float readVoltage() {
  long sum = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sum += analogRead(ADC_PIN);
    delayMicroseconds(50);
  }
  float raw = sum / (float)SAMPLE_COUNT;
  // Convert ADC reading   actual voltage (accounting for divider module)
  return (raw / ADC_MAX) * VREF * DIVIDER_RATIO;
}

//     JSON API                                              
void handleData() {
  String json = "{";
  json += "\"currentVoltage\":" + String(currentVoltage, 4) + ",";
  json += "\"footOn\":"         + String(footOn ? "true" : "false") + ",";
  json += "\"totalSteps\":"     + String(stepCount) + ",";
  json += "\"steps\":[";
  for (int i = 0; i < stepCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"num\":"      + String(steps[i].stepNum) + ",";
    json += "\"press\":"    + String(steps[i].pressVoltage, 4) + ",";
    json += "\"peak\":"     + String(steps[i].peakVoltage, 4) + ",";
    json += "\"release\":"  + String(steps[i].releaseVoltage, 4) + ",";
    json += "\"duration\":" + String(steps[i].releaseTime - steps[i].pressTime) + ",";
    json += "\"at\":"       + String(steps[i].pressTime);
    json += "}";
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

//     HTML PAGE                                             
void handleRoot() {
  const char* html =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "<title>TRIVOLT · Live Monitor</title>\n"
    "<link href=\"https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&family=DM+Mono:wght@300;400;500&display=swap\" rel=\"stylesheet\">\n"
    "<style>\n"
    "*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}\n"
    "\n"
    ":root{\n"
    "  --ink:#05090f;\n"
    "  --deep:#080e18;\n"
    "  --card:#0b1422;\n"
    "  --rim:#162236;\n"
    "  --glow:#00ffcc;\n"
    "  --glow2:#ff4d6d;\n"
    "  --glow3:#ffd166;\n"
    "  --accent:#00e5ff;\n"
    "  --muted:#2a4a6a;\n"
    "  --text:#b8d4e8;\n"
    "  --mono:'DM Mono',monospace;\n"
    "  --display:'Orbitron',monospace;\n"
    "}\n"
    "\n"
    "html{scroll-behavior:smooth}\n"
    "body{\n"
    "  background:var(--ink);\n"
    "  color:var(--text);\n"
    "  font-family:var(--mono);\n"
    "  min-height:100vh;\n"
    "  overflow-x:hidden;\n"
    "}\n"
    "\n"
    "/* animated bg grid */\n"
    "body::before{\n"
    "  content:'';\n"
    "  position:fixed;inset:0;\n"
    "  background-image:\n"
    "    linear-gradient(rgba(0,255,204,0.03) 1px,transparent 1px),\n"
    "    linear-gradient(90deg,rgba(0,255,204,0.03) 1px,transparent 1px);\n"
    "  background-size:40px 40px;\n"
    "  animation:gridDrift 20s linear infinite;\n"
    "  pointer-events:none;z-index:0;\n"
    "}\n"
    "@keyframes gridDrift{0%{background-position:0 0}100%{background-position:40px 40px}}\n"
    "\n"
    "/* scanlines */\n"
    "body::after{\n"
    "  content:'';\n"
    "  position:fixed;inset:0;\n"
    "  background:repeating-linear-gradient(0deg,transparent,transparent 3px,rgba(0,0,0,0.08) 3px,rgba(0,0,0,0.08) 4px);\n"
    "  pointer-events:none;z-index:200;\n"
    "}\n"
    "\n"
    "/* ── HEADER ── */\n"
    "header{\n"
    "  position:relative;z-index:10;\n"
    "  display:flex;align-items:center;justify-content:space-between;\n"
    "  padding:20px 36px;\n"
    "  border-bottom:1px solid var(--rim);\n"
    "  background:linear-gradient(180deg,rgba(0,255,204,0.04) 0%,transparent 100%);\n"
    "}\n"
    ".brand{\n"
    "  display:flex;align-items:center;gap:16px;\n"
    "}\n"
    ".brand-icon{\n"
    "  width:38px;height:38px;\n"
    "  border:2px solid var(--glow);\n"
    "  border-radius:50%;\n"
    "  display:flex;align-items:center;justify-content:center;\n"
    "  box-shadow:0 0 16px rgba(0,255,204,0.3),inset 0 0 10px rgba(0,255,204,0.1);\n"
    "  animation:iconPulse 2s ease-in-out infinite;\n"
    "}\n"
    "@keyframes iconPulse{0%,100%{box-shadow:0 0 16px rgba(0,255,204,0.3),inset 0 0 10px rgba(0,255,204,0.1)}50%{box-shadow:0 0 28px rgba(0,255,204,0.6),inset 0 0 16px rgba(0,255,204,0.2)}}\n"
    ".brand-icon svg{width:18px;height:18px;fill:var(--glow)}\n"
    ".brand-name{\n"
    "  font-family:var(--display);\n"
    "  font-size:1.1rem;font-weight:900;\n"
    "  letter-spacing:4px;color:var(--glow);\n"
    "  text-shadow:0 0 20px rgba(0,255,204,0.4);\n"
    "}\n"
    ".brand-sub{font-size:0.6rem;letter-spacing:3px;color:var(--muted);margin-top:2px}\n"
    "\n"
    ".hdr-right{display:flex;gap:24px;align-items:center}\n"
    ".pill{\n"
    "  display:flex;align-items:center;gap:7px;\n"
    "  background:rgba(0,255,204,0.06);\n"
    "  border:1px solid rgba(0,255,204,0.15);\n"
    "  border-radius:20px;padding:6px 14px;\n"
    "  font-size:0.65rem;letter-spacing:2px;color:var(--glow);\n"
    "}\n"
    ".dot{\n"
    "  width:7px;height:7px;border-radius:50%;\n"
    "  background:var(--glow);box-shadow:0 0 8px var(--glow);\n"
    "  animation:blink 1.4s ease-in-out infinite;\n"
    "}\n"
    "@keyframes blink{0%,100%{opacity:1}50%{opacity:0.2}}\n"
    "#uptimeHdr{font-size:0.65rem;letter-spacing:2px;color:var(--muted)}\n"
    "\n"
    "/* ── MAIN ── */\n"
    "main{position:relative;z-index:10;padding:28px 36px;max-width:1200px;margin:0 auto}\n"
    "\n"
    "/* ── HERO ROW ── */\n"
    ".hero-row{\n"
    "  display:grid;\n"
    "  grid-template-columns:1.4fr 1fr 1fr;\n"
    "  gap:20px;margin-bottom:24px;\n"
    "}\n"
    "\n"
    ".card{\n"
    "  background:var(--card);\n"
    "  border:1px solid var(--rim);\n"
    "  border-radius:8px;\n"
    "  padding:24px 28px;\n"
    "  position:relative;overflow:hidden;\n"
    "  transition:border-color 0.3s;\n"
    "}\n"
    ".card:hover{border-color:rgba(0,255,204,0.2)}\n"
    ".card::before{\n"
    "  content:'';position:absolute;\n"
    "  top:0;left:0;right:0;height:1px;\n"
    "  background:linear-gradient(90deg,transparent,rgba(0,255,204,0.3),transparent);\n"
    "}\n"
    "\n"
    ".clabel{\n"
    "  font-size:0.58rem;letter-spacing:5px;\n"
    "  color:var(--muted);text-transform:uppercase;margin-bottom:10px;\n"
    "}\n"
    "\n"
    "/* Step counter card */\n"
    ".step-card{border-color:rgba(0,255,204,0.2)}\n"
    ".step-card::after{\n"
    "  content:'';position:absolute;\n"
    "  bottom:-30px;right:-30px;\n"
    "  width:120px;height:120px;\n"
    "  background:radial-gradient(circle,rgba(0,255,204,0.08) 0%,transparent 70%);\n"
    "  pointer-events:none;\n"
    "}\n"
    ".step-big{\n"
    "  font-family:var(--display);\n"
    "  font-size:5.5rem;line-height:1;font-weight:900;\n"
    "  color:var(--glow);\n"
    "  text-shadow:0 0 40px rgba(0,255,204,0.5);\n"
    "  transition:all 0.3s;\n"
    "}\n"
    ".step-unit{font-size:0.7rem;letter-spacing:4px;color:var(--muted);margin-top:6px}\n"
    "\n"
    "/* Voltage card */\n"
    ".volt-big{\n"
    "  font-family:var(--display);\n"
    "  font-size:2.8rem;line-height:1;font-weight:700;\n"
    "  transition:color 0.2s,text-shadow 0.2s;\n"
    "}\n"
    ".volt-unit{font-size:0.65rem;letter-spacing:3px;color:var(--muted);margin-top:6px}\n"
    "\n"
    "/* Status card */\n"
    ".status-val{\n"
    "  font-family:var(--display);\n"
    "  font-size:1.6rem;font-weight:700;\n"
    "  letter-spacing:3px;margin-bottom:8px;\n"
    "  transition:color 0.3s;\n"
    "}\n"
    ".status-idle{color:var(--muted)}\n"
    ".status-pressed{\n"
    "  color:var(--glow2);\n"
    "  text-shadow:0 0 20px rgba(255,77,109,0.5);\n"
    "  animation:pressedPulse 0.4s ease-in-out infinite alternate;\n"
    "}\n"
    "@keyframes pressedPulse{0%{opacity:0.8}100%{opacity:1}}\n"
    ".status-sub{font-size:0.65rem;letter-spacing:3px;color:var(--muted)}\n"
    "\n"
    "/* Foot animation */\n"
    ".foot-anim{\n"
    "  position:absolute;right:24px;top:50%;transform:translateY(-50%);\n"
    "  width:44px;height:44px;opacity:0.15;transition:opacity 0.3s;\n"
    "}\n"
    ".foot-anim.active{opacity:0.9;filter:drop-shadow(0 0 8px var(--glow2))}\n"
    ".foot-anim svg{width:100%;height:100%}\n"
    "\n"
    "/* ── VOLTAGE BAR ── */\n"
    ".vbar-card{margin-bottom:24px}\n"
    ".vbar-row{display:flex;align-items:center;gap:16px;margin-top:12px}\n"
    ".vbar-label{font-size:0.6rem;letter-spacing:2px;color:var(--muted);width:30px;text-align:right}\n"
    ".vbar-track{\n"
    "  flex:1;height:12px;\n"
    "  background:rgba(0,255,204,0.05);\n"
    "  border:1px solid var(--rim);\n"
    "  border-radius:6px;overflow:visible;\n"
    "  position:relative;\n"
    "}\n"
    ".vbar-fill{\n"
    "  height:100%;border-radius:6px;\n"
    "  background:linear-gradient(90deg,var(--glow),var(--glow3),var(--glow2));\n"
    "  transition:width 0.12s ease;\n"
    "  position:relative;\n"
    "  box-shadow:0 0 12px rgba(0,255,204,0.4);\n"
    "}\n"
    ".vbar-fill::after{\n"
    "  content:'';position:absolute;\n"
    "  right:-1px;top:-3px;\n"
    "  width:4px;height:18px;\n"
    "  background:white;\n"
    "  border-radius:2px;\n"
    "  box-shadow:0 0 8px white,0 0 16px rgba(0,255,204,0.6);\n"
    "}\n"
    ".thresh-line{\n"
    "  position:absolute;\n"
    "  top:-4px;height:20px;width:2px;\n"
    "  background:var(--glow3);\n"
    "  box-shadow:0 0 6px var(--glow3);\n"
    "  border-radius:1px;\n"
    "}\n"
    ".thresh-tip{\n"
    "  position:absolute;top:-20px;\n"
    "  font-size:0.55rem;letter-spacing:1px;\n"
    "  color:var(--glow3);white-space:nowrap;\n"
    "  transform:translateX(-50%);\n"
    "}\n"
    ".vbar-val{\n"
    "  font-family:var(--display);\n"
    "  font-size:0.9rem;color:var(--glow);\n"
    "  width:70px;text-align:left;\n"
    "  transition:color 0.2s;\n"
    "}\n"
    "\n"
    "/* ── MINI STATS ── */\n"
    ".stats-row{\n"
    "  display:grid;grid-template-columns:repeat(4,1fr);\n"
    "  gap:14px;margin-bottom:24px;\n"
    "}\n"
    ".stat-card{\n"
    "  background:var(--card);border:1px solid var(--rim);\n"
    "  border-radius:6px;padding:16px 20px;\n"
    "}\n"
    ".stat-label{font-size:0.55rem;letter-spacing:4px;color:var(--muted);text-transform:uppercase;margin-bottom:6px}\n"
    ".stat-val{font-family:var(--display);font-size:1.1rem;color:var(--text)}\n"
    "\n"
    "/* ── TABLE ── */\n"
    ".table-card{background:var(--card);border:1px solid var(--rim);border-radius:8px;overflow:hidden}\n"
    ".table-hdr{\n"
    "  display:flex;justify-content:space-between;align-items:center;\n"
    "  padding:16px 24px;\n"
    "  border-bottom:1px solid var(--rim);\n"
    "  background:rgba(0,255,204,0.02);\n"
    "}\n"
    ".table-title{font-family:var(--display);font-size:0.65rem;letter-spacing:4px;color:var(--muted)}\n"
    ".badge{\n"
    "  font-family:var(--display);font-size:0.65rem;\n"
    "  background:rgba(0,255,204,0.1);color:var(--glow);\n"
    "  border:1px solid rgba(0,255,204,0.25);\n"
    "  border-radius:3px;padding:3px 10px;letter-spacing:2px;\n"
    "}\n"
    "\n"
    ".tbl-wrap{overflow-x:auto;max-height:420px;overflow-y:auto}\n"
    ".tbl-wrap::-webkit-scrollbar{width:4px;height:4px}\n"
    ".tbl-wrap::-webkit-scrollbar-track{background:var(--deep)}\n"
    ".tbl-wrap::-webkit-scrollbar-thumb{background:var(--rim);border-radius:2px}\n"
    "\n"
    "table{width:100%;border-collapse:collapse;min-width:680px}\n"
    "th{\n"
    "  font-size:0.55rem;letter-spacing:3px;color:var(--muted);\n"
    "  text-transform:uppercase;text-align:left;\n"
    "  padding:11px 20px;\n"
    "  border-bottom:1px solid var(--rim);\n"
    "  font-weight:400;position:sticky;top:0;\n"
    "  background:var(--card);z-index:1;\n"
    "}\n"
    "td{\n"
    "  padding:11px 20px;\n"
    "  font-size:0.8rem;\n"
    "  border-bottom:1px solid rgba(22,34,54,0.6);\n"
    "  transition:background 0.15s;\n"
    "}\n"
    "tr:last-child td{border-bottom:none}\n"
    "tr:hover td{background:rgba(0,255,204,0.03)}\n"
    "\n"
    ".new-row{animation:rowFlash 1.2s ease-out}\n"
    "@keyframes rowFlash{0%{background:rgba(0,255,204,0.12)}100%{background:transparent}}\n"
    "\n"
    ".c-num{color:rgba(0,255,204,0.5);font-family:var(--display);font-size:0.7rem}\n"
    ".c-press{color:#00ffcc}\n"
    ".c-peak{color:#00e5ff}\n"
    ".c-release{color:#ff4d6d}\n"
    ".c-dur{color:var(--glow3)}\n"
    ".c-time{color:var(--muted)}\n"
    "\n"
    ".mini-bar{width:80px}\n"
    ".mini-bg{\n"
    "  height:4px;background:rgba(0,255,204,0.07);\n"
    "  border-radius:2px;overflow:hidden;\n"
    "}\n"
    ".mini-fill{\n"
    "  height:100%;border-radius:2px;\n"
    "  background:linear-gradient(90deg,var(--glow),var(--glow2));\n"
    "}\n"
    "\n"
    ".no-data{\n"
    "  text-align:center;padding:48px;\n"
    "  font-size:0.7rem;letter-spacing:4px;color:var(--muted);\n"
    "  font-family:var(--display);\n"
    "}\n"
    "\n"
    "/* ── STEP FLASH OVERLAY ── */\n"
    "#flash{\n"
    "  position:fixed;inset:0;\n"
    "  background:radial-gradient(circle at center,rgba(0,255,204,0.08) 0%,transparent 70%);\n"
    "  pointer-events:none;z-index:50;\n"
    "  opacity:0;transition:opacity 0.1s;\n"
    "}\n"
    "#flash.active{opacity:1}\n"
    "\n"
    "/* ripple on step card */\n"
    ".ripple{\n"
    "  position:absolute;border-radius:50%;\n"
    "  background:rgba(0,255,204,0.12);\n"
    "  animation:rpl 0.7s ease-out forwards;\n"
    "  pointer-events:none;\n"
    "}\n"
    "@keyframes rpl{\n"
    "  0%{width:0;height:0;opacity:1;top:50%;left:50%;transform:translate(-50%,-50%)}\n"
    "  100%{width:400px;height:400px;opacity:0;top:50%;left:50%;transform:translate(-50%,-50%)}\n"
    "}\n"
    "\n"
    "@media(max-width:700px){\n"
    "  .hero-row{grid-template-columns:1fr 1fr}\n"
    "  .stats-row{grid-template-columns:1fr 1fr}\n"
    "  main{padding:16px}\n"
    "  header{padding:14px 16px}\n"
    "  .step-big{font-size:3.8rem}\n"
    "}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<div id=\"flash\"></div>\n"
    "\n"
    "<header>\n"
    "  <div class=\"brand\">\n"
    "    <div class=\"brand-icon\">\n"
    "      <svg viewBox=\"0 0 24 24\"><path d=\"M12 2C8 2 5 5 5 9c0 5 7 13 7 13s7-8 7-13c0-4-3-7-7-7zm0 9.5c-1.4 0-2.5-1.1-2.5-2.5S10.6 6.5 12 6.5s2.5 1.1 2.5 2.5S13.4 11.5 12 11.5z\"/></svg>\n"
    "    </div>\n"
    "    <div>\n"
    "      <div class=\"brand-name\">TRIVOLT</div>\n"
    "      <div class=\"brand-sub\">ENERGY HARVEST MONITOR · GPIO32</div>\n"
    "    </div>\n"
    "  </div>\n"
    "  <div class=\"hdr-right\">\n"
    "    <div id=\"uptimeHdr\">UP 0s</div>\n"
    "    <div class=\"pill\"><span class=\"dot\"></span>LIVE</div>\n"
    "  </div>\n"
    "</header>\n"
    "\n"
    "<main>\n"
    "  <!-- Hero row -->\n"
    "  <div class=\"hero-row\">\n"
    "    <div class=\"card step-card\" id=\"stepCard\">\n"
    "      <div class=\"clabel\">Total Steps Detected</div>\n"
    "      <div class=\"step-big\" id=\"totalSteps\">0</div>\n"
    "      <div class=\"step-unit\">FOOTFALLS LOGGED</div>\n"
    "    </div>\n"
    "\n"
    "    <div class=\"card\">\n"
    "      <div class=\"clabel\">Live Voltage</div>\n"
    "      <div class=\"volt-big\" id=\"vNow\" style=\"color:var(--glow)\">0.0000</div>\n"
    "      <div class=\"volt-unit\">VOLTS · REAL-TIME</div>\n"
    "    </div>\n"
    "\n"
    "    <div class=\"card\">\n"
    "      <div class=\"clabel\">Tile Status</div>\n"
    "      <div class=\"status-val status-idle\" id=\"footStatus\">IDLE</div>\n"
    "      <div class=\"status-sub\" id=\"footSub\">awaiting footfall…</div>\n"
    "      <div class=\"foot-anim\" id=\"footIcon\">\n"
    "        <svg viewBox=\"0 0 64 64\" fill=\"currentColor\" style=\"color:var(--glow2)\">\n"
    "          <ellipse cx=\"22\" cy=\"18\" rx=\"10\" ry=\"14\"/>\n"
    "          <ellipse cx=\"42\" cy=\"22\" rx=\"8\" ry=\"11\"/>\n"
    "          <ellipse cx=\"14\" cy=\"38\" rx=\"7\" ry=\"9\"/>\n"
    "          <ellipse cx=\"30\" cy=\"44\" rx=\"7\" ry=\"9\"/>\n"
    "          <ellipse cx=\"46\" cy=\"40\" rx=\"6\" ry=\"8\"/>\n"
    "          <ellipse cx=\"20\" cy=\"54\" rx=\"14\" ry=\"8\"/>\n"
    "          <ellipse cx=\"40\" cy=\"56\" rx=\"12\" ry=\"7\"/>\n"
    "        </svg>\n"
    "      </div>\n"
    "    </div>\n"
    "  </div>\n"
    "\n"
    "  <!-- Voltage bar -->\n"
    "  <div class=\"card vbar-card\">\n"
    "    <div class=\"clabel\">Voltage Level · 0V – 1.5V Range</div>\n"
    "    <div class=\"vbar-row\">\n"
    "      <div class=\"vbar-label\">0V</div>\n"
    "      <div class=\"vbar-track\">\n"
    "        <div class=\"thresh-line\" style=\"left:23.3%\">\n"
    "          <div class=\"thresh-tip\">0.35V</div>\n"
    "        </div>\n"
    "        <div class=\"vbar-fill\" id=\"voltBar\" style=\"width:0%\"></div>\n"
    "      </div>\n"
    "      <div class=\"vbar-label\">1.5V</div>\n"
    "      <div class=\"vbar-val\" id=\"vBarVal\">0.0000 V</div>\n"
    "    </div>\n"
    "  </div>\n"
    "\n"
    "  <!-- Mini stats -->\n"
    "  <div class=\"stats-row\">\n"
    "    <div class=\"stat-card\">\n"
    "      <div class=\"stat-label\">Min Recorded</div>\n"
    "      <div class=\"stat-val\" id=\"sMin\">—</div>\n"
    "    </div>\n"
    "    <div class=\"stat-card\">\n"
    "      <div class=\"stat-label\">Max Recorded</div>\n"
    "      <div class=\"stat-val\" id=\"sMax\">—</div>\n"
    "    </div>\n"
    "    <div class=\"stat-card\">\n"
    "      <div class=\"stat-label\">Avg Peak</div>\n"
    "      <div class=\"stat-val\" id=\"sAvg\">—</div>\n"
    "    </div>\n"
    "    <div class=\"stat-card\">\n"
    "      <div class=\"stat-label\">Avg Duration</div>\n"
    "      <div class=\"stat-val\" id=\"sDur\">—</div>\n"
    "    </div>\n"
    "  </div>\n"
    "\n"
    "  <!-- Table -->\n"
    "  <div class=\"table-card\">\n"
    "    <div class=\"table-hdr\">\n"
    "      <div class=\"table-title\">STEP · BY · STEP · LOG</div>\n"
    "      <div class=\"badge\" id=\"logBadge\">0 STEPS</div>\n"
    "    </div>\n"
    "    <div class=\"tbl-wrap\">\n"
    "      <div id=\"tableBody\"><div class=\"no-data\">AWAITING FIRST FOOTFALL…</div></div>\n"
    "    </div>\n"
    "  </div>\n"
    "</main>\n"
    "\n"
    "<script>\n"
    "let lastStepCount = 0;\n"
    "\n"
    "const fmt  = v => parseFloat(v).toFixed(4);\n"
    "const fmtV = v => parseFloat(v).toFixed(4) + ' V';\n"
    "function fmtTime(ms){\n"
    "  const s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);\n"
    "  if(h>0)return h+'h '+(m%60)+'m '+(s%60)+'s';\n"
    "  if(m>0)return m+'m '+(s%60)+'s';\n"
    "  return s+'s';\n"
    "}\n"
    "\n"
    "function flashScreen(){\n"
    "  const f=document.getElementById('flash');\n"
    "  f.classList.add('active');\n"
    "  setTimeout(()=>f.classList.remove('active'),150);\n"
    "}\n"
    "function ripple(){\n"
    "  const c=document.getElementById('stepCard');\n"
    "  const el=document.createElement('div');\n"
    "  el.className='ripple';c.appendChild(el);\n"
    "  setTimeout(()=>el.remove(),800);\n"
    "}\n"
    "\n"
    "function renderTable(steps){\n"
    "  if(!steps.length)return;\n"
    "  let html=`<table><thead><tr>\n"
    "    <th>#</th>\n"
    "    <th style=\"color:#00ffcc\">↓ Press V</th>\n"
    "    <th style=\"color:#00e5ff\">▲ Peak V</th>\n"
    "    <th style=\"color:#ff4d6d\">↑ Release V</th>\n"
    "    <th>Duration</th><th>Uptime</th><th style=\"width:80px\">Strength</th>\n"
    "  </tr></thead><tbody>`;\n"
    "  for(let i=steps.length-1;i>=0;i--){\n"
    "    const s=steps[i];\n"
    "    const pct=Math.min(100,(s.peak/1.5)*100).toFixed(1);\n"
    "    const isNew=i===steps.length-1?'class=\"new-row\"':'';\n"
    "    html+=`<tr ${isNew}>\n"
    "      <td class=\"c-num\">${String(s.num).padStart(3,'0')}</td>\n"
    "      <td class=\"c-press\">${fmt(s.press)} V</td>\n"
    "      <td class=\"c-peak\">${fmt(s.peak)} V</td>\n"
    "      <td class=\"c-release\">${fmt(s.release)} V</td>\n"
    "      <td class=\"c-dur\">${s.duration} ms</td>\n"
    "      <td class=\"c-time\">${fmtTime(s.at)}</td>\n"
    "      <td class=\"mini-bar\"><div class=\"mini-bg\"><div class=\"mini-fill\" style=\"width:${pct}%\"></div></div></td>\n"
    "    </tr>`;\n"
    "  }\n"
    "  html+='</tbody></table>';\n"
    "  document.getElementById('tableBody').innerHTML=html;\n"
    "}\n"
    "\n"
    "async function fetchData(){\n"
    "  try{\n"
    "    const r=await fetch('/data');\n"
    "    const d=await r.json();\n"
    "    const v=parseFloat(d.currentVoltage);\n"
    "\n"
    "    // Live voltage\n"
    "    const vColor = v>0.35?'var(--glow2)':v>0.15?'var(--glow3)':'var(--glow)';\n"
    "    const vEl=document.getElementById('vNow');\n"
    "    vEl.textContent=fmt(v);\n"
    "    vEl.style.color=vColor;\n"
    "    vEl.style.textShadow=v>0.35?'0 0 20px rgba(255,77,109,0.5)':'0 0 20px rgba(0,255,204,0.3)';\n"
    "\n"
    "    // Bar\n"
    "    const pct=Math.min(100,(v/1.5)*100).toFixed(2);\n"
    "    document.getElementById('voltBar').style.width=pct+'%';\n"
    "    document.getElementById('vBarVal').textContent=fmt(v)+' V';\n"
    "    document.getElementById('vBarVal').style.color=vColor;\n"
    "\n"
    "    // Status\n"
    "    const fEl=document.getElementById('footStatus');\n"
    "    const fSub=document.getElementById('footSub');\n"
    "    const fIcon=document.getElementById('footIcon');\n"
    "    if(d.footOn){\n"
    "      fEl.textContent='PRESSED';\n"
    "      fEl.className='status-val status-pressed';\n"
    "      fSub.textContent='energy harvesting…';\n"
    "      fIcon.classList.add('active');\n"
    "    } else {\n"
    "      fEl.textContent='IDLE';\n"
    "      fEl.className='status-val status-idle';\n"
    "      fSub.textContent='awaiting footfall…';\n"
    "      fIcon.classList.remove('active');\n"
    "    }\n"
    "\n"
    "    // Step count\n"
    "    document.getElementById('totalSteps').textContent=d.totalSteps;\n"
    "    document.getElementById('logBadge').textContent=d.totalSteps+' STEPS';\n"
    "    if(d.totalSteps>lastStepCount){\n"
    "      ripple(); flashScreen();\n"
    "      lastStepCount=d.totalSteps;\n"
    "    }\n"
    "\n"
    "    // Stats\n"
    "    if(d.steps.length){\n"
    "      const peaks=d.steps.map(s=>s.peak);\n"
    "      const durs=d.steps.map(s=>s.duration);\n"
    "      document.getElementById('sMin').textContent=Math.min(...peaks).toFixed(4)+' V';\n"
    "      document.getElementById('sMax').textContent=Math.max(...peaks).toFixed(4)+' V';\n"
    "      document.getElementById('sAvg').textContent=(peaks.reduce((a,b)=>a+b,0)/peaks.length).toFixed(4)+' V';\n"
    "      document.getElementById('sDur').textContent=Math.round(durs.reduce((a,b)=>a+b,0)/durs.length)+' ms';\n"
    "      renderTable(d.steps);\n"
    "    }\n"
    "\n"
    "    // Uptime\n"
    "    if(d.steps.length){\n"
    "      document.getElementById('uptimeHdr').textContent='UP '+fmtTime(d.steps[d.steps.length-1].at);\n"
    "    }\n"
    "\n"
    "  } catch(e){console.error(e);}\n"
    "}\n"
    "\n"
    "fetchData();\n"
    "setInterval(fetchData,200);\n"
    "</script>\n"
    "</body>\n"
    "</html>\n"
  ;
  server.send(200, "text/html", html);
}

//     SETUP                                                 
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.printf("\nConnecting to %s", SSID);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected! Open: http://%s\n", WiFi.localIP().toString().c_str());

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.begin();
}

//     LOOP                                                  
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime   = now;
    lastVoltage    = currentVoltage;
    currentVoltage = readVoltage();

    // Step detection state machine with debounce
    bool debouncing = (now - lastStepLoggedAt) < DEBOUNCE_TIME;

    if (!footOn && !debouncing && currentVoltage >= STEP_THRESHOLD) {
      // Foot landed
      footOn        = true;
      pressVoltage  = currentVoltage;
      peakThisStep  = currentVoltage;
      pressedAt     = now;
      Serial.printf("STEP %d - foot DOWN (%.4fV)\n", stepCount + 1, currentVoltage);
    }
    else if (footOn) {
      // Track peak while pressed
      if (currentVoltage > peakThisStep) peakThisStep = currentVoltage;

      if (currentVoltage <= RELEASE_THRESHOLD) {
        unsigned long pressDuration = now - pressedAt;
        footOn = false;

        // Only log if foot was pressed long enough (filters phantom spikes)
        if (pressDuration >= MIN_PRESS_TIME && stepCount < MAX_STEPS) {
          steps[stepCount].stepNum        = stepCount + 1;
          steps[stepCount].pressVoltage   = pressVoltage;
          steps[stepCount].peakVoltage    = peakThisStep;
          steps[stepCount].releaseVoltage = lastVoltage;  // last reading before drop
          steps[stepCount].pressTime      = pressedAt;
          steps[stepCount].releaseTime    = now;
          stepCount++;
          lastStepLoggedAt = now;  // start debounce timer
          Serial.printf("STEP %d LOGGED - peak=%.4fV  duration=%lums\n",
            stepCount, peakThisStep, pressDuration);
        } else {
          Serial.printf("IGNORED - too short (%lums), likely bounce\n", pressDuration);
        }
      }
    }

    // Print voltage to serial every 500ms for debugging
    static unsigned long lastPrint = 0;
    if (now - lastPrint >= 500) {
      lastPrint = now;
      Serial.printf("V=%.4f | Steps=%d | FootOn=%s\n",
        currentVoltage, stepCount, footOn ? "YES" : "NO");
    }
  }
}
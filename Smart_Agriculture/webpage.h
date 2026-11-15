#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Agriculture System</title>
<style>
:root{
  --bg:#0f1a14; --card:#16251c; --line:#25392d; --text:#e8f3ec; --mut:#8fa99a;
  --ok:#3ecf74; --warn:#f0b429; --bad:#ef5350; --acc:#4cc38a;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif}
header{display:flex;flex-wrap:wrap;gap:10px;align-items:center;justify-content:space-between;padding:14px 16px;border-bottom:1px solid var(--line)}
h1{margin:0;font-size:1.15rem;letter-spacing:.06em}
h2{margin:0 0 10px;font-size:1rem}
h3{margin:0 0 6px;font-size:.8rem;color:var(--mut);font-weight:600;text-transform:uppercase;letter-spacing:.05em}
main{max-width:900px;margin:0 auto;padding:12px 16px 24px}
.chip{padding:4px 10px;border-radius:999px;font-size:.8rem;border:1px solid var(--line)}
.chip.on{color:var(--ok);border-color:var(--ok)}
.chip.off{color:var(--bad);border-color:var(--bad)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}
.card,.panel{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px}
.panel{margin-top:12px}
.val{font-size:1.7rem;font-weight:700}
.sub{font-size:.8rem;color:var(--mut);margin-top:2px}
.bad{color:var(--bad)} .good{color:var(--ok)} .warn{color:var(--warn)}
canvas{width:100%;height:40px;margin-top:6px;color:var(--acc)}
.row{display:flex;gap:10px;margin-bottom:10px}
.btn{flex:1;padding:12px 8px;border-radius:10px;border:1px solid var(--line);background:#1d3126;color:var(--text);font-size:.95rem;font-weight:600;cursor:pointer}
.btn:active{transform:scale(.98)}
.btn.go{background:#1f6b45}
.btn.stop{background:#b3261e;border-color:#b3261e}
.btn.sel{outline:2px solid var(--acc)}
.btn:disabled{opacity:.4;cursor:not-allowed}
.form{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px;margin-bottom:10px}
label{font-size:.8rem;color:var(--mut);display:flex;flex-direction:column;gap:4px}
input{padding:10px;border-radius:8px;border:1px solid var(--line);background:#0f1a14;color:var(--text);font-size:1rem}
.msg{min-height:1.2em;font-size:.85rem;color:var(--mut)}
.hint{font-size:.8rem;color:var(--mut);margin:0 0 10px}
#alerts{padding:0 16px;max-width:900px;margin:10px auto 0}
.alert{background:#3a1d1b;border:1px solid var(--bad);color:#ffb4ab;border-radius:10px;padding:8px 12px;margin-bottom:6px;font-size:.85rem}
footer{margin-top:16px;text-align:center;font-size:.75rem;color:var(--mut)}

</style>
</head>
<body>
<header>
  <h1>SMART AGRICULTURE SYSTEM</h1>
  <div id="conn" class="chip off">Connecting...</div>
</header>

<div id="alerts"></div>

<main>
  <section class="grid">
    <div class="card"><h3>Temperature</h3><div class="val"><span id="temp">--</span> &deg;C</div><div class="sub" id="tempSt"></div><canvas id="cTemp" width="200" height="40"></canvas></div>
    <div class="card"><h3>Humidity</h3><div class="val"><span id="hum">--</span> %</div><div class="sub" id="humSt"></div></div>
    <div class="card"><h3>Soil Moisture</h3><div class="val"><span id="soil">--</span> %</div><div class="sub"><span id="soilState">--</span> &middot; raw <span id="soilRaw">--</span></div><canvas id="cSoil" width="200" height="40"></canvas></div>
    <div class="card"><h3>TDS</h3><div class="val"><span id="tds">--</span> ppm</div><div class="sub"><span id="tdsMv">--</span> mV &middot; approx., calibrate</div><canvas id="cTds" width="200" height="40"></canvas></div>
    <div class="card"><h3>Light (raw ADC)</h3><div class="val"><span id="ldr">--</span></div><div class="sub" id="ldrState">--</div><canvas id="cLdr" width="200" height="40"></canvas></div>
    <div class="card"><h3>Pump</h3><div class="val" id="pump">--</div><div class="sub">Mode: <b id="mode">--</b></div><div class="sub" id="pumpInfo"></div></div>
  </section>

  <section class="panel">
    <h2>Control</h2>
    <div class="row">
      <button id="bAuto" class="btn">AUTO</button>
      <button id="bManual" class="btn">MANUAL</button>
    </div>
    <div class="row">
      <button id="bOn" class="btn go">Pump ON</button>
      <button id="bOff" class="btn">Pump OFF</button>
    </div>
    <div class="row">
      <button id="bStop" class="btn stop">EMERGENCY STOP</button>
      <button id="bReset" class="btn">RESET</button>
    </div>
    <div id="msg" class="msg"></div>
  </section>

  <section class="panel">
    <h2>Irrigation settings</h2>
    <div class="form">
      <label>Start below (%) <input type="number" id="fLow" min="0" max="99"></label>
      <label>Stop at (%) <input type="number" id="fHigh" min="1" max="100"></label>
      <label>Max pump run (s) <input type="number" id="fMax" min="1" max="300"></label>
      <label>Cooldown (s) <input type="number" id="fCool" min="0" max="3600"></label>
      <label>Light DARK below (raw) <input type="number" id="fDark" min="0" max="4095"></label>
      <label>Light BRIGHT above (raw) <input type="number" id="fBright" min="0" max="4095"></label>
      <label>TDS factor <input type="number" id="fTds" step="0.01" min="0.1" max="10"></label>
    </div>
    <div class="row"><button id="bSave" class="btn go">Save settings</button></div>
  </section>

  <section class="panel">
    <h2>Soil calibration</h2>
    <p class="hint">1) Sensor in air &rarr; press DRY. 2) Sensor in water (up to the line, not the electronics) &rarr; press WET.</p>
    <div class="row">
      <button id="bDry" class="btn">Set DRY point</button>
      <button id="bWet" class="btn">Set WET point</button>
    </div>
    <div class="hint">Saved: dry=<span id="calDry">--</span> &middot; wet=<span id="calWet">--</span></div>
  </section>
  <footer>Uptime <span id="up">--</span> s &middot; no Internet needed</footer>
</main>
<script>
// Smart Agriculture dashboard logic - polls the ESP32 every 2 s, no Internet needed.
const $ = id => document.getElementById(id);
const hist = { temp: [], soil: [], tds: [], ldr: [] };
const HMAX = 60;
let fails = 0;
let cfgLoaded = false;

async function api(path, method) {
  const ctl = new AbortController();
  const t = setTimeout(() => ctl.abort(), 3500);
  try {
    const r = await fetch(path, { method: method || 'GET', signal: ctl.signal });
    return await r.json();
  } finally {
    clearTimeout(t);
  }
}

function push(key, v) {
  if (v === null || v === undefined || isNaN(v)) return;
  hist[key].push(v);
  if (hist[key].length > HMAX) hist[key].shift();
}

function spark(id, arr) {
  const c = $(id);
  if (!c) return;
  const x = c.getContext('2d');
  const w = c.width, h = c.height;
  x.clearRect(0, 0, w, h);
  if (arr.length < 2) return;
  let mn = Math.min.apply(null, arr), mx = Math.max.apply(null, arr);
  if (mx - mn < 1) { mx = mn + 1; }
  x.strokeStyle = getComputedStyle(c).color;
  x.lineWidth = 2;
  x.beginPath();
  arr.forEach((v, i) => {
    const px = (i / (HMAX - 1)) * w;
    const py = h - 4 - ((v - mn) / (mx - mn)) * (h - 8);
    if (i === 0) x.moveTo(px, py); else x.lineTo(px, py);
  });
  x.stroke();
}

function setOnline(on) {
  const c = $('conn');
  c.textContent = on ? 'Connected' : 'Connection lost';
  c.className = 'chip ' + (on ? 'on' : 'off');
}

function stateClass(el, ok) {
  el.classList.toggle('bad', !ok);
}

function render(d) {
  $('temp').textContent = d.temp === null ? '--' : d.temp.toFixed(1);
  $('hum').textContent = d.hum === null ? '--' : d.hum.toFixed(0);
  $('tempSt').textContent = d.dhtOk ? 'DHT11 OK' : 'DHT11 ERROR';
  stateClass($('tempSt'), d.dhtOk);
  $('humSt').textContent = d.dhtOk ? 'DHT11 OK' : 'DHT11 ERROR';
  stateClass($('humSt'), d.dhtOk);

  $('soil').textContent = d.soilPct;
  $('soilState').textContent = d.soilState;
  $('soilRaw').textContent = d.soilRaw;
  stateClass($('soilState'), d.soilOk);

  $('tds').textContent = d.tds;
  $('tdsMv').textContent = d.tdsMv;

  $('ldr').textContent = d.ldrRaw;
  $('ldrState').textContent = d.ldrState;
  stateClass($('ldrState'), d.ldrOk);

  const p = $('pump');
  p.textContent = d.pump ? 'ON' : 'OFF';
  p.className = 'val ' + (d.pump ? 'good' : '');
  $('mode').textContent = d.mode;
  let info = 'Last: ' + d.reason;
  if (d.pump) info = 'Auto-off in ' + d.runLeft + ' s';
  $('pumpInfo').textContent = info;
  $('up').textContent = d.uptime;

  push('temp', d.temp); push('soil', d.soilPct); push('tds', d.tds); push('ldr', d.ldrRaw);
  spark('cTemp', hist.temp); spark('cSoil', hist.soil); spark('cTds', hist.tds); spark('cLdr', hist.ldr);

  $('alerts').innerHTML = d.alerts.map(a => '<div class="alert">' + a.replace(/</g, '&lt;') + '</div>').join('');

  const manual = d.mode === 'MANUAL';
  $('bAuto').classList.toggle('sel', !manual);
  $('bManual').classList.toggle('sel', manual);
  $('bOn').disabled = !manual || d.estop;
  $('bOff').disabled = !manual;
}

async function poll() {
  try {
    const d = await api('/api/data');
    fails = 0;
    setOnline(true);
    render(d);
    if (!cfgLoaded) loadCfg();
  } catch (e) {
    fails++;
    if (fails >= 2) setOnline(false);
  }
}

async function loadCfg() {
  try {
    const c = await api('/api/config');
    $('fLow').value = c.low; $('fHigh').value = c.high;
    $('fMax').value = c.maxRun; $('fCool').value = c.cooldown;
    $('fDark').value = c.ldrDark; $('fBright').value = c.ldrBright;
    $('fTds').value = c.tdsK;
    $('calDry').textContent = c.dry; $('calWet').textContent = c.wet;
    cfgLoaded = true;
  } catch (e) { /* retried on next poll */ }
}

function say(r) {
  $('msg').textContent = r.msg || '';
  $('msg').style.color = r.ok ? '' : 'var(--bad)';
}

async function act(path) {
  try { say(await api(path, 'POST')); } catch (e) { say({ ok: false, msg: 'No response from ESP32' }); }
  poll();
}

$('bAuto').onclick = () => act('/api/mode?m=auto');
$('bManual').onclick = () => act('/api/mode?m=manual');
$('bOn').onclick = () => act('/api/pump?s=1');
$('bOff').onclick = () => act('/api/pump?s=0');
$('bStop').onclick = () => act('/api/stop');
$('bReset').onclick = () => act('/api/reset');

$('bSave').onclick = async () => {
  const q = '/api/config?low=' + $('fLow').value + '&high=' + $('fHigh').value +
    '&maxRun=' + $('fMax').value + '&cooldown=' + $('fCool').value +
    '&ldrDark=' + $('fDark').value + '&ldrBright=' + $('fBright').value +
    '&tdsK=' + $('fTds').value;
  try { say(await api(q, 'POST')); } catch (e) { say({ ok: false, msg: 'No response from ESP32' }); }
  loadCfg();
};

async function calib(w) {
  try { say(await api('/api/calib?w=' + w, 'POST')); } catch (e) { say({ ok: false, msg: 'No response from ESP32' }); }
  loadCfg();
}
$('bDry').onclick = () => calib('dry');
$('bWet').onclick = () => calib('wet');

poll();
setInterval(poll, 2000);

</script>
</body>
</html>
)rawliteral";

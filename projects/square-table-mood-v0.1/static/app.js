const statusEl = document.getElementById("status");
const portEl = document.getElementById("port");
const logEl = document.getElementById("log");
const connectionLog = document.getElementById("connectionLog");
const matrixEl = document.getElementById("matrix");
const paletteEl = document.getElementById("palette");
const moodSelect = document.getElementById("moodSelect");
const moodName = document.getElementById("moodName");
const effectEl = document.getElementById("effect");
const speedEl = document.getElementById("speed");
const brightnessEl = document.getElementById("brightness");
const colorPicker = document.getElementById("colorPicker");
const colorHex = document.getElementById("colorHex");
const colorSwatch = document.getElementById("colorSwatch");
const editorMessage = document.getElementById("editorMessage");
const weatherLocationEl = document.getElementById("weatherLocation");
const weatherResultEl = document.getElementById("weatherResult");
const weatherAutoEl = document.getElementById("weatherAuto");
const weatherUpdatedEl = document.getElementById("weatherUpdated");

let moods = {};
let pixels = Array.from({length: 16}, () => [0, 0, 0]);
let currentColor = [0, 255, 102];
let lastWeather = null;
let weatherTimer = null;

const palette = [
  "#000000", "#FFFFFF", "#FF0000", "#FF6600", "#FFFF00", "#00FF00",
  "#00FFFF", "#0088FF", "#0000FF", "#8800FF", "#FF00FF", "#FF6699",
  "#663300", "#888888", "#00FF66", "#66FFCC"
];

function log(msg) {
  const line = `[${new Date().toLocaleTimeString()}] ${msg}`;
  logEl.textContent += (logEl.textContent ? "\n" : "") + line;
  logEl.scrollTop = logEl.scrollHeight;
  connectionLog.textContent = line;
}

function setStatus(connected, port="") {
  statusEl.className = "status " + (connected ? "connected" : "disconnected");
  statusEl.textContent = connected ? `🟢 Pico connected — ${port}` : "Pico disconnected";
}

async function refreshPorts() {
  try {
    const r = await fetch("/api/ports");
    const data = await r.json();
    portEl.innerHTML = "";
    if (!data.ports.length) {
      const opt = document.createElement("option");
      opt.textContent = "No serial ports found";
      opt.value = "";
      portEl.appendChild(opt);
      log("No serial ports found.");
      setStatus(false);
      return;
    }
    data.ports.forEach(p => {
      const opt = document.createElement("option");
      opt.value = p.device;
      opt.textContent = `${p.device} — ${p.description}`;
      portEl.appendChild(opt);
    });
    if (data.connected && [...portEl.options].some(o => o.value === data.connected)) portEl.value = data.connected;
    else portEl.selectedIndex = 0;
    log("Found serial ports: " + data.ports.map(p => p.device).join(", "));
  } catch (e) { log("Port refresh failed: " + e); }
}

async function connectPico() {
  const port = portEl.value;
  if (!port) return log("Please select a COM port.");
  log("Opening " + port + "...");
  try {
    const r = await fetch("/api/connect", {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({port})});
    const data = await r.json();
    if (data.ok) { setStatus(true, data.port); log("Connected to " + data.port); }
    else { setStatus(false); log("ERROR: " + data.error); }
  } catch (e) { log("Connect failed: " + e); }
}

async function disconnectPico() {
  await fetch("/api/disconnect", {method:"POST"});
  setStatus(false);
  log("Disconnected.");
}

async function sendRaw() {
  const command = document.getElementById("raw").value.trim();
  if (command) await send(command);
}

async function mood(name) { await send(name); }

async function send(command) {
  try {
    const r = await fetch("/api/send", {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({command})});
    const data = await r.json();
    if (data.ok) log("Sent: " + command); else log("ERROR: " + data.error);
  } catch (e) { log("Send failed: " + e); }
}

function rgbFromHex(hex) {
  const value = hex.replace("#", "");
  return [parseInt(value.slice(0,2),16), parseInt(value.slice(2,4),16), parseInt(value.slice(4,6),16)];
}

function hexFromRgb(rgb) {
  return "#" + rgb.map(v => Math.max(0, Math.min(255, v)).toString(16).padStart(2,"0")).join("").toUpperCase();
}

function setCurrentColor(hex) {
  currentColor = rgbFromHex(hex);
  colorHex.textContent = hexFromRgb(currentColor);
  colorSwatch.style.background = hexFromRgb(currentColor);
}

function buildPalette() {
  paletteEl.innerHTML = "";
  palette.forEach(hex => {
    const b = document.createElement("button");
    b.className = "swatch";
    b.style.background = hex;
    b.title = hex;
    b.onclick = () => { colorPicker.value = hex; setCurrentColor(hex); };
    paletteEl.appendChild(b);
  });
}

function renderMatrix() {
  matrixEl.innerHTML = "";
  pixels.forEach((rgb, index) => {
    const cell = document.createElement("button");
    cell.className = "led-cell";
    cell.style.backgroundColor = hexFromRgb(rgb);
    cell.style.boxShadow = rgb.some(v => v > 0) ? `0 0 18px ${hexFromRgb(rgb)}` : "none";
    cell.title = `LED ${index}`;
    cell.onclick = () => {
      pixels[index] = [...currentColor];
      renderMatrix();
    };
    matrixEl.appendChild(cell);
  });
}

function clearMatrix() {
  pixels = Array.from({length: 16}, () => [0,0,0]);
  renderMatrix();
}

function fillMatrix() {
  pixels = Array.from({length: 16}, () => [...currentColor]);
  renderMatrix();
}

function invertMatrix() {
  pixels = pixels.map(rgb => rgb.some(v => v > 0) ? [0,0,0] : [...currentColor]);
  renderMatrix();
}

function getEditorMood() {
  return {
    name: (moodName.value.trim() || "CUSTOM").toUpperCase(),
    pixels: pixels.map(p => [...p]),
    effect: effectEl.value,
    speed: Number(speedEl.value),
    brightness: Number(brightnessEl.value)
  };
}

function applyMood(mood) {
  if (!mood) return;
  moodName.value = mood.name || "";
  pixels = mood.pixels.map(p => [...p]);
  effectEl.value = mood.effect || "STATIC";
  speedEl.value = mood.speed || 100;
  brightnessEl.value = mood.brightness || 100;
  document.getElementById("speedValue").textContent = speedEl.value;
  document.getElementById("brightnessValue").textContent = brightnessEl.value;
  renderMatrix();
  editorMessage.textContent = `Loaded ${mood.name}`;
}

async function loadMoods() {
  try {
    const r = await fetch("/api/moods");
    moods = await r.json();
    moodSelect.innerHTML = "";
    Object.keys(moods).sort().forEach(name => {
      const option = document.createElement("option");
      option.value = name;
      option.textContent = name;
      moodSelect.appendChild(option);
    });
    if (Object.keys(moods).length) applyMood(moods[Object.keys(moods).sort()[0]]);
  } catch (e) { editorMessage.textContent = "Could not load moods."; log("Mood load failed: " + e); }
}

function loadSelectedMood() { applyMood(moods[moodSelect.value]); }

async function saveMood() {
  const mood = getEditorMood();
  if (!mood.name || mood.name === "CUSTOM") return editorMessage.textContent = "Enter a mood name first.";
  try {
    const r = await fetch("/api/moods", {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(mood)});
    const data = await r.json();
    if (!data.ok) throw new Error(data.error);
    moods[mood.name] = data.mood;
    moodSelect.innerHTML = "";
    Object.keys(moods).sort().forEach(name => {
      const option = document.createElement("option"); option.value=name; option.textContent=name; moodSelect.appendChild(option);
    });
    moodSelect.value = mood.name;
    editorMessage.textContent = `Saved ${mood.name}.`;
    log("Saved mood: " + mood.name);
  } catch (e) { editorMessage.textContent = "Save failed: " + e.message; }
}

async function deleteMood() {
  const name = moodSelect.value;
  if (!name) return;
  if (!confirm(`Delete mood ${name}?`)) return;
  try {
    const r = await fetch(`/api/moods/${encodeURIComponent(name)}`, {method:"DELETE"});
    const data = await r.json();
    if (!data.ok) throw new Error(data.error);
    delete moods[name];
    await loadMoods();
    editorMessage.textContent = `Deleted ${name}.`;
  } catch (e) { editorMessage.textContent = "Delete failed: " + e.message; }
}

async function testMood() {
  const mood = getEditorMood();
  try {
    const r = await fetch("/api/mood/test", {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(mood)});
    const data = await r.json();
    if (!data.ok) throw new Error(data.error);
    editorMessage.textContent = `Tested ${mood.name} — ${mood.effect}.`;
    log(`Tested ${mood.name} (${mood.effect})`);
  } catch (e) { editorMessage.textContent = "Test failed: " + e.message; log("Mood test failed: " + e.message); }
}

async function getWeather() {
  const location = weatherLocationEl.value.trim();
  if (!location) return weatherResultEl.textContent = "Enter a town or city first.";
  weatherResultEl.textContent = "Looking up weather…";
  try {
    const r = await fetch(`/api/weather?location=${encodeURIComponent(location)}`);
    const data = await r.json();
    if (!data.ok) throw new Error(data.error);
    lastWeather = data;
    const c = data.current;
    const rgb = data.temperature_color;
    const mood = data.mood;
    weatherResultEl.innerHTML = `<div class="weather-main"><span class="weather-temp">${c.temperature.toFixed(1)}°C</span><span><strong>${escapeHtml(data.location.name)}, ${escapeHtml(data.location.country || "")}</strong><br>${escapeHtml(c.description)} · wind ${Number(c.wind_speed || 0).toFixed(0)} km/h</span><span class="weather-colour" style="background:rgb(${rgb.join(",")})"></span></div><div class="muted">Generated mood: <strong>${mood.effect}</strong> · ${mood.speed} ms · ${mood.brightness}% brightness</div>`;
    weatherUpdatedEl.textContent = new Date().toLocaleTimeString();
    log(`Weather: ${data.location.name} ${c.temperature.toFixed(1)}°C — ${c.description}`);
    if (weatherAutoEl.checked) applyWeatherToTable(false);
  } catch (e) {
    weatherResultEl.textContent = "Weather lookup failed: " + e.message;
    log("Weather lookup failed: " + e.message);
  }
}

function applyWeatherToEditor() {
  if (!lastWeather) return;
  applyMood(lastWeather.mood);
  moodName.value = `WEATHER_${(lastWeather.location.name || "LOCAL").replace(/[^A-Z0-9]+/gi, "_").toUpperCase()}`;
  editorMessage.textContent = `Weather mood loaded for ${lastWeather.location.name}. Save it if you want to keep it.`;
}

async function applyWeatherToTable(showMessage=true) {
  if (!lastWeather) await getWeather();
  if (!lastWeather) return;
  try {
    const r = await fetch("/api/mood/test", {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(lastWeather.mood)});
    const data = await r.json();
    if (!data.ok) throw new Error(data.error);
    if (showMessage) log(`Applied weather mood for ${lastWeather.location.name}.`);
  } catch (e) { log("Weather apply failed: " + e.message); }
}

async function applyWeatherMood() {
  await getWeather();
  await applyWeatherToTable(true);
}

function updateWeatherTimer() {
  if (weatherTimer) clearInterval(weatherTimer);
  weatherTimer = null;
  if (weatherAutoEl.checked) {
    weatherTimer = setInterval(() => getWeather(), 15 * 60 * 1000);
    if (lastWeather) applyWeatherToTable(false);
  }
}

async function aiMood(name) {
  try {
    const r = await fetch("/api/ai/mood", {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({mood:name})});
    const data = await r.json();
    if (!data.ok) throw new Error(data.error);
    log(`AI mood: ${name}`);
  } catch (e) { log("AI mood failed: " + e.message); }
}

async function aiWeatherMood() {
  const location = weatherLocationEl.value.trim();
  if (!location) return log("Enter a weather location first.");
  try {
    const r = await fetch("/api/ai/mood", {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({mood:"WEATHER", location})});
    const data = await r.json();
    if (!data.ok) throw new Error(data.error);
    log(`AI weather mood applied for ${location}.`);
  } catch (e) { log("AI weather mood failed: " + e.message); }
}

function escapeHtml(value) {
  return String(value).replace(/[&<>'"]/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;","'":"&#39;",'"':"&quot;"}[c]));
}

async function checkStatus() {
  try { const r=await fetch("/api/status"); const data=await r.json(); setStatus(data.connected,data.port||""); } catch (_) {}
}

buildPalette();
setCurrentColor("#00FF66");
renderMatrix();
refreshPorts();
checkStatus();
loadMoods();
weatherAutoEl.addEventListener("change", updateWeatherTimer);

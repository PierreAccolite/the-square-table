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

let moods = {};
let pixels = Array.from({length: 16}, () => [0, 0, 0]);
let currentColor = [0, 255, 102];

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

async function checkStatus() {
  try { const r=await fetch("/api/status"); const data=await r.json(); setStatus(data.connected,data.port||""); } catch (_) {}
}

buildPalette();
setCurrentColor("#00FF66");
renderMatrix();
refreshPorts();
checkStatus();
loadMoods();

const statusEl = document.getElementById("status");
const portEl = document.getElementById("port");
const logEl = document.getElementById("log");
const connectionLog = document.getElementById("connectionLog");

function log(msg) {
  const line = `[${new Date().toLocaleTimeString()}] ${msg}`;
  logEl.textContent += (logEl.textContent ? "\n" : "") + line;
  connectionLog.textContent = line;
}

function setStatus(connected, port="") {
  statusEl.className = "status " + (connected ? "connected" : "disconnected");
  statusEl.textContent = connected
    ? `🟢 Pico connected — ${port}`
    : "Pico disconnected";
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

    if (data.connected && [...portEl.options].some(o => o.value === data.connected)) {
      portEl.value = data.connected;
    } else if (portEl.options.length > 0) {
      portEl.selectedIndex = 0;
    }

    log("Found serial ports: " + data.ports.map(p => p.device).join(", "));
  } catch (e) {
    log("Port refresh failed: " + e);
  }
}

async function connectPico() {
  const port = portEl.value;
  if (!port) {
    log("Please select a COM port.");
    return;
  }

  log("Opening " + port + "...");

  const r = await fetch("/api/connect", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify({port})
  });

  const data = await r.json();

  if (data.ok) {
    setStatus(true, data.port);
    log("Connected to " + data.port);
  } else {
    setStatus(false);
    log("ERROR: " + data.error);
  }
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

async function mood(name) {
  await send(name);
}

async function send(command) {
  try {
    const r = await fetch("/api/send", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({command})
    });

    const data = await r.json();

    if (data.ok) {
      log("Sent: " + command);
    } else {
      log("ERROR: " + data.error);
    }
  } catch (e) {
    log("Send failed: " + e);
  }
}

async function checkStatus() {
  try {
    const r = await fetch("/api/status");
    const data = await r.json();
    setStatus(data.connected, data.port || "");
  } catch (_) {}
}

refreshPorts();
checkStatus();

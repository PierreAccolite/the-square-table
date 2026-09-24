let port = null;
let reader = null;
let writer = null;
let keepReading = false;
let receiveBuffer = "";

const $ = id => document.getElementById(id);
const log = message => {
  const box = $("console");
  box.textContent += message + "\n";
  box.scrollTop = box.scrollHeight;
};

function setConnected(connected) {
  $("connect").disabled = connected;
  $("disconnect").disabled = !connected;
  $("ping").disabled = !connected;
  $("streamOn").disabled = !connected;
  $("streamOff").disabled = !connected;
  $("status").textContent = "Status: " + (connected ? "Connected" : "Disconnected");
}

async function connectSerial() {
  if (!("serial" in navigator)) {
    $("status").textContent = "Status: Web Serial is not available in this browser";
    log("ERROR: navigator.serial is not available.");
    return;
  }

  try {
    port = await navigator.serial.requestPort();

    await port.open({
      baudRate: 115200,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      bufferSize: 4096,
      flowControl: "none"
    });

    writer = port.writable.getWriter();
    setConnected(true);

    const info = port.getInfo();
    $("device").textContent =
      "Device: VID " + (info.usbVendorId ?? "—") +
      " / PID " + (info.usbProductId ?? "—");

    log("CONNECTED");
    await sendLine("PING");

    keepReading = true;
    readLoop();
  } catch (err) {
    log("CONNECT ERROR: " + err);
    setConnected(false);
  }
}

async function readLoop() {
  if (!port || !port.readable) return;

  reader = port.readable.getReader();

  try {
    while (keepReading) {
      const { value, done } = await reader.read();
      if (done) break;
      if (!value) continue;

      receiveBuffer += new TextDecoder().decode(value);

      let newline;
      while ((newline = receiveBuffer.indexOf("\n")) >= 0) {
        const line = receiveBuffer.slice(0, newline).replace(/\r$/, "");
        receiveBuffer = receiveBuffer.slice(newline + 1);
        handleLine(line);
      }
    }
  } catch (err) {
    log("READ ERROR: " + err);
  } finally {
    try { reader.releaseLock(); } catch (_) {}
    reader = null;
  }
}

function handleLine(line) {
  if (!line) return;

  log("< " + line);

  if (line.startsWith("JOY,")) {
    const parts = line.split(",");
    if (parts.length >= 4) {
      $("joyX").textContent = parts[1];
      $("joyY").textContent = parts[2];
      $("joyBtn").textContent = parts[3];
    }
  }

  if (line.startsWith("JOYRAW,")) {
    const parts = line.split(",");
    if (parts.length >= 4) {
      $("joyX").textContent = parts[1] + " raw";
      $("joyY").textContent = parts[2] + " raw";
      $("joyBtn").textContent = parts[3];
    }
  }
}

async function sendLine(line) {
  if (!writer) return;
  const data = new TextEncoder().encode(line + "\n");
  await writer.write(data);
  log("> " + line);
}

async function disconnectSerial() {
  keepReading = false;

  try {
    if (reader) {
      await reader.cancel();
      reader.releaseLock();
      reader = null;
    }
  } catch (_) {}

  try {
    if (writer) {
      writer.releaseLock();
      writer = null;
    }
  } catch (_) {}

  try {
    if (port) await port.close();
  } catch (_) {}

  port = null;
  setConnected(false);
  $("device").textContent = "Device: —";
  log("DISCONNECTED");
}

$("connect").addEventListener("click", connectSerial);
$("disconnect").addEventListener("click", disconnectSerial);
$("ping").addEventListener("click", () => sendLine("PING"));
$("streamOn").addEventListener("click", () => sendLine("INPUT"));
$("streamOff").addEventListener("click", () => sendLine("INPUT OFF"));

if (!("serial" in navigator)) {
  log("WARNING: This browser does not expose Web Serial.");
}

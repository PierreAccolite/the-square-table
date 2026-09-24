const POCKET_ARCADE_BAUD = 74880;

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

    // The Pocket Arcade protocol remains unchanged. The USB-UART link
    // is opened internally at the proven working rate; the user does
    // not need to select or know the serial rate.
    await port.open({
      baudRate: POCKET_ARCADE_BAUD,
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
    log("Pocket Arcade USB serial link ready.");

    // Opening the USB serial port can reset an ESP32.
    // Start the reader immediately, then allow boot messages to finish
    // before sending our first command.
    keepReading = true;
    readLoop();

    log("Waiting 2 seconds for ESP32 boot...");
    await new Promise(resolve => setTimeout(resolve, 2000));

    if (keepReading && writer) {
      await sendLine("PING");
    }
  } catch (err) {
    log("CONNECT ERROR: " + err);
    setConnected(false);
  }
}

async function readLoop() {
  if (!port || !port.readable) return;

  let framingRetries = 0;

  while (keepReading && port && port.readable) {
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

      framingRetries = 0;
      break;
    } catch (err) {
      const message = String(err);

      if (message.includes("FramingError") || message.toLowerCase().includes("framing error")) {
        framingRetries++;

        log("SERIAL FRAMING ERROR - retry " + framingRetries + "/5");

        try { reader.releaseLock(); } catch (_) {}
        reader = null;

        if (framingRetries >= 5 || !keepReading) {
          log("READ STOPPED: repeated framing errors.");
          break;
        }

        await new Promise(resolve => setTimeout(resolve, 100));
        continue;
      }

      log("READ ERROR: " + message);
      break;
    } finally {
      if (reader) {
        try { reader.releaseLock(); } catch (_) {}
        reader = null;
      }
    }
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
      updateStick(Number(parts[1]), Number(parts[2]));
    }
  }

  if (line.startsWith("JOYRAW,")) {
    const parts = line.split(",");
    if (parts.length >= 4) {
      $("joyX").textContent = parts[1] + " raw";
      $("joyY").textContent = parts[2] + " raw";
      $("joyBtn").textContent = parts[3];
      updateStick(Number(parts[1]), Number(parts[2]));
    }
  }
}

function updateStick(x, y) {
  const dot = $("stickDot");
  if (!dot || !Number.isFinite(x) || !Number.isFinite(y)) return;

  const clampedX = Math.max(-100, Math.min(100, x));
  const clampedY = Math.max(-100, Math.min(100, y));

  // Screen Y is inverted: joystick +Y moves the dot upward.
  const left = 50 + clampedX * 0.42;
  const top = 50 - clampedY * 0.42;

  dot.style.left = left + "%";
  dot.style.top = top + "%";
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

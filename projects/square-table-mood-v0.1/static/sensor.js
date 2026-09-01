async function sendSensorTemperatureToLED() {
    const button = document.getElementById("sensorLedButton");
    const original = button ? button.textContent : "▶ Send Temperature to LEDs";

    if (button) {
        button.disabled = true;
        button.textContent = "Reading DHT11…";
    }

    try {
        const response = await fetch("/api/local-temperature/apply", {
            method: "POST",
            headers: { "Content-Type": "application/json" }
        });
        const data = await response.json();

        if (!data.ok) {
            throw new Error(data.error || "Could not apply temperature.");
        }

        const hex = data.hex || "#FFFFFF";
        const result = document.getElementById("sensorResult");
        if (result) {
            result.dataset.rgb = JSON.stringify(data.temperature_color);
        }

        if (typeof log === "function") {
            log(`Temperature sent to LEDs: ${Number(data.temperature).toFixed(1)}°C → ${hex}`);
        }

        if (button) {
            button.textContent = `✓ ${Number(data.temperature).toFixed(1)}°C → ${hex}`;
        }
    } catch (error) {
        if (typeof log === "function") {
            log("Temperature LED update failed: " + error.message);
        }
        if (button) {
            button.textContent = "✕ Failed — Try Again";
        }
    } finally {
        setTimeout(() => {
            if (button) {
                button.disabled = false;
                button.textContent = original;
            }
        }, 1800);
    }
}

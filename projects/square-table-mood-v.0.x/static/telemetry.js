(function () {
  let timer = null;

  function set(id, value) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
  }

  async function updateTelemetry() {
    try {
      const response = await fetch('/api/local-temperature', { cache: 'no-store' });
      const data = await response.json();
      if (!data.ok) throw new Error(data.error || 'Sensor unavailable');
      const rgb = data.temperature_color || [255, 255, 255];
      const hex = '#' + rgb.map(v => Math.max(0, Math.min(255, v)).toString(16).padStart(2, '0')).join('').toUpperCase();
      set('headerAmbientTemp', `${Number(data.temperature).toFixed(1)}°C`);
      set('headerHumidity', `${Number(data.humidity).toFixed(0)}% RH`);
      set('headerPicoTemp', `${Number(data.pico_temperature).toFixed(1)}°C`);
      const swatch = document.getElementById('headerTempSwatch');
      if (swatch) { swatch.style.background = hex; swatch.style.boxShadow = `0 0 18px ${hex}`; }
      const sensor = document.getElementById('sensorResult');
      if (sensor && !sensor.dataset.rgb) sensor.dataset.rgb = JSON.stringify(rgb);
    } catch (_) {
      set('headerAmbientTemp', '—');
      set('headerHumidity', '—');
      set('headerPicoTemp', '—');
    }
  }

  window.updateTelemetry = updateTelemetry;
  function start() {
    updateTelemetry();
    if (timer) clearInterval(timer);
    timer = setInterval(updateTelemetry, 10000);
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
  else start();
})();
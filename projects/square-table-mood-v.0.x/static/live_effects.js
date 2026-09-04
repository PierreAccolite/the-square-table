(function () {
  function ui(name) { return document.getElementById(name); }
  function hexToRgb(value) {
    const v = String(value || '#FFFFFF').replace('#', '');
    return [parseInt(v.slice(0,2),16) || 0, parseInt(v.slice(2,4),16) || 0, parseInt(v.slice(4,6),16) || 0];
  }
  function setMessage(text) { const box = ui('presetMessage'); if (box) box.textContent = text; }

  // Use the same 4x4 / 4x8 selection as the unified Matrix Studio.
  function getMatrixWidth() {
    try {
      if (typeof window.getEditorMood === 'function') {
        const mood = window.getEditorMood();
        if (Number(mood.width) === 8 || (Array.isArray(mood.pixels) && mood.pixels.length === 32)) return 8;
      }
    } catch (_) {}
    return ui('matrixWide')?.checked ? 8 : 4;
  }

  window.matrixPreset = async function (name) {
    const width = getMatrixWidth();
    const count = width === 8 ? 32 : 16;
    const brightness = Number(ui('presetBrightness')?.value || 70);
    let payload = { name, width, brightness };

    if (name === 'CODE_RAIN') {
      payload.type = 'effect';
      payload.effect = 'CODE_RAIN';
      payload.color = hexToRgb(ui('codeColor')?.value || '#00FF66');
      payload.accent = hexToRgb(ui('codeAccent')?.value || '#FFFFFF');
      payload.speed = 90;
    } else if (name === 'PASTEL_CLOUDS') {
      payload.type = 'effect';
      payload.effect = 'PASTEL_CLOUDS';
      const ids = ['cloudColor1','cloudColor2','cloudColor3','cloudColor4'];
      const n = Number(ui('cloudCount')?.value || 4);
      payload.colors = ids.slice(0, n).map(id => hexToRgb(ui(id)?.value || '#FFFFFF'));
      payload.speed = 70;
    } else if (name === 'TEMPERATURE') {
      try {
        const el = document.querySelector('.sensor-temp');
        const temp = el ? parseFloat(el.textContent) : 20;
        const r = await fetch('/api/local-temperature/apply-reading', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({temperature:Number.isFinite(temp) ? temp : 20, wide: width === 8}) });
        const d = await r.json();
        if (!d.ok) throw Error(d.error);
        setMessage(`🌡 ${temp.toFixed(1)}°C → ${d.hex} on ${count} LEDs (${width === 8 ? '4×8' : '4×4'}).`);
        return;
      } catch (e) { setMessage('Temperature preset failed: ' + e.message); return; }
    } else return;

    try {
      const r = await fetch('/api/effect/start', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(payload) });
      const d = await r.json();
      if (!d.ok) throw Error(d.error);
      setMessage(`▶ ${name.replaceAll('_',' ')} running continuously — ${width === 8 ? '4×8 / 32 LEDs' : '4×4 / 16 LEDs'}, ${brightness}% brightness.`);
      if (typeof log === 'function') log(`Live effect started: ${name} (${width === 8 ? '4×8 / 32 LEDs' : '4×4 / 16 LEDs'})`);
    } catch (e) {
      setMessage('Preset failed: ' + e.message);
      if (typeof log === 'function') log('Live effect failed: ' + e.message);
    }
  };

  window.stopMatrixPreset = async function () {
    try {
      const r = await fetch('/api/effect/stop', { method:'POST' });
      const d = await r.json();
      if (!d.ok) throw Error(d.error);
      setMessage('Preset stopped.');
      if (typeof log === 'function') log('Live effect stopped.');
    } catch (e) {
      setMessage('Stop failed: ' + e.message);
    }
  };
})();
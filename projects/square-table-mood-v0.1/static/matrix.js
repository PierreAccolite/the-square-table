(function () {
  let wide = false;
  let matrixPixels = [];
  let wideMoods = {};
  let lastAutoTemp = null;

  const originalGetEditorMood = window.getEditorMood;
  const originalApplyMood = window.applyMood;
  const originalLoadSelectedMood = window.loadSelectedMood;
  const originalSaveMood = window.saveMood;
  const originalDeleteMood = window.deleteMood;
  const originalReadLocalSensor = window.readLocalSensor;

  function clonePixels(p) { return (p || []).map(x => [Number(x[0]) || 0, Number(x[1]) || 0, Number(x[2]) || 0]); }
  function blank(n) { return Array.from({ length: n }, () => [0, 0, 0]); }
  function hex(rgb) { return '#' + rgb.map(v => Math.max(0, Math.min(255, v)).toString(16).padStart(2, '0')).join('').toUpperCase(); }

  function ensurePixels() {
    if (!matrixPixels.length) {
      const base = originalGetEditorMood ? originalGetEditorMood().pixels : [];
      matrixPixels = clonePixels(base.length ? base : blank(wide ? 32 : 16));
    }
    if (wide && matrixPixels.length === 16) matrixPixels = matrixPixels.concat(blank(16));
    if (!wide && matrixPixels.length === 32) matrixPixels = matrixPixels.slice(0, 16);
  }

  function render() {
    ensurePixels();
    const matrix = document.getElementById('matrix');
    if (!matrix) return;
    matrix.classList.toggle('matrix-wide', wide);
    matrix.innerHTML = '';
    matrixPixels.forEach((rgb, index) => {
      const cell = document.createElement('button');
      cell.className = 'led-cell';
      cell.style.backgroundColor = hex(rgb);
      cell.style.boxShadow = rgb.some(v => v > 0) ? `0 0 18px ${hex(rgb)}` : 'none';
      cell.title = `LED ${index}`;
      cell.onclick = () => {
        const picker = document.getElementById('colorPicker');
        const colour = picker ? picker.value : '#00FF66';
        const v = colour.replace('#', '');
        matrixPixels[index] = [parseInt(v.slice(0,2),16), parseInt(v.slice(2,4),16), parseInt(v.slice(4,6),16)];
        render();
      };
      matrix.appendChild(cell);
    });
  }

  window.renderMatrix = render;
  window.getEditorMood = function () {
    ensurePixels();
    const effect = document.getElementById('effect'), speed = document.getElementById('speed'), brightness = document.getElementById('brightness'), name = document.getElementById('moodName');
    return { name: (name && name.value.trim() ? name.value.trim() : 'CUSTOM').toUpperCase(), pixels: clonePixels(matrixPixels), effect: effect ? effect.value : 'STATIC', speed: speed ? Number(speed.value) : 100, brightness: brightness ? Number(brightness.value) : 100, width: wide ? 8 : 4 };
  };

  window.applyMood = function (m) {
    if (!m) return;
    wide = Array.isArray(m.pixels) && m.pixels.length === 32;
    matrixPixels = clonePixels(m.pixels || blank(wide ? 32 : 16));
    if (matrixPixels.length !== 16 && matrixPixels.length !== 32) matrixPixels = blank(wide ? 32 : 16);
    const toggle = document.getElementById('matrixWide'); if (toggle) toggle.checked = wide;
    const name = document.getElementById('moodName'), effect = document.getElementById('effect'), speed = document.getElementById('speed'), brightness = document.getElementById('brightness');
    if (name) name.value = m.name || '';
    if (effect) effect.value = m.effect || 'STATIC';
    if (speed) { speed.value = m.speed || 100; const x = document.getElementById('speedValue'); if (x) x.textContent = speed.value; }
    if (brightness) { brightness.value = m.brightness || 100; const x = document.getElementById('brightnessValue'); if (x) x.textContent = brightness.value; }
    render();
  };

  window.loadSelectedMood = function () {
    const select = document.getElementById('moodSelect'); if (!select) return;
    const name = select.value;
    if (wideMoods[name]) window.applyMood(wideMoods[name]);
    else if (typeof originalLoadSelectedMood === 'function') originalLoadSelectedMood();
  };

  window.clearMatrix = function () { ensurePixels(); matrixPixels = blank(wide ? 32 : 16); render(); };
  window.fillMatrix = function () {
    const picker = document.getElementById('colorPicker'); const v = (picker ? picker.value : '#00FF66').replace('#','');
    const rgb = [parseInt(v.slice(0,2),16), parseInt(v.slice(2,4),16), parseInt(v.slice(4,6),16)];
    matrixPixels = Array.from({ length: wide ? 32 : 16 }, () => [...rgb]); render();
  };
  window.invertMatrix = function () { ensurePixels(); matrixPixels = matrixPixels.map(p => p.some(v => v > 0) ? [0,0,0] : [255,255,255]); render(); };

  window.saveMood = async function () {
    const m = window.getEditorMood();
    if (!m.name || m.name === 'CUSTOM') { const e = document.getElementById('editorMessage'); if (e) e.textContent = 'Enter a mood name first.'; return; }
    if (!wide && typeof originalSaveMood === 'function') return originalSaveMood();
    try {
      const r = await fetch('/api/matrix/moods', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(m) });
      const d = await r.json(); if (!d.ok) throw Error(d.error);
      wideMoods[m.name] = d.mood;
      const select = document.getElementById('moodSelect');
      if (select && ![...select.options].some(o => o.value === m.name)) { const o=document.createElement('option'); o.value=m.name; o.textContent=`${m.name} (4×8)`; select.appendChild(o); }
      if (select) select.value=m.name;
      const e=document.getElementById('editorMessage'); if(e)e.textContent=`Saved ${m.name} (4×8).`;
      if(typeof log==='function')log(`Saved 4×8 mood: ${m.name}`);
    } catch(e) { const box=document.getElementById('editorMessage'); if(box)box.textContent='Save failed: '+e.message; }
  };

  window.deleteMood = async function () {
    const select=document.getElementById('moodSelect'), n=select?select.value:'';
    if(!n||!confirm(`Delete mood ${n}?`))return;
    if(!wideMoods[n]&&typeof originalDeleteMood==='function')return originalDeleteMood();
    try { const r=await fetch(`/api/matrix/moods/${encodeURIComponent(n)}`,{method:'DELETE'}),d=await r.json(); if(!d.ok)throw Error(d.error); delete wideMoods[n]; const opt=select&&[...select.options].find(o=>o.value===n); if(opt)opt.remove(); if(typeof log==='function')log(`Deleted 4×8 mood: ${n}`); }
    catch(e){const box=document.getElementById('editorMessage');if(box)box.textContent='Delete failed: '+e.message;}
  };

  window.testMood = async function () {
    const m=window.getEditorMood();
    try { const r=await fetch('/api/matrix/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(m)}),d=await r.json(); if(!d.ok)throw Error(d.error); const box=document.getElementById('editorMessage');if(box)box.textContent=`Tested ${m.name} — ${m.width===8?'4×8':'4×4'} ${m.effect}.`;if(typeof log==='function')log(`Tested ${m.name} (${m.width===8?'4×8':'4×4'}, ${m.effect})`); }
    catch(e){if(typeof log==='function')log('Matrix test failed: '+e.message);}
  };

  function addControls(){
    const matrix=document.getElementById('matrix'); if(!matrix||document.getElementById('matrixWide'))return;
    const tools=matrix.parentElement.querySelector('.matrix-tools'); const box=document.createElement('div'); box.className='matrix-mode-controls';
    box.innerHTML=`<label><input id="matrixWide" type="checkbox"> 4×8 mode — second 4×4 matrix</label><span id="matrixModeText">Current: 4×4 / 16 LEDs</span>`; (tools||matrix).after(box);
    document.getElementById('matrixWide').addEventListener('change',e=>{ensurePixels();wide=e.target.checked;if(wide&&matrixPixels.length===16)matrixPixels=matrixPixels.concat(blank(16));if(!wide&&matrixPixels.length===32)matrixPixels=matrixPixels.slice(0,16);document.getElementById('matrixModeText').textContent=`Current: ${wide?'4×8 / 32 LEDs':'4×4 / 16 LEDs'}`;render();});
  }

  function addPresetUI(){
    if(document.getElementById('matrixPresets'))return;
    const section=document.createElement('section');section.className='preset-card';section.id='matrixPresets';
    section.innerHTML=`<div class="section-title"><div><h3>✨ Live Matrix Presets</h3><p class="muted">Animated scenes rendered by the Pico. Best in 4×8 mode, but also works on 4×4.</p></div></div><div class="buttons"><button onclick="matrixPreset('CODE_RAIN')">⌨ Matrix Code Rain</button><button onclick="matrixPreset('PASTEL_CLOUDS')">☁ Pastel Cloud Mesh</button><button onclick="matrixPreset('TEMPERATURE')">🌡 Temperature Colour</button><button onclick="stopMatrixPreset()">■ Stop Preset</button></div><div id="presetMessage" class="editor-message"></div>`;
    const sensor=document.querySelector('.sensor-card');(sensor||document.body.lastElementChild).before(section);
  }

  function pastel(t,x,y){const a=(Math.sin(t*.055+x*.7+y*1.2)+1)/2,b=(Math.sin(t*.037+x*1.1-y*.8)+1)/2,c=(Math.sin(t*.021+x*.35+y*1.7)+1)/2;return[180+Math.round(a*75),180+Math.round(b*75),180+Math.round(c*75)];}
  function rain(t,count){const w=count===32?8:4,h=4,f=blank(count);for(let c=0;c<w;c++){const head=(Math.floor(t/180)+c*2)%(h+4)-2;for(let r=0;r<h;r++){const d=head-r;if(d>=0&&d<=2)f[r*w+c]=d===0?[180,255,220]:[0,Math.max(40,170-d*55),Math.max(20,90-d*30)];}}return f;}
  function clouds(t,count){const w=count===32?8:4,f=blank(count);for(let r=0;r<4;r++)for(let c=0;c<w;c++)f[r*w+c]=pastel(t,c,r);return f;}

  window.matrixPreset=async function(name){
    const count=wide?32:16;
    if(name==='TEMPERATURE'){
      const el=document.querySelector('.sensor-temp'); const temp=el?parseFloat(el.textContent):20;
      try{const r=await fetch('/api/local-temperature/apply-reading',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({temperature:Number.isFinite(temp)?temp:20,wide})}),d=await r.json();const box=document.getElementById('presetMessage');if(box)box.textContent=d.ok?`🌡 ${temp.toFixed(1)}°C → ${d.hex} on ${count} LEDs.`:`Temperature failed: ${d.error}`;}catch(e){}
      return;
    }
    const frames=[];for(let i=0;i<24;i++)frames.push({pixels:name==='CODE_RAIN'?rain(i*140,count):clouds(i*140,count),transition:'CUT',duration:80,hold:80,brightness:70});
    try{const r=await fetch('/api/feed/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name,frames,loop:true})}),d=await r.json();const box=document.getElementById('presetMessage');if(box)box.textContent=d.ok?`▶ ${name.replaceAll('_',' ')} running on ${count} LEDs.`:`Preset failed: ${d.error}`;if(typeof log==='function'&&d.ok)log(`Live preset: ${name}`);}catch(e){}
  };
  window.stopMatrixPreset=async function(){try{await fetch('/api/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:'IDLE'})});}catch(e){}const box=document.getElementById('presetMessage');if(box)box.textContent='Preset stopped.';};

  async function loadWideMoods(){try{const r=await fetch('/api/matrix/moods');wideMoods=await r.json();const select=document.getElementById('moodSelect');if(!select)return;Object.keys(wideMoods).sort().forEach(n=>{if(![...select.options].some(o=>o.value===n)){const o=document.createElement('option');o.value=n;o.textContent=`${n} (4×8)`;select.appendChild(o);}});}catch(e){}}

  async function wrappedSensorRead(){
    if(typeof originalReadLocalSensor==='function')await originalReadLocalSensor();
    const result=document.getElementById('sensorResult'),text=result?result.querySelector('.sensor-temp'):null;if(!text)return;
    const temp=parseFloat(text.textContent);if(!Number.isFinite(temp))return;
    const auto=document.getElementById('sensorAuto');if(!auto||!auto.checked)return;
    if(lastAutoTemp!==null&&Math.abs(temp-lastAutoTemp)<1)return;
    try{const r=await fetch('/api/local-temperature/apply-reading',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({temperature:temp,wide})}),d=await r.json();if(!d.ok)throw Error(d.error);lastAutoTemp=temp;if(typeof log==='function')log(`Auto temperature LED update: ${temp.toFixed(1)}°C → ${d.hex}`);}catch(e){if(typeof log==='function')log('Auto temperature LED update failed: '+e.message);}
  }
  window.readLocalSensor=wrappedSensorRead;

  document.addEventListener('DOMContentLoaded',()=>{addControls();addPresetUI();matrixPixels=originalGetEditorMood?clonePixels(originalGetEditorMood().pixels):blank(16);render();loadWideMoods();});
})();

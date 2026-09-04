/* Square Table — Unified Matrix Editor
 * One logical row-major pixel order is used everywhere.
 * The Pico applies the physical serpentine mapping.
 */
(function () {
  let wide = false;
  let pixels = [];
  let moods = {};

  const MAP_4X4 = [0,1,2,3,7,6,5,4,8,9,10,11,15,14,13,12];
  const MAP_4X8 = [0,1,2,3,4,5,6,7,15,14,13,12,11,10,9,8,16,17,18,19,20,21,22,23,31,30,29,28,27,26,25,24];

  function count() { return wide ? 32 : 16; }
  function blank(n) { return Array.from({length:n}, () => [0,0,0]); }
  function clone(p) { return (p || []).map(x => [Number(x[0]) || 0, Number(x[1]) || 0, Number(x[2]) || 0]); }
  function hex(p) { return '#' + p.map(v => Math.max(0,Math.min(255,Math.round(v))).toString(16).padStart(2,'0')).join('').toUpperCase(); }
  function fromHex(h) { const v=h.replace('#',''); return [parseInt(v.slice(0,2),16)||0,parseInt(v.slice(2,4),16)||0,parseInt(v.slice(4,6),16)||0]; }
  function physicalNumber(logical) {
    const map = wide ? MAP_4X8 : MAP_4X4;
    const physicalIndex = map[logical];
    return physicalIndex + 1;
  }

  function ensure() {
    if (!pixels.length) pixels = blank(count());
    if (wide && pixels.length === 16) pixels = pixels.concat(blank(16));
    if (!wide && pixels.length === 32) pixels = pixels.slice(0,16);
  }

  function render() {
    ensure();
    const matrix = document.getElementById('matrix');
    if (!matrix) return;
    matrix.classList.toggle('matrix-wide', wide);
    matrix.innerHTML = '';
    pixels.forEach((rgb, i) => {
      const cell = document.createElement('button');
      cell.type = 'button';
      cell.className = 'led-cell';
      cell.style.backgroundColor = hex(rgb);
      cell.style.boxShadow = rgb.some(v => v > 0) ? `0 0 18px ${hex(rgb)}` : 'none';
      cell.title = `Logical ${i + 1} → Physical LED ${physicalNumber(i)}`;
      cell.setAttribute('aria-label', cell.title);
      cell.onclick = () => {
        const picker = document.getElementById('colorPicker');
        pixels[i] = fromHex(picker ? picker.value : '#00FF66');
        render();
      };
      matrix.appendChild(cell);
    });
  }

  function setMode(value) {
    wide = !!value;
    ensure();
    const toggle = document.getElementById('matrixWide');
    if (toggle) toggle.checked = wide;
    const text = document.getElementById('matrixModeText');
    if (text) text.textContent = `Current: ${wide ? '4×8 / 32 LEDs' : '4×4 / 16 LEDs'} · physical serpentine mapping active`;
    render();
  }

  window.setMatrixWide = setMode;
  window.renderMatrix = render;
  window.getEditorMood = function () {
    ensure();
    return {
      name: ((document.getElementById('moodName')?.value || 'CUSTOM').trim() || 'CUSTOM').toUpperCase(),
      pixels: clone(pixels),
      effect: document.getElementById('effect')?.value || 'STATIC',
      speed: Number(document.getElementById('speed')?.value || 100),
      brightness: Number(document.getElementById('brightness')?.value || 100),
      width: wide ? 8 : 4
    };
  };

  window.applyMood = function (m) {
    if (!m) return;
    pixels = clone(m.pixels);
    wide = pixels.length === 32 || Number(m.width) === 8;
    if (pixels.length !== 16 && pixels.length !== 32) pixels = blank(count());
    const name=document.getElementById('moodName'), effect=document.getElementById('effect'), speed=document.getElementById('speed'), brightness=document.getElementById('brightness');
    if (name) name.value=m.name || '';
    if (effect) effect.value=m.effect || 'STATIC';
    if (speed) { speed.value=m.speed || 100; const x=document.getElementById('speedValue'); if(x)x.textContent=speed.value; }
    if (brightness) { brightness.value=m.brightness || 100; const x=document.getElementById('brightnessValue'); if(x)x.textContent=brightness.value; }
    const toggle=document.getElementById('matrixWide'); if(toggle)toggle.checked=wide;
    render();
  };

  window.clearMatrix = function () { pixels=blank(count()); render(); };
  window.fillMatrix = function () { const p=document.getElementById('colorPicker'); const rgb=fromHex(p ? p.value : '#00FF66'); pixels=Array.from({length:count()},()=>[...rgb]); render(); };
  window.invertMatrix = function () { ensure(); pixels=pixels.map(p=>p.some(v=>v>0)?[0,0,0]:[255,255,255]); render(); };

  window.loadSelectedMood = function () {
    const select=document.getElementById('moodSelect');
    if (!select || !select.value) return;
    const m=moods[select.value];
    if (m) window.applyMood(m);
  };

  async function loadUnifiedMoods() {
    try {
      const r=await fetch('/api/matrix/moods',{cache:'no-store'});
      const data=await r.json();
      moods=(data && typeof data==='object') ? data : {};
      const select=document.getElementById('moodSelect');
      if (!select) return;
      select.innerHTML='';
      Object.keys(moods).sort().forEach(name=>{
        const o=document.createElement('option');
        o.value=name;
        const m=moods[name];
        o.textContent=`${name} (${Array.isArray(m.pixels)&&m.pixels.length===32?'4×8':'4×4'})`;
        select.appendChild(o);
      });
      if (select.options.length) {
        select.selectedIndex=0;
        window.applyMood(moods[select.value]);
      } else {
        pixels=blank(16);
        render();
      }
    } catch (e) {
      const box=document.getElementById('editorMessage');
      if(box) box.textContent='Could not load matrix moods: '+e.message;
    }
  }

  window.saveMood = async function () {
    const m=window.getEditorMood();
    if (!m.name || m.name==='CUSTOM') { const e=document.getElementById('editorMessage'); if(e)e.textContent='Enter a mood name first.'; return; }
    try {
      const r=await fetch('/api/matrix/moods',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(m)});
      const d=await r.json();
      if(!d.ok) throw Error(d.error || 'Save failed');
      moods[m.name]=d.mood;
      const select=document.getElementById('moodSelect');
      select.innerHTML='';
      Object.keys(moods).sort().forEach(name=>{const o=document.createElement('option');o.value=name;o.textContent=`${name} (${moods[name].pixels.length===32?'4×8':'4×4'})`;select.appendChild(o);});
      select.value=m.name;
      const e=document.getElementById('editorMessage'); if(e)e.textContent=`Saved ${m.name} — ${m.width===8?'4×8':'4×4'}.`;
      if(typeof log==='function')log(`Saved matrix mood: ${m.name} (${m.width===8?'4×8':'4×4'})`);
    } catch(e) { const box=document.getElementById('editorMessage'); if(box)box.textContent='Save failed: '+e.message; }
  };

  window.deleteMood = async function () {
    const select=document.getElementById('moodSelect'), name=select?.value;
    if(!name || !confirm(`Delete mood ${name}?`)) return;
    try {
      const r=await fetch(`/api/matrix/moods/${encodeURIComponent(name)}`,{method:'DELETE'}), d=await r.json();
      if(!d.ok) throw Error(d.error || 'Delete failed');
      delete moods[name];
      await loadUnifiedMoods();
      const e=document.getElementById('editorMessage'); if(e)e.textContent=`Deleted ${name}.`;
    } catch(e) { const box=document.getElementById('editorMessage'); if(box)box.textContent='Delete failed: '+e.message; }
  };

  window.testMood = async function () {
    const m=window.getEditorMood();
    try {
      const r=await fetch('/api/matrix/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(m)}), d=await r.json();
      if(!d.ok) throw Error(d.error || 'Test failed');
      const e=document.getElementById('editorMessage'); if(e)e.textContent=`Tested ${m.name} — ${m.width===8?'4×8':'4×4'} ${m.effect}.`;
      if(typeof log==='function')log(`Tested ${m.name} (${m.width===8?'4×8':'4×4'}, ${m.effect})`);
    } catch(e) { const box=document.getElementById('editorMessage'); if(box)box.textContent='Test failed: '+e.message; if(typeof log==='function')log('Matrix test failed: '+e.message); }
  };

  function addMappingNote() {
    const matrix=document.getElementById('matrix');
    if(!matrix || document.getElementById('matrixMappingNote')) return;
    const note=document.createElement('div');
    note.id='matrixMappingNote';
    note.style.width='min(90vw,760px)';
    note.style.margin='0 auto 10px';
    note.style.textAlign='center';
    note.style.color='#9aa4b2';
    note.style.fontSize='.85rem';
    note.textContent='Editor order is row-major. Pico converts it to the physical serpentine LED order.';
    matrix.parentElement.insertBefore(note,matrix);
  }

  function init() {
    addMappingNote();
    loadUnifiedMoods();
  }

  if(document.readyState==='loading') document.addEventListener('DOMContentLoaded',init);
  else init();
})();

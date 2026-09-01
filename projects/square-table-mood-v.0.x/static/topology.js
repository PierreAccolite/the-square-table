(function () {
  function panel(label) {
    return `<div class="topology-panel"><strong>${label}</strong><small>4×4</small></div>`;
  }

  function planner(layout) {
    const [rows, cols] = layout;
    const box = document.getElementById('topologyPreview');
    if (!box) return;
    box.innerHTML = '';
    box.style.gridTemplateColumns = `repeat(${cols}, minmax(70px, 1fr))`;
    box.style.gridTemplateRows = `repeat(${rows}, minmax(70px, 1fr))`;
    let n = 1;
    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        box.insertAdjacentHTML('beforeend', panel(`Panel ${n++}`));
      }
    }
    const note = document.getElementById('topologyNote');
    const actual = rows * 4 + '×' + cols * 4;
    if (note) note.textContent = actual === '4×4' || actual === '4×8' ? `${actual} hardware layout active.` : `${actual} planning view — hardware mapping will be enabled when enough panels are connected.`;
  }

  function build() {
    const old = document.querySelector('.matrix-mode-controls');
    if (!old || document.getElementById('matrixTopology')) return;
    const section = document.createElement('div');
    section.id = 'matrixTopology';
    section.className = 'topology-card';
    section.innerHTML = `
      <div class="topology-heading"><div><strong>Matrix Layout</strong><small>Choose how your 4×4 panels are arranged.</small></div><span id="topologyNote">4×4 hardware layout active.</span></div>
      <div class="topology-options">
        <button type="button" data-layout="1,1"><span class="layout-icon one">■</span><b>4×4</b><small>1 panel</small></button>
        <button type="button" data-layout="1,2"><span class="layout-icon two">■■</span><b>4×8</b><small>2 panels horizontal</small></button>
        <button type="button" data-layout="2,1"><span class="layout-icon two vertical">■<br>■</span><b>8×4</b><small>2 panels vertical</small></button>
        <button type="button" data-layout="2,2"><span class="layout-icon four">■■<br>■■</span><b>8×8</b><small>4 panels</small></button>
      </div>
      <div id="topologyPreview" class="topology-preview"></div>`;
    old.replaceWith(section);

    section.querySelectorAll('[data-layout]').forEach(button => {
      button.addEventListener('click', () => {
        section.querySelectorAll('[data-layout]').forEach(b => b.classList.remove('selected'));
        button.classList.add('selected');
        const [rows, cols] = button.dataset.layout.split(',').map(Number);
        planner([rows, cols]);
        const wide = rows === 1 && cols === 2;
        const toggle = document.getElementById('matrixWide');
        if (toggle) {
          toggle.checked = wide;
          toggle.dispatchEvent(new Event('change'));
        }
        if (rows > 1) {
          const msg = document.getElementById('editorMessage');
          if (msg) msg.textContent = 'Planning view selected. The editor remains at the supported 4×4/4×8 hardware size for now.';
        }
      });
    });
    const first = section.querySelector('[data-layout="1,1"]');
    first.classList.add('selected');
    planner([1, 1]);
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', build);
  else build();
})();
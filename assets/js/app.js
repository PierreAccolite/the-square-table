const REPO = 'PierreAccolite/the-square-table';
const API = `https://api.github.com/repos/${REPO}/contents/`;
const RAW = `https://raw.githubusercontent.com/${REPO}/main/`;
const $ = selector => document.querySelector(selector);

let projects = [];
let current = null;
let timer = null;

const settingsKey = 'squareTable.settings';
const seenKey = project => `squareTable.seen.${project.id}`;

function settings() {
  return JSON.parse(localStorage.getItem(settingsKey) || '{}');
}

function saveSettings(value) {
  localStorage.setItem(settingsKey, JSON.stringify(value));
}

function getSeen(project) {
  try {
    const value = JSON.parse(localStorage.getItem(seenKey(project)) || '[]');
    return new Set(Array.isArray(value) ? value : []);
  } catch {
    return new Set();
  }
}

function saveSeen(project, seen) {
  localStorage.setItem(seenKey(project), JSON.stringify([...seen]));
}

async function gh(path) {
  const response = await fetch(`${API}${path}?ref=main`, {
    headers: { Accept: 'application/vnd.github+json' },
    cache: 'no-store'
  });

  if (response.status === 403 || response.status === 429) {
    const reset = Number(response.headers.get('x-ratelimit-reset') || 0);
    const retry = reset ? new Date(reset * 1000).toLocaleTimeString() : 'later';
    throw new Error(`GitHub rate limit reached. Retry after ${retry}.`);
  }

  if (!response.ok) {
    throw new Error(`GitHub ${response.status} for ${path}`);
  }

  return response.json();
}

async function raw(path) {
  const response = await fetch(`${RAW}${path}`, { cache: 'no-store' });
  if (!response.ok) throw new Error(`Cannot load ${path}`);
  return response.text();
}

function esc(value = '') {
  return value.replace(/[&<>"']/g, char => ({
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#39;'
  }[char]));
}

function meta(text, key) {
  const match = text.match(new RegExp(`\\*\\*${key}:\\*\\*\\s*(.+)`, 'i'));
  return match ? match[1].trim() : '—';
}

function messageTitle(text, fallback) {
  const match = text.match(/^#\s+(.+)$/m);
  return match ? match[1] : fallback;
}

function numericMessageOrder(name) {
  const match = name.match(/^(\d+)/);
  return match ? Number(match[1]) : -1;
}

function setLastRefresh(text) {
  $('#lastRefresh').textContent = `Last refresh: ${text}`;
}

function openSettings() {
  const value = settings();
  $('#gptUrl').value = value.gptUrl || '';
  $('#grokUrl').value = value.grokUrl || '';
  $('#pollSeconds').value = value.pollSeconds || 120;
  $('#settingsDialog').showModal();
}

async function init() {
  projects = await fetch('projects/projects.json', { cache: 'no-store' }).then(response => response.json());
  $('#projectSelect').innerHTML = projects
    .map(project => `<option value="${project.id}">${esc(project.name)}</option>`)
    .join('');

  bind();
  await selectProject(projects[0]?.id);
  const value = settings();
  if (!value.gptUrl && !value.grokUrl) $('#setupNudge').hidden = false;
  schedule();
}

async function selectProject(id) {
  current = projects.find(project => project.id === id);
  if (!current) return;

  $('#projectName').textContent = current.name;
  $('#projectStatus').textContent = current.status;
  $('#projectDescription').textContent = current.description;
  $('#previewFrame').src = current.preview || './';

  await Promise.all([
    loadMessages(),
    loadMarkdown('PROJECT.md', '#projectBrief'),
    loadDirectory('decisions', '#decisions'),
    loadDirectory('disagreements', '#disagreements')
  ]);
}

async function loadMessages() {
  const box = $('#messages');
  try {
    const files = (await gh(`${current.path}/messages`))
      .filter(item => item.type === 'file' && item.name.endsWith('.md'))
      .sort((a, b) => numericMessageOrder(b.name) - numericMessageOrder(a.name) || b.name.localeCompare(a.name));

    const seen = getSeen(current);
    const visible = files.slice(0, 12);
    const cards = [];
    const unseen = [];
    let gptUnread = 0;
    let grokUnread = 0;

    for (const file of visible) {
      const text = await raw(file.path);
      const to = meta(text, 'To');
      const from = meta(text, 'From');
      const type = meta(text, 'Type');
      const isUnread = !seen.has(file.sha);

      if (isUnread) {
        unseen.push(file);
        if (/GPT/i.test(to)) gptUnread++;
        if (/GROK/i.test(to)) grokUnread++;
      }

      const githubUrl = `https://github.com/${REPO}/blob/main/${file.path}`;
      cards.push(`
        <article class="message-card ${isUnread ? 'new' : ''}">
          <div class="message-meta">
            <span>${esc(from)} → ${esc(to)}</span>
            <span>${esc(type)} · ${esc(file.name)}</span>
          </div>
          <div class="message-title">${esc(messageTitle(text, file.name))}</div>
          <div class="message-body">${esc(text)}</div>
          <div class="message-actions">
            <a href="${githubUrl}" target="_blank" rel="noopener">Open on GitHub</a>
          </div>
        </article>`);
    }

    box.innerHTML = cards.join('') || '<p class="muted">No project messages yet.</p>';

    const unreadCount = unseen.length;
    $('#mailBadge').textContent = unreadCount ? `📬 ${unreadCount} new message${unreadCount === 1 ? '' : 's'}` : '✓ Mail up to date';
    $('#mailBadge').classList.toggle('new', unreadCount > 0);
    $('#gptMail').textContent = gptUnread ? `MAIL ${gptUnread}` : 'READY';
    $('#grokMail').textContent = grokUnread ? `MAIL ${grokUnread}` : 'READY';
    $('#activitySummary').textContent = unreadCount
      ? `${unreadCount} unread project message${unreadCount === 1 ? '' : 's'} · GPT ${gptUnread} · Grok ${grokUnread}`
      : `No unread project mail · showing newest ${visible.length} of ${files.length}`;

    box.dataset.visibleShas = JSON.stringify(visible.map(file => file.sha));
    setLastRefresh(new Date().toLocaleTimeString());
  } catch (error) {
    box.innerHTML = `<p class="muted">${esc(error.message)}</p>`;
    $('#mailBadge').textContent = 'Mail check failed';
    $('#activitySummary').textContent = error.message;
    setLastRefresh('failed');
  }
}

async function loadMarkdown(name, target) {
  try {
    $(target).textContent = await raw(`${current.path}/${name}`);
  } catch (error) {
    $(target).textContent = error.message;
  }
}

async function loadDirectory(name, target) {
  const box = $(target);
  try {
    const files = (await gh(`${current.path}/${name}`))
      .filter(item => item.type === 'file' && item.name.endsWith('.md'));

    const cards = [];
    for (const file of files) {
      const text = await raw(file.path);
      cards.push(`
        <article class="file-card">
          <div class="message-title">${esc(file.name)}</div>
          <div class="message-body">${esc(text)}</div>
        </article>`);
    }
    box.innerHTML = cards.join('') || '<p class="muted">None yet.</p>';
  } catch (error) {
    box.innerHTML = `<p class="muted">${esc(error.message)}</p>`;
  }
}

function openUrl(url, label) {
  if (!url) {
    alert(`Set your ${label} conversation URL in Settings first.`);
    openSettings();
    return;
  }
  window.open(url, '_blank', 'noopener');
}

function markVisibleRead() {
  if (!current) return;
  const seen = getSeen(current);
  let shas = [];
  try {
    shas = JSON.parse($('#messages').dataset.visibleShas || '[]');
  } catch {}
  shas.forEach(sha => seen.add(sha));
  saveSeen(current, seen);
  loadMessages();
}

function bind() {
  $('#projectSelect').onchange = event => selectProject(event.target.value);

  document.querySelectorAll('.tab').forEach(button => {
    button.onclick = () => {
      document.querySelectorAll('.tab').forEach(item => item.classList.remove('active'));
      document.querySelectorAll('.panel').forEach(item => item.classList.remove('active'));
      button.classList.add('active');
      $(`#${button.dataset.tab}Panel`).classList.add('active');
    };
  });

  $('#refreshBtn').onclick = loadMessages;
  $('#refreshBtnSide').onclick = loadMessages;
  $('#markReadBtn').onclick = markVisibleRead;
  $('#openGitBtn').onclick = () => window.open(`https://github.com/${REPO}/tree/main/${current.path}`, '_blank', 'noopener');
  $('#openGptBtn').onclick = () => openUrl(settings().gptUrl, 'ChatGPT');
  $('#openGrokBtn').onclick = () => openUrl(settings().grokUrl, 'Grok');
  $('#settingsBtn').onclick = openSettings;
  $('#setupSettingsBtn').onclick = openSettings;

  $('#settingsForm').onsubmit = event => {
    event.preventDefault();
    saveSettings({
      gptUrl: $('#gptUrl').value.trim(),
      grokUrl: $('#grokUrl').value.trim(),
      pollSeconds: Math.max(60, Number($('#pollSeconds').value) || 120)
    });
    $('#setupNudge').hidden = !!(settings().gptUrl || settings().grokUrl);
    $('#settingsDialog').close();
    schedule();
  };
}

function schedule() {
  if (timer) clearInterval(timer);
  const seconds = Math.max(60, settings().pollSeconds || 120);
  timer = setInterval(() => current && loadMessages(), seconds * 1000);
}

init().catch(error => {
  $('#projectName').textContent = 'Square Table failed to load';
  $('#projectDescription').textContent = error.message;
});

const REPO = 'PierreAccolite/the-square-table';
const API = `https://api.github.com/repos/${REPO}/contents/`;
const RAW = `https://raw.githubusercontent.com/${REPO}/main/`;
const $ = selector => document.querySelector(selector);

let projects = [];
let current = null;
let timer = null;
let rateLimitUntil = 0;

const settingsKey = 'squareTable.settings';
const seenKey = project => `squareTable.seen.${project.id}`;
const cacheKey = (project, area) => `squareTable.cache.${project.id}.${area}`;

function settings() {
  try { return JSON.parse(localStorage.getItem(settingsKey) || '{}'); }
  catch { return {}; }
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

function saveCache(project, area, value) {
  try {
    localStorage.setItem(cacheKey(project, area), JSON.stringify({ savedAt: Date.now(), value }));
  } catch {}
}

function loadCache(project, area) {
  try { return JSON.parse(localStorage.getItem(cacheKey(project, area)) || 'null'); }
  catch { return null; }
}

async function gh(path) {
  const value = settings();
  const headers = { Accept: 'application/vnd.github+json' };
  if (value.githubToken) headers.Authorization = `Bearer ${value.githubToken}`;

  const response = await fetch(`${API}${path}?ref=main`, {
    headers,
    cache: 'no-store'
  });

  if (response.status === 403 || response.status === 429) {
    const reset = Number(response.headers.get('x-ratelimit-reset') || 0);
    rateLimitUntil = reset ? reset * 1000 : Date.now() + 15 * 60 * 1000;
    const retry = new Date(rateLimitUntil).toLocaleTimeString();
    const error = new Error(`GitHub rate limit reached. Retry after ${retry}.`);
    error.rateLimited = true;
    throw error;
  }

  if (!response.ok) throw new Error(`GitHub ${response.status} for ${path}`);
  return response.json();
}

async function raw(path) {
  const response = await fetch(`${RAW}${path}`, { cache: 'no-store' });
  if (!response.ok) throw new Error(`Cannot load ${path}`);
  return response.text();
}

function esc(value = '') {
  return String(value).replace(/[&<>"']/g, char => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
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

function showStale(message = '') {
  $('#staleBanner').hidden = false;
  $('#staleBannerText').textContent = message || 'Showing cached project data.';
}

function hideStale() {
  $('#staleBanner').hidden = true;
}

function openSettings() {
  const value = settings();
  $('#gptUrl').value = value.gptUrl || '';
  $('#grokUrl').value = value.grokUrl || '';
  $('#githubToken').value = value.githubToken || '';
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

  if (current.preview) {
    $('#previewFrame').hidden = false;
    $('#previewFrame').src = current.preview;
    $('#previewEmpty').hidden = true;
  } else {
    $('#previewFrame').hidden = true;
    $('#previewFrame').removeAttribute('src');
    $('#previewEmpty').hidden = false;
  }

  await Promise.all([
    loadMessages(),
    loadMarkdown('PROJECT.md', '#projectBrief'),
    loadDirectory('decisions', '#decisions'),
    loadDirectory('disagreements', '#disagreements')
  ]);
}

async function fetchMessageRecords() {
  const files = (await gh(`${current.path}/messages`))
    .filter(item => item.type === 'file' && item.name.endsWith('.md'))
    .sort((a, b) => numericMessageOrder(b.name) - numericMessageOrder(a.name) || b.name.localeCompare(a.name));

  const visible = files.slice(0, 12);
  const records = [];
  for (const file of visible) {
    records.push({
      name: file.name,
      path: file.path,
      sha: file.sha,
      text: await raw(file.path)
    });
  }
  return { records, total: files.length };
}

function renderMessages(payload, stale = false) {
  const box = $('#messages');
  const seen = getSeen(current);
  const cards = [];
  const unseen = [];
  let gptUnread = 0;
  let grokUnread = 0;

  for (const file of payload.records) {
    const to = meta(file.text, 'To');
    const from = meta(file.text, 'From');
    const type = meta(file.text, 'Type');
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
        <div class="message-title">${esc(messageTitle(file.text, file.name))}</div>
        <div class="message-body">${esc(file.text)}</div>
        <div class="message-actions"><a href="${githubUrl}" target="_blank" rel="noopener">Open on GitHub</a></div>
      </article>`);
  }

  box.innerHTML = cards.join('') || '<p class="muted">No project messages yet.</p>';
  box.dataset.visibleShas = JSON.stringify(payload.records.map(file => file.sha));

  const unreadCount = unseen.length;
  $('#mailBadge').textContent = stale
    ? `⚠ Cached mail · ${unreadCount} unread`
    : unreadCount ? `📬 ${unreadCount} new message${unreadCount === 1 ? '' : 's'}` : '✓ Mail up to date';
  $('#mailBadge').classList.toggle('new', unreadCount > 0);
  $('#gptMail').textContent = stale ? (gptUnread ? `MAIL ${gptUnread}` : 'CACHED') : (gptUnread ? `MAIL ${gptUnread}` : 'READY');
  $('#grokMail').textContent = stale ? (grokUnread ? `MAIL ${grokUnread}` : 'CACHED') : (grokUnread ? `MAIL ${grokUnread}` : 'READY');
  $('#activitySummary').textContent = `${stale ? 'Cached · ' : ''}${unreadCount ? `${unreadCount} unread · GPT ${gptUnread} · Grok ${grokUnread}` : `No unread project mail · showing newest ${payload.records.length} of ${payload.total}`}`;
  $('#markReadBtn').disabled = payload.records.length === 0;
}

async function loadMessages() {
  try {
    const payload = await fetchMessageRecords();
    saveCache(current, 'messages', payload);
    renderMessages(payload, false);
    hideStale();
    setLastRefresh(new Date().toLocaleTimeString());
  } catch (error) {
    const cached = loadCache(current, 'messages');
    if (cached?.value) {
      renderMessages(cached.value, true);
      const age = new Date(cached.savedAt).toLocaleTimeString();
      showStale(`${error.message} Showing cached mail from ${age}.`);
      setLastRefresh(`cached ${age}`);
    } else {
      $('#messages').innerHTML = `<p class="muted">${esc(error.message)}</p>`;
      $('#mailBadge').textContent = 'Mail unavailable';
      $('#gptMail').textContent = 'UNKNOWN';
      $('#grokMail').textContent = 'UNKNOWN';
      $('#activitySummary').textContent = error.message;
      $('#markReadBtn').disabled = true;
      showStale(error.message);
      setLastRefresh('failed');
    }
    if (error.rateLimited) schedule();
  }
}

async function loadMarkdown(name, target) {
  try { $(target).textContent = await raw(`${current.path}/${name}`); }
  catch (error) { $(target).textContent = error.message; }
}

async function fetchDirectoryRecords(name) {
  const files = (await gh(`${current.path}/${name}`))
    .filter(item => item.type === 'file' && item.name.endsWith('.md'));
  const records = [];
  for (const file of files) records.push({ name: file.name, path: file.path, text: await raw(file.path) });
  return records;
}

function renderDirectory(records, target, stale = false) {
  const box = $(target);
  box.innerHTML = records.map(file => `
    <article class="file-card">
      <div class="message-title">${esc(file.name)}${stale ? ' · CACHED' : ''}</div>
      <div class="message-body">${esc(file.text)}</div>
    </article>`).join('') || '<p class="muted">None yet.</p>';
}

async function loadDirectory(name, target) {
  try {
    const records = await fetchDirectoryRecords(name);
    saveCache(current, name, records);
    renderDirectory(records, target, false);
  } catch (error) {
    const cached = loadCache(current, name);
    if (cached?.value) renderDirectory(cached.value, target, true);
    else $(target).innerHTML = `<p class="muted">${esc(error.message)}</p>`;
  }
}

function openUrl(url, label, windowName) {
  if (!url) {
    alert(`Set your ${label} conversation URL in Settings first.`);
    openSettings();
    return;
  }
  const tab = window.open(url, windowName);
  try { if (tab) tab.opener = null; } catch {}
}

function markVisibleRead() {
  if (!current) return;
  const seen = getSeen(current);
  let shas = [];
  try { shas = JSON.parse($('#messages').dataset.visibleShas || '[]'); } catch {}
  shas.forEach(sha => seen.add(sha));
  saveSeen(current, seen);
  const cached = loadCache(current, 'messages');
  if (cached?.value) renderMessages(cached.value, !!$('#staleBanner').hidden === false);
  else loadMessages();
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
  $('#openGitBtn').onclick = () => window.open(`https://github.com/${REPO}/tree/main/${current.path}`, 'squareTableGitHub');
  $('#openGptBtn').onclick = () => openUrl(settings().gptUrl, 'ChatGPT', 'squareTableGPT');
  $('#openGrokBtn').onclick = () => openUrl(settings().grokUrl, 'Grok', 'squareTableGrok');
  $('#settingsBtn').onclick = openSettings;
  $('#setupSettingsBtn').onclick = openSettings;

  $('#settingsForm').onsubmit = event => {
    event.preventDefault();
    saveSettings({
      gptUrl: $('#gptUrl').value.trim(),
      grokUrl: $('#grokUrl').value.trim(),
      githubToken: $('#githubToken').value.trim(),
      pollSeconds: Math.max(60, Number($('#pollSeconds').value) || 120)
    });
    rateLimitUntil = 0;
    $('#setupNudge').hidden = !!(settings().gptUrl || settings().grokUrl);
    $('#settingsDialog').close();
    loadMessages();
    schedule();
  };
}

function schedule() {
  if (timer) clearTimeout(timer);
  const normalDelay = Math.max(60, settings().pollSeconds || 120) * 1000;
  const resetDelay = Math.max(0, rateLimitUntil - Date.now() + 5000);
  const delay = Math.max(normalDelay, resetDelay);
  timer = setTimeout(async () => {
    if (current) await loadMessages();
    schedule();
  }, delay);
}

init().catch(error => {
  $('#projectName').textContent = 'Square Table failed to load';
  $('#projectDescription').textContent = error.message;
});

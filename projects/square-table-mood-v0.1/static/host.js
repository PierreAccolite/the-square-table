(function () {
  const originalConnect = window.connectPico;
  const originalDisconnect = window.disconnectPico;

  window.connectPico = async function () {
    if (typeof originalConnect === 'function') await originalConnect();
    const status = document.getElementById('status');
    if (status && status.classList.contains('connected')) {
      try {
        await fetch('/api/host/on', { method: 'POST' });
        if (typeof log === 'function') log('PC control enabled — standalone temperature mode paused.');
      } catch (e) {
        if (typeof log === 'function') log('Host control handshake failed: ' + e.message);
      }
    }
  };

  window.disconnectPico = async function () {
    try { await fetch('/api/host/off', { method: 'POST' }); } catch (_) {}
    if (typeof originalDisconnect === 'function') await originalDisconnect();
  };
})();

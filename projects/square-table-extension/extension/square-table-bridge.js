// Bridge only the Square Table's own navigation buttons to the extension.
// This does not run on ChatGPT or Grok and does not inspect their DOM/content.

const TARGETS = {
  openGptBtn: 'gpt',
  openGrokBtn: 'grok',
  openGitBtn: 'github'
};

function wireButton(id, target) {
  const button = document.getElementById(id);
  if (!button || button.dataset.squareTableExtensionBound === '1') return;

  button.dataset.squareTableExtensionBound = '1';
  button.addEventListener('click', async event => {
    // Run before the page's own fallback handler and stop it from opening
    // another tab when the extension is installed.
    event.preventDefault();
    event.stopImmediatePropagation();

    const original = button.textContent;
    try {
      const result = await chrome.runtime.sendMessage({ type: 'open', target });
      if (result?.error) throw new Error(result.error);
      button.textContent = result?.reused ? `${original} ✓` : `${original} ↗`;
    } catch (error) {
      console.warn('Square Table extension navigation failed:', error);
      button.textContent = `${original} !`;
    }
    setTimeout(() => { button.textContent = original; }, 900);
  }, true);
}

function wire() {
  for (const [id, target] of Object.entries(TARGETS)) wireButton(id, target);
}

wire();
new MutationObserver(wire).observe(document.documentElement, { childList: true, subtree: true });

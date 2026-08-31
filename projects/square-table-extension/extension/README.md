# Square Table Browser Extension v0.1

## Install in Chrome / Edge
1. Download or clone this repository locally.
2. Open `chrome://extensions/` (Chrome) or `edge://extensions/` (Edge).
3. Enable **Developer mode**.
4. Choose **Load unpacked**.
5. Select this folder: `projects/square-table-extension/extension`.
6. Pin **The Square Table** extension to the toolbar.
7. Open the extension Options page and set the GPT/Grok conversation URLs. The defaults already point at PierreAccolite/the-square-table and the live Square Table Pages URL.
8. Optional: add a repo-scoped fine-grained GitHub token with **Contents: read-only** for reliable polling.

## What v0.1 does
- Polls the configured project message folder in the background.
- Shows unread count on the toolbar badge.
- Popup shows latest message headers.
- Square Table / GPT / Grok / GitHub buttons search existing browser tabs and focus them first; only opens a new tab when none exists.

## What it intentionally does not do
No content scripts, no scraping ChatGPT/Grok, no automatic typing, no write token, no autonomous loops.

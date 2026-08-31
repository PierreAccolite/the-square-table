# Project 002 — Square Table Browser Extension

## Goal
Create a small Manifest V3 browser extension that complements the Square Table GitHub Pages app.

## v0.1 scope
- Watch one Square Table project for unread mail in the background.
- Show unread count on the extension badge.
- Popup with latest project mail summary.
- Open or focus existing Square Table / GPT / Grok / GitHub tabs.
- Browser-local settings for repo, project path, GPT URL, Grok URL, Pages URL, optional read-only GitHub token, and polling interval.

## Explicitly deferred
- No content scripts on ChatGPT or Grok.
- No DOM scraping or automatic typing.
- No write token in the extension.
- No autonomous model loop.

GitHub remains the source of truth. The extension is only another view/notifier/navigation helper.

# Message 0001 — Project 002 Extension Baseline

**From:** GPT-5.6 Sol  
**To:** Grok (xAI), Pierre  
**Type:** HANDOFF / REVIEW

Project 002 is now live in the repository.

Implemented MVP:
- Manifest V3 extension under `projects/square-table-extension/extension/`
- background GitHub mail polling with toolbar unread badge
- popup with latest project mail headers
- browser-local options for repo/project/GPT/Grok/Pages/read-only token/poll interval
- open-or-focus behavior using browser `tabs` permission for Square Table, GPT, Grok and GitHub
- no content scripts, no DOM scraping, no automatic typing, no write credential

Project 001 also received the token UX hardening discussed in message 0007: saved token is no longer echoed into Settings, explicit clear control, token verification on replacement, and visible API auth/quota state.

**Grok review request:** inspect the extension code as a Manifest V3 implementation. Focus especially on service-worker lifecycle, alarm behavior, unread/seen semantics, tab matching/focus behavior, permissions, and any obvious Chrome/Edge compatibility issues. Please write the next numbered message in this Project 002 folder.

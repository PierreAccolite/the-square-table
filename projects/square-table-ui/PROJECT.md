# Project 001 — Square Table Interface

**Status:** ACTIVE  
**Owner:** Pierre  
**Primary coder:** GPT  
**Independent reviewer / co-builder:** Grok

## Objective

Build the shared interface that will become the front door for future Square Table projects.

The interface should make it easy for Pierre to:

- switch between projects;
- see recent project messages and new mail;
- open the relevant GPT and Grok browser conversations quickly;
- open the GitHub project folder;
- preview the project result;
- inspect decisions and disagreements;
- retain human control over the collaboration tempo.

## Privacy rule

ChatGPT/Grok consumer conversation URLs are **not stored in the public repository**. The web interface stores Pierre's personal navigation URLs only in browser `localStorage`.

## v0.1 interface scope

1. Project selector
2. Project summary/status
3. Recent message feed from GitHub
4. New-mail/activity detection via GitHub polling
5. GPT / Grok quick-open buttons
6. GitHub project quick-open
7. Preview panel
8. Decisions panel
9. Disagreements panel
10. Browser-local settings for personal chat URLs

## Not in this baseline

- automatic consumer-site message sending;
- iframe embedding of ChatGPT or Grok;
- autonomous model loops;
- model API billing/integration;
- code execution;
- authentication/multi-user support.

## Review request for Grok

Review the live baseline and source independently. Focus on usability, protocol integrity, project scaling, mail detection, and unnecessary complexity. Write the review as the next numbered message in this project's `messages/` folder.

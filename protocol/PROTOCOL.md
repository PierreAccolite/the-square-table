# The Square Table Protocol — v0.1

The repository is the durable shared source of truth. Individual model conversations are useful private context, but are not authoritative project state.

## Participants

- **Pierre** — human root / product owner / final authority
- **GPT** — primary implementation role unless a project says otherwise
- **Grok** — independent reviewer / challenger / co-builder unless a project says otherwise

## Message rules

Messages are immutable, numbered Markdown files inside each project's `messages/` directory.

Recommended header:

```md
# Message 0001

**From:** Pierre
**To:** GPT, Grok
**In-Reply-To:** none
**Type:** QUESTION
```

Supported starting types:

- `DISCUSS`
- `QUESTION`
- `REVIEW`
- `DECISION`
- `HANDOFF`
- `INDEPENDENT`
- `TERMINATE`

Termination reasons:

- `CONSENSUS`
- `BLOCKED`
- `HUMAN_REQUIRED`
- `BUDGET_EXHAUSTED`

## Independence

For an `INDEPENDENT` task, models must not read the peer answer until both independent answers exist and the task is explicitly revealed.

## Disagreement

Disagreement is useful state, not noise. Classify it as:

- `FACT`
- `LOGIC`
- `ASSUMPTION`
- `TRADE-OFF`
- `PREFERENCE`

Factual/logical disagreement should be pushed toward evidence. Assumptions should be surfaced. Trade-offs should be quantified and escalated to Pierre when needed. Preferences should not consume excessive turns.

## Human control

No autonomous unbounded model-to-model loop. Any future automatic exchange must have a human-originated task, a low explicit turn budget, a hard cost/token ceiling, termination conditions, auditability, and a global STOP.

Pierre remains root.

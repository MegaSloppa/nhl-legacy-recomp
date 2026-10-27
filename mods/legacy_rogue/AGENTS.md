# AGENTS.md - Legacy Rogue (read me first, every session)

You are working on **Legacy Rogue**, a mod for the NHL Legacy static recompilation
(`nhl-legacy-recomp`) that adds a career-long roguelite power-fantasy layer to Be-A-Pro.

Your context may have been wiped. Do not trust memory. Rebuild state from these files,
in this order, before doing anything:

1. `docs/00-START-HERE.md` - what this is, hard rules, where things live (5 min).
2. `docs/current-status.md` - AUTHORITATIVE task board: what is done, what is next, who owns what.
3. `docs/handoff-log.md` - read the LAST 2 entries only.
4. Whatever doc the task you pick points to (`design.md`, `re-findings.md`).

Before you stop (even mid-task): update `current-status.md` and append a `handoff-log.md`
entry using the template there. If you skip this, the next agent repeats your work.

Parent project docs also apply (`../../docs/current-status.md`,
`../../docs/agent-coordination-handoff.md`, `../../DEV-README.md`). This mod never edits
parent-project files except the four wiring lines listed in `docs/user-guide.md`.

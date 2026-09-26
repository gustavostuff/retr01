# Retr01 board sim

Bring-up labs as isolated trees. Each tier has its own board recipe and does not share netlist wiring with the others. Chips, tests, font, and PNG chrome that match live in [`common/`](common/). The discrete IC engine is [`tools/discrete_ic/`](../../tools/discrete_ic/).

| Lab | Path | Goal |
| --- | --- | --- |
| Tier A | [tier-a/](tier-a/) | Beam + color PROM + virtual screen (color bars) |
| Tier B | [tier-b/](tier-b/) | Compositor priority over the Tier A color path. Parts seated on three breadboards |
| Tier H | [tier-h/](tier-h/) | Imported full-board sim. Soft `$7Fxx` and AT27C256R. AD724 is not in this tree yet |

# Retr01 board sim

Bring-up labs as isolated trees. Each tier has its own board recipe and does not share netlist wiring with the others. The discrete IC engine is [`apps/netlist_sim/`](../netlist_sim/).

| Lab | Path | Goal |
| --- | --- | --- |
| Tier A | [tier-a/](tier-a/) | Beam + color PROM + virtual screen (color bars) |
| Tier B | [tier-b/](tier-b/) | Compositor priority over the Tier A color path. Parts seated on three breadboards |

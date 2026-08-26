# Startup catchup threading

Why IC bring-up runs on a worker thread, why a synchronous boot freezes the window, and how we might show a live board UI during catchup later.

## What "boot catchup" is

On startup (and after Ctrl+R reset), the sim must get the bring-up **palette + start-screen MAP** into VRAM before the LCD hold lifts. Default (**SIM PIN**): ~12k island steps of the IC `$FE93`->`$FE12` stream (`r01s_board_catchup_bringup` / yielding worker in `app.c`). **SIM FAST** (`R01S_FAST=1`): same VRAM/pal end state via a word-level apply - see [`R01S_FAST` below](#r01s_fast-unset-sim-pin-vs-r01s_fast1-sim-fast).

Until pin-mode catchup finishes, the board is mid-stream: buses, map pointer, and sink contents change every step.

## Why a synchronous boot hangs the app

SDL (and this sim's UI) expect the **main thread** to keep pumping events and presenting frames. Catchup is CPU-heavy: each step walks the wired island group.

If catchup runs **inline** on the main thread before the frame loop:

1. `SDL_CreateWindow` may show an empty/black surface.
2. No `PollEvent` / `RenderPresent` runs for the whole catchup (~seconds).
3. The desktop marks the window as **not responding**; Esc/quit never run.

Even if catchup is moved *into* the frame loop but still done as one blocking stretch (or while holding `board_mu` across a full UI draw), the UI starves:

| Approach | What happens |
|----------|----------------|
| Sync catchup before loop | Frozen empty window until done |
| Sync catchup inside frame, long lock | Main holds mutex for draw *and* steps; FPS collapses (~4-12) |
| Worker steps + main `LockMutex` for full UI every frame | Same contention: main waits on long step batches |

So "hang" here means **UI thread starvation**, not necessarily a deadlock.

## What we do today

```
main thread                          worker thread
-------------                        -------------
pump events / present
draw black "Booting console..."  <---  after each step batch:
advance spinner when signaled          unlock board_mu
                                       set catchup_ui_req
                                       wait briefly for ack
                                       lock + step again
```

- **Worker** owns board steps under `board_mu` in batches (`R01S_CATCHUP_BATCH_STEPS`).
- **Main** never blocks on that mutex for paint; it only acks `catchup_ui_req` to tick the spinner.
- After catchup, main joins the worker and resumes the normal board UI (LCD via streaming texture blit).

Code: `r01s_app_start_ic_catchup`, `catchup_thread_fn_yielding`, `r01s_ui_draw_boot` in `src/app.c` / `src/ui.c`.

## Future: show the whole board during boot

Goal: live (or near-live) islands, pins, sidebar, and LCD while the stream runs, without freezing or tanking FPS.

### 1. Snapshot texture (simple, "frozen then refresh")

- After each worker batch (or on `catchup_ui_req`), main **TryLock**/short-lock, `r01s_ui_draw` into a secondary texture, unlock, then present that texture every frame.
- Present path never waits on the worker.
- Content update rate ~= batch rate (e.g. tens of Hz if batches are small); HUD can stay ~60 FPS.
- Tradeoff: UI is one generation behind; pan/click during catchup still awkward.

Tried briefly in development; works, but feels stale unless batches are short and the worker yields for a real redraw.

### 2. Explicit UI snapshot buffer (better isolation)

- Under the short lock, copy only **display state** (pin levels, probe bits, health, LCD RGB) into a UI-owned buffer.
- Unlock immediately; main draws from the buffer with **no** board lock.
- Worker never waits on SDL.
- Needs a defined "what the UI reads" snapshot API so draw does not touch live entities.

### 3. Dual-rate main loop (interleaved steps)

- No worker: each frame do N steps under a time budget (like post-boot `R01S_SIM_BUDGET_MS`), then draw.
- Window stays alive from frame one.
- Catchup wall time grows (steps share the frame with draw); progress is visible every frame.
- Simplest mental model; may be enough if ~12k steps finish in a few seconds at 60 FPS budgets.

### 4. Faster catchup / less work

**Partially landed:** `R01S_FAST` / SIM FAST (word MAP catchup + thin settle/beam) - details [below](#r01s_fast-unset-sim-pin-vs-r01s_fast1-sim-fast). Further ideas: skip idle islands, or pin-mode catchup that omits beam until MAP completes.

### Recommended direction

Prefer **(2)** or **(3)** for a productized live boot view:

- **(3)** if we accept slightly longer catchup and want one thread.
- **(2)** if we keep the worker for max step throughput and want smooth 60 FPS chrome.

Avoid holding `board_mu` across a full `r01s_ui_draw` while the worker is stepping - that is what produced the ~4 FPS boot UI.

## Related knobs

| Item | Role |
|------|------|
| `R01S_SOFTBOOT=1` | Skip IC stream; host poke (triage only) - different from FAST |
| `R01S_FAST=1` / sidebar **SIM FAST** | Word MAP catchup + thin settle/beam; default **SIM PIN** is full netlist |
| LCD `SCALE` 1x/2x | Sidebar / `G`; independent of catchup threading |
| LCD texture blit | Streaming upload of sink RGB (not per-pixel `DrawPoint`) |

## `R01S_FAST` unset (SIM PIN) vs `R01S_FAST=1` (SIM FAST)

Flag lives on the board as `board->sim_fast` (`r01s_board_set_sim_fast` / `r01s_board_sim_fast`).  
`R01S_FAST=1` at `r01s_board_build` sets it before catchup starts. The sidebar **SIM PIN / SIM FAST** button toggles the same flag at runtime (after boot, that only affects **ongoing** `board_step`; catchup already finished unless you Ctrl+R).

These modes share the same chip entities, wires, and UI. What changes is **how much electrical work each step does**, and **how catchup fills VRAM**.

### Mental model

| | SIM PIN (default) | SIM FAST |
|--|-------------------|----------|
| Goal | PCB / pin truth | Throughput / snappy boot |
| Catchup | ~12k real netlist steps of bring-up PRG | One word-level MAP+pal apply |
| Each `board_step` | Deep settle + fat beam burst | Shallow settle + thin beam burst |
| Settle wires | Full (incl. BG fetch + linebuf) | Skips BG fetch + linebuf in settle (beam loop only) |
| AVR ticks (328P/1284P) | Every PHI2 step | Skipped (smoke filled at word catchup) |
| Host Play | Off | On after catchup (pads / camera / OAM shortcut) |
| Bus fights / settle depth | Visible | Easier to miss |
| End VRAM/pal after catchup | Start screen in VRAM | Same *data* end state |

`R01S_SOFTBOOT=1` is a third path (host softboot + some steps). FAST is **not** softboot: softboot is explicitly opt-in debug; FAST still uses cart flash peeks and sets `map_addr` / health flags via `board_fast_apply_map_stream`, but **does not** execute the `$FE93`->`$FE12` loop on the 6502.

---

### 1. Boot catchup - what the worker does

**PIN (`sim_fast == 0`) - code path in `app.c` + `r01s_board_catchup_bringup`:**

1. Worker locks `board_mu`, runs batches of `r01s_island_group_step` (~64), unlocks, signals spinner, repeats.
2. Each step is a full `board_step` (below): PHI2, CPU micro-ops, decode, MAP/VRAM ports, beam dots, video wire.
3. Bring-up PRG on the cart overlay actually runs: LDA `$FE93` / STA `$FE12` (and palette path) until `map_addr >= cart_off_map_screen0 + 480` and VRAM\[0\] matches flash.
4. Wall time is on the order of **~10-15 s** (~1 ms/step x ~12k steps on a typical machine).
5. Side effect: CPU PC, latches, PHI2 edges, beam position, and partial LCD samples advance for real during the stream.

**FAST (`sim_fast == 1`) - early exit in the worker:**

1. Worker locks once, calls `r01s_board_catchup_bringup` -> `board_fast_apply_map_stream`.
2. That function **does not** step the netlist. It:
   - peeks one global BG row + one global sprite row (**16+16** B from the 128 B planes, using world `default_pal_row`) -> `board->active_pal[]`
   - peeks 480 MAP bytes -> `r01s_as6c62256_poke` into VRAM
   - `poke_map_addr_latches(..., screen0 + 480)`
   - sets `health_saw_map` / `health_saw_vram` / `health_saw_latch`
3. Unlock, one spinner tick, catchup done (**milliseconds**).
4. Side effect: **no** bring-up instruction stream, **no** per-byte `$FE93`/`$FE12` pin traffic, beam/LCD mostly still at reset until the post-catchup frame loop runs.

So: PIN *simulates the stream*; FAST *installs the stream's result*.

---

### 2. Ongoing simulation - what one `board_step` does

Both modes still call the same `board_step` in `board.c`. The branch is only how expensive two inner loops are.

**Settle (`board_settle` -> `board_settle_n`):**

Each settle pass runs, in order: `wire_power_clock_reset`, `wire_memory`, `wire_io`, `wire_beam`, `wire_bg_fetch`, `wire_vram`, `wire_linebuf` (drive/sense pins across islands).

| | Passes per `board_settle` call |
|--|--------------------------------|
| PIN | `R01S_SETTLE_PASSES` (**4**) |
| FAST | `R01S_SETTLE_PASSES_FAST` (**1**) |

`board_step` invokes settle **several times** per step (around PHI2 / CPU / beam). So PIN does roughly **4x** the wire work of FAST on every settle call. Deep settle exists so decode -> CE -> DQ can propagate in one half-cycle; one pass is usually enough once the netlist is "warm," but shallow settle can hide multi-level glue races.

**Beam / video burst (inside `board_step`):**

After osc/APU/1284 ticks, both modes loop DOT:

- tick DOT osc -> drive beam -> tick beam -> NMI edge -> `wire_video_dot` (BG/sprite/compositor/PROM -> LCD sink).

| | DOT iterations per board step |
|--|-------------------------------|
| PIN | `R01S_BEAM_DOTS_PER_STEP` (**128**) |
| FAST | `R01S_BEAM_DOTS_PER_STEP_FAST` (**8**) |

Same silicon timing model (341x262), but FAST advances the raster **16x slower per board step**. Under the UI's ~10 ms step budget you still get video, but fewer dots (and fewer sink plots) per wall-clock frame. PIN burns most of its step time here during catchup and play.

**Unchanged in both modes:**

- Same `r01s_entity_tick` / `eval` on CPU, osc, AVRs, beam, etc.
- Same pin structs and UI drawing (LCD still blits the sink texture).
- Same auto-inc rules for `$FE12` / `$FE93` when those cycles *do* happen on the pin path.

---

### 3. What you gain / lose

**FAST helps**

- Boot: catchup effectively free vs multi-second worker.
- Play: more PHI2/CPU progress per UI frame for the same wall budget (~5-20x less work per step from settlexbeam cuts, depending on hot path).
- Iteration when you care about "screen up + game logic," not pin fights.

**PIN keeps**

- Electrical realism: multi-pass settle, fat beam, IC MAP ownership through the real `$FExx` path.
- Better odds of catching bus fights, wrong CE owners, and "works in soft poke, fails on silicon" bugs.
- Catchup that proves bring-up PRG + decode + flash + VRAM port together.

**Not the same as softboot:** softboot is an explicit host bypass (`R01S_SOFTBOOT`). FAST catchup is a **word transaction** of the same cart bytes the IC stream would write, still keyed off cart meta (`cart_off_map_screen0`, `cart_off_pal_bg`).

---

### 4. Code map

| Concern | Where |
|---------|--------|
| Flag / env default | `board->sim_fast`, `r01s_board_build` (`R01S_FAST`), UI `ui_toggle_sim_fast` |
| Catchup branch | `r01s_board_catchup_bringup`, worker early path in `app.c` |
| Word MAP apply | `board_fast_apply_map_stream` |
| Settle depth | `board_settle` -> `R01S_SETTLE_PASSES` vs `_FAST` |
| Beam burst | `board_step` -> `R01S_BEAM_DOTS_PER_STEP` vs `_FAST` |

See also: [`README.md`](README.md), [`docs/08_simulator.md`](../docs/08_simulator.md).

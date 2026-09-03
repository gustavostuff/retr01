# Startup catchup threading

Why IC bring-up runs on a worker thread, why a synchronous boot freezes the window, and how we might show a live board UI during catchup later.

## What "boot catchup" is

On startup (and after Ctrl+R reset), the sim must get the bring-up **palette + start-screen MAP** into VRAM before the LCD hold lifts. That is ~12k island steps of the IC `$FE93`->`$FE12` stream (`r01s_board_catchup_bringup` / yielding worker in `app.c`).

Until catchup finishes, the board is mid-stream: buses, map pointer, and sink contents change every step.

## Why a synchronous boot hangs the app

SDL (and this sim's UI) expect the **main thread** to keep pumping events and presenting frames. Catchup is CPU-heavy: each step walks the wired island group.

If catchup runs **inline** on the main thread before the frame loop:

1. `SDL_CreateWindow` may show an empty/black surface.
2. No `PollEvent` / `RenderPresent` runs for the whole catchup (~seconds).
3. The desktop marks the window as **not responding**. Esc/quit never run.

Even if catchup is moved *into* the frame loop but still done as one blocking stretch (or while holding `board_mu` across a full UI draw), the UI starves:

| Approach | What happens |
|----------|----------------|
| Sync catchup before loop | Frozen empty window until done |
| Sync catchup inside frame, long lock | Main holds mutex for draw *and* steps. FPS collapses (~4-12) |
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
- **Main** never blocks on that mutex for paint. It only acks `catchup_ui_req` to tick the spinner.
- After catchup, main joins the worker and resumes the normal board UI (LCD via streaming texture blit).

Code: `r01s_app_start_ic_catchup`, `catchup_thread_fn_yielding`, `r01s_ui_draw_boot` in `src/app.c` / `src/ui.c`.

## Future: show the whole board during boot

Goal: live (or near-live) islands, pins, sidebar, and LCD while the stream runs, without freezing or tanking FPS.

### 1. Snapshot texture (simple, "frozen then refresh")

- After each worker batch (or on `catchup_ui_req`), main **TryLock**/short-lock, `r01s_ui_draw` into a secondary texture, unlock, then present that texture every frame.
- Present path never waits on the worker.
- Content update rate ~= batch rate (e.g. tens of Hz if batches are small). HUD can stay ~60 FPS.
- Tradeoff: UI is one generation behind. Pan/click during catchup still awkward.

Tried briefly in development. Works, but feels stale unless batches are short and the worker yields for a real redraw.

### 2. Explicit UI snapshot buffer (better isolation)

- Under the short lock, copy only **display state** (pin levels, probe bits, health, LCD RGB) into a UI-owned buffer.
- Unlock immediately. Main draws from the buffer with **no** board lock.
- Worker never waits on SDL.
- Needs a defined "what the UI reads" snapshot API so draw does not touch live entities.

### 3. Dual-rate main loop (interleaved steps)

- No worker: each frame do N steps under a time budget (like post-boot `R01S_SIM_BUDGET_MS`), then draw.
- Window stays alive from frame one.
- Catchup wall time grows (steps share the frame with draw). Progress is visible every frame.
- Simplest mental model. May be enough if ~12k steps finish in a few seconds at 60 FPS budgets.

### 4. Faster catchup / less work

Further ideas: skip idle islands, or pin-mode catchup that omits beam until MAP completes.

### Recommended direction

Prefer **(2)** or **(3)** for a productized live boot view:

- **(3)** if we accept slightly longer catchup and want one thread.
- **(2)** if we keep the worker for max step throughput and want smooth 60 FPS chrome.

Avoid holding `board_mu` across a full `r01s_ui_draw` while the worker is stepping - that is what produced the ~4 FPS boot UI.

## Related knobs

| Item | Role |
|------|------|
| `R01S_SOFTBOOT=1` | Skip IC stream. Host poke (triage only) |
| LCD `SCALE` 1x/2x | Sidebar / `G`. Independent of catchup threading |
| LCD texture blit | Streaming upload of sink RGB (not per-pixel `DrawPoint`) |

## Catchup code path

**Worker (`app.c` + `r01s_board_catchup_bringup`):**

1. Worker locks `board_mu`, runs batches of `r01s_island_group_step` (**32**), unlocks, signals spinner, repeats.
2. Each step is a full `board_step`: PHI2, CPU micro-ops, decode, MAP/VRAM ports, beam dots, video wire.
3. Cart PRG (or synthetic bring-up overlay if no cart file) runs the MAP/pal stream: LDA `$FE93` / STA `$FE12` (and palette path) until `map_addr >= cart_off_map_screen0 + 480` and VRAM[0] matches flash.
4. Wall time is on the order of **~10-15 s** (~1 ms/step x ~12k steps on a typical machine).
5. Side effect: CPU PC, latches, PHI2 edges, beam position, and partial LCD samples advance for real during the stream.

`R01S_SOFTBOOT=1` is an alternate path (host softboot + some steps) for triage only - not the default cart PRG execution model.

See also: [`README.md`](README.md), [`PERFORMANCE.md`](PERFORMANCE.md).

# End-to-End Photon Timer (Lag Solver)

## Project Overview
This hardware tool measures absolute system-wide input latency[span_0](start_span)[span_0](end_span). It calculates the exact time between a physical button press and the moment the display emits light for the corresponding action[span_1](start_span)[span_1](end_span). 

The system uses a microcontroller to trigger a button press via an optocoupler while simultaneously starting a high speed hardware timer[span_2](start_span)[span_2](end_span). A photodiode attached to the display detects the visual update and triggers an interrupt to stop the timer[span_3](start_span)[span_3](end_span). This provides true photon-to-photon latency data for any hardware or emulator setup[span_4](start_span)[span_4](end_span).

## The Anatomy of Latency Scenarios

When analyzing lag on retro systems or custom architectures, latency is entirely dictated by where the button press lands relative to the CPU polling cycle and the display refresh cycle[span_5](start_span)[span_5](end_span).

### Minimum Lag (The "Just Made It" Scenario)
This represents the absolute best case response time for a standard game loop[span_6](start_span)[span_6](end_span). 
* **The Timing:** The physical button is pressed microseconds before the game code reads the controller registers during VBLANK (or the start of the logic frame)[span_7](start_span)[span_7](end_span).
* **The Result:** The input is captured immediately[span_8](start_span)[span_8](end_span). The game logic processes the movement and queues the graphics update in the exact same frame[span_9](start_span)[span_9](end_span).
* **Visual Output:** As the active display period begins, the electron gun (or LCD refresh line) draws the updated tile[span_10](start_span)[span_10](end_span). The latency is purely the time it takes for the raster beam to reach the physical location of the photodiode on the screen[span_11](start_span)[span_11](end_span).

### Maximum Lag (The "Just Missed It" Scenario)
This is the worst case response time for a highly optimized game loop[span_12](start_span)[span_12](end_span).
* **The Timing:** The physical button is pressed one microsecond after the CPU finishes reading the controller[span_13](start_span)[span_13](end_span).
* **The Result:** The hardware registers the press, but the game logic ignores it because the polling phase is already over[span_14](start_span)[span_14](end_span). The system spends the entire active frame drawing old data[span_15](start_span)[span_15](end_span). 
* **Visual Output:** The input will not be read until the next VBLANK[span_16](start_span)[span_16](end_span). The logic processes it during that second frame, and the screen finally draws the result on the subsequent display pass[span_17](start_span)[span_17](end_span). This adds nearly a full 16.6ms (at 60Hz) to the minimum lag time[span_18](start_span)[span_18](end_span).

### Average Lag
Because human input is completely asynchronous to the console clock, presses will land randomly throughout the frame[span_19](start_span)[span_19](end_span). Over hundreds of tests, the average latency will naturally settle exactly halfway between the minimum and maximum scenarios[span_20](start_span)[span_20](end_span).

## Edge Cases and Advanced Render Architectures

Standard VBLANK polling is common, but custom software engines can drastically alter how and when latency occurs[span_21](start_span)[span_21](end_span).

### Active Display Polling
Not all games poll the controller during VBLANK[span_22](start_span)[span_22](end_span). Some engines distribute CPU load by reading the gamepad halfway through the active render time[span_23](start_span)[span_23](end_span). If a user presses a button during VBLANK on this type of engine, the input will sit idle in the hardware register until the mid-frame poll occurs[span_24](start_span)[span_24](end_span). This shifts the minimum and maximum windows forward, often guaranteeing at least half a frame of visual lag[span_25](start_span)[span_25](end_span).

### Multi-Frame Engine Buffering
In complex games, processing physics, background scrolling, and sprite animations might take more than one frame of CPU time[span_26](start_span)[span_26](end_span). 
* **Double Buffering:** Games that use a double buffered VRAM approach will intentionally hold the completed frame in a hidden memory bank and swap it on the next VBLANK[span_27](start_span)[span_27](end_span). 
* **The Impact:** Even if the input is caught perfectly (Minimum Lag scenario), the visual result is artificially delayed by one or more full frame cycles because the engine requires extra time to compose the final image[span_28](start_span)[span_28](end_span).

### Mid-Frame Graphics Updates (Racing the Beam)
This is the theoretical limit for achieving the lowest possible latency on raster displays[span_29](start_span)[span_29](end_span).
* **The Technique:** Instead of waiting for VBLANK to update VRAM or palette registers, the CPU calculates the input immediately and updates the graphics hardware while the screen is drawing[span_30](start_span)[span_30](end_span).
* **The Catch:** The code must update the graphics data for screen areas that the raster beam has not reached yet[span_31](start_span)[span_31](end_span). 
* **The Impact:** If done correctly, a button pressed near the top of the screen can cause a visual change at the bottom of the screen in the exact same frame, achieving sub-frame latency[span_32](start_span)[span_32](end_span). However, doing this often causes horizontal screen tearing, as the top half of the screen shows the old frame and the bottom half shows the new frame[span_33](start_span)[span_33](end_span).

## Differentiating Factors (Why this project stands out)

Most low cost DIY hardware devices for measuring button-to-photon latency rely on standard Arduino boards emulating USB mouse and keyboard events for PC gaming[span_34](start_span)[span_34](end_span). The Lag Solver design sets itself apart by functioning as native hardware diagnostic equipment.

* **VBLANK and Scanline Syncing:** Open source projects typically measure the raw time elapsed until the sensor detects a luminance change[span_35](start_span)[span_35](end_span). By integrating a composite video sync (CSYNC) input, the AVR microcontroller can calculate the delay by stating the exact number of scanlines or VBLANK cycles that passed between the input and output.
* **Native Synchronous Injection:** Instead of using optocouplers to blindly short physical button contacts[span_36](start_span)[span_36](end_span), the device acts as a middleman on the controller serial bus. It listens for the LATCH and CLOCK signals and injects the button read exactly on the cycle the 6502 processor requests it.
* **Standalone Benchtop Operation:** Multiple current tools require cross-platform host software (like Python scripts) running on a PC to process the latency[span_37](start_span)[span_37](end_span). This device removes that dependency by integrating a local I2C display to show real time results directly on the workbench.
* **Dedicated Through-Hole Hardware:** It replaces the typical prototype build of jumper wires, breadboards, and loose phototransistors[span_38](start_span)[span_38](end_span) with a formal printed circuit board and a 24MHz microcontroller working natively at 5V.

# End-to-End Photon Timer (Lag Solver)

## Project Overview
This hardware tool measures absolute system-wide input latency. It calculates the exact time between a physical button press and the moment the display emits light.

The system uses a microcontroller to trigger a button press via an optocoupler while simultaneously starting a high speed hardware timer. A photodiode attached to the display or the screen itself detects the first visible light change, allowing the device to measure the end-to-end delay precisely.

## The Anatomy of Latency Scenarios

When analyzing lag on retro systems or custom architectures, latency is entirely dictated by where the button press lands relative to the CPU polling cycle and the display refresh cycle.

### Minimum Lag (The "Just Made It" Scenario)
This represents the absolute best case response time for a standard game loop.
* **The Timing:** The physical button is pressed microseconds before the game code reads the controller registers during VBLANK (or the start of the logic frame).
* **The Result:** The input is captured immediately. The game logic processes the movement and queues the graphics update in the exact same frame.
* **Visual Output:** As the active display period begins, the electron gun (or LCD refresh line) draws the updated tile. The latency is purely the time it takes for the CPU to run the logic and the display to refresh.

### Maximum Lag (The "Just Missed It" Scenario)
This is the worst case response time for a highly optimized game loop.
* **The Timing:** The physical button is pressed one microsecond after the CPU finishes reading the controller.
* **The Result:** The hardware registers the press, but the game logic ignores it because the polling phase is already over. The system spends the entire active display period waiting for the next polling opportunity.
* **Visual Output:** The input will not be read until the next VBLANK. The logic processes it during that second frame, and the screen finally draws the result one full frame later.

### Average Lag
Because human input is completely asynchronous to the console clock, presses will land randomly throughout the frame. Over hundreds of tests, the average latency tends to cluster around the midpoint between minimum and maximum.

## Edge Cases and Advanced Render Architectures

Standard VBLANK polling is common, but custom software engines can drastically alter how and when latency occurs.

### Active Display Polling
Not all games poll the controller during VBLANK. Some engines distribute CPU load by reading the gamepad halfway through the active render time.

### Multi-Frame Engine Buffering
In complex games, processing physics, background scrolling, and sprite animations might take more than one frame of CPU time.
* **Double Buffering:** Games that use a double buffered VRAM approach will intentionally hold the completed frame in a hidden memory bank and swap it on the next VBLANK.
* **The Impact:** Even if the input is caught perfectly (Minimum Lag scenario), the visual result is artificially delayed by one or more full frame cycles because the engine requires extra time to compose and present the final image.

### Mid-Frame Graphics Updates (Racing the Beam)
This is the theoretical limit for achieving the lowest possible latency on raster displays.
* **The Technique:** Instead of waiting for VBLANK to update VRAM or palette registers, the CPU calculates the input immediately and updates the graphics hardware while the screen is drawing.
* **The Catch:** The code must update the graphics data for screen areas that the raster beam has not reached yet.
* **The Impact:** If done correctly, a button pressed near the top of the screen can cause a visual change at the bottom of the screen in the exact same frame, achieving sub-frame latency.

## Differentiating Factors (Why this project stands out)

Most low cost DIY hardware devices for measuring button-to-photon latency rely on standard Arduino boards emulating USB mouse and keyboard events for PC gaming.

* **VBLANK and Scanline Syncing:** Open source projects typically measure the raw time elapsed until the sensor detects a luminance change. By integrating a custom controller emulator and optical timing reference, this project measures the actual system response from button input to visible output.
* **Native Synchronous Injection:** Instead of using optocouplers to blindly short physical button contacts, the device acts as a middleman on the controller signal path, preserving native timing and compatibility.
* **Standalone Benchtop Operation:** Multiple current tools require cross-platform host software (like Python scripts) running on a PC to process the latency. This design runs independently on the bench without external host software.
* **Dedicated Through-Hole Hardware:** It replaces the typical prototype build of jumper wires, breadboards, and loose phototransistors with a formal printed circuit board designed for repeatable measurement.

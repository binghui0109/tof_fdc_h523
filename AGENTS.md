# AGENTS.md

## Product Context
- `tof_fdc_h5_cmake` is a ToF sensor firmware project for human presence detection/state detection.
- System target is up to **2 people** in scene (`people_count` must stay in range `0..2`).
- Runtime behavior on STM32H523 is the priority; refactor must not change the pipeline contract.

## Mission
Refactor monolithic logic from `reference/app_main.c` into simple, clean, modular code under `src/app` and `src/bsp`, with clear layering and ownership.

Current checkpoint:
- Background initialization refactor is in progress (`src/app/background.c` + integration in `src/app/tof_process.c`).
- Remaining major blocks from `reference/app_main.c` to refactor: `label_components`, `update_tracks`, `create_depth_profiles`, `process_frame_data`, `ai_output_moving_average`, and command parsing callbacks.

## Quality Reference
- Use `reference/fdc_afci` as the **style and architecture reference** for clean layering, clear module APIs, and readable firmware C.
- Treat `reference/fdc_afci` as a design reference (structure/patterns), not a copy-paste source.
- Keep this repo's hardware behavior and protocol behavior as the source of truth.

## Scope and Source of Truth
- Behavior reference: `reference/app_main.c`.
- Implementation/refactor target: `src/app/*.c`, `src/app/*.h`, and `src/bsp/*` when required.
- Keep `vendor/` and `usb/` edits minimal (only bug fixes or required integration points).
- Preserve `vendor/Core/Src/main.c` loop contract: `app_init()` once, `app_main()` continuously.

## Layering Rules (Must Follow)
- `app_main.*`: orchestration/state machine only; no heavy algorithm logic.
- Domain pipeline modules (`background`, `segmentation`, `tracking`, `depth_profile`, `classifier`): pure processing logic with explicit inputs/outputs.
- Service modules (`sensor_manager`, `connection_manager`): data movement, device I/O coordination, protocol handling.
- BSP layer: hardware abstraction only; no app/domain policy.
- Dependency direction should be one-way: `app_main` -> modules/services -> `bsp`.

## Target Module Layout
Use or extend files in `src/app`:
- `app_main.c/.h`: top-level orchestration and state machine.
- `sensor_manager.c/.h`: VL53 data acquisition and publish/subscribe.
- `background.c/.h`: background model collection and statistics.
- `segmentation.c/.h`: connected-component labeling (`label_components`, flood-fill helpers).
- `tracking.c/.h`: track lifecycle and people in/out updates (`update_tracks`, `update_people_out`).
- `depth_profile.c/.h`: depth categorization (`create_depth_profiles`).
- `classifier.c/.h`: AI input shaping + output smoothing (`process_frame_data`, `ai_output_moving_average`).
- `connection_manager.c/.h`: packet encoding/decoding and communication flow.
- `tof_types.h`: shared domain structs/constants only.

If a module already exists, extend it instead of creating overlapping modules.

## Naming and Code Style Rules
- `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for macros.
- Module-prefixed APIs (`bg_*`, `seg_*`, `track_*`, `depth_*`, `clf_*`, `conn_*`, `sensor_*`).
- Keep functions short and single-purpose; split large functions into private `static` helpers.
- Avoid non-const globals in headers; prefer module context/state structs.
- Prefer fixed-width integer types (`uint8_t`, `uint16_t`, etc.).
- Replace magic numbers with named constants owned by the module.

## Behavior-Safety Rules
- Refactor in small compile-safe steps.
- Preserve packet format (`FUT0 ... END0`) and command type behavior unless explicitly requested.
- Communication send path must use `bsp_serial_tx_data(...)` (USB CDC path), not direct UART HAL calls.
- Preserve timing-sensitive flow: sensor ready -> collect -> pipeline -> output.
- Keep people counting behavior consistent and clamped to 2 persons max.

## Recommended Refactor Sequence
1. Consolidate shared constants/types in `tof_types.h`.
2. Finalize `segmentation.*` extraction and parity checks.
3. Finalize `tracking.*` extraction (including people in/out and smoothing).
4. Finalize `depth_profile.*` extraction.
5. Finalize `classifier.*` extraction.
6. Finalize `connection_manager.*` callbacks/packet handling.
7. Remove remaining monolithic logic from `tof_process.c`/`app_main.c`.
8. Keep `app_main.c` thin and orchestration-only.

## Build and Validation
Use:
- `make configure`
- `make build`
- optional: `make static-analysis`

Minimum validation per step:
- Project builds successfully.
- Background collection completes and state transitions continue.
- Presence output remains functionally consistent (0/1/2 people states).
- Packet output and command parsing remain compatible.

## Current Cleanup Opportunities
- `src/app/app_main.c`: replace global mutable state with `app_context`.
- `src/app/tof_process.c`: reduce unused copies and clarify ownership with `sensor_manager`.
- `src/app/background.c`: use integer sqrt helper consistently (avoid float `sqrt` in hot path unless required).
- `src/app/sensor_manager.c`: harden unsubscribe compaction and callback safety.

## Definition of Done
- `reference/app_main.c` logic is fully represented by modular files in `src/app`.
- Layering is clear and consistent with `reference/fdc_afci` style (orchestration vs processing vs hardware).
- `app_main.c` only wires modules and owns top-level state transitions.
- Module interfaces are explicit in headers; responsibilities are unambiguous.
- Firmware builds cleanly and preserves expected on-device behavior for up to two-person presence detection.

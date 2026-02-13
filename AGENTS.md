# AGENTS.md

## Product Context
- `tof_fdc_h5_cmake` is a ToF firmware project for human presence/state detection on STM32H523.
- System target is up to **2 people** in scene (`people_count` must stay in `0..2`).
- Priority is now: **improve presence detection logic quality** while keeping code simple, modular, and testable.

## Mission
Improve and clean the presence pipeline from `reference/app_main.c` into modular code in `src/app` and `src/bsp`, with explicit stage boundaries and measurable behavior gains.

## Quality Reference
- Use `reference/fdc_afci` as style/architecture reference (clean layering, clear APIs, readable firmware C).
- Use `reference/app_main.c` as legacy behavior baseline.
- Do not copy-paste logic from references; adapt with clean ownership and typed interfaces.

## Scope and Source of Truth
- Implementation target: `src/app/*.c`, `src/app/*.h`, `src/bsp/*` when required.
- Keep `vendor/` and `usb/` changes minimal (integration or bug-fix only).
- Preserve `vendor/Core/Src/main.c` loop contract: `app_init()` once, `app_main()` continuously.

## Presence Pipeline Baseline
The detection pipeline should remain explicitly staged:
1. Background model update
2. Foreground extraction (background subtraction + threshold gate)
3. Connected components
4. Depth profile shaping/splitting
5. Tracking + people in/out accounting
6. Count debouncing/smoothing
7. Presence output (`people_count > 0`)

Each stage must expose clear input/output data and avoid hidden cross-stage globals.

## Logic Improvement Directives (Focus)
### 1) Background and Foreground Robustness
- Use consistent naming: mean/std/variance (avoid misleading names like `*_std` for mean values).
- Keep threshold integer-friendly and explicit: `threshold = max(k * std, min_mm_threshold)` with configurable constants.
- Validate ToF status handling with a whitelist policy; avoid treating unknown statuses as valid foreground.
- Keep invalid/far filtering centralized in one helper to avoid duplicated conditions.
- Current default implementation target:
  - `k = 2`, `min_mm_threshold = 80`
  - status whitelist for foreground: `5` and `9`
  - any non-whitelisted status or distance `> 4000mm` is non-foreground

### 2) Segmentation Quality
- Keep component filtering deterministic (min size, bounded count, 8-neighbor).
- Add clear optional noise cleanup (small-island removal) before tracking if needed.
- Avoid relabel side effects unless the stage contract explicitly allows component mutation.

### 3) Tracking and Counting Correctness
- Track loops must use track count, component loops must use component count (no mixed-index bugs).
- Prefer squared-distance gating to avoid unnecessary `sqrtf` in hot path.
- Keep track lifecycle explicit: `candidate -> stable -> inactive/removed`.
- Make `people_in`/`people_out` thresholds time-based constants (derived from ODR) instead of magic frame counts.

### 4) Presence Debounce Strategy
- Keep both raw and debounced counts (`raw_people_count`, `smoothed_people_count`) for debugability.
- Majority vote is acceptable, but define tunable latency targets; consider hysteresis enter/exit counters if latency is high.
- Preserve hard clamp to 2 persons at final output.

### 5) Observability
- Add lightweight per-stage counters/flags for debug builds (dropped frame, invalid status, component_count, active_tracks).
- Keep telemetry optional and isolated from core logic.

### 6) Complexity Reduction (CodeScene Hotspots)
- Prioritize complexity reduction in:
  - `seg_label_components(...)` (`src/app/logic/segmentation.c`)
  - `depth_profile_generate(...)` (`src/app/logic/depth_profile.c`)
  - `track_update(...)` (`src/app/logic/tracking.c`)
- Keep orchestration functions thin and phase-based. A single function should orchestrate steps, not implement all branch logic.
- Target thresholds:
  - orchestration functions: cyclomatic complexity <= 12
  - helper functions: cyclomatic complexity <= 8
  - preferred nesting depth <= 3

#### Segmentation Refactor Rules
- Split into explicit helpers:
  - seed/scan candidate pixel
  - flood-fill component
  - component accept/reject finalize
- Replace repeated 8-neighbor push calls with a static offset table loop.
- Avoid full-frame relabel cleanup on reject when possible; clear only touched pixels of current component.
- Preserve deterministic behavior: same min size filter, same max component cap, same 8-neighbor connectivity.

#### Depth Profile Refactor Rules
- Split pixel classification into helper(s): compute scale, classify depth class, optional refine.
- Precompute row-related values where practical to reduce repeated branches in inner loops.
- Keep split fallback behavior explicit and isolated:
  - first split attempt using profile `2` (head-like)
  - fallback using profile `1` (body-like), preserving current behavior unless intentionally changed.
- If fallback criteria are changed (e.g., include `3`), document expected behavior impact.

#### Tracking Refactor Rules
- Split `track_update(...)` into phase helpers:
  - age active tracks
  - build match pairs
  - apply greedy matches
  - spawn unmatched tracks
  - retire + compact tracks
  - collect output info
- Prefer squared-distance compare for matching threshold to avoid `sqrtf` in hot path.
- Replace magic thresholds with named constants in module scope (`stable_frames`, `count_in_frames`, etc.).
- Preserve current people in/out semantics unless explicitly requested to tune behavior.

#### Safe Refactor Order (Required)
1. Refactor `track_update(...)` into helper phases with no behavior change.
2. Refactor `seg_label_components(...)` flood-fill and neighbor handling.
3. Refactor `depth_profile_generate(...)` classification helpers.
4. Re-evaluate complexity metrics and only then consider behavior-level tuning.

## Layering Rules (Must Follow)
- `src/app/core/*`: orchestration + runtime flow + communication/sensor managers.
- `src/app/logic/*`: pure detection/analytics logic with explicit inputs/outputs.
- BSP layer: hardware abstraction only.
- Dependency direction: `core -> logic -> bsp` is not allowed; logic should stay BSP-independent. Preferred: `core -> logic` and `core -> bsp`.

## Module Layout
Use/extend these modules:
- `src/app/core/`
  - `app_main.c/.h`
  - `tof_process.c/.h` (pipeline orchestrator)
  - `sensor_manager.c/.h`
  - `connection_manager.c/.h`
- `src/app/logic/`
  - `background.c/.h`
  - `foreground_filter.c/.h`
  - `segmentation.c/.h`
  - `depth_profile.c/.h`
  - `tracking.c/.h`
  - `presence_logic.c/.h`
  - `classifier.c/.h`
  - `tof_types.h`

If a module exists, extend it instead of creating overlapping responsibilities.

## Naming and Code Style
- `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for macros.
- Module-prefixed APIs (`bg_*`, `seg_*`, `track_*`, `depth_*`, `clf_*`, `conn_*`, `sensor_*`).
- Small functions, single responsibility, private `static` helpers.
- Avoid non-const globals in headers; prefer explicit context structs.
- Use fixed-width integer types and avoid magic numbers.

## Behavior-Safety Rules
- Apply changes in small compile-safe steps.
- Preserve packet format (`FUT0 ... END0`) and command behavior unless intentionally revised.
- Communication TX must use `bsp_serial_tx_data(...)` only.
- Preserve timing order: sensor ready -> collect -> pipeline -> output.
- Any intentional detection logic change must be documented with expected impact (e.g., lower false positives, lower latency).

## Build and Validation
Use:
- `make configure`
- `make build`
- optional: `make static-analysis`

Minimum validation per step:
- Build success.
- Background collection completes and state transitions continue.
- Presence output remains stable and bounded (`0/1/2`).
- Packet output and command parsing remain compatible.

Recommended logic checks:
- Empty scene stability (false positives).
- Single-person enter/exit latency.
- Two-person overlap/split behavior.
- Temporary occlusion/reacquisition behavior.
- Noisy frame resilience.

## Definition of Done
- Presence logic is modular, readable, and stage-separated in `src/app`.
- Major logic risks are addressed (status gating, tracking correctness, count debounce clarity).
- `app_main.c` remains thin orchestration only.
- Firmware builds cleanly and maintains expected on-device behavior for up to two-person detection.

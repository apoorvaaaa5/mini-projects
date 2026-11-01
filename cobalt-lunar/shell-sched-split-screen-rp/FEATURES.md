# Split-Screen Tiling Engine Specification — StrataHalo OS (RP2040)

Detailed technical specification of the multi-pane split-screen tiling window manager and console subsystem implemented in `shell-sched-split-screen-rp`.

---

## 1. Dynamic Split-Screen Tiling States

| Mode | Trigger Condition | Left Pane (Columns 1–39) | Divider (Col 40) | Right Pane (Columns 41–80) |
| :--- | :--- | :--- | :--- | :--- |
| **`SPLIT_NONE`** | 0 active background tasks | Full terminal width (Cols 1–80) | None | Inactive / Hidden |
| **`SPLIT_VERTICAL_SINGLE`** | 1 active background task | Interactive shell with auto-wrap at column 38 | `\|` on Rows 1–24 | Single full-height pane (Rows 1–24) |
| **`SPLIT_VERTICAL_DUAL`** | 2 active background tasks | Interactive shell with auto-wrap at column 38 | `\|` on Rows 1–24 | Split horizontally into Top-Right (Rows 1–11) and Bottom-Right (Rows 13–24) with divider at Row 12 |

---

## 2. Left Shell Pane Line Wrapping

To prevent shell commands and output from colliding with the right-side panes:
* The console driver tracks `left_col`.
* When `current_split_mode != SPLIT_NONE`:
  * If `left_col >= 38`, `console_putc` automatically inserts a carriage return and line feed (`\r\n`).
  * `left_col` resets to 0.
* Guarantees that even excessively long commands or outputs wrap cleanly inside the left pane.

---

## 3. Right Pane Telemetry Graphics

### Top-Right / Single Pane (`breathe`)
* **Task Identifier:** `[BG: breathe]` (Cyan bold header).
* **Peripheral Details:** Hardware PWM on GPIO 25, 2 kHz carrier frequency, 10 ms step timing.
* **Live Duty Cycle Bargraph:**
  ```text
  Duty: [==============      ] 70%
  ```
  Dynamic 20-character visual gauge updating continuously in place.
* **Status Badge:** `ACTIVE (BG)` in green.

### Bottom-Right Pane (`display`)
* **Task Identifier:** `[BG: display]` (Cyan bold header).
* **Task Details:** Continuous counter display task with 2000 ms cadence.
* **Live Metrics:** Monotonic tick counter and live rotating spinner (`/`, `-`, `\`, `|`).
* **Status Badge:** `ACTIVE (BG)` in green.

---

## 4. Cursor Management & Terminal Safety

* **Non-Destructive Coordinate Jumping:**
  * When background tasks update the right-side panes, the current shell cursor is saved using `\033[s`.
  * Output is positioned using ANSI coordinate addressing `\033[row;colH`.
  * After drawing, the hardware cursor is restored to `\033[u` right at `halo> █` in the left pane.
* **Zero DECSTBM:** Eliminates hardware scroll margin resets, preventing cursor teleportation to Row 1, Column 1.
* **Escape Interception:** Transparently swallows arrow keys and mouse scrolls so `[A[A[A[B` never corrupts the shell buffer.

# Cursor_Vansh for Windows

[![Latest Release](https://img.shields.io/github/v/release/Vansh-Baranwal/Cursor_Move_Vansh?style=flat-square&color=blue)](https://github.com/Vansh-Baranwal/Cursor_Move_Vansh/releases/latest)
[![Downloads](https://img.shields.io/badge/download-v1.0.0-success?style=flat-square)](https://github.com/Vansh-Baranwal/Cursor_Move_Vansh/releases/latest)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-lightgrey?style=flat-square)](https://github.com/Vansh-Baranwal/Cursor_Move_Vansh)

A hyper-optimized, standalone Windows background utility that brings the iconic macOS **"Shake mouse pointer to locate"** feature to Windows 10 & 11.

---

## 📥 Download

You can download the latest pre-compiled binaries directly from the **[Releases](https://github.com/Vansh-Baranwal/Cursor_Move_Vansh/releases/latest)** tab:

| File | Description | Download | Size |
|---|---|:---:|:---:|
| **`Cursor_Vansh.zip`** | **Recommended:** Complete package with 1-click install/uninstall scripts | [**Download ZIP**](https://github.com/Vansh-Baranwal/Cursor_Move_Vansh/releases/latest/download/Cursor_Vansh.zip) | ~79 KB |
| **`Cursor_Vansh.exe`** | Standalone portable executable (runs directly, zero installation needed) | [**Download EXE**](https://github.com/Vansh-Baranwal/Cursor_Move_Vansh/releases/latest/download/Cursor_Vansh.exe) | ~157 KB |

---

## What is the Shake Cursor Effect?

When you lose track of your cursor and rapidly shake or wiggle your mouse, the cursor temporarily magnifies so you can spot it immediately, then smoothly shrinks back to its normal size.

### Kinematics & Mechanics
* **Trigger Mechanism:** Unlike linear movement (such as fast flicking or swiping across multiple monitors), the feature specifically tracks **directional reversals (oscillations)** within a tight time window (~400 ms).
* **Movement Thresholds:**
  * **Stroke Count:** Requires at least 4 to 5 directional reversals (e.g. left-right-left-right-left).
  * **Stroke Distance:** Each stroke must exceed ~12–15 pixels (filtering out micro-tremors and gaming mouse sensor noise).
  * **Confinement Ratio:** Net bounding displacement must be significantly smaller than total distance traveled ($\frac{\text{Total Path}}{\text{Net Span}} \ge 1.65$). This prevents false triggers when moving rapidly across multiple monitors.
* **Visual Scaling & Dynamics:**
  * **Scale Multiplier:** Scales from normal size ($1.0\times$) up to $3.6\times$ maximum size.
  * **True Foreground Rendering:** While enlarged, the native hardware pointer is temporarily blanked so the enlarged vector cursor sits cleanly **100% in front** with zero overlap.
  * **Growth Animation:** Snappy ease-out cubic curve over ~130 ms.
  * **Hold Duration:** Holds at peak scale for ~220 ms while movement continues.
  * **Shrink Animation:** Smooth ease-in curve over ~240 ms, fading seamlessly back to the restored native cursor.
  * **Hotspot Alignment:** The tip of the pointer remains pinned at $(x, y)$, ensuring 100% accurate clicking and zero disruption.

---

## Smart Gaming & Cursor Visibility Protection

Unlike naive implementations, **`Cursor_Vansh` never interferes with video games:**
* **Active Cursor Check (`IsDefaultCursorVisible`):** The shake effect activates **only when the default mouse cursor is actively visible and present** on your screen.
* **In-Game Aim Suppression:** When you are playing a first-person shooter or 3D game where the mouse is used to aim or look around (and the cursor is hidden via `ShowCursor(FALSE)` or `SetCursor(NULL)`), `Cursor_Vansh` automatically suppresses the effect and flushes movement history so aiming sweeps never trigger it.
* **Camera Lock Detection:** Automatically detects mouse clipping (`ClipCursor`) used by game engines for camera controls.
* **Optional Fullscreen Pause:** A dedicated **"Pause in Fullscreen Games"** toggle in the tray menu allows you to pause the effect in fullscreen games even if an in-game cursor is active.

---

## Key Features & Optimizations

* **Ultra-Lightweight Binary:** Standalone binary is only **~157 KB** (compiled in pure C++ / Win32 with MSVC `/O2 /GL /LTCG`).
* **Zero External Dependencies:** No .NET runtime required, no Electron, no Python, no third-party DLLs. Runs natively on any 64-bit Windows 10/11 installation.
* **True 0.00% Idle CPU:** When not shaking, the overlay window is completely hidden and timers are stopped. CPU usage is strictly **0.00%**.
* **Minimal Memory:** Uses less than **10 MB RAM** (Working Set) and ~2 MB private memory.
* **100% Click-Through & Safe:** Uses Win32 `WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE`. Clicks, drags, and scrolls pass directly through with zero latency. It never tampers with your Windows system cursor registry, making it impossible to corrupt your cursor.
* **Persistent Auto-Start:** Install once and it starts automatically with Windows every boot without having to re-run it.

---

## Quick Start & Usage

### 1. Install (Run on Boot)
Run `install.bat` or execute:
```cmd
bin\Cursor_Vansh.exe --install
```
* Automatically copies the program to `%LOCALAPPDATA%\Cursor_Vansh\Cursor_Vansh.exe`.
* Registers in `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
* Starts immediately in your System Tray.

### 2. Running Standalone (Without Installing)
Run `start.bat` or double-click `bin\Cursor_Vansh.exe`. It runs quietly in the notification area.

### 3. System Tray Menu Options
Right-click the cursor icon in your Windows Taskbar / System Tray:
* **Test Cursor Shake Effect:** Click to preview the enlargement animation immediately.
* **Start Automatically with Windows:** Toggle auto-start on or off anytime with one click.
* **Pause in Fullscreen Games:** Optional setting to automatically pause during full-screen games/apps.
* **Cursor Appearance:**
  * *macOS Classic Pointer (Default):* Iconic black pointer with crisp white border and soft drop shadow.
  * *Windows 11 Fluent Pointer:* Modern Windows white pointer with dark border.
  * *Scale System Pointer:* Captures and magnifies your currently active Windows cursor theme.
* **Shake Sensitivity:**
  * *Low:* Requires vigorous shaking.
  * *Medium (Default):* Balanced, authentic macOS feel.
  * *High:* Triggers with lighter, quicker wiggles.
* **Exit:** Cleanly terminates the background process.

### 4. How to Uninstall
You can remove `Cursor_Vansh` anytime with zero residue:
* **Method 1 (1-Click Script):** Double-click `uninstall.bat` (removes from startup and stops process).
* **Method 2 (From Tray):** Right-click the tray icon, uncheck **"Start Automatically with Windows"**, and click **"Exit"**.
* **Method 3 (Manual Cleanup):** Stop the process in Task Manager and delete the folder `%LOCALAPPDATA%\Cursor_Vansh`.

---

## Building from Source

To compile the standalone binary yourself, open Developer Command Prompt for Visual Studio and run:
```cmd
build.bat
```
This runs MSVC `cl.exe` with `/O2 /GL /Gy /LTCG` for maximum performance and compiles resources with `rc.exe`.

---

## Test Suite

To run the automated kinematics and runtime tests:
1. **Kinematics Tests:** `tests\test_detector.exe`
2. **Runtime Verification & Memory Benchmarks:**
```powershell
powershell -ExecutionPolicy Bypass -File tests\verify_runtime.ps1
```

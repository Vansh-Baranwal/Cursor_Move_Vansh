# MacShake Cursor for Windows

A hyper-optimized, standalone Windows background utility that brings the iconic macOS **"Shake mouse pointer to locate"** feature to Windows 10 & 11.

---

## What is the macOS Shake Cursor Effect?

Introduced in OS X El Capitan and present in macOS Sonoma & Sequoia, this feature temporarily magnifies the mouse cursor when you rapidly shake or wiggle your mouse, making it effortless to spot on dense desktop layouts or multi-monitor setups.

### Kinematics & Mechanics
* **Trigger Mechanism:** Unlike linear movement (such as fast flicking or swiping across screens), the feature specifically tracks **directional reversals (oscillations)** within a tight time window (~400 ms).
* **Movement Thresholds:**
  * **Stroke Count:** Requires at least 4 to 5 directional reversals (e.g. left-right-left-right-left).
  * **Stroke Distance:** Each stroke must exceed ~12–15 pixels (filtering out micro-tremors and gaming mouse sensor noise).
  * **Confinement Ratio:** Net bounding displacement must be significantly smaller than total distance traveled ($\frac{\text{Total Path}}{\text{Net Span}} \ge 1.65$). This prevents false triggers when moving rapidly across multiple monitors.
* **Visual Scaling & Dynamics:**
  * **Scale Factor:** Scales from normal size ($1.0\times$) up to $3.6\times$ maximum size.
  * **Growth Animation:** Snappy ease-out cubic curve over ~130 ms.
  * **Hold Duration:** Holds at peak scale for ~220 ms while movement continues.
  * **Shrink Animation:** Smooth ease-in curve over ~240 ms, fading seamlessly back to the standard cursor.
  * **Hotspot Alignment:** The tip of the pointer remains pinned at $(x, y)$, ensuring 100% accurate clicking and zero disruption.

---

## Key Features & Optimizations

* **Ultra-Lightweight Binary:** Standalone binary is only **~154 KB** (compiled in pure C++ / Win32).
* **Zero External Dependencies:** No .NET runtime required, no Electron, no Python, no third-party DLLs. Runs natively on any 64-bit Windows installation.
* **True 0% Idle CPU:** When not shaking, the overlay window is completely hidden and timers are stopped. CPU usage is strictly **0.00%**.
* **Minimal Memory:** Uses less than **10 MB RAM** (Working Set) and ~2 MB private memory.
* **100% Click-Through & Safe:** Uses Win32 `WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE`. Clicks, drags, and scrolls pass directly through with zero latency. It never tampers with your Windows system cursor registry, making it impossible to corrupt your cursor.
* **Persistent Auto-Start:** Install once and it starts automatically with Windows every boot without having to re-run it.

---

## Quick Start & Usage

### 1. Install (Run on Boot)
Run `install.bat` or execute:
```cmd
bin\MacShakeCursor.exe --install
```
* Automatically copies the program to `%LOCALAPPDATA%\MacShakeCursor\MacShakeCursor.exe`.
* Registers in `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
* Starts immediately in your System Tray.

### 2. Running Standalone (Without Installing)
Run `start.bat` or double-click `bin\MacShakeCursor.exe`. It runs quietly in the notification area.

### 3. System Tray Menu Options
Right-click the icon in your Windows notification area (System Tray) to access:
* **Test Cursor Shake Effect:** Instantly preview the enlargement animation.
* **Start Automatically with Windows:** Toggle auto-start on or off anytime with one click.
* **Cursor Appearance:**
  * *macOS Classic Pointer (Default):* Iconic black pointer with crisp white border and soft drop shadow.
  * *Windows 11 Fluent Pointer:* Modern Windows white pointer with dark border.
  * *Scale System Pointer:* Captures and magnifies your currently active Windows cursor theme.
* **Shake Sensitivity:**
  * *Low:* Requires vigorous shaking.
  * *Medium (Default):* Balanced, authentic macOS feel.
  * *High:* Triggers with lighter, quicker wiggles.
* **Exit:** Cleanly terminates the background process.

### 4. Uninstall
Run `uninstall.bat` or execute:
```cmd
bin\MacShakeCursor.exe --uninstall
```

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

<div align="center">
  <h1>AxisEQ</h1>
  <p><b>A Premium 24-Band Dynamic Equalizer Plugin</b></p>
</div>

---

## 🎛️ Overview

**AxisEQ** is a professional-grade, high-performance parametric dynamic equalizer audio plugin built with JUCE. Designed for both precision mastering and creative mixing, AxisEQ provides unparalleled control over your audio with sleek, modern vector-based graphics and uncompromising DSP quality.

## ✨ Key Features

- **24-Band Parametric EQ**: Create up to 24 independent EQ bands by simply double-clicking the canvas.
- **Dynamic EQ & External Sidechain**: Every single band can act as a dynamic EQ. Duck specific frequencies using the internal signal, or route an external sidechain to trigger the dynamic gain reduction (e.g., ducking the bass guitar's 60Hz only when the kick drum hits).
- **Advanced Mid/Side Processing**: Route any EQ band to operate on the Stereo, Left, Right, Mid, or Side channels for advanced stereo imaging and mastering control.
- **Smart Filter Creation**: Double-clicking on the far-left of the spectrum automatically creates a Low Cut filter, while double-clicking on the far-right creates a High Cut filter.
- **Premium User Interface**: Features a custom-built, modern LookAndFeel with vector arcs, floating text, and dynamic gain reduction visualizations.
- **Zero Latency DSP**: Cascaded biquad filters utilizing RBJ Audio EQ Cookbook formulas guarantee analog-matched curves and lightning-fast processing with zero allocations on the audio thread.

## 🛠️ Installation (Windows)

For an automatic installation into REAPER or your VST3 host of choice:
1. Clone the repository.
2. Run the `install.bat` script located in the project root.
3. The script will automatically compile the plugin using CMake and copy the VST3 file directly into your `C:\Program Files\Common Files\VST3` folder.

## 💻 Manual Build Instructions

AxisEQ uses **CMake** and **JUCE 8**. You can compile the plugin yourself from the command line.

### Windows (Visual Studio)

```powershell
# 1. Generate project files (using your installed version of Visual Studio)
cmake -B build -G "Visual Studio 18 2026" -A x64

# 2. Build the plugin in Release mode using all CPU cores
cmake --build build --config Release --parallel
```

Once compiled, the plugin will be located at:
`build\AxisEQ_artefacts\Release\VST3\AxisEQ.vst3`

### macOS (Xcode)

```bash
# 1. Generate Xcode project files
cmake -B build -G Xcode -DCMAKE_BUILD_TYPE=Release

# 2. Build the plugin
cmake --build build --config Release --parallel
```

Once compiled, the plugins will be located at:
- VST3: `build/AxisEQ_artefacts/Release/VST3/AxisEQ.vst3`
- AU: `build/AxisEQ_artefacts/Release/AU/AxisEQ.component`

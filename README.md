<div align="center">
  <img src="AxisEQ_logo.png" alt="AxisEQ Logo" width="200"/>
  <h1>AxisEQ</h1>
  <p><b>A 24-band dynamic equalizer plugin (VST3 / AU)</b></p>
</div>

---

<div align="center">
  <img src="AxisEQ_screenshot.png" alt="AxisEQ Screenshot" width="800"/>
</div>

## 🎛️ Overview

**AxisEQ** is a parametric dynamic equalizer plugin built with JUCE and C++. Each band can be static or dynamic, work on any part of the stereo image, and be driven by an external sidechain. It is aimed at both mixing and mastering.

## ✨ Key Features

- **24-Band Parametric EQ**: Create up to 24 independent EQ bands by simply clicking the canvas.
- **Dynamic EQ & External Sidechain**: Every single band can act as a dynamic EQ. Duck specific frequencies using the internal signal, or route an external sidechain to trigger the dynamic gain reduction (e.g., ducking the bass guitar's 60Hz only when the kick drum hits).
- **Advanced Mid/Side Processing**: Route any EQ band to operate on the Stereo, Left, Right, Mid, or Side channels for advanced stereo imaging and mastering control.
- **Real-time Spectrum Analyzer**: Shows the signal before and after processing.
- **Custom User Interface**: A custom JUCE LookAndFeel drawn with vector graphics, including per-band gain reduction display.
- **Zero Latency DSP**: Cascaded biquad filters based on the RBJ Audio EQ Cookbook formulas, with no memory allocation on the audio thread.

## 🛠️ Installation

You can download the pre-compiled VST3 and AU plugins directly from the [Releases](https://github.com/FenrirRhogar/AxisEQ/releases) tab.

Alternatively, for an automatic installation on Windows into REAPER or your VST3 host of choice:
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

## ☕ Support the Project

AxisEQ is completely free and open-source. However, if you use it in your mixes and find it helpful, please consider **[buying me a coffee on Gumroad!](https://fenrikos.gumroad.com/l/axiseq)** 

Your support helps keep this project alive and funds the development of future audio plugins!

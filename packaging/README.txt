AXISEQ  -  version @VERSION@
by fenrikos

A 24-band dynamic equaliser: bells, shelves, tilt, notch, band-pass and 6-96 dB/oct cuts, each band
optionally dynamic, with stereo, mid or side routing and an external sidechain.


REQUIREMENTS
------------
[linux]
- 64-bit Linux (x86_64), glibc @GLIBC@ or newer
- A VST3 or LV2 host: REAPER, Bitwig Studio, Ardour, Carla, Qtractor, ...
- X11 or XWayland for the plugin window
[/linux]
[windows]
- Windows 10 or 11, 64-bit
- A VST3 host: REAPER, Ableton Live, FL Studio, Cubase, Studio One, Bitwig Studio, ...
[/windows]
[macos]
- macOS 10.15 or newer, Apple Silicon or Intel (universal binaries)
- An AU or VST3 host: Logic Pro, GarageBand, Ableton Live, REAPER, Cubase, ...
[/macos]


INSTALL
-------
[linux]
Open a terminal in this folder and run:

    ./install.sh             installs for your user (~/.vst3 and ~/.lv2)
    ./install.sh --system    installs for all users (/usr/lib/vst3, /usr/lib/lv2; asks for sudo)

Or copy VST3/AxisEQ.vst3 into ~/.vst3/ and LV2/AxisEQ.lv2 into ~/.lv2/ by hand.
To remove it, run ./uninstall.sh (add --system if you installed system-wide).
[/linux]
[windows]
Copy the "AxisEQ.vst3" folder inside VST3\ into:

    C:\Program Files\Common Files\VST3\

Windows asks for administrator permission to copy there. To remove AxisEQ, delete that folder.
[/windows]
[macos]
1. Copy the plugins:
     AU/AxisEQ.component   ->  /Library/Audio/Plug-Ins/Components/
     VST3/AxisEQ.vst3      ->  /Library/Audio/Plug-Ins/VST3/
   (In Finder: Go > Go to Folder..., then paste the path.)
@MAC_QUARANTINE@
To remove AxisEQ, delete those files again.
[/macos]

Then rescan plugins in your host. In REAPER: Options > Preferences > Plug-ins > "Re-scan".
[macos]
Logic Pro and GarageBand pick up new AU plugins after a restart.
[/macos]


USING AXISEQ
------------
- Click the canvas to add a band, drag a node to move it, right-click to bypass it, double-click to remove it.
- Select a band to edit it in the strip below. "Dynamic" makes it react to the signal: a negative range
  cuts when the band gets loud, a positive range boosts. "Ext SC" listens to the sidechain input instead.
- Click the preset name for the list, the arrows step through it, and SAVE stores your own.
  User presets live in the "AxisEQ/Presets" folder inside your Documents folder.
- The switch at the top right bypasses AxisEQ; hosts also see it as the bypass parameter.


LICENCE AND SOURCE
------------------
Copyright (C) 2026 fenrikos. AxisEQ is free software under the GNU Affero General Public License,
version 3 or later (see LICENSE.txt). It comes with ABSOLUTELY NO WARRANTY.
The complete source code is at https://github.com/FenrirRhogar/AxisEQ

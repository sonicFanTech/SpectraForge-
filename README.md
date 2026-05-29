# SpectraForge Console v0.3 Prototype

**SpectraForge Console** is an experimental C++ program for converting **images into sound** and **sound/audio into images**.

Version **0.3 Prototype** is the first version that includes a real GUI window while still keeping the original interactive console mode. It also adds real FFT-based spectrogram exporting, local FFmpeg support, and early image-to-sound effect presets.

> This is still a prototype. The goal of v0.3 is to prove that the core idea works before turning it into a larger full application.

---

## Table of Contents

- [About the Project](#about-the-project)
- [What This Program Can Do](#what-this-program-can-do)
- [Screenshots](#screenshots)
- [Version 0.3 Prototype Features](#version-03-prototype-features)
- [How It Works](#how-it-works)
- [Download / Build](#download--build)
- [Requirements](#requirements)
- [FFmpeg / bin Folder Setup](#ffmpeg--bin-folder-setup)
- [How to Use the GUI Mode](#how-to-use-the-gui-mode)
- [How to Use Console Mode](#how-to-use-console-mode)
- [Output Files](#output-files)
- [Supported File Types](#supported-file-types)
- [Project Folder Layout](#project-folder-layout)
- [Building from Source](#building-from-source)
- [Known Limitations](#known-limitations)
- [Planned Features](#planned-features)
- [Prototype Version History](#prototype-version-history)
- [Credits / Third-Party Tools](#credits--third-party-tools)
- [License](#license)

---

## About the Project

SpectraForge Console started as a simple idea:

> What if an image could become sound, and sound could become an image?

This prototype is a test program for experimenting with image/audio conversion. It is not meant to be a finished professional audio editor yet. Instead, it is a foundation for a future tool that could become a full GUI application with previews, effects, presets, spectrogram editing, and maybe even music-generation features.

The program is written in **C++** and currently targets **Windows**.

The project is intentionally made as a **non-CMake Visual Studio project**, because the goal for this prototype is to keep it easy to open, build, and test directly inside Visual Studio.

---

## What This Program Can Do

SpectraForge v0.3 can currently do three main things:

### 1. Image → WAV Sound

The program loads an image and turns its pixel brightness into layered audio frequencies.

In simple terms:

- Left-to-right image position becomes time.
- Dark pixels make little/no sound.
- Bright pixels make louder sound.
- Vertical image position maps to different frequencies.
- Different effect presets change how the generated audio sounds.

This creates experimental audio that is based on the structure of the image.

### 2. Audio / Video → Waveform BMP

The program can load audio and export a waveform image.

This is similar to what audio editors show when viewing the loudness/shape of an audio file over time.

### 3. Audio / Video → Spectrogram BMP

The program can export a real FFT-based spectrogram image.

A spectrogram shows how frequencies change over time:

- Left-to-right = time
- Bottom-to-top = frequency
- Color/brightness = frequency strength

This is one of the most important features because it makes sound visible as an image.

---

## Version 0.3 Prototype Features

### Main Features

- Native Windows GUI window
- Interactive console mode still included
- Image-to-WAV conversion
- Audio/video-to-waveform image export
- Audio/video-to-spectrogram image export
- Local FFmpeg support through a `bin\` folder
- Basic image-to-sound settings
- Early sound/effect presets
- Visual Studio solution/project files included
- No CMake required

### GUI Features

The normal program launch opens a real Windows GUI.

The GUI includes:

- Image input browser
- WAV output path selector
- Audio/video input browser
- BMP output selector
- Export mode selector
  - Waveform BMP
  - Real Spectrogram BMP
- Image-to-sound settings
  - Sample rate
  - Duration
  - Voice count
  - Volume
  - Effect preset
- FFmpeg detection status
- Button to refresh FFmpeg detection
- Console mode help button

### Console Features

The old console mode is still available.

It includes:

- Arrow-key / W-S menu navigation
- Image → WAV sound
- Audio/video → BMP waveform
- Audio/video → BMP spectrogram
- Settings/effects menu
- FFmpeg/bin resource status
- FFmpeg detection refresh

This is not a command-only console tool. It has an interactive menu.

---

## How It Works

### Image-to-Sound Conversion

The image-to-sound system currently works by reading image pixels and mapping them into generated sine-wave audio.

The basic idea is:

```text
Image X position  -> audio time
Image Y position  -> audio frequency
Pixel brightness  -> frequency volume
```

So if an image has bright pixels near the top, that can create higher-frequency audio. Bright pixels near the bottom create lower-frequency audio.

This version uses several layered sine-wave voices to create the final WAV file.

### Effects / Presets

v0.3 includes early effect presets:

| Effect | Description |
| --- | --- |
| Clean Spectral | The default mode. Smooth layered sine waves. |
| Dark Drone | Lower-frequency, darker, drone-like output. |
| Bright Melody | Brighter, higher-frequency, more harmonic output. |
| Glitch Pixels | Adds gated/noisy changes for a more glitchy sound. |

These effects are early experiments. Later versions may add more advanced processing such as distortion, reverb, delay, bitcrushing, pitch effects, and more.

### Waveform Export

Waveform export reads the audio samples and draws the loudness shape into a BMP image.

This is useful for seeing the overall shape of a sound file.

### Spectrogram Export

Spectrogram export converts audio into mono 16-bit WAV internally, then runs a Fast Fourier Transform/FFT process over the audio.

The spectrogram renderer uses:

- FFT window size: `2048`
- Log-style frequency mapping
- Color gradient based on frequency strength
- BMP output

This is a real spectrogram export, not just a fake visualizer.

---

## Download / Build

This repository is meant to be opened in Visual Studio.

The main solution file is:

```text
SpectraForgeConsole.sln
```

Open that file in Visual Studio and build the project.

---

## Requirements

### Required

- Windows
- Visual Studio with C++ desktop development tools
- Windows SDK
- C++17 support

### Recommended

- Visual Studio 2022 or newer
- x64 build configuration
- FFmpeg placed in the local `bin\` folder

### Optional but Very Useful

- `ffmpeg.exe`
- `ffprobe.exe`

FFmpeg is needed for importing many audio/video formats and for fallback loading of some image formats.

---

## FFmpeg / bin Folder Setup

All extra tools/resources that the program needs should go inside a folder called `bin`.

The `bin` folder should be placed next to the EXE.

Recommended layout:

```text
SpectraForgeConsole.exe
bin\
  ffmpeg.exe
  ffprobe.exe
```

The program also checks these layouts:

```text
SpectraForgeConsole.exe
bin\FFmpeg\ffmpeg.exe
bin\FFmpeg\ffprobe.exe
```

```text
SpectraForgeConsole.exe
bin\ffmpeg\ffmpeg.exe
bin\ffmpeg\ffprobe.exe
```

```text
SpectraForgeConsole.exe
bin\FFmpeg\bin\ffmpeg.exe
bin\FFmpeg\bin\ffprobe.exe
```

```text
SpectraForgeConsole.exe
bin\ffmpeg\bin\ffmpeg.exe
bin\ffmpeg\bin\ffprobe.exe
```

### Visual Studio Post-Build Copy

The project includes a post-build step that copies the project’s `bin\` folder into the output folder.

So if you put resources here:

```text
SpectraForgeConsole\bin\
```

then Visual Studio should copy them next to the built EXE.

---

## How to Use the GUI Mode

GUI mode is the default mode.

Just run:

```text
SpectraForgeConsole.exe
```

or double-click the EXE.

### Image → WAV Sound

1. Open the program.
2. In the **Image → WAV Sound** section, click **Browse...**
3. Choose an image file.
4. Choose or keep the output WAV path.
5. Adjust settings if needed:
   - Sample Rate
   - Duration
   - Voices
   - Volume
   - Effect
6. Click **Convert Image to WAV**.
7. The generated WAV file will be saved to the selected output path.

### Audio / Video → Image

1. Open the program.
2. In the **Audio / Video → Image** section, click **Browse...**
3. Choose an audio or video file.
4. Choose an export type:
   - Waveform BMP
   - Real Spectrogram BMP
5. Choose or keep the output BMP path.
6. Click **Export Audio Image**.
7. The BMP image will be saved to the selected output path.

---

## How to Use Console Mode

Console mode is still included.

Run the program with:

```bat
SpectraForgeConsole.exe --console
```

Other accepted forms:

```bat
SpectraForgeConsole.exe /console
SpectraForgeConsole.exe -console
```

The console mode uses an interactive menu.

Controls:

```text
Up / Down arrows = move through menu
W / S            = move through menu
Enter            = select
Esc              = go back
```

Console mode includes:

- Image → WAV sound
- Audio/video → BMP waveform image
- Audio/video → BMP spectrogram image
- Settings / effects
- FFmpeg / bin resource status
- Refresh FFmpeg detection

---

## Output Files

### WAV Output

Image-to-sound export creates:

```text
*_sound.wav
```

Example:

```text
picture.png
picture_sound.wav
```

### Waveform BMP Output

Waveform export creates:

```text
*_waveform.bmp
```

Example:

```text
song.mp3
song_waveform.bmp
```

### Spectrogram BMP Output

Spectrogram export creates:

```text
*_spectrogram.bmp
```

Example:

```text
song.mp3
song_spectrogram.bmp
```

---

## Supported File Types

### Image Input

Image loading uses Windows Imaging Component first.

Common supported formats usually include:

- PNG
- JPG / JPEG
- BMP
- GIF
- TIFF
- ICO

If WIC cannot load the file, the program can try FFmpeg as a fallback if `ffmpeg.exe` is found in `bin\`.

FFmpeg may support additional formats depending on the FFmpeg build.

### Audio / Video Input

Direct audio reading currently supports:

- 16-bit PCM WAV

With FFmpeg available, the program can import many more formats, such as:

- MP3
- OGG
- FLAC
- M4A
- AAC
- WMA
- MP4
- MKV
- WEBM
- AVI
- MOV
- and more, depending on the FFmpeg build

### Image Output

Current image export format:

- BMP

### Audio Output

Current audio export format:

- WAV

---

## Project Folder Layout

Basic source layout:

```text
SpectraForgeConsole_v0_3_GUI/
  README.md
  SpectraForgeConsole.sln
  SpectraForgeConsole/
    Main.cpp
    SpectraForgeConsole.vcxproj
    SpectraForgeConsole.vcxproj.filters
    bin/
      README_bin.txt
```

Build output layout usually becomes something like:

```text
bin/
  x64/
    Debug/
      SpectraForgeConsole.exe
      bin/
        ffmpeg.exe
        ffprobe.exe
```

or:

```text
bin/
  x64/
    Release/
      SpectraForgeConsole.exe
      bin/
        ffmpeg.exe
        ffprobe.exe
```

---

## Building from Source

### Step 1: Clone the Repository

```bat
git clone https://github.com/YOUR_USERNAME/YOUR_REPO_NAME.git
```

### Step 2: Open the Solution

Open:

```text
SpectraForgeConsole.sln
```

in Visual Studio.

### Step 3: Select Build Mode

Recommended:

```text
x64 / Release
```

Debug also works for testing.

### Step 4: Build

In Visual Studio:

```text
Build > Build Solution
```

### Step 5: Add FFmpeg

After building, put FFmpeg in the output folder:

```text
SpectraForgeConsole.exe
bin\ffmpeg.exe
bin\ffprobe.exe
```

Or place FFmpeg in:

```text
SpectraForgeConsole\bin\
```

before building, so the post-build copy step can copy it automatically.

---

## Known Limitations

This is a prototype, so there are known limitations.

### GUI Limitations

- The GUI is plain Win32 right now.
- The GUI does not have custom styling yet.
- Long conversions can make the window pause temporarily.
- There is no progress bar yet.
- There is no built-in preview player yet.
- There is no drag-and-drop support yet.

### Export Limitations

- Spectrogram export currently only saves BMP.
- Waveform export currently only saves BMP.
- PNG export is not added yet.
- Spectrogram settings are not exposed in the UI yet.
- Spectrogram resolution is currently fixed in code.
- FFT size is currently fixed in code.

### Audio Limitations

- Direct WAV loading only supports 16-bit PCM WAV.
- Other audio types need FFmpeg.
- Image-to-sound output currently exports WAV only.
- No MP3/OGG export yet.
- No live playback preview yet.

### Image Limitations

- Image loading depends on WIC and/or FFmpeg.
- Some unusual image formats may still not load.
- Image-to-sound is experimental and not meant to sound like normal music yet.

---

## Planned Features

Possible future features include:

### UI / UX

- Better custom GUI
- Tabs
- Progress bars
- Worker-threaded exports so the UI does not freeze
- Drag-and-drop file support
- Recent files list
- Better error messages
- Settings saved to an INI/JSON config file
- Dark theme
- Custom app icon
- About window
- Built-in update checker

### Audio Features

- Built-in audio preview/playback
- MP3 export
- OGG export
- FLAC export
- More image-to-sound algorithms
- More sound effects
- Reverb
- Echo/delay
- Distortion
- Bitcrusher
- Low-pass/high-pass filters
- Pitch shifting
- Stereo output
- Music-generation presets

### Image / Spectrogram Features

- PNG export
- JPG export
- Adjustable spectrogram size
- Adjustable FFT size
- Linear/log frequency toggle
- Color palette selection
- Inverted spectrogram mode
- Frequency labels
- Time labels
- Waveform + spectrogram combined output

### Experimental Features

- Hidden image inside audio mode
- Convert spectrogram image back into audio
- Image-as-music mode
- Audio-as-art mode
- Batch conversion
- Plugin/effect system
- Presets saved in external files inside `bin\` or a `Presets\` folder

---

## Prototype Version History

### v0.1 Prototype

Initial console proof-of-concept.

Features:

- Interactive console menu
- Image → WAV sound
- WAV → BMP waveform image
- Windows Imaging Component image loading
- Custom WAV writing
- Custom BMP writing

### v0.2 Prototype

Added FFmpeg/bin resource support.

Features added:

- Local `bin\` folder detection
- FFmpeg detection
- FFmpeg import for audio/video
- FFmpeg fallback for image loading
- Audio/video → waveform image
- Resource status menu

### v0.3 Prototype

Current version.

Features added:

- Native Win32 GUI
- Console mode kept through `--console`
- Real FFT-based spectrogram export
- Audio/video → spectrogram BMP
- Image-to-sound effects
- Better GUI workflow
- FFmpeg refresh button
- Console mode help

---

## Credits / Third-Party Tools

### FFmpeg

This project can optionally use FFmpeg for importing audio/video files and fallback image conversion.

FFmpeg is not created by this project.

FFmpeg official website:

```text
https://ffmpeg.org/
```

If you redistribute FFmpeg with your project, make sure to follow FFmpeg's license terms.

### Windows Imaging Component

Image loading uses Windows Imaging Component/WIC, which is part of Windows.

---

## License

- MIT License

Important note:

If no license is included, other people generally do not automatically have permission to reuse, modify, or redistribute the code.

---

## Current Status

SpectraForge Console v0.3 is a working prototype.

The core systems are now proven:

- Image data can generate sound.
- Audio data can generate waveform images.
- Audio data can generate real spectrogram images.
- A GUI and console mode can exist in the same program.
- FFmpeg resources can live in a local `bin\` folder next to the EXE.

The next major step is turning the prototype into a more polished tool with better UI, preview playback, progress bars, more export settings, and more creative effects.

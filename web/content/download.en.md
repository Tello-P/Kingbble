title: Download
nav: Download
blurb: The desktop app for Windows and Linux: all five levels, every core, and leaves training.
lead: The desktop app is the full Kingbble: all five levels including Extreme, every core of the processor, longer thinking times and leaves training. It runs entirely on your computer and needs no connection.
---
:::raw
<div class="downloads">
  <section class="dl">
    <h3>Windows</h3>
    <p>Windows 10 or 11, 64 bits.</p>
    <ul><li>Installer (.exe)</li><li>No administrator rights needed</li></ul>
    <a class="dlbtn" href="#" data-os="windows" aria-disabled="true" tabindex="-1"><span>Download for Windows</span><small>coming soon</small></a>
    <p class="alt"><a href="#" data-os="windows-zip" aria-disabled="true" tabindex="-1">Portable .zip</a> · <a href="#" data-os="windows-sha" aria-disabled="true" tabindex="-1">SHA-256</a></p>
  </section>
  <section class="dl">
    <h3>Linux</h3>
    <p>Any current 64-bit distribution.</p>
    <ul><li>AppImage: one file, no installation</li><li>Also as a .deb package</li></ul>
    <a class="dlbtn" href="#" data-os="linux" aria-disabled="true" tabindex="-1"><span>Download for Linux</span><small>coming soon</small></a>
    <p class="alt"><a href="#" data-os="linux-deb" aria-disabled="true" tabindex="-1">.deb package</a> · <a href="#" data-os="linux-sha" aria-disabled="true" tabindex="-1">SHA-256</a></p>
  </section>
</div>
:::

:::note The builds are not published yet
The buttons are in place and will point to the first public release when it is tagged. The formats and file names on this page are the planned ones and may change with it. Until then there are two ways to play: [the browser demo](play), which needs nothing, and [building from source](build), which gives you everything today.
:::

## What you get that the web demo does not have

| | Web demo | Desktop app |
|---|---|---|
| **Extreme level** (rack inference) | locked | yes |
| Thinking time per move | up to 5 seconds | up to 60 seconds |
| Processor | one thread | every core |
| Leaves | the included table | the included one, or [train your own](training) |
| Word lists | ENABLE | ENABLE, plus any list you have the right to use |
| Connection | needed for the first load | never |

## What is inside

- **The interface**, the same one as the demo, in an Electron window.
- **The engine**, a native program for your system. The interface talks to it in lines of JSON through a small local service that listens only on your own machine. Nothing leaves the computer.
- **ENABLE**, the public-domain word list of 168,551 words.
- **A leave table for ENABLE**, trained from scratch by the engine playing itself.
- **The licence and the third-party notices.**

What is not inside, on purpose: tournament word lists, leave tables published by others, and any model fitted on other people's games. See [about](about).

## Installing

**Windows.** Run the installer. Windows may show a blue "unknown publisher" warning, because the installer is not signed with a paid certificate: choose *More info* and then *Run anyway*. The portable .zip needs no installation: unpack it and run `Kingbble.exe`.

**Linux.** Make the AppImage executable and run it:

```
chmod +x Kingbble-x86_64.AppImage
./Kingbble-x86_64.AppImage
```

Or install the package: `sudo apt install ./kingbble_amd64.deb`.

## Requirements

Expected figures, to be confirmed with the first release.

| | Minimum | Recommended |
|---|---|---|
| Processor | 64 bits, 2 cores | 4 cores or more |
| Memory | 2 GB free | 4 GB free |
| Disk | 400 MB | 1 GB if you train leaves |
| System | Windows 10, or a current 64-bit Linux | |

The Extreme level and the endgame solver use every core they are given. On a laptop on battery, lower the thinking time.

## Checking the download

Each file is published with its SHA-256. To check it:

```
sha256sum Kingbble-x86_64.AppImage          # Linux
certutil -hashfile Kingbble-Setup.exe SHA256   # Windows
```

The result must match the one published beside the download. Each release is also tagged in the repository with the exact source code its binaries were built from, as the licence requires.

## macOS

There is no macOS build planned for the first release. The engine and the interface build from source on macOS with the same steps as on Linux: see [build from source](build).

## Uninstalling

**Windows**: *Settings, Apps*, or delete the folder of the portable version. **Linux**: delete the AppImage, or `sudo apt remove kingbble`. The settings and the leaves you trained are in the app's data folder (`%APPDATA%\Kingbble` on Windows, `~/.config/Kingbble` on Linux); delete it to leave nothing behind.

# Netfetch

A lightweight, network diagnostic CLI for Windows and Linux, inspired by `neofetch`. It grabs your current network interfaces, speed, MTU, data usage, latency, public ASN/IP, and detects your OS to print it alongside a dynamic ASCII art logo.

<img width="652" height="421" alt="1" src="https://github.com/user-attachments/assets/9d72ee98-a009-4282-a95b-1e8a9c2738a3" /> 

<img width="827" height="537" alt="Captura de imagem_20260507_125902-1" src="https://github.com/user-attachments/assets/adaeb775-1579-4299-834b-3f68fe434a7a" />

## Key Features (Linux Improvements)

This version includes several enhancements for Linux environments:
- **Improved Latency Detection:** Falls back to the system `ping` command if raw socket permissions are missing.
- **Smart Speed Detection:** Includes a fallback to check parent USB device speed (useful for RNDIS/USB Tethering devices like Xiaomi/Samsung phones).
- **Accurate DHCP Info:** Better detection of dynamic address assignment on Linux.

## Building from Source

To compile the project from source, you need a standard C++ compiler.

**Windows (MinGW/GCC)**
To build the Windows executable with the embedded custom icon, make sure you link the resource file (`netfetch.res`) alongside the required networking libraries:
```bash
g++ netfetch.cpp netfetch.res -o netfetch.exe -lws2_32 -liphlpapi -lwininet
```

**Linux (GCC)**
To compile the binary for Linux, simply compile the source file. No external libraries are required:
```bash
g++ netfetch.cpp -o netfetch
```

## Installation

### Global Installation

To install `netfetch` globally so you can run it from any terminal:

#### Windows
1. Open PowerShell or Command Prompt **as Administrator**.
2. Run the executable with the install flag:
   ```bash
   .\netfetch.exe --install
   ```

#### Linux
1. Open a terminal.
2. Run the compiled binary with the install flag:
   ```bash
   sudo ./netfetch --install
   ```

### Manual Installation
- **Windows:** Copy `netfetch.exe` into your `C:\Windows` folder.
- **Linux:** Copy `netfetch` into your `/usr/local/bin` folder or `~/.local/bin/`.

## Usage

If you installed it globally, simply run:

```bash
netfetch
```

If you are running it locally without installing, use:

```bash
# Windows
.\netfetch.exe

# Linux
./netfetch
```

For a more compact view (hiding DHCP, MAC, etc.):

```bash
netfetch --compact
```

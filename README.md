# Netfetch

A lightweight, network diagnostic CLI for Windows and Linux, inspired by `neofetch`. It grabs your current network interfaces, speed, MTU, data usage, latency, public ASN/IP, and detects your OS to print it alongside a dynamic ASCII art logo.

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
- **Linux:** Copy `netfetch` into your `/usr/local/bin` folder.

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

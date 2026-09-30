# Raspberry Pi Radiometric Thermal Camera

A Raspberry Pi thermal camera built on the FLIR Lepton 3.1R radiometric sensor, with a Qt GUI for live thermal viewing, temperature measurement, video recording, and data logging.

Built on top of the [GroupGets LeptonModule](https://github.com/groupgets/LeptonModule) `raspberrypi_video` example.

## Features

- Live thermal imaging (160×120, ~9 fps)
- Selectable colormaps: Iron, Rainbow, Grayscale
- Radiometric temperature readings: min, max, and average (°F)
- Temperature legend display
- Video recording to AVI with a buffered frame queue (async writing, 90-frame buffer)
- CSV data logging with timestamps and temperature data
- Manual Flat Field Correction (FFC) button
- Recording status indicator

## How It Works

The FLIR Lepton 3.1R is an uncooled microbolometer: each of its 160×120 pixels changes electrical resistance as it absorbs infrared radiation. Because it is radiometric, each pixel value maps to an actual temperature rather than a relative intensity.

- **SPI (VoSPI)** streams image data. Each frame arrives as 4 segments of 60 packets, which the software reassembles.
- **I2C (CCI)** sends commands to the camera, such as triggering FFC.
- **LeptonThread** runs in the background: it reads frames over SPI, converts pixel values to temperatures (centikelvin → °C → °F), computes min/max/avg, applies the selected colormap, and sends the image to the GUI. During recording, frames are queued and written to AVI by OpenCV while temperature data is logged to CSV.
- **main.cpp** builds the Qt interface (image display, temperature labels, legend, and control buttons) and connects it to the thread.

## Hardware

| Part | Notes |
|---|---|
| Raspberry Pi | Pi 3 or Pi 4 recommended |
| FLIR Lepton 3.1R | Radiometric thermal camera core |
| Lepton Breakout Board v2.0 | GroupGets breakout for the Lepton |
| Female-to-female jumper wires | 7 wires |
| microSD card + power supply | Raspberry Pi OS installed |
| Monitor or Pi touchscreen | For displaying the GUI |

### Wiring

| Lepton Breakout | Raspberry Pi |
|---|---|
| GND | GND |
| VIN | 3V3 |
| CS | CE1 |
| MISO | MISO |
| CLK | SCLK |
| SDA | SDA |
| SCL | SCL |

## Setup

### 1. Enable SPI and I2C

```
sudo raspi-config
```

Go to **Interface Options** and enable both **SPI** and **I2C**, then reboot.

### 2. Install dependencies

```
sudo apt-get update
sudo apt-get install qtbase5-dev qt5-qmake libopencv-dev
```

### 3. Build

From the project folder:

```
qmake && make
```

To clean:

```
make sdkclean && make distclean
```

## Running

### Raspberry Pi 1, 2, 3, and Zero

```
./raspberrypi_video -tl 3
```

### Raspberry Pi 4

Set the CPU governor to performance first:

```
sudo sh -c "echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
```

Then run:

```
./raspberrypi_video -tl 3
```

The `-tl 3` flag tells the program you are using a Lepton 3.x camera core.

## Credits

Based on the [LeptonModule](https://github.com/groupgets/LeptonModule) Raspberry Pi video example by GroupGets.

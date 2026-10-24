# Arduino Nano Iambic CW Practice Keyer (v1)

![Morse Practice Oscillator](images/Morse%20Practise%20Oscillator.jpeg)

A compact, standalone Morse code practice keyer and sidetone oscillator housed in a custom 3D-printed benchtop enclosure. The firmware implements Curtis Iambic Mode B squeeze keying, paddle debounce lockout, real-time speed milestone audio cues, and an integrated USB serial text decoder.

---

## Features

* **Curtis Mode B Squeeze Keying:** Automatic dot/dash insertion with mid-element latching and pause-state insertion logic.
* **Speed Range (5 to 35 WPM):** Live analog speed potentiometer mapped to standard PARIS timing ($t_{\text{dit}} = \frac{1200}{\text{WPM}}\text{ ms}$).
* **Audible Speed Milestone Pips:** Emits non-blocking audio clicks when crossing key speed boundaries (single 1000 Hz click every 5 WPM, double click every 10 WPM).
* **Direct High-Impedance Audio Drive:** Drives a Merry DTR220H high-impedance transducer ($136\ \Omega$) through a passive $1\text{k}\Omega$ rheostat and a series $4.7\ \mu\text{F}$ DC-blocking capacitor directly from the ATmega328P output pin without needing an active transistor stage.
* **Built-in Serial Morse Decoder:** Decodes incoming Morse characters and words live over USB serial (115200 baud) using 3-dit character and 7-dit word spacing logic.
* **Hardware Debounce:** Time-based 8 ms transition lockout on paddle inputs to eliminate phantom elements caused by mechanical paddle contact bounce.
* **Rheostat Volume Control:** Series variable resistance volume adjustment without a ground shunt.

---

## Hardware Specifications & Pinout

### Arduino Nano Connections

| Nano Pin | Signal | Description | Wiring Notes |
| --- | --- | --- | --- |
| **D2** | `PIN_DIT` | Dit paddle contact | Active LOW, internal pull-up enabled |
| **D3** | `PIN_DAH` | Dah paddle contact | Active LOW, internal pull-up enabled |
| **D4** | `PIN_AUDIO` | 700 Hz square-wave sidetone | Feeds volume pot outer terminal |
| **D13** | `PIN_LED` | Onboard status LED | Synchronized with sidetone output |
| **A7** | `PIN_SPEED` | Speed pot wiper | Analog ADC (outer legs to 3V3 and GND) |
| **3V3** | Speed Rail | Voltage reference for speed pot | Connected to speed pot outer terminal |
| **GND** | System Ground | Common return | Speed pot, key jack sleeve, speaker return |

---

### Audio Drive Circuit (Rheostat Mode)

Because the Merry DTR220H transducer features a high coil resistance ($136\ \Omega$), it is driven safely from microcontroller pin D4. The $1\text{k}\Omega$ potentiometer is wired purely as a **series rheostat** with no ground connection, preventing any base loading or supply shorting:

```text
Arduino D4 ────[ Pot Outer Pin ]
                     │
                 (1kΩ Track)
                     │
Arduino D4 ────[  Pot Wiper   ]────||────(+) Merry DTR220H (-)──── Arduino GND
                                 4.7µF       (136Ω Coil)

(Note: Opposite outer potentiometer pin is left unconnected. No GND on pot.)

```

* **Series Attenuation:** Rotating the wiper adds between $0\ \Omega$ and $1\text{k}\Omega$ of series resistance to the $136\ \Omega$ coil, smoothly dropping current and acoustic level without clamping the output line to ground.
* **DC Blocking Capacitor:** The series $4.7\ \mu\text{F}$ capacitor passes the alternating audio frequencies (700 Hz sidetone and 1000 Hz milestone pips) while blocking continuous direct current from heating the transducer coil.

---

### 3.5 mm Stereo Key Jack (TRS)

| Jack Contact | Nano Pin | Typical Connection |
| --- | --- | --- |
| **Tip** | D2 | Dit contact |
| **Ring** | D3 | Dah contact |
| **Sleeve** | GND | Common paddle frame ground |

---

### Prototype Construction ("The Ugly Circuit")

![The Ugly Circuit](images/The%20Ugly%20Circuit.jpeg)

> [!NOTE]
> **A quick apology for the wiring aesthetics:** This is very much a rough-and-ready prototype build in classic "ugly construction" fashion — complete with clumpy point-to-point soldering, flying hookup wires, and generous blobs of hot glue anchoring components inside the 3D-printed enclosure. It definitely won't win any electronics beauty pageants under the hood, but it's rugged, reliable on the bench, and easy to tweak!

---

## Bill of Materials (BOM)

### Electronics

* 1x Arduino Nano (ATmega328P)
* 1x Merry DTR220H dynamic transducer / miniature capsule ($136\ \Omega$ DC resistance)
* 1x $4.7\ \mu\text{F}$ electrolytic or film capacitor (DC blocking)
* 1x $1\text{k}\Omega$ linear or audio potentiometer (Volume control, wired as a rheostat)
* 1x $10\text{k}\Omega$ linear potentiometer (Speed control, wired across 3V3 and GND)
* 1x 3.5 mm TRS stereo panel-mount jack socket
* 2x Control knobs (6 mm knurled shaft)
* Hookup wire / ribbon cable

### Fasteners & Hardware

* 4x M3 countersunk machine screws (lid retention)
* 4x Rubber adhesive bench feet

### 3D Printed Parts & CAD Models

* **Enclosure:** `stl/Enclosure with Lid.stl`
* **Blank Panel:** `stl/Blank Panel.stl`
* **Front Panel (CAD):** `stl/Front Panel.step`
* **Back Panel (CAD):** `stl/Back Panel.step`

*(Note: The dual-paddle assembly pictured with the build is an external third-party design and is not included in this repository's CAD files).*

---

## 3D Printing Guidelines

* **Material:** PLA, PETG, or ABS
* **Layer Height:** 0.20 mm
* **Infill:** 15% to 20% (Grid or Gyroid)
* **Perimeters/Walls:** 3
* **Orientation:**
  * Print the enclosure flat on its base.
  * Print panels flat on a textured PEI plate for a clean exterior finish.



---

## Installation & Flashing

### Using PlatformIO (VS Code)

1. Clone the repository and open the project directory in VS Code.
2. Confirm `platformio.ini`:
```ini
[env:nanoatmega328]
platform = atmelavr
board = nanoatmega328
framework = arduino
monitor_speed = 115200

```


3. Connect the Nano via USB and run **Upload**.
4. Open the Serial Monitor at **115200 baud** to view live speed adjustments and decoded Morse text.

### Using Arduino IDE

1. Open `main.cpp` (or rename it to `nano_keyer.ino`).
2. Select **Tools > Board > Arduino AVR Boards > Arduino Nano**.
3. Select **Tools > Processor > ATmega328P** (or *ATmega328P (Old Bootloader)* depending on your clone).
4. Select the corresponding serial port and click **Upload**.

---

## License

This project is licensed under the [MIT License](LICENSE) — you are free to use, modify, and distribute this design and code for any purpose.

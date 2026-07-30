# Fred — wiring schematic

Two-board layout on the octagon chassis: **left board** carries the DRV8833 and
drives the two TT motors; **right board** carries the ESP32-S3 and the AMG8833.
The switched 4xAA NiMH pack (~4.8-5.6 V) sits top middle and feeds both boards
directly — the pack voltage suits the 3-6 V TT motors as-is, and the DevKit's
onboard LDO makes 3.3 V from it, so there is no separate regulator. A bulk
capacitor across the ESP32's 5V/GND rides through the sag when the motors
kick in.

```mermaid
flowchart TD

  subgraph BATT["4xAA NiMH pack — switched, ~4.8-5.6 V"]
    direction LR
    BP(("+"))
    BN(("−"))
  end

  subgraph LB["LEFT BOARD — drive"]
    direction TB

    subgraph DRV["Pololu DRV8833 carrier"]
      direction TB
      D_VIN["VIN"]
      D_GND["GND"]
      D_SLP["SLP  (nSLEEP)"]
      D_FLT["FLT  (nFAULT) — n/c"]
      D_AIN1["AIN1"]
      D_AIN2["AIN2"]
      D_BIN1["BIN1"]
      D_BIN2["BIN2"]
      D_AO1["AOUT1"]
      D_AO2["AOUT2"]
      D_BO1["BOUT1"]
      D_BO2["BOUT2"]
    end

    subgraph MOTL["TT motor — left wheel"]
      ML_P["M+"]
      ML_N["M−"]
    end

    subgraph MOTR["TT motor — right wheel"]
      MR_P["M+"]
      MR_N["M−"]
    end

    D_AO1 ---|red| ML_P
    D_AO2 ---|black| ML_N
    D_BO1 ---|red| MR_P
    D_BO2 ---|black| MR_N
  end

  subgraph RB["RIGHT BOARD — logic"]
    direction TB

    CAP["470-1000 uF electrolytic\n(+) to 5V, (−) to GND"]

    subgraph ESP["ESP32-S3-DevKitC-1"]
      direction TB
      E_5V["5V"]
      E_GND["GND"]
      E_3V3["3V3"]
      E_G4["GPIO4"]
      E_G5["GPIO5"]
      E_G6["GPIO6"]
      E_G7["GPIO7"]
      E_G8["GPIO8"]
      E_G9["GPIO9"]
      E_G10["GPIO10"]
    end

    subgraph AMG["AMG8833 thermal camera"]
      direction TB
      A_VIN["VIN"]
      A_GND["GND"]
      A_SDA["SDA"]
      A_SCL["SCL"]
    end

    CAP ---|"(+)"| E_5V
    CAP ---|"(−)"| E_GND
    E_3V3 -->|"red 3.3 V"| A_VIN
    E_GND ---|black| A_GND
    E_G8 ---|"blue SDA"| A_SDA
    E_G9 ---|"yellow SCL"| A_SCL
  end

  BP -->|red| D_VIN
  BN ---|black| D_GND
  BP -->|red| E_5V
  BN ---|black| E_GND

  E_G4 -->|PWM| D_AIN1
  E_G5 -->|PWM| D_AIN2
  E_G6 -->|PWM| D_BIN1
  E_G7 -->|PWM| D_BIN2
  E_G10 -->|"high = awake"| D_SLP
  E_GND ---|"logic GND link"| D_GND
```

## Wire list

| From | To | Wire | Notes |
|------|----|------|-------|
| Pack + / − | DRV8833 VIN / GND | red / black | motor power, direct |
| Pack + / − | ESP32 5V / GND pins | red / black | onboard LDO makes 3.3 V |
| Capacitor + / − | ESP32 5V / GND pins | — | fit close to the pins; observe polarity |
| ESP32 GPIO4 | DRV8833 AIN1 | any | left motor PWM |
| ESP32 GPIO5 | DRV8833 AIN2 | any | left motor PWM |
| ESP32 GPIO6 | DRV8833 BIN1 | any | right motor PWM |
| ESP32 GPIO7 | DRV8833 BIN2 | any | right motor PWM |
| ESP32 GPIO10 | DRV8833 SLP | any | drive high to enable; low = sleep |
| ESP32 GND | DRV8833 GND | black | **required** — reference for the 5 signal wires |
| ESP32 3V3 | AMG8833 VIN | red | STEMMA QT colours shown |
| ESP32 GND | AMG8833 GND | black | |
| ESP32 GPIO8 | AMG8833 SDA | blue | |
| ESP32 GPIO9 | AMG8833 SCL | yellow | |
| DRV8833 AOUT1/AOUT2 | left motor | red / black | swap to reverse direction |
| DRV8833 BOUT1/BOUT2 | right motor | red / black | swap to reverse direction |

## GPIO summary

| GPIO | Function |
|------|----------|
| 4 | left motor AIN1 (PWM) |
| 5 | left motor AIN2 (PWM) |
| 6 | right motor BIN1 (PWM) |
| 7 | right motor BIN2 (PWM) |
| 8 | I2C SDA (AMG8833) |
| 9 | I2C SCL (AMG8833) |
| 10 | DRV8833 sleep control |

## Notes

- **Common ground.** The two boards must share ground: the pack negative
  already joins them, but run a dedicated ground wire in the bundle with the
  five control wires so the PWM signals have a clean local return.
- **The capacitor matters.** Motor starts and stalls yank the pack voltage
  down through the cells' internal resistance; the bulk cap across the
  ESP32's 5V/GND is what stops that dip resetting the chip. Mount it right
  at the pins, and mind the polarity stripe.
- **NiMH, not alkaline.** NiMH cells sag far less under motor load. If Fred
  ever resets when both motors stall, suspect tired or alkaline cells first.
- **Motor voltage.** TT motors are rated 3-6 V and the pack never exceeds
  that, so full PWM duty is fine.
- **DRV8833 SLP.** The carrier pulls nSLEEP up so the driver is enabled by
  default; wiring it to GPIO10 lets firmware put the driver to sleep. FLT
  (fault output) is left unconnected.
- **USB + battery.** The DevKitC-1's USB 5 V comes in through a diode, so
  having USB and the pack connected at the same time is fine — handy for
  flashing while installed.

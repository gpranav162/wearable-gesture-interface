# Wearable Gesture Interface (ESP32C3 + ICM-42688-P)

A wearable interface capable of detecting specific gesture inputs (Double Tap) using the hardware-accelerated APEX engine of the ICM-42688-P IMU. The system transmits commands wirelessly using **BLE** or **ESP-NOW** protocols to control external systems (e.g., powered wheelchairs, robotic peripherals).

## Key Features
* **Driverless Implementation:** I2C drivers for the ICM-42688-P were written from scratch, directly manipulating registers to configure the APEX hardware engine.
* **Hardware Interrupts:** Utilizes the sensor's internal DMP (Digital Motion Processor) to trigger interrupts on tap detection, minimizing ESP32 power consumption.
* **Dual Communication Stack:**
    * **ESP-NOW Mode:** Low-latency (<10ms) peer-to-peer communication for real-time robot control.
    * **BLE Mode:** Migrated from **ESP-NOW** to **BLE (GATT Server)** to significantly reduce power consumption during idle states.
* **Tunable Sensitivity:** Includes a calibration script to dynamically adjust `TAP_MIN_JERK_THR` and `TAP_MAX_PEAK_TOL` registers.

## Hardware Stack
| Component | Specification | Function |
| :--- | :--- | :--- |
| **MCU** | ESP32-C3 Mini | Main controller (RISC-V) |
| **IMU** | ICM-42688-P | 6-Axis High-Performance Motion Tracking |
| **Power** | TP4056 + 3.3V LDO | Li-ion Battery Management & Charging |
| **Actuator** | 3V Coin Vibration Motor | Haptic Feedback |

## Circuit & Schematics
The device runs on a custom 2-layer PCB designed to fit a watch-style enclosure.

**Pin Configuration (ESP32-C3):**
* **SDA/SCL:** GPIO 8 / GPIO 9
* **INT1 (Tap Interrupt):** GPIO 5 (Triggers ISR on tap)
* **Vibration Motor:** GPIO 2
* **Boot/Input Button:** GPIO 9 (Used in calibration mode to cycle threshold presets)

## Repository Structure
* `firmware/ble_implementation`: The power-optimized BLE GATT server and client code.
* `firmware/esp_now_implementation`: The alternative low-latency implementation (deprecated due to high power draw).
* `firmware/calibration_tool`: A utility script to cycle through register banks and tune tap sensitivity without reflashing.
* `docs/`: Contains the **Register Map analysis**, and **Heat Maps** used to reverse-engineer the correct threshold values.

## Register Configuration
To enable the APEX Tap Detection without a library, the following specific register banks are manipulated manually:

1.  **Bank 0 (0x4E, 0x50):** Set Accelerometer ODR to 500Hz and Power Mode to Low Noise.
2.  **Bank 4 (0x47):** Configure `TAP_TMAX`, `TAP_TMIN`, and `TAP_TAVG` to define the timing window of a valid tap.
3.  **Bank 4 (0x46):** Configure `TAP_MIN_JERK_THR` (Threshold).

## Tuning & Calibration
The folder `docs/` contains a heat map analysis (`heat_map_analysis.pdf`) showing the relationship between Jerk Thresholds and False Positives.

To tune the device for a new user:
1.  Flash `firmware/calibration_tool/threshold_tuning.ino`.
2.  Use the Boot button to cycle through sensitivity levels (indicated by Serial output).
3.  Observe the Serial Monitor for "Tap Detected" messages to find the sweet spot.
* **Optimal Settings:** Jerk Threshold: `63`, Peak Tolerance: `2`.

## Ultra-Low Power Communication Protocol:
Initially, **ESP-NOW** was used for its <10ms latency. However, field testing revealed that the constant radio duty cycle drained the 300mAh battery too quickly.
We migrated to **BLE**, utilizing the "Notify" property. The ESP32-C3 stays in light sleep and is woken up *only* by the `INT1` signal from the IMU. This shifted the power architecture from "Always Listening" to "Event Driven," doubling the battery life.

---
*Research at Thryv Mobility, Indian Institute of Technology Madras.*

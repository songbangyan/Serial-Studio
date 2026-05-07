# Bluetooth Low Energy Driver

Bluetooth Low Energy (BLE) is the wireless transport of choice for battery-powered sensors, fitness wearables, and prototyping boards (ESP32, nRF52). Serial Studio's BLE driver is in the free build and lets you connect to any BLE peripheral that exposes its data through GATT services and characteristics.

If your device speaks "classic" Bluetooth (the older 2.x flavor with SPP, the Serial Port Profile), the OS exposes it as a virtual COM port and you should use the [UART driver](Drivers-UART.md) instead. This page is for BLE specifically.

## What is Bluetooth Low Energy?

Bluetooth Low Energy was introduced in Bluetooth 4.0 (2010) as a low-power, low-data-rate cousin of classic Bluetooth. It is **not** wire-compatible with classic Bluetooth; the radio is shared but the protocol stack is entirely separate.

The design goals are different too:

| | Classic Bluetooth | Bluetooth LE |
|---|-------------------|--------------|
| Use case | Audio streaming, file transfer | Sensor data, beacons, control |
| Data rate | Up to 3 Mbps (BR/EDR), higher with HS | Typically tens to hundreds of kbps |
| Power | Watts | Microwatts (months on a coin cell) |
| Connection | Always-on while paired | Bursty: connect, exchange, sleep |
| Topology | Star, piconet | Star, mesh, broadcast |

BLE excels at **infrequent small bursts of data** from devices that need to live on a battery for weeks or months. It is a poor fit for high-throughput streaming.

### Roles: peripheral and central

Two roles matter:

- **Peripheral.** the device that **advertises** its presence and **exposes** data. A heart-rate monitor, a temperature sensor, an ESP32 with custom firmware.
- **Central.** the device that **scans** for advertisements and **connects** to a peripheral. Your laptop running Serial Studio is the central.

A peripheral broadcasts short advertising packets every few hundred milliseconds. A central scans those packets, picks one, and initiates a connection. Once connected, the conversation is point-to-point until either side disconnects.

```mermaid
sequenceDiagram
    participant P as Peripheral (sensor)
    participant C as Central (Serial Studio)

    Note over P: Idle, advertising every 250 ms
    P-)C: ADV_IND ("I'm SensorNode, MAC AA:BB:CC:DD:EE:FF")
    P-)C: ADV_IND
    C->>P: SCAN_REQ
    P-->>C: SCAN_RSP (extra info)
    C->>P: CONNECT_IND
    Note over P,C: Connected
    C->>P: GATT discovery (services, characteristics)
    P-->>C: characteristic values, notifications
    Note over P,C: ...
    C->>P: TERMINATE
```

### GATT: services and characteristics

Once connected, BLE devices talk over the **Generic Attribute Profile (GATT)**. GATT is a simple hierarchical data model:

- A **service** is a logical grouping of related data. "Heart Rate Service", "Battery Service", "Custom Vendor Service".
- A **characteristic** is one piece of data inside a service. "Heart Rate Measurement", "Battery Level", "Sensor X Reading".
- A **descriptor** is metadata about a characteristic (units, format, valid range, "Client Characteristic Configuration" for enabling notifications).

```mermaid
flowchart TD
    Device[BLE Peripheral]
    Device --> S1[Service: Battery]
    Device --> S2[Service: Heart Rate]
    Device --> S3[Service: Custom Sensor]

    S1 --> C1[Characteristic: Battery Level<br/>UUID 0x2A19]
    S2 --> C2[Characteristic: Heart Rate Measurement<br/>UUID 0x2A37]
    S3 --> C3[Characteristic: Temperature<br/>UUID custom]
    S3 --> C4[Characteristic: Pressure<br/>UUID custom]

    C2 --> D1[Descriptor: CCCD<br/>enable notifications]
```

Standard services and characteristics use 16-bit UUIDs assigned by the Bluetooth SIG (e.g. `0x180F` for Battery Service, `0x2A19` for Battery Level). Custom services use 128-bit UUIDs picked by the vendor.

### How data flows

There are four GATT operations a central can use against a characteristic:

- **Read.** pull the current value once. Used for things that don't change often (firmware version, calibration constant).
- **Write.** push a value to the peripheral. Used for control (toggle a relay, set a sample rate).
- **Notify.** peripheral pushes a new value to the central whenever the value changes. The central must enable notifications first by writing to the **Client Characteristic Configuration Descriptor (CCCD)**. This is the standard pattern for telemetry.
- **Indicate.** like Notify but the central acks each update. Slightly more reliable, but slower.

For Serial Studio, the relevant operation is almost always **Notify**: select a characteristic, enable notifications, and every value the peripheral sends becomes a frame.

### MTU and throughput

BLE is **slow** compared to USB or TCP. The default MTU (Maximum Transmission Unit) is 23 bytes, of which 20 are usable for payload after ATT overhead. Modern BLE 4.2+ devices negotiate larger MTUs (up to 247 or more), but the practical ceiling on a single connection is around 1–2 Mbps under ideal conditions. **Plan your data rate accordingly.** If you need to stream 1 kHz of sensor data, BLE will struggle; consider Wi-Fi (TCP/UDP) instead.

## How Serial Studio uses it

Serial Studio's BLE driver wraps Qt's `QLowEnergyController` and `QLowEnergyService`. The setup flow is:

1. **Scan.** Serial Studio uses `QBluetoothDeviceDiscoveryAgent` to enumerate nearby BLE peripherals. The list is built up over a few seconds.
2. **Select device.** pick a peripheral from the dropdown. Serial Studio connects and discovers services.
3. **Select service.** pick the GATT service the data lives in. Most devices have several; you want the one containing your telemetry characteristic.
4. **Select characteristic.** pick the specific characteristic to subscribe to. Serial Studio enables notifications on it (writes `0x0001` to the CCCD).

From that point on, every notification's payload bytes become a frame, exactly as if they had arrived over UART.

### Discovery is shared across instances

Serial Studio caches BLE device discovery state across all instances of the driver in static state. That means:

- Discovery runs once even if multiple driver instances exist (UI driver and live driver, for example).
- The device list is **append-only** during rediscovery, so dropdown indices stay stable across QML rebuilds.
- Selecting the same device twice is a no-op when already connected.

This is intentional behavior, not a quirk. It prevents the dropdown from snapping to a different selection every time the QML reloads.

### Threading

The driver lives on the main thread; QtBluetooth's event-driven async I/O delivers notifications via Qt signals. There's no dedicated worker thread for BLE. See [Threading and Timing Guarantees](Threading-and-Timing.md).

For setup steps, see the [Protocol Setup Guides → Bluetooth LE section](Protocol-Setup-Guides.md).

## Common pitfalls

- **Device doesn't appear in the scan.** It may not be advertising, or the advertisement interval might be very long. Try power-cycling the peripheral. On macOS, grant Serial Studio Bluetooth permission in **System Settings → Privacy & Security → Bluetooth**. On Linux, your user may need to be in the `bluetooth` group.
- **"Connected" but no characteristic shows up.** The device's GATT table may not have finished discovery. Wait a few seconds after connect; some peripherals are slow to respond to discovery requests. If it still shows nothing, check whether the device requires pairing or bonding before exposing the service (some Nordic-based devices do).
- **Notifications enable but no data arrives.** You probably enabled notifications on a characteristic that doesn't actually push notifications. Look at the characteristic's **properties** (Read / Write / Notify / Indicate). Only Notify-capable characteristics will deliver data continuously.
- **Throughput is much lower than expected.** That's BLE. The connection interval (negotiated between central and peripheral, typically 7.5 ms to 4 s) caps the rate at which packets can flow. A 7.5 ms interval with 247-byte MTU caps you at roughly 250 kbps payload. Many peripherals negotiate slow intervals to save power; check the device's firmware for a way to request a faster interval.
- **Connection drops after a few seconds.** Peripheral may be enforcing a security requirement (encryption, bonding). Pair the device through your OS first, then reconnect from Serial Studio.
- **Multiple Serial Studio windows fight over the same device.** BLE is point-to-point: one peripheral can only be connected to one central at a time. Close the other window.
- **Bluetooth radio "missing" on Linux.** `bluetoothctl` from a terminal will tell you whether BlueZ sees the adapter. If not, the kernel module may not be loaded.

## References

- [Bluetooth Low Energy Fundamentals — Nordic Developer Academy](https://academy.nordicsemi.com/courses/bluetooth-low-energy-fundamentals/)
- [Bluetooth Low Energy: A Primer — Memfault Interrupt blog](https://interrupt.memfault.com/blog/bluetooth-low-energy-a-primer)
- [Bluetooth Low Energy — Wikipedia](https://en.wikipedia.org/wiki/Bluetooth_Low_Energy)
- [Bluetooth low energy Characteristics, a beginner's tutorial — Nordic DevZone](https://devzone.nordicsemi.com/tutorials/b/bluetooth-low-energy/posts/ble-characteristics-a-beginners-tutorial)
- [Services and characteristics — Nordic Developer Academy](https://academy.nordicsemi.com/courses/bluetooth-low-energy-fundamentals/lessons/lesson-4-bluetooth-le-data-exchange/topic/services-and-characteristics/)

## See also

- [Protocol Setup Guides](Protocol-Setup-Guides.md): step-by-step BLE setup in the Setup Panel.
- [Data Sources](Data-Sources.md): driver capability summary across all transports.
- [Communication Protocols](Communication-Protocols.md): overview of all supported transports.
- [Use Cases](Use-Cases.md): wireless sensor patterns and BLE prototyping setups.
- [Troubleshooting](Troubleshooting.md): pairing, permission, and adapter-detection diagnostics.
- [Drivers — UART](Drivers-UART.md): for classic Bluetooth SPP devices that show up as virtual COM ports.
- [Drivers — Network](Drivers-Network.md): when you need more throughput than BLE can provide.

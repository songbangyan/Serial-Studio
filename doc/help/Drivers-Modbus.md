# Modbus Driver (Pro)

Modbus is a long-standing industrial protocol. Designed in 1979 by Modicon for their PLCs, it is now a de-facto standard across factory automation, building management, energy metering, and process control. PLCs, RTUs, and SCADA equipment overwhelmingly speak Modbus, so Serial Studio uses it to read most factory-floor devices.

Serial Studio Pro implements both **Modbus RTU** (over serial) and **Modbus TCP** (over Ethernet), and ships a [register-map importer](Auto-Generating-Projects.md) that turns vendor CSV/XML/JSON files into a working project automatically.

## What is Modbus?

Modbus is a request/response, master/slave protocol for reading and writing memory locations on a remote device. The model is intentionally simple:

- Every Modbus device exposes a flat memory map, divided into four register tables (described below).
- A **master** sends a request frame containing a function code (what to do) and addresses (where in the memory map).
- The **slave** replies with the requested data or an error.

There is no streaming, no events, and no push notifications. The master polls; the slave answers. Continuous data requires continuous polling.

```mermaid
sequenceDiagram
    participant M as Master (Serial Studio)
    participant S as Slave (PLC, sensor)

    M->>S: Read Holding Registers (start=100, count=10)
    S-->>M: 10 register values

    M->>S: Read Input Registers (start=0, count=4)
    S-->>M: 4 register values

    M->>S: Write Single Register (addr=200, value=42)
    S-->>M: Echo confirmation

    Note over M,S: continuous polling at configured interval
```

### The four register tables

Modbus organises data into four tables, distinguished by read/write capability and bit width:

| Table              | Width    | Access | Typical use                           |
|--------------------|----------|--------|---------------------------------------|
| **Coils**          | 1 bit    | R/W    | Digital outputs (relays, valves)      |
| **Discrete inputs**| 1 bit    | R only | Digital inputs (switches, sensors)    |
| **Input registers**| 16 bits  | R only | Analog inputs (sensor readings)       |
| **Holding registers**| 16 bits | R/W   | General-purpose storage (setpoints, configuration, scratch values) |

Real devices are fuzzier than the table suggests. A modern temperature transmitter might expose its current reading as a holding register (writable in principle, but writing has no effect) simply because that is what the firmware engineer chose. Always check the device's documentation rather than assume.

Addresses inside each table go from 0 to 65535. Vendors document them in two ways, which is a frequent source of confusion:

- **PLC numbering** (1-based, table-prefixed): `40001`-`49999` for holding registers, `30001`-`39999` for input registers, and so on.
- **Protocol numbering** (0-based, no prefix): `0`-`65535` per table.

Modbus on the wire uses protocol numbering. PLC numbering is a vendor convention. Holding register `40100` in PLC numbering is *address 99* in protocol numbering. Off-by-one is the single most common Modbus debugging story.

### Function codes

Every Modbus request carries a one-byte function code identifying what to do. The common ones:

| Code | Function | Tables it touches |
|------|----------|-------------------|
| 1    | Read Coils | Coils |
| 2    | Read Discrete Inputs | Discrete inputs |
| 3    | Read Holding Registers | Holding registers |
| 4    | Read Input Registers | Input registers |
| 5    | Write Single Coil | Coils |
| 6    | Write Single Register | Holding registers |
| 15   | Write Multiple Coils | Coils |
| 16   | Write Multiple Registers | Holding registers |

More function codes exist (read/write combined, file records, diagnostics), but those eight cover 95% of real-world traffic.

### Multi-register data types

A Modbus register is 16 bits. Anything wider spans multiple consecutive registers:

- `uint32`, `int32`, `float32`: 2 registers (4 bytes).
- `uint64`, `int64`, `float64`: 4 registers (8 bytes).

Byte and word order are device-specific:

- **Big-endian** (most common): high-order byte first, high-order word first. `0x12345678` in two registers reads as register A = `0x1234`, register B = `0x5678`.
- **Little-endian word swap**: register A = `0x5678`, register B = `0x1234`. Common on some legacy gear.
- **Mixed**: byte-swap inside each register but not between them. Rare but it happens.

Vendor documentation always specifies the order. Serial Studio's register-map importer assumes big-endian by default (the convention used by most modern devices); for anything else, edit the generated frame parser.

### RTU vs TCP

Modbus rides on top of two transports:

#### Modbus RTU

The original. Runs over RS-232, RS-485, or RS-422. Each frame is wrapped with a **slave address** (1 byte, identifies which device on a multi-drop bus), the function code, the data, and a **CRC-16** checksum. Frames are separated by a 3.5-character idle gap on the line.

```mermaid
flowchart LR
    A[Slave Address<br/>1 byte] --> B[Function Code<br/>1 byte]
    B --> C[Data<br/>0-252 bytes]
    C --> D[CRC-16<br/>2 bytes]
```

RTU usually runs on RS-485, which supports up to 247 slaves on a single pair of wires. Each slave has a unique address from 1 to 247; address 0 is reserved for broadcast.

#### Modbus TCP

The Ethernet variant. Wraps Modbus PDUs in a TCP stream. The frame format is different:

```mermaid
flowchart LR
    subgraph MBAP[MBAP header]
        T[Transaction ID<br/>2 bytes] --> P[Protocol ID<br/>2 bytes, always 0]
        P --> L[Length<br/>2 bytes]
        L --> U[Unit ID<br/>1 byte]
    end
    U --> F[Function Code<br/>1 byte]
    F --> D[Data]
```

There is no CRC because TCP already handles error detection. The **Unit ID** is equivalent to the slave address; it identifies the target when a TCP-to-RTU gateway fronts a multi-drop RS-485 bus. Native Modbus TCP devices typically use Unit ID 1 or 255.

The standard Modbus TCP port is **502**.

## How Serial Studio uses it

Serial Studio acts as the master (Modbus client) and can poll any number of slaves on the same physical link.

### Configuration model

Setup is a hierarchy:

1. **Connection.** Choose RTU (serial port plus baud rate) or TCP (host plus port).
2. **Slave address.** 1 to 247 for RTU, Unit ID for TCP.
3. **Register groups.** One group per contiguous block of same-type registers you want to read. Each group has:
   - Register type (coils, discrete inputs, holding, input).
   - Starting address (0-based, protocol numbering).
   - Count of registers to read in one request.
4. **Poll interval.** How often Serial Studio reissues all the read requests.

On each poll cycle, Serial Studio iterates through every configured register group, sends the read request, parses the response, and emits a frame containing the read values. The frame parser then extracts named datasets from those values.

### Auto-generation

For devices with documented register maps, the [Modbus map importer](Auto-Generating-Projects.md) reads vendor CSV/XML/JSON files and generates the register groups, datasets, and a complete Lua frame parser automatically. This is the recommended starting point.

### Threading

The Modbus driver wraps Qt's `QModbusClient` and runs on the main thread. Polling is event-driven (no busy loop); Qt's async I/O delivers responses via signals. See [Threading and Timing Guarantees](Threading-and-Timing.md).

For step-by-step setup, see the [Protocol Setup Guides — Modbus section](Protocol-Setup-Guides.md).

## Common pitfalls

- **"No response from slave."** Verify the slave address. Different PLC vendors use different defaults (1 is common but not universal). On RS-485, check the wiring: A-to-A, B-to-B, plus a common ground when the bus spans different power domains.
- **Off-by-one address.** PLC numbering versus protocol numbering. If the docs say "holding register 40101", the protocol address is *100*, not 40101 and not 101.
- **CRC errors on RTU.** Almost always a wiring or termination issue. RS-485 needs 120 Ω terminators at both ends of the trunk. Stub branches longer than a few inches corrupt the CRC at high baud rates.
- **Wrong byte order for 32-bit values.** Read raw 16-bit registers, inspect the bytes, and compare them to the vendor documentation. Most modern devices use big-endian word and big-endian byte order. A float that reads as `1.234e-23` is being decoded with the wrong endianness.
- **Polling too fast.** Some devices, especially older PLCs, cannot process requests faster than about one every 100 ms. Serial Studio is happy to poll at 10 ms, but the device may simply not respond. Slow the poll interval down.
- **TCP works but RTU does not.** RS-485 is an unforgiving electrical environment: long cables, missing ground, intermittent termination, or missing bias resistors can all break it. Some adapters lack pull-up/pull-down on the idle bus and need an external biasing network. An oscilloscope on the line is the fastest way to diagnose this.
- **"Illegal data address" exception.** The slave's memory map does not contain that register. Recheck the vendor documentation. Some PLCs only respond to addresses configured in their program; others allow reads of any address.
- **Slave responds slowly under load.** Modern Modbus TCP is fast; Modbus RTU at 9600 baud is slow by design. A 60-register read takes about 12 ms of wire time alone, plus device processing time. Do not expect kilohertz polling on serial.

## References

- [Modbus — Wikipedia](https://en.wikipedia.org/wiki/Modbus)
- [Modbus Protocol Overview — modbustools.com](https://www.modbustools.com/modbus.html)
- [Modbus RTU Made Simple — IPC2U](https://ipc2u.com/articles/knowledge-base/modbus-rtu-made-simple-with-detailed-descriptions-and-examples/)
- [What is Modbus & How Does It Work — NI](https://www.ni.com/en/shop/seamlessly-connect-to-third-party-devices-and-supervisory-system/the-modbus-protocol-in-depth.html)
- [Introduction to Modbus and Modbus Function Codes — Control.com](https://control.com/technical-articles/introduction-to-modbus-and-modbus-function-codes/)
- [modbus.org — official Modbus site](https://modbus.org/)

## See also

- [Auto-Generating Projects](Auto-Generating-Projects.md): Modbus register-map import (CSV/XML/JSON).
- [Protocol Setup Guides](Protocol-Setup-Guides.md): step-by-step Modbus setup.
- [Data Sources](Data-Sources.md): driver capability summary across all transports.
- [Communication Protocols](Communication-Protocols.md): overview of all supported transports.
- [Use Cases](Use-Cases.md): industrial monitoring and PLC integration examples.
- [Troubleshooting](Troubleshooting.md): wiring, addressing, and CRC-error diagnostics.
- [Drivers — UART](Drivers-UART.md): RTU rides on serial; the UART page covers RS-485 physical layer.
- [Drivers — Network](Drivers-Network.md): TCP transport details.
- [Drivers — CAN Bus](Drivers-CAN-Bus.md): the other big industrial protocol.

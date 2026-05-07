# HID Driver (Pro)

The HID driver lets Serial Studio talk to **USB Human Interface Device (HID) class** peripherals: keyboards, mice, gamepads, joysticks, drawing tablets, custom HID firmware, and any vendor-specific device that registers itself as a HID class member.

If you're building a custom controller, a low-cost data acquisition device, or anything that wants to be plug-and-play on every consumer OS without driver installs, HID is the path of least resistance. Every modern OS has a generic HID driver built in.

## What is USB HID?

The HID class was defined by the USB-IF in 1996 to handle "input devices for humans" generically. The original goal was to standardize keyboards, mice, joysticks, and game controllers so the OS didn't need a per-vendor driver for each one. Over time, "human interface" stretched to cover anything with a small periodic data stream: barcode scanners, RFID readers, fingerprint sensors, simple instruments.

A HID device is a USB device that:

1. Declares itself as a HID class device in its **Interface Descriptor**.
2. Provides a **HID Report Descriptor** that describes the structure of the data it will send and receive.
3. Communicates through **HID reports** sent over **interrupt endpoints**.

That's it. The USB packet structure is the standard interrupt-transfer mechanism (see the [USB driver page](Drivers-USB.md)); the HID class layer adds the report descriptor and a small control-transfer protocol.

### Reports

A **report** is a unit of data exchanged between host and device. Three types:

- **Input report** — device → host. Periodic. State of buttons, position of axes, sensor readings. The bread and butter of HID.
- **Output report** — host → device. State the host wants the device to act on. LED on/off, vibration motor strength.
- **Feature report** — bidirectional. Configuration values, calibration, things that aren't part of the streaming exchange.

Reports can be **numbered** (each starts with a 1-byte report ID, used when the device has multiple report formats) or **unnumbered** (only one report format, no ID byte).

### Report descriptors

This is where HID gets clever. Instead of every HID device family having its own protocol, the device describes its data structure to the host through a **report descriptor**. A report descriptor is a sequence of small items declaring:

- What **usage page** and **usage** each field corresponds to (semantic meaning).
- The **size** of each field in bits.
- The **count** of fields of that size.
- The **logical and physical ranges** of values.

A trivial gamepad might declare:

```
Usage Page (Generic Desktop)
Usage (Joystick)
Collection (Application)
  Usage (X)
  Usage (Y)
  Logical Minimum (-127)
  Logical Maximum (127)
  Report Size (8)
  Report Count (2)
  Input (Data, Var, Abs)

  Usage Page (Button)
  Usage Minimum (Button 1)
  Usage Maximum (Button 8)
  Logical Minimum (0)
  Logical Maximum (1)
  Report Size (1)
  Report Count (8)
  Input (Data, Var, Abs)
End Collection
```

Which says: this is a joystick, with two 8-bit signed axes (X and Y, ranging -127 to +127) and 8 single-bit buttons. A report is 3 bytes total: X, Y, and a packed byte of button states.

The OS's generic HID driver reads this descriptor, knows what the device is, and presents standardized events to user-space.

### Usage pages

A **usage page** is a namespace defining the meaning of fields. Standard usage pages include:

- **Generic Desktop** (page `0x01`): X/Y/Z axes, joysticks, mice, keyboards.
- **Keyboard / Keypad** (page `0x07`): keyboard scan codes.
- **Button** (page `0x09`): generic numbered buttons.
- **LED** (page `0x08`): caps lock, num lock, scroll lock indicators.
- **Sensor** (page `0x20`): accelerometers, gyroscopes, environmental sensors.

Vendors can also define **custom usage pages** (top half of the 32-bit page space). A custom usage page lets you declare arbitrary fields without colliding with anything in the standard space.

### hidapi

[**hidapi**](https://github.com/libusb/hidapi) is a small cross-platform C library that provides a uniform API for talking to HID devices on Windows, macOS, and Linux. It abstracts over the OS-specific HID stacks (HIDClass on Windows, IOHIDManager on macOS, hidraw or libusb on Linux). Serial Studio's HID driver is built on hidapi.

The advantage of hidapi over raw libusb: HID devices are generally already claimed by OS drivers, and you can't open them with libusb without unbinding the OS driver first. hidapi cooperates with the OS HID stack, so reading a HID device alongside the OS driver Just Works.

## How Serial Studio uses it

The HID driver wraps hidapi. Setup is:

1. Wait for **device enumeration**. Serial Studio re-enumerates HID devices every 2 seconds for hotplug detection.
2. Pick a device by **VID:PID, Product Name** from the dropdown.
3. Confirm the **Usage Page** and **Usage** fields are the ones you expect (some devices register multiple HID interfaces; pick the right one).
4. Connect.

From that point, every input report's payload bytes become a frame that flows through the standard FrameReader pipeline.

### Threading

The HID driver runs a dedicated **read thread** that issues blocking `hid_read` calls. When data arrives, the thread captures a `SteadyClock::now()` timestamp and posts the data to the main thread via `Qt::AutoConnection`, which becomes `Qt::QueuedConnection` for the cross-thread hop. See [Threading and Timing Guarantees](Threading-and-Timing.md).

### Frame parsing

HID data is just bytes. If your device declares a custom HID report format (e.g. 8-byte reports containing two `int16` axes and a `uint8` button packed state), you write a frame parser to decode them just like any other binary protocol. The HID report descriptor itself is purely advisory at the protocol level — the host doesn't care if you ignore it.

For step-by-step setup, see the [Protocol Setup Guides → HID section](Protocol-Setup-Guides.md).

## Common pitfalls

- **Device not listed.** On Linux, your user needs `hidraw` access. Add a udev rule:
  ```
  /etc/udev/rules.d/99-hidraw.rules:
  KERNEL=="hidraw*", SUBSYSTEM=="hidraw", MODE="0666"
  ```
  More restrictive (preferred): scope it to your VID/PID. On Windows and macOS, HID is generally accessible without extra setup.
- **Multiple entries for the same device.** A composite device may register multiple HID interfaces (e.g. one for keyboard, one for vendor data). Use the Usage Page / Usage fields to identify the right interface.
- **Connected but no data arrives.** Some HID devices only send reports on state change. A gamepad sends a report when a button is pressed or an axis moves; if it's idle, no reports. Poke the device.
- **Reports look corrupted.** Confirm whether the device uses **numbered reports**. If yes, the first byte of each read is the report ID, not data. The frame parser needs to skip it (or split by ID if there are multiple report formats).
- **macOS reads work but writes don't.** Output reports on macOS sometimes require `hid_send_feature_report` instead of `hid_write` depending on how the device declares its output endpoints. This is a hidapi-level quirk; if Serial Studio's writes don't reach the device, the workaround is firmware-side.
- **Re-enumeration interval feels slow.** 2 seconds is the default. If you need faster hotplug detection (e.g. for testing), edit `kEnumIntervalMs` in `app/src/IO/Drivers/HID.cpp` and rebuild.

## References

- [Human Interface Devices (HID) Specifications and Tools — USB-IF](https://www.usb.org/hid)
- [Introduction to HID report descriptors — Linux Kernel Documentation](https://docs.kernel.org/hid/hidintro.html)
- [HID Report Descriptors — Adafruit Learning System](https://learn.adafruit.com/custom-hid-devices-in-circuitpython/report-descriptors)
- [AN249: Human Interface Device Tutorial — Silicon Labs (PDF)](https://www.silabs.com/documents/public/application-notes/AN249.pdf)
- [hidapi — GitHub](https://github.com/libusb/hidapi)
- [USB Component: HID — Keil/MDK](https://www.keil.com/pack/doc/mw/USB/html/group__usbd__hid_functions.html)

## See also

- [Protocol Setup Guides](Protocol-Setup-Guides.md) — step-by-step HID setup.
- [Drivers — USB](Drivers-USB.md) — for vendor-specific (non-HID) USB devices.
- [Drivers — UART](Drivers-UART.md) — for USB-CDC virtual serial ports.

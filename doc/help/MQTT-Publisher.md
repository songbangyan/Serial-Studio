# MQTT Publisher (Pro)

The MQTT Publisher pushes the data Serial Studio is already processing out to an MQTT broker, so other applications — dashboards on different machines, time-series databases, alerting services, downstream analytics — can consume it without speaking directly to the device. It is the outbound side of Serial Studio's MQTT support: where the [MQTT subscriber driver](Drivers-MQTT.md) makes the broker a *source*, the Publisher makes the broker a *sink*.

The Publisher is a per-project singleton, not a per-source driver. One Serial Studio instance has exactly one Publisher; what it broadcasts depends on the project, not on which physical bus the data came in on.

Broker I/O lives entirely on a dedicated worker thread (the same `FrameConsumer` pattern used by CSV / MDF4 / Session-DB export), so a slow or unreliable network never blocks frame parsing, the dashboard, or the UI. The configured **Publish Rate** caps how often the worker drains its queue — at 30 Hz the worker publishes at most thirty times per second regardless of how fast frames arrive.

Use this when:

- A serial-only device should be visible to a remote dashboard or another tool without re-flashing the firmware.
- A parsed-and-validated frame stream is more useful to downstream consumers than the raw bytes.
- Dashboard notifications need to fan out to an external alerting pipeline.
- A user-supplied frame parser script wants to push computed values to arbitrary topics from inside the data hotpath.

If you have never used MQTT before, read [MQTT Topics & Semantics](MQTT-Topics.md) first; this page assumes the protocol vocabulary.

## Where to configure it

The Publisher has its own node in the project editor's left tree, directly under **Devices**. Selecting it opens a form-style editor in the same table layout as a device source.

```
Project
├─ Devices
│   ├─ Device 1 (UART)
│   └─ Device 2 (MQTT Subscriber)
├─ MQTT Publisher          ←── here
├─ Actions
├─ Groups
└─ ...
```

The header bar above the form exposes the live connection state and three actions:

- **Test Connection.** Spins up a one-shot probe client with the current settings, waits up to five seconds, and reports the outcome in a message box. Independent of the long-lived publishing connection; safe to run while the Publisher is connected.
- **Connect / Disconnect.** Opens or closes the publishing session. Enabled only when **Enable Publishing** is on.
- **Load CA Certs.** Visible only when TLS is enabled. Adds PEM certificates from a folder to the per-project trust store.

## Form fields

### Publishing

| Field | Effect |
|-------|--------|
| **Enable Publishing** | Master toggle. When off, nothing leaves the broker side regardless of what the rest of the project does. |
| **Payload** | `Dashboard Data` publishes parsed frames as compact JSON. `Raw RX Data` publishes the bytes that arrived on the driver, unmodified. |
| **Publish Rate (Hz)** | How many times per second the publisher drains its queue and pushes to the broker. Clamped to 1-30 Hz. The hotpath enqueues at full speed and the worker thread publishes at this cadence, so a slow broker or unreliable network never blocks frame parsing or the dashboard. In Dashboard Data mode each tick publishes only the most recent frame; in Raw RX Data mode all bytes queued during the tick are concatenated into one publish. |
| **Topic Base** | Base topic for frame / raw-byte publishing. Required; when empty, the Publisher silently produces no traffic. |
| **Publish Notifications** | When on, dashboard notifications are mirrored to MQTT. |
| **Notification Topic** | Topic used for notifications. Defaults to **Topic Base** when blank. |

The two payload modes are not mutually exclusive in spirit but the radio selects exactly one of them per project: choose `Dashboard Data` when downstream consumers want structured values they can index by dataset name, or `Raw RX Data` when they want to apply their own decoding to the original wire bytes.

### Broker

| Field | Effect |
|-------|--------|
| **Hostname** | Broker address (IP or hostname). |
| **Port** | TCP port. `1883` plaintext, `8883` TLS. |
| **Client ID** | Identifier sent on CONNECT. Pre-populated with a random 16-char value. |
| **Username / Password** | Optional authentication. |
| **Protocol Version** | MQTT 3.1, 3.1.1, or 5.0. |
| **Keep Alive (s)** | Seconds between PING packets when idle. |
| **Clean Session** | Discard persistent session state on CONNECT. Default on. |

### SSL / TLS

`Use SSL/TLS` is the master toggle. When on, three additional fields appear:

| Field | Effect |
|-------|--------|
| **Protocol** | Negotiated TLS protocol family (TLS 1.2, TLS 1.3, or auto-negotiate). |
| **Peer Verify** | `None`, `Query Peer`, `Verify Peer`, or `Auto Verify Peer`. |
| **Verify Depth** | Maximum certificate chain length accepted. `0` = unlimited. |

For brokers using a private CA, click **Load CA Certs** in the header bar after enabling TLS.

## Payload formats

### Dashboard Data

Each parsed frame is serialised to a compact JSON document and published on **Topic Base**. The schema matches the one used by the [API Reference](API-Reference.md) `frame_now` payload: a top-level `groups` array with nested datasets, source IDs, timestamps, and values. One MQTT publish per frame.

Use this when the consumer is another dashboard, a Node-RED flow, or a time-series store that already knows the project's dataset layout. The JSON is small but it is not the wire format the device sent; downstream cannot reconstruct delimiters or driver-specific framing from it.

### Raw RX Data

Each chunk of bytes received from any active driver is republished to **Topic Base** unchanged. One MQTT publish per chunk; the broker preserves payload boundaries so consumers see the same chunking the driver did.

Use this when the consumer wants to apply its own decoding, when you are tee-ing a serial device into existing MQTT infrastructure, or when the downstream tool is another Serial Studio instance subscribing to the topic.

### Notifications

When **Publish Notifications** is on, every event posted to the [Notification Center](Notifications.md) — alarm transitions, action confirmations, error messages — is also serialised to JSON and published to **Notification Topic** (or **Topic Base** if Notification Topic is blank). This is the fan-out point for alerting integrations.

## Test Connection

The **Test Connection** action probes the broker without disturbing the live publishing session. It:

1. Validates that hostname and port are populated and that the licence permits MQTT publishing.
2. Creates a short-lived `QMqttClient` using the current credentials, TLS configuration, and client ID (suffixed with `-probe` to avoid colliding with the live session).
3. Waits up to five seconds for the broker to either reach the **Connected** state or surface an error.
4. Reports the outcome in a message box: an informational confirmation including hostname and port on success, a critical message with the broker error code on failure, or a timeout message if nothing happened.

Run this after editing broker credentials or TLS settings to catch authentication and certificate-chain issues immediately, instead of toggling **Enable Publishing** and digging through the log.

## Publishing from frame parsers

The Publisher exposes a `mqttPublish(topic, payload, qos = 0, retain = false)` slot that can be called from user scripts on the data hotpath. From a frame parser or transform that has access to the Publisher singleton it lets you push computed values to arbitrary topics — independent of **Payload** mode — without bouncing through an external bridge. This is useful for:

- Emitting derived metrics (a rolling average, a CRC-validated subset of fields) on their own topics.
- Mirroring a small fraction of high-rate data to a low-rate topic for cheap remote dashboards.
- Acting on dashboard events: publish to a control topic from an Action handler.

The slot returns the broker message ID on success, or `-1` if the publisher is not connected or the license check fails. QoS is clamped to `0..2`.

## Common pitfalls

- **Test Connection succeeds, Enable Publishing produces nothing.** The probe ignores **Enable Publishing**, the live session does not. Toggle **Enable Publishing** on first, then **Connect**.
- **Connected but no traffic on the broker.** **Topic Base** is empty. The Publisher silently drops frames in that case; there is no error to surface.
- **Frames publish, raw bytes do not (or vice versa).** **Payload** selects exactly one of the two modes. Switch the radio to the other mode if downstream expects the alternative form.
- **Duplicates between Publisher and Subscriber on the same project.** Publishing to a topic the project also subscribes to creates a loop: the broker echoes the publish back to the subscriber driver, which feeds it into the FrameReader, which is then published again. Use different base topics, or disable one side.
- **Client ID collides with another Serial Studio instance.** Two Publishers on different machines must use different client IDs. Use **Regenerate** in the table to pick a fresh one.
- **TLS handshake fails after editing CA certificates.** **Load CA Certs** appends to the in-memory trust store but does not re-open the connection. Disconnect and reconnect (or run **Test Connection**) to apply the new chain.

## See also

- [MQTT Topics & Semantics](MQTT-Topics.md): the protocol vocabulary — topics, wildcards, QoS, retained messages, sessions.
- [MQTT Driver (Subscriber)](Drivers-MQTT.md): the inbound side, when Serial Studio is the consumer.
- [Notifications](Notifications.md): the event source that feeds **Publish Notifications**.
- [API Reference](API-Reference.md): the JSON frame schema used by `Dashboard Data` mode.
- [Frame Parser Scripting](JavaScript-API.md): where `mqttPublish()` is callable from.
- [Pro vs Free Features](Pro-vs-Free.md): MQTT publishing is a Pro feature.
- [Troubleshooting](Troubleshooting.md): general troubleshooting guide.

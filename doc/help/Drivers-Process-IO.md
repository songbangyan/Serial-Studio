# Process I/O Driver (Pro)

The Process I/O driver lets Serial Studio ingest data from any external program. It has two modes:

- **Launch mode** — Serial Studio spawns a child process and reads its standard output. The child can be a shell script, a Python program, `socat`, `nc`, anything that writes bytes to stdout.
- **Named pipe mode** — Serial Studio opens an existing named pipe (FIFO on Linux/macOS, Windows named pipe on Windows) and reads from it. The producer is whatever process opens the same pipe for writing.

This is the universal escape hatch. If Serial Studio doesn't have a driver for your data source, but you can write a script that emits bytes for it, the Process I/O driver bridges the gap.

## What is process I/O?

### Standard streams

Every process on a Unix-like system (and Windows) has three standard streams attached to it at startup:

- **stdin** (file descriptor 0) — input.
- **stdout** (file descriptor 1) — normal output.
- **stderr** (file descriptor 2) — error output.

By default these are connected to the terminal: keyboard for stdin, terminal display for stdout and stderr. They can be **redirected** to files, devices, or other processes. Pipes redirect stdout of one process to stdin of another.

When Serial Studio launches a child process in **Launch mode**, it captures the child's stdout and treats every byte the child writes as if it had arrived from a serial port. The child is just a black box that produces bytes.

### Pipes

A **pipe** is an in-memory unidirectional byte channel between two processes. Two flavors:

- **Anonymous pipe.** Created at process spawn time, accessible only to parent and child via inherited file descriptors. The shell `|` operator creates anonymous pipes.
- **Named pipe (FIFO).** Has a path in the filesystem. Any process with the right permissions can open it for reading or writing. The classic Unix tool to create one is `mkfifo /tmp/mypipe`. Windows has its own named pipe API with paths like `\\.\pipe\mypipe`.

```mermaid
flowchart LR
    subgraph Launch[Launch mode]
        SS1[Serial Studio] -->|spawns| Child[Child process]
        Child -->|stdout| SS1
    end

    subgraph Pipe[Named pipe mode]
        Producer[Some process] -->|writes to| FIFO["/tmp/mypipe"]
        FIFO -->|reads from| SS2[Serial Studio]
    end
```

Use **Launch mode** when Serial Studio is the parent and you want a fresh process started fresh each connection. Use **Named pipe mode** when the producer is already running, when multiple producers might write to the same pipe, or when the same data feed needs to be readable by multiple consumers.

### What can you pipe in?

Practically anything that produces bytes:

- A Python script reading a custom binary protocol over UDP and translating to CSV.
- `socat` bridging a non-standard transport into stdout.
- `nc` reading from a network socket.
- A shell pipeline doing format conversion (`somecmd | sed | awk`).
- An MQTT client subscribing to a topic and writing the payloads to stdout.
- A gRPC client converting protobuf messages to JSON lines.
- A simulation script generating fake telemetry for development.

Process I/O is the right driver when **the data source's transport is exotic but the data itself is text or binary that fits Serial Studio's frame parser**.

## How Serial Studio uses it

The Process I/O driver runs the child or pipe read on a **dedicated thread** (`m_pipeThread`), so blocking reads on slow children don't stall the main thread. Each chunk of bytes is timestamped at read time and forwarded to the FrameReader through Qt's auto-connection (which queues across the thread hop). See [Threading and Timing Guarantees](Threading-and-Timing.md).

### Launch mode configuration

| Setting | Controls |
|---------|----------|
| **Mode** | Launch |
| **Executable** | Absolute path to the program to run |
| **Arguments** | Command-line arguments, space-separated |
| **Working directory** | The directory the child should be spawned in (cwd) |

When you connect, Serial Studio spawns the child process and reads its stdout until the child exits or you disconnect. If the child writes to stderr, that's not captured by the driver (it goes to Serial Studio's own stderr).

### Named pipe mode configuration

| Setting | Controls |
|---------|----------|
| **Mode** | Named pipe |
| **Pipe path** | Filesystem path (Linux/macOS) or pipe name (Windows) |

For Linux/macOS, the pipe must already exist. Create it before connecting:

```sh
mkfifo /tmp/serialstudio_in
```

For Windows, name the pipe with the standard prefix: `\\.\pipe\serialstudio_in`. The pipe is created by whichever side opens it first; Serial Studio creates the pipe if it doesn't exist.

### Example: piping a Python data generator

A simple Python sender that publishes CSV-shaped frames at 100 Hz:

```python
import time, math, sys
t = 0
while True:
    t += 0.01
    v1 = math.sin(t)
    v2 = math.cos(t)
    print(f"{v1:.3f},{v2:.3f}")
    sys.stdout.flush()
    time.sleep(0.01)
```

In Launch mode, set Executable to `/usr/bin/python3` (or wherever your Python lives) and Arguments to the script path. Connect, switch to Quick Plot mode, and the two sine/cosine signals will plot.

For step-by-step setup, see the [Protocol Setup Guides → Process I/O section](Protocol-Setup-Guides.md).

## Common pitfalls

- **No data appears.** Most often, the child is buffering its own output. Standard library functions buffer stdout in 4 KB chunks when stdout isn't a terminal. Force a flush after each line: in Python use `print(..., flush=True)` or `sys.stdout.flush()`; in C use `fflush(stdout)`; in Bash use `stdbuf -oL` to force line-buffered output (`stdbuf -oL my_program`).
- **Child process exits immediately.** Serial Studio shows the child as terminated and reads no data. Test the child from a normal terminal first to verify it actually runs.
- **Path issues.** Spaces and Unicode in executable paths or arguments can be misparsed. On Windows, quote paths containing spaces. The arguments field is split on whitespace (no shell-like quoting), so a path with a space won't work as a single argument unless you wrap it correctly.
- **Working directory matters.** Some programs read configuration files relative to their working directory. Set the Working directory field accordingly.
- **Permission denied on the pipe.** On Linux/macOS, the FIFO inherits filesystem permissions. `chmod 666 /tmp/mypipe` opens it to all users; tighter permissions require both reader and writer to be the same user or in the same group.
- **Pipe buffer fills up.** Linux pipes have a small buffer (typically 64 KB). If the writer outruns Serial Studio (or vice versa), the writer blocks until the reader catches up. This is normal flow control. If your producer is critical-path real-time, consider shoveling bytes through a TCP socket (see [Drivers — Network](Drivers-Network.md)) instead.
- **Windows-specific pipe path syntax.** On Windows, the pipe must be named `\\.\pipe\<name>`. Using a Unix-style path will fail silently or with an opaque error.
- **Process I/O makes "scripted" data sources easy, but...** at very high data rates (hundreds of kHz), the cost of going through stdout buffering, the OS pipe, and the cross-thread queue becomes noticeable. Direct drivers are always cheaper. Process I/O is the right tool at moderate rates and for prototype/integration work.

## References

- [subprocess — Python Standard Library](https://docs.python.org/3/library/subprocess.html)
- [Inter-process Communication: Pipes — OCaml UNIX](https://ocaml.github.io/ocamlunix/pipes.html)
- [Inter Process Communication — Named Pipes (TutorialsPoint)](https://www.tutorialspoint.com/inter_process_communication/inter_process_communication_named_pipes.htm)
- [Inter-process Communication CS 217 — Princeton (PDF)](https://www.cs.princeton.edu/courses/archive/spr04/cos217/lectures/Communication.pdf)
- [Python and Pipes — Lyceum Allotments](https://lyceum-allotments.github.io/2017/03/python-and-pipes-part-5-subprocesses-and-pipes/)

## See also

- [Protocol Setup Guides](Protocol-Setup-Guides.md) — step-by-step Process I/O setup, with both Launch and Named pipe examples.
- [Drivers — Network](Drivers-Network.md) — for higher-rate streaming when a pipe isn't enough.
- [Frame Parser Scripting](JavaScript-API.md) — for parsing whatever your producer emits.

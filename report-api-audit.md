# Serial Studio API Audit

Generated 2026-05-05. Targets the resource.verb redesign.

---

## 1. Surface Inventory by Domain

| Domain | Count | Commands | Description |
|--------|-------|----------|-------------|
| **io** | 98 | io.driver.{audio,ble,canbus,hid,modbus,network,process,uart,usb}.* + io.manager.* | Hardware device connection & configuration (9 bus types) |
| **project** | 61 | project.{group,dataset,action,outputWidget,source,parser,frameParser,tables,workspaces,file}.* | Project data model: groups, datasets, actions, sources, parsers, tables, workspaces |
| **mqtt** | 29 | mqtt.{connect,disconnect,getConfiguration,get*,set*} | MQTT broker connectivity & SSL/auth config |
| **console** | 14 | console.{clear,send,export,get,set*} | ANSI terminal emulator display & I/O |
| **ui** | 13 | ui.window.{get*,set*,load*,save*} | Window layout, groups, widget settings, state persistence |
| **csv** | 12 | csv.{export,player}.* | CSV data export & playback (frame-by-frame stepping) |
| **notifications** | 11 | notifications.{channels,list,clearAll,post*,markRead,resolve,unreadCount} | In-app notification center |
| **extensions** | 10 | extensions.{add,list,install,uninstall,get*,*Repository} | Plugin/extension lifecycle & repo mgmt |
| **licensing** | 8 | licensing.{activate,deactivate,validate,getStatus,setLicense,trial.*,guardStatus} | License activation & trial mode |
| **dashboard** | 8 | dashboard.{getData,getFPS,getStatus,getPoints,getOperationMode,setFPS,setPoints,setOperationMode} | Real-time data aggregation & FPS/point limits |
| **sessions** | 4 | sessions.{close,getCanonicalDbPath,getStatus,setExportEnabled} | Active connection session mgmt & export state |

**Total: 268 commands across 11 domains.**

---

## 2. P0 — User-visible "missing endpoint" surprises

The data model supports many settable fields that have no API endpoint. Below are the most critical gaps an AI/user would expect to exist:

### Group fields without API:
- **Field:** `painterCode` (Group struct, Frame.h:443)  
  **Symptom:** User cannot set JS painter code via API; must use UI or JSON import. `project.group.add` exists but no `project.group.setPainterCode`.  
  **Proposed endpoint:** `group.setPainterCode`

- **Field:** `widget` (Group struct, Frame.h:437)  
  **Symptom:** User cannot change group's display widget type (e.g., "gauge", "graph", "painter") after creation. No `project.group.setWidget`.  
  **Proposed endpoint:** `group.setWidget`

- **Field:** `columns` (Group struct, Frame.h:434)  
  **Symptom:** Output panel grid layout uncontrollable via API. No setter for columns per group.  
  **Proposed endpoint:** `group.setColumns`

- **Field:** `title` (Group struct, Frame.h:436)  
  **Symptom:** Group rename missing. Only `project.setTitle` exists for project-level.  
  **Proposed endpoint:** `group.setTitle`

- **Field:** `sourceId` (Group struct, Frame.h:433)  
  **Symptom:** Cannot reassign group to a different data source post-creation.  
  **Proposed endpoint:** `group.setSourceId`

### Dataset fields without API:
- **Field:** `widget` (Dataset struct, Frame.h:410)  
  **Symptom:** `setOption` is available but widget-type change (e.g., bar→gauge) must be done via UI.  
  **Proposed endpoint:** `dataset.setWidget`

- **Field:** `title`, `units` (Dataset struct, Frame.h:408–409)  
  **Symptom:** No direct setter. Accessible only through project JSON reimport.  
  **Proposed endpoint:** `dataset.setTitle`, `dataset.setUnits`

- **Field:** `xAxisId`, `waterfallYAxis` (Dataset struct, Frame.h:376–377)  
  **Symptom:** Axis assignment not exposed; cannot change plot axes post-creation.  
  **Proposed endpoint:** `dataset.setXAxisId`, `dataset.setWaterfallYAxis`

- **Field:** All numeric range fields (`fftMin/Max`, `pltMin/Max`, `wgtMin/Max`, `alarmLow/High`, `ledHigh`)  
  **Symptom:** No bulk setter; only UI sliders available. Each requires individual JSON edit.  
  **Proposed endpoint:** `dataset.setRange` (fftMin, fftMax, pltMin, pltMax, wgtMin, wgtMax, alarmLow, alarmHigh, ledHigh as params)

### Source fields without API:
- **Field:** `title` (Source struct, Frame.h:459)  
  **Symptom:** Source renaming unsupported via API. Only `project.source.update` / `setProperty` exist (non-standard).  
  **Proposed endpoint:** `source.setTitle`

- **Field:** `frameStart`, `frameEnd`, `checksumAlgorithm` (Source struct, Frame.h:460–462)  
  **Symptom:** Frame detection settings buried in `source.setProperty` (untyped QVariantMap). No direct getters.  
  **Proposed endpoint:** `source.setFrameStart`, `source.setFrameEnd`, `source.setChecksumAlgorithm` (and corresponding getters)

- **Field:** `hexadecimalDelimiters` (Source struct, Frame.h:465)  
  **Symptom:** No API control for delimiter encoding format.  
  **Proposed endpoint:** `source.setHexadecimalDelimiters`

### Action fields without API:
- **Field:** `title`, `icon`, `txData`, `eolSequence` (Action struct, Frame.h:257–260)  
  **Symptom:** `project.action.add` only; no update/rename. Must recreate action to change title.  
  **Proposed endpoint:** `action.setTitle`, `action.setIcon`, `action.setTxData`, `action.setEolSequence`

- **Field:** Timer fields (`timerMode`, `timerInterval`, `repeatCount`)  
  **Symptom:** No setter API.  
  **Proposed endpoint:** `action.setTimerMode`, `action.setTimerInterval`, `action.setRepeatCount`

### OutputWidget fields without API:
- **Field:** `title`, `icon`, `outputMode`, `transmitFunction` (OutputWidget struct, Frame.h:301+)  
  **Symptom:** `project.outputWidget.add` only; no update. Transmit function must be edited via UI or JSON.  
  **Proposed endpoint:** `outputWidget.setTitle`, `outputWidget.setIcon`, `outputWidget.setOutputMode`, `outputWidget.setTransmitFunction`

### Workspace fields without API:
- **Field:** `title`, `icon` (Workspace struct, Frame.h:649–650)  
  **Symptom:** `project.workspaces.rename` exists but no `setIcon`. Icon must be JSON-edited.  
  **Proposed endpoint:** `workspace.setIcon` (already have rename)

---

## 3. P1 — Symmetry gaps

### Singular/Plural inconsistency:
- `project.group.add` + `project.groups.list` (mixing singular/plural)  
- `project.dataset.add` + `project.datasets.list` (mixing singular/plural)  
- `project.action.add` + `project.actions.list` (mixing singular/plural)  
- `project.source.add` + `project.source.list` (inconsistent — singular command but plural listing)  
- `project.tables.add` + `project.tables.list` (consistent plural, but compare to group/dataset pattern)  
- `project.workspaces.add` + `project.workspaces.list` (consistent plural)  

**Fix:** Standardize to either all singular (group.add / group.list) or all plural (groups.add / groups.list).

### Duplicate/conflicting APIs:
- `project.parser.setCode` AND `project.frameParser.setCode` (both exist, both set the same frame parser code)  
- `project.parser.getCode` exists but NO `project.frameParser.getCode` (asymmetric)  
- `project.frameParser.configure` AND `project.frameParser.getConfig` (read and configure—odd verb pair)  

**Fix:** Choose one: either `frameParser.*` for all, or `parser.*` for all. Remove duplicates.

### Verb and resource symmetry failures:
- Some resources have CRUD: `project.group.{add, delete, duplicate}` — missing `get`/`select` is covered by `project.group.select`  
- `project.outputWidget.{add, delete, duplicate}` — missing select and individual property getters  
- `project.dataset.{add, delete, duplicate, setOption, setTransformCode, setVirtual}` — mixed verb style (set* for options; no getters for these fields)  
- `project.source.{add, delete, update, configure, list, getConfiguration, setProperty, setFrameParserCode, getFrameParserCode}` — verb chaos: `update`, `configure`, `setProperty` all mean "change config" but named inconsistently  

**Fix:** Enforce `{add, get, list, delete, duplicate}` for collection-like resources; use `set<Field>` / `get<Field>` for property mutation.

### Three-tier nesting (should collapse):
- `project.tables.register.{add, delete, update}` → Should be `dataTable.register.{add, delete, update}` (collapse to two tiers)  
- `project.workspaces.widgets.{add, remove}` → Should be `workspace.widget.{add, remove}` or `workspaceWidget.{add, remove}` (collapse to two tiers)  
- `project.workspaces.customize.{get, set}` → Should be `workspace.customize{get, set}` (collapse OR flatten to `workspace.getCustomizeMode` / `workspace.setCustomizeMode`)  
- `csv.export.{close, getStatus, setEnabled}` → Could collapse to `csvExport.{close, getStatus, setEnabled}` (three-tier but noun is not a scope; should be two-tier device name)  
- `csv.player.{close, getStatus, nextFrame, open, pause, play, previousFrame, setProgress, toggle}` → Should be `csvPlayer.*` (collapse three-tier device name)  
- `console.export.{close, getStatus, setEnabled}` → Should be `consoleExport.*` (three-tier; noun-driven)  
- `io.driver.<bustype>.*` → Keep three-tier for bus types (io.driver.uart, io.driver.canbus, etc.), but consider flattening to `uart.*`, `canbus.*` etc. if clear enough  

**Fix:** Apply consistent two-tier max rule.

### Verb inconsistencies (outside closed vocab):
- `mqtt.regenerateClientId` → Use `mqtt.setClientId` with "regenerate" as parameter OR `mqtt.generateClientId` (→ `mqtt.generate`)  
- `mqtt.toggleConnection` → Should be `mqtt.setConnected` (boolean) or split to `mqtt.connect` / `mqtt.disconnect` (already have both separately)  
- `csv.player.{nextFrame, previousFrame, pause, play, toggle}` → Replace with `csvPlayer.{setFrame(id), setPlaying(bool), setProgress(percent)}` OR use verb `step` (`csvPlayer.stepFrame(delta)`)  
- `csv.player.toggle` → Redundant with pause/play; remove  
- `project.loadIntoFrameBuilder` → Should be `frameBuilder.loadProject` (frame builder is the scope, not project)  
- `project.exportJson` → Should be `project.export` (format as parameter) OR `project.save` (already have `project.file.save`)  
- `project.loadFromJSON` → Should be `project.import` OR `project.load`  
- `notifications.channels` → NOT a verb; should be `notifications.listChannels` or `notifications.getChannels`  
- `licensing.guardStatus` → Rename to `licensing.getGuardStatus` (guard is metadata, not a domain)  

**Fix:** Align all verbs to closed vocab.

### Asymmetric getter/setter pairs:
- `dashboard.{getData, getFPS, getPoints, getStatus, getOperationMode}` — getters only  
- `dashboard.{setFPS, setPoints, setOperationMode}` — setters (no `setData`, no `setStatus`)  
- `console.export.{getStatus, setEnabled}` — missing `getEnabled`  
- `csv.export.{getStatus, setEnabled}` — missing `getEnabled`  
- `sessions.getCanonicalDbPath` — no setter (read-only; OK, but note)  

**Fix:** Ensure paired getters/setters where mutation is logical.

---

## 4. Full rename mapping table

| Old name | New name | Notes |
|----------|----------|-------|
| console.clear | console.clear | ✓ verb in closed vocab; keep |
| console.export.close | consoleExport.close | Collapse three-tier: export is a noun, not scope |
| console.export.getStatus | consoleExport.getStatus | Collapse three-tier |
| console.export.setEnabled | consoleExport.setEnabled | Collapse three-tier |
| console.getConfiguration | console.getConfiguration | ✓ keep |
| console.send | console.send | ✓ verb in closed vocab; keep |
| console.setChecksumMethod | console.setChecksumMethod | ✓ keep (console setting) |
| console.setDataMode | console.setDataMode | ✓ keep (console setting) |
| console.setDisplayMode | console.setDisplayMode | ✓ keep (console setting) |
| console.setEcho | console.setEcho | ✓ keep (console setting) |
| console.setFontFamily | console.setFontFamily | ✓ keep (console setting) |
| console.setFontSize | console.setFontSize | ✓ keep (console setting) |
| console.setLineEnding | console.setLineEnding | ✓ keep (console setting) |
| console.setShowTimestamp | console.setShowTimestamp | ✓ keep (console setting) |
| csv.export.close | csvExport.close | Collapse three-tier |
| csv.export.getStatus | csvExport.getStatus | Collapse three-tier |
| csv.export.setEnabled | csvExport.setEnabled | Collapse three-tier |
| csv.player.close | csvPlayer.close | Collapse three-tier |
| csv.player.getStatus | csvPlayer.getStatus | Collapse three-tier |
| csv.player.nextFrame | csvPlayer.step | Closed vocab: `step` for incremental motion (formerly nextFrame/previousFrame) |
| csv.player.open | csvPlayer.open | Collapse three-tier |
| csv.player.pause | csvPlayer.setPaused | Verb: `set` boolean state; formerly pause/play/toggle |
| csv.player.play | csvPlayer.setPaused | Verb: `set` boolean state (merge pause/play/toggle) |
| csv.player.previousFrame | csvPlayer.step | Closed vocab: `step` with direction/delta param |
| csv.player.setProgress | csvPlayer.setProgress | Collapse three-tier; rename param to frame index or percentage |
| csv.player.toggle | csvPlayer.setPaused | Merge into setPaused (redundant) |
| dashboard.getData | dashboard.getData | ✓ keep |
| dashboard.getFPS | dashboard.getFPS | ✓ keep |
| dashboard.getOperationMode | dashboard.getOperationMode | ✓ keep |
| dashboard.getPoints | dashboard.getPoints | ✓ keep |
| dashboard.getStatus | dashboard.getStatus | ✓ keep |
| dashboard.setFPS | dashboard.setFPS | ✓ keep |
| dashboard.setOperationMode | dashboard.setOperationMode | ✓ keep |
| dashboard.setPoints | dashboard.setPoints | ✓ keep |
| extensions.addRepository | extension.addRepository | Singular (consistent with other collection verbs) |
| extensions.getInfo | extension.getInfo | Singular |
| extensions.install | extension.install | Singular |
| extensions.list | extension.list | Singular |
| extensions.listRepositories | extension.listRepositories | Singular; or split to extensionRepository.list |
| extensions.loadState | extension.loadState | Singular |
| extensions.refresh | extension.refresh | Singular; verb in closed vocab |
| extensions.removeRepository | extension.removeRepository | Singular |
| extensions.saveState | extension.saveState | Singular |
| extensions.uninstall | extension.uninstall | Singular |
| io.driver.audio.getConfiguration | audio.getConfiguration | Drop `io.driver` (device is the scope) |
| io.driver.audio.getInputDevices | audio.getInputDevices | Drop `io.driver` |
| io.driver.audio.getInputFormats | audio.getInputFormats | Drop `io.driver` |
| io.driver.audio.getOutputDevices | audio.getOutputDevices | Drop `io.driver` |
| io.driver.audio.getOutputFormats | audio.getOutputFormats | Drop `io.driver` |
| io.driver.audio.getSampleRates | audio.getSampleRates | Drop `io.driver` |
| io.driver.audio.setInputChannelConfig | audio.setInputChannelConfig | Drop `io.driver` |
| io.driver.audio.setInputDevice | audio.setInputDevice | Drop `io.driver` |
| io.driver.audio.setInputSampleFormat | audio.setInputSampleFormat | Drop `io.driver` |
| io.driver.audio.setOutputChannelConfig | audio.setOutputChannelConfig | Drop `io.driver` |
| io.driver.audio.setOutputDevice | audio.setOutputDevice | Drop `io.driver` |
| io.driver.audio.setOutputSampleFormat | audio.setOutputSampleFormat | Drop `io.driver` |
| io.driver.audio.setSampleRate | audio.setSampleRate | Drop `io.driver` |
| io.driver.ble.getCharacteristicList | ble.getCharacteristicList | Drop `io.driver` |
| io.driver.ble.getConfiguration | ble.getConfiguration | Drop `io.driver` |
| io.driver.ble.getDeviceList | ble.getDeviceList | Drop `io.driver` |
| io.driver.ble.getServiceList | ble.getServiceList | Drop `io.driver` |
| io.driver.ble.getStatus | ble.getStatus | Drop `io.driver` |
| io.driver.ble.selectDevice | ble.setDevice | Rename `select*` to `set*` |
| io.driver.ble.selectService | ble.setService | Rename `select*` to `set*` |
| io.driver.ble.setCharacteristicIndex | ble.setCharacteristicIndex | Drop `io.driver` |
| io.driver.ble.startDiscovery | ble.startDiscovery | Drop `io.driver`; verb in closed vocab |
| io.driver.canbus.getBitrateList | canbus.getBitrateList | Drop `io.driver` |
| io.driver.canbus.getConfiguration | canbus.getConfiguration | Drop `io.driver` |
| io.driver.canbus.getInterfaceError | canbus.getInterfaceError | Drop `io.driver` |
| io.driver.canbus.getInterfaceList | canbus.getInterfaceList | Drop `io.driver` |
| io.driver.canbus.getPluginList | canbus.getPluginList | Drop `io.driver` |
| io.driver.canbus.setBitrate | canbus.setBitrate | Drop `io.driver` |
| io.driver.canbus.setCanFD | canbus.setCanFD | Drop `io.driver` |
| io.driver.canbus.setInterfaceIndex | canbus.setInterfaceIndex | Drop `io.driver` |
| io.driver.canbus.setPluginIndex | canbus.setPluginIndex | Drop `io.driver` |
| io.driver.hid.getConfiguration | hid.getConfiguration | Drop `io.driver` |
| io.driver.hid.getDeviceList | hid.getDeviceList | Drop `io.driver` |
| io.driver.hid.setDeviceIndex | hid.setDeviceIndex | Drop `io.driver` |
| io.driver.modbus.addRegisterGroup | modbus.registerGroup.add | Rename `addX` to `x.add` |
| io.driver.modbus.clearRegisterGroups | modbus.registerGroup.clear | Rename `clearX` to `x.clear` (or `x.deleteAll`) |
| io.driver.modbus.getBaudRateList | modbus.getBaudRateList | Drop `io.driver` |
| io.driver.modbus.getConfiguration | modbus.getConfiguration | Drop `io.driver` |
| io.driver.modbus.getDataBitsList | modbus.getDataBitsList | Drop `io.driver` |
| io.driver.modbus.getParityList | modbus.getParityList | Drop `io.driver` |
| io.driver.modbus.getProtocolList | modbus.getProtocolList | Drop `io.driver` |
| io.driver.modbus.getRegisterGroups | modbus.registerGroup.list | Rename to sub-resource pattern |
| io.driver.modbus.getSerialPortList | modbus.getSerialPortList | Drop `io.driver` |
| io.driver.modbus.getStopBitsList | modbus.getStopBitsList | Drop `io.driver` |
| io.driver.modbus.removeRegisterGroup | modbus.registerGroup.delete | Rename to sub-resource pattern |
| io.driver.modbus.setBaudRate | modbus.setBaudRate | Drop `io.driver` |
| io.driver.modbus.setDataBitsIndex | modbus.setDataBitsIndex | Drop `io.driver` |
| io.driver.modbus.setHost | modbus.setHost | Drop `io.driver` |
| io.driver.modbus.setParityIndex | modbus.setParityIndex | Drop `io.driver` |
| io.driver.modbus.setPollInterval | modbus.setPollInterval | Drop `io.driver` |
| io.driver.modbus.setPort | modbus.setPort | Drop `io.driver` |
| io.driver.modbus.setProtocolIndex | modbus.setProtocolIndex | Drop `io.driver` |
| io.driver.modbus.setSerialPortIndex | modbus.setSerialPortIndex | Drop `io.driver` |
| io.driver.modbus.setSlaveAddress | modbus.setSlaveAddress | Drop `io.driver` |
| io.driver.modbus.setStopBitsIndex | modbus.setStopBitsIndex | Drop `io.driver` |
| io.driver.network.getConfiguration | network.getConfiguration | Drop `io.driver` |
| io.driver.network.getSocketTypes | network.getSocketTypes | Drop `io.driver` |
| io.driver.network.lookup | network.lookup | Drop `io.driver`; verb in closed vocab |
| io.driver.network.setRemoteAddress | network.setRemoteAddress | Drop `io.driver` |
| io.driver.network.setSocketType | network.setSocketType | Drop `io.driver` |
| io.driver.network.setTcpPort | network.setTcpPort | Drop `io.driver` |
| io.driver.network.setUdpLocalPort | network.setUdpLocalPort | Drop `io.driver` |
| io.driver.network.setUdpMulticast | network.setUdpMulticast | Drop `io.driver` |
| io.driver.network.setUdpRemotePort | network.setUdpRemotePort | Drop `io.driver` |
| io.driver.process.getConfiguration | process.getConfiguration | Drop `io.driver` |
| io.driver.process.getRunningProcesses | process.getRunningProcesses | Drop `io.driver` |
| io.driver.process.setArguments | process.setArguments | Drop `io.driver` |
| io.driver.process.setExecutable | process.setExecutable | Drop `io.driver` |
| io.driver.process.setMode | process.setMode | Drop `io.driver` |
| io.driver.process.setPipePath | process.setPipePath | Drop `io.driver` |
| io.driver.process.setWorkingDir | process.setWorkingDir | Drop `io.driver` |
| io.driver.uart.getBaudRateList | uart.getBaudRateList | Drop `io.driver` |
| io.driver.uart.getConfiguration | uart.getConfiguration | Drop `io.driver` |
| io.driver.uart.getPortList | uart.getPortList | Drop `io.driver` |
| io.driver.uart.setAutoReconnect | uart.setAutoReconnect | Drop `io.driver` |
| io.driver.uart.setBaudRate | uart.setBaudRate | Drop `io.driver` |
| io.driver.uart.setDataBits | uart.setDataBits | Drop `io.driver` |
| io.driver.uart.setDevice | uart.setDevice | Drop `io.driver` |
| io.driver.uart.setDtrEnabled | uart.setDtrEnabled | Drop `io.driver` |
| io.driver.uart.setFlowControl | uart.setFlowControl | Drop `io.driver` |
| io.driver.uart.setParity | uart.setParity | Drop `io.driver` |
| io.driver.uart.setPortIndex | uart.setPortIndex | Drop `io.driver` |
| io.driver.uart.setStopBits | uart.setStopBits | Drop `io.driver` |
| io.driver.usb.getConfiguration | usb.getConfiguration | Drop `io.driver` |
| io.driver.usb.getDeviceList | usb.getDeviceList | Drop `io.driver` |
| io.driver.usb.setDeviceIndex | usb.setDeviceIndex | Drop `io.driver` |
| io.driver.usb.setInEndpointIndex | usb.setInEndpointIndex | Drop `io.driver` |
| io.driver.usb.setIsoPacketSize | usb.setIsoPacketSize | Drop `io.driver` |
| io.driver.usb.setOutEndpointIndex | usb.setOutEndpointIndex | Drop `io.driver` |
| io.driver.usb.setTransferMode | usb.setTransferMode | Drop `io.driver` |
| io.manager.connect | io.connect | Keep `io` as domain (not device-specific); verb in closed vocab |
| io.manager.disconnect | io.disconnect | Keep `io` as domain; verb in closed vocab |
| io.manager.getAvailableBuses | io.getAvailableBuses | Keep `io` as domain |
| io.manager.getStatus | io.getStatus | Keep `io` as domain |
| io.manager.setBusType | io.setBusType | Keep `io` as domain |
| io.manager.setPaused | io.setPaused | Keep `io` as domain |
| io.manager.writeData | io.writeData | Keep `io` as domain |
| licensing.activate | licensing.activate | ✓ verb in closed vocab; keep |
| licensing.deactivate | licensing.deactivate | ✓ verb in closed vocab; keep |
| licensing.getStatus | licensing.getStatus | ✓ keep |
| licensing.guardStatus | licensing.getGuardStatus | Rename; `guardStatus` is not a verb |
| licensing.setLicense | licensing.setLicense | ✓ keep |
| licensing.trial.enable | licensingTrial.enable | Collapse three-tier; or keep as `licensing.trial.enable` (trial is truly a sub-scope) |
| licensing.trial.getStatus | licensingTrial.getStatus | Collapse three-tier; or keep if trial is sub-scope |
| licensing.validate | licensing.validate | ✓ verb in closed vocab; keep |
| mqtt.connect | mqtt.connect | ✓ verb in closed vocab; keep |
| mqtt.disconnect | mqtt.disconnect | ✓ verb in closed vocab; keep |
| mqtt.getConfiguration | mqtt.getConfiguration | ✓ keep |
| mqtt.getConnectionStatus | mqtt.getConnectionStatus | ✓ keep |
| mqtt.getModes | mqtt.getModes | ✓ keep |
| mqtt.getMqttVersions | mqtt.getMqttVersions | ✓ keep |
| mqtt.getPeerVerifyModes | mqtt.getPeerVerifyModes | ✓ keep |
| mqtt.getSslProtocols | mqtt.getSslProtocols | ✓ keep |
| mqtt.regenerateClientId | mqtt.generateClientId | Rename `regenerate` to `generate` (closed vocab) |
| mqtt.setAutoKeepAlive | mqtt.setAutoKeepAlive | ✓ keep |
| mqtt.setCleanSession | mqtt.setCleanSession | ✓ keep |
| mqtt.setClientId | mqtt.setClientId | ✓ keep |
| mqtt.setHostname | mqtt.setHostname | ✓ keep |
| mqtt.setKeepAlive | mqtt.setKeepAlive | ✓ keep |
| mqtt.setMode | mqtt.setMode | ✓ keep |
| mqtt.setMqttVersion | mqtt.setMqttVersion | ✓ keep |
| mqtt.setPassword | mqtt.setPassword | ✓ keep |
| mqtt.setPeerVerifyDepth | mqtt.setPeerVerifyDepth | ✓ keep |
| mqtt.setPeerVerifyMode | mqtt.setPeerVerifyMode | ✓ keep |
| mqtt.setPort | mqtt.setPort | ✓ keep |
| mqtt.setSslEnabled | mqtt.setSslEnabled | ✓ keep |
| mqtt.setSslProtocol | mqtt.setSslProtocol | ✓ keep |
| mqtt.setTopic | mqtt.setTopic | ✓ keep |
| mqtt.setUsername | mqtt.setUsername | ✓ keep |
| mqtt.setWillMessage | mqtt.setWillMessage | ✓ keep |
| mqtt.setWillQoS | mqtt.setWillQoS | ✓ keep |
| mqtt.setWillRetain | mqtt.setWillRetain | ✓ keep |
| mqtt.setWillTopic | mqtt.setWillTopic | ✓ keep |
| mqtt.toggleConnection | mqtt.setConnected | Rename `toggle` to `set` (boolean param) |
| notifications.channels | notifications.listChannels | Rename noun to verb |
| notifications.clearAll | notifications.clearAll | ✓ verb; keep (or `deleteAll`) |
| notifications.clearChannel | notifications.deleteChannel | Use `delete` instead of `clear` (or keep `clear`) |
| notifications.list | notifications.list | ✓ verb in closed vocab; keep |
| notifications.markRead | notifications.markRead | ✓ verb; keep |
| notifications.post | notifications.post | ✓ verb in closed vocab; keep |
| notifications.postCritical | notifications.post | Collapse to one `post` with severity parameter; or keep separate |
| notifications.postInfo | notifications.post | Collapse to one `post` with severity parameter; or keep separate |
| notifications.postWarning | notifications.post | Collapse to one `post` with severity parameter; or keep separate |
| notifications.resolve | notifications.resolve | ✓ verb in closed vocab; keep |
| notifications.unreadCount | notifications.getUnreadCount | Rename to `get*` pattern |
| project.action.add | action.add | Drop `project` prefix (action is project-resident) |
| project.action.delete | action.delete | Drop `project` prefix |
| project.action.duplicate | action.duplicate | Drop `project` prefix |
| project.actions.list | action.list | Drop `project` prefix; singular |
| project.dataset.add | dataset.add | Drop `project` prefix |
| project.dataset.delete | dataset.delete | Drop `project` prefix |
| project.dataset.duplicate | dataset.duplicate | Drop `project` prefix |
| project.dataset.setOption | dataset.setOption | Drop `project` prefix |
| project.dataset.setTransformCode | dataset.setTransformCode | Drop `project` prefix |
| project.dataset.setVirtual | dataset.setVirtual | Drop `project` prefix |
| project.datasets.list | dataset.list | Drop `project` prefix; singular |
| project.exportJson | project.export | Rename; verb `export` is in closed vocab; format as param |
| project.file.new | project.new | Simplify; verb in closed vocab |
| project.file.open | project.open | Simplify; verb in closed vocab |
| project.file.save | project.save | Simplify; verb in closed vocab |
| project.frameParser.configure | frameParser.configure | Drop `project` prefix (frame parser is project-resident); OR keep as `project.frameParser.*` if tied to a specific source—check design |
| project.frameParser.getConfig | frameParser.getConfig | Drop `project` prefix |
| project.frameParser.getLanguage | frameParser.getLanguage | Drop `project` prefix |
| project.frameParser.setCode | frameParser.setCode | Drop `project` prefix |
| project.frameParser.setLanguage | frameParser.setLanguage | Drop `project` prefix |
| project.getStatus | project.getStatus | ✓ keep |
| project.group.add | group.add | Drop `project` prefix |
| project.group.delete | group.delete | Drop `project` prefix |
| project.group.duplicate | group.duplicate | Drop `project` prefix |
| project.group.select | group.select | Drop `project` prefix; verb in closed vocab |
| project.groups.list | group.list | Drop `project` prefix; singular |
| project.loadFromJSON | project.load | Simplify; verb `load` is in closed vocab |
| project.loadIntoFrameBuilder | frameBuilder.load | Rename; frame builder is the scope, not project |
| project.outputWidget.add | outputWidget.add | Drop `project` prefix |
| project.outputWidget.delete | outputWidget.delete | Drop `project` prefix |
| project.outputWidget.duplicate | outputWidget.duplicate | Drop `project` prefix |
| project.parser.getCode | parser.getCode | Drop `project` prefix; eliminate duplicate (choose `frameParser.*` or `parser.*`) |
| project.parser.setCode | parser.setCode | Drop `project` prefix; eliminate duplicate |
| project.setTitle | project.setTitle | ✓ keep (project document-level) |
| project.source.add | source.add | Drop `project` prefix |
| project.source.configure | source.configure | Drop `project` prefix; verb in closed vocab |
| project.source.delete | source.delete | Drop `project` prefix |
| project.source.getConfiguration | source.getConfiguration | Drop `project` prefix |
| project.source.getFrameParserCode | source.getFrameParserCode | Drop `project` prefix |
| project.source.list | source.list | Drop `project` prefix |
| project.source.setFrameParserCode | source.setFrameParserCode | Drop `project` prefix |
| project.source.setProperty | source.setProperty | Drop `project` prefix; rename to specific field setters (setTitle, setFrameStart, etc.) |
| project.source.update | source.update | Drop `project` prefix; rename to specific field setters or use `configure` |
| project.tables.add | dataTable.add | Drop `project` prefix; rename `tables` to singular `dataTable` |
| project.tables.delete | dataTable.delete | Drop `project` prefix; singular |
| project.tables.get | dataTable.get | Drop `project` prefix; singular |
| project.tables.list | dataTable.list | Drop `project` prefix; singular |
| project.tables.register.add | dataTableRegister.add | Collapse three-tier to two (device = dataTableRegister) |
| project.tables.register.delete | dataTableRegister.delete | Collapse three-tier |
| project.tables.register.update | dataTableRegister.update | Collapse three-tier |
| project.tables.rename | dataTable.rename | Drop `project` prefix; singular |
| project.workspaces.add | workspace.add | Drop `project` prefix |
| project.workspaces.autoGenerate | workspace.autoGenerate | Drop `project` prefix; verb `generate` in closed vocab |
| project.workspaces.customize.get | workspace.getCustomizeMode | Collapse three-tier; rename to clear verb |
| project.workspaces.customize.set | workspace.setCustomizeMode | Collapse three-tier; rename to clear verb |
| project.workspaces.delete | workspace.delete | Drop `project` prefix |
| project.workspaces.get | workspace.get | Drop `project` prefix |
| project.workspaces.list | workspace.list | Drop `project` prefix |
| project.workspaces.rename | workspace.rename | Drop `project` prefix |
| project.workspaces.widgets.add | workspaceWidget.add | Collapse three-tier (device = workspaceWidget) |
| project.workspaces.widgets.remove | workspaceWidget.remove | Collapse three-tier; rename `remove` to `delete` (consistency) |
| sessions.close | session.close | Drop prefix; verb in closed vocab |
| sessions.getCanonicalDbPath | session.getCanonicalDbPath | Drop prefix; read-only path |
| sessions.getStatus | session.getStatus | Drop prefix |
| sessions.setExportEnabled | session.setExportEnabled | Drop prefix |
| ui.window.getGroups | ui.window.getGroups | Keep `ui.window` (scope is UI window state) |
| ui.window.getLayout | ui.window.getLayout | Keep `ui.window` |
| ui.window.getStatus | ui.window.getStatus | Keep `ui.window` |
| ui.window.getWidgetSettings | ui.window.getWidgetSettings | Keep `ui.window` |
| ui.window.getWindowStates | ui.window.getWindowStates | Keep `ui.window` |
| ui.window.loadLayout | ui.window.loadLayout | Keep `ui.window`; verb in closed vocab |
| ui.window.saveLayout | ui.window.saveLayout | Keep `ui.window`; verb in closed vocab |
| ui.window.setActiveGroup | ui.window.setActiveGroup | Keep `ui.window` |
| ui.window.setActiveGroupIndex | ui.window.setActiveGroupIndex | Keep `ui.window` |
| ui.window.setAutoLayout | ui.window.setAutoLayout | Keep `ui.window` |
| ui.window.setLayout | ui.window.setLayout | Keep `ui.window` |
| ui.window.setWidgetSetting | ui.window.setWidgetSetting | Keep `ui.window` |
| ui.window.setWindowState | ui.window.setWindowState | Keep `ui.window` |

---

### Open questions for user resolution:

1. **`frameParser` vs `parser`**: Should frame parsing APIs use `frameParser.*` or `parser.*`? Current state has duplicates (`project.parser.setCode` AND `project.frameParser.setCode`). Recommend unifying to `frameParser.*` (more explicit) and deleting `parser.*`.

2. **`io.driver.*` flattening**: Should `io.driver.uart.setBaudRate` become `uart.setBaudRate` (drop `io.driver`), or `io.uart.setBaudRate` (collapse to two tiers, keep `io` prefix)? The former is cleaner but loses the "io" domain signal. Recommend **drop `io.driver`** entirely since device type is self-identifying.

3. **CSV player motion verbs**: Should `csvPlayer.nextFrame` / `previousFrame` / `pause` / `play` / `toggle` become a single `csvPlayer.step(direction: int)` + `csvPlayer.setPaused(bool)`? Or keep them separate for clarity? Recommend **collapse to `setPaused` + `step`**.

4. **Licensing trial scope**: Should `licensing.trial.*` stay three-tier (trial is a distinct scope) or collapse to `licensingTrial.*`? Recommend **keep three-tier** if trial mode is sufficiently orthogonal.

5. **Notifications severity**: Should `notifications.{postCritical, postInfo, postWarning}` collapse to a single `notifications.post(message, severity: string)`, or keep separate for backward compat? Recommend **collapse to single `post` with severity param**.

6. **Workspace customize**: Should `workspace.customize.{get,set}` become `workspace.{getCustomizeMode, setCustomizeMode}` or `workspace.customization.{get,set}`? Recommend **flatten to `workspace.setCustomizeMode` + `workspace.getCustomizeMode`** (customize → customizeMode is clearer).

7. **Project prefix strategy**: The mapping above drops `project.` from most project-resident resources (group, dataset, etc.). This improves conciseness but reduces discoverability. Alternative: keep `project.resource.*` for all project data, `io.*` for hardware, `ui.*` for UI, etc. Which principle wins? Recommend **drop `project.` prefix** (cleaner API, groups are unambiguous).

---

**End of audit.**

/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#include "AI/ContextBuilder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include "AI/Logging.h"
#include "AI/ToolDispatcher.h"

/** @brief Reads a Qt resource into a QString, returning empty on failure. */
static QString readResource(const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(AI::serialStudioAI) << "Resource not readable:" << path;
    return {};
  }

  const auto data = file.readAll();
  file.close();
  return QString::fromUtf8(data);
}

//--------------------------------------------------------------------------------------------------
// Static text blocks
//--------------------------------------------------------------------------------------------------

// Single string-literal return; line count is content, not control flow
// code-verify off
/** @brief Returns the role description used as the first cached system block. */
QString AI::ContextBuilder::roleBlock()
{
  return QStringLiteral("You are the in-app AI assistant for Serial Studio, a cross-platform "
                        "telemetry dashboard. Your job is to help the user build and edit their "
                        "project: add sources, groups, datasets, frame parsers, transforms, "
                        "tables, output widgets, and painters by calling the tools you have "
                        "available.\n\n"
                        "Tool discovery model:\n"
                        "Your tool list intentionally exposes only ~13 tools at a time: three "
                        "meta-tools and a small curated essentials set. Serial Studio actually "
                        "has ~190 commands. To use anything outside the essentials:\n"
                        "  1. Call meta.listCommands(prefix) to find the right command name. "
                        "Useful prefixes: \"project.\" (project editing), \"io.driver.\" "
                        "(driver config), \"ui.window.\" (dashboard layout), \"console.\" "
                        "(serial console), \"mqtt.\" (MQTT), etc.\n"
                        "  2. Call meta.describeCommand(name) to see the full input schema for "
                        "any unfamiliar command.\n"
                        "  3. Call meta.executeCommand(name, arguments) to actually run it. "
                        "The arguments object must match the schema you got from step 2.\n"
                        "Use the curated essentials directly (project.file.new, "
                        "project.groups.list, etc) without going through meta.executeCommand.\n\n"
                        "Documentation lookup:\n"
                        "Call meta.fetchHelp(path) to read the canonical Serial Studio "
                        "documentation from the doc/help markdown source on GitHub. Two "
                        "rules: (1) when you don't know what page exists, pass \"help.json\" "
                        "FIRST to get the index. It returns a JSON array of {id, title, "
                        "section, file} entries -- pick the right `file` and call again. "
                        "(2) When you DO know the file name from a previous index lookup, "
                        "pass the bare page name without .md (e.g. \"Painter-Widget\", "
                        "\"Frame-Parser\", \"Project-Editor\", \"API-Reference\"). "
                        "Multi-word page names use hyphens. If you 404, the tool "
                        "auto-redirects to help.json so you can correct the name on the "
                        "next call. NEVER guess a page name -- the cost of one extra "
                        "index fetch is far smaller than producing wrong information.\n\n"
                        "Scripting reference lookup:\n"
                        "Before writing or modifying ANY user-authored script, call "
                        "meta.fetchScriptingDocs(kind) for the matching context. The six "
                        "kinds are: frame_parser_js, frame_parser_lua (per-source frame "
                        "parsers), transform_js, transform_lua (per-dataset value "
                        "transforms), output_widget_js (output widgets), painter_js "
                        "(painter widget). Each returns the exact API surface, idioms, and "
                        "worked examples. APIs differ between contexts -- do not invent "
                        "function names from one context in another.\n\n"
                        "How-to recipes:\n"
                        "When the user asks for one of these multi-step tasks, call "
                        "meta.howTo(task) FIRST and follow the returned steps in order. "
                        "Available tasks: add_painter, add_workspace, "
                        "add_widget_to_workspace, add_output_widget, "
                        "add_executive_dashboard, add_dataset, add_transform. These "
                        "recipes are authoritative for the right sequence of tool calls "
                        "and the gotchas (e.g. widgetType=0 is Terminal not a default; "
                        "workspace tiles need an icon; executive dashboards must pick "
                        "groups by datasetSummary not array index). Improvise only when "
                        "no recipe applies.\n\n"
                        "Behavioral rules:\n"
                        "\n"
                        "Talking is non-optional. The user is reading a chat window; tool "
                        "results are shown as collapsible cards but are NOT what the user "
                        "wants to read. Treat every turn like a conversation:\n"
                        "  - Before any tool call, write one short sentence about what you "
                        "are about to look up or do (\"Let me check the current sources\"). "
                        "Keep it brief.\n"
                        "  - After tool results come back, you MUST write a real summary in "
                        "your own words. Translate the JSON into something a human would "
                        "say. Don't just restate one field and stop. For project state "
                        "results in particular: describe the relevant fields meaningfully "
                        "(busType, frameDetection, frameStart/frameEnd, hasFrameParser, "
                        "checksumAlgorithm, etc), not the raw values. Highlight what's "
                        "notable, what's missing, what the user might want to do next. "
                        "If a list is short, summarize each item. If long, group them.\n"
                        "  - NEVER end your turn with tool calls and no follow-up text. If "
                        "the user asks a question, answer it in prose, not by handing them "
                        "raw JSON.\n"
                        "  - When the user asks for X, deliver X. Don't ask back-and-forth "
                        "questions if the next step is obvious -- just take it.\n"
                        "\n"
                        "Reading tool results:\n"
                        "  - When a result has a `_summary` field, use it as the spine of "
                        "your reply -- expand on it with the specific details the user "
                        "asked about.\n"
                        "  - Most enum-shaped fields come with both the raw int (e.g. "
                        "`busType: 0`) and a friendly twin (`busTypeLabel: \"UART (serial "
                        "port)\"`). Use the label form in your prose; ignore the int.\n"
                        "  - `hasFrameParser: true` means a JS or Lua script is decoding "
                        "frames; `false` means raw bytes go straight to the dashboard.\n"
                        "  - `frameStart` / `frameEnd` are the literal byte sequences "
                        "delimiting a logical frame. `\"$\"` / `\"\\n\"` is the standard "
                        "default.\n"
                        "\n"
                        "Other rules:\n"
                        "  - Be concise. Trim greetings and filler. Match the user's "
                        "register.\n"
                        "  - Discover before you act: when in doubt, list/describe before "
                        "executing. One focused tool call per step beats speculative "
                        "batches.\n"
                        "  - When you propose code (frame parser, transform, output, "
                        "painter), consult the scripting reference in this prompt. Do not "
                        "invent APIs.\n"
                        "  - For any tool tagged Confirm, the user will be asked to "
                        "approve. Explain briefly what each call will do before issuing "
                        "it.\n"
                        "  - Never ask for an API key. Never ask the user to run shell "
                        "commands.\n"
                        "  - If a tool returns an error, surface it in plain language and "
                        "try a different approach. Do not loop on the same failing call.\n");
}

// code-verify on

/** @brief Returns the list of canned how-to task ids meta.howTo accepts. */
QStringList AI::ContextBuilder::howToTasks()
{
  return {
    QStringLiteral("add_painter"),
    QStringLiteral("add_workspace"),
    QStringLiteral("add_widget_to_workspace"),
    QStringLiteral("add_output_widget"),
    QStringLiteral("add_executive_dashboard"),
    QStringLiteral("add_dataset"),
    QStringLiteral("add_transform"),
  };
}

// String-literal recipe table; line count is content, not control flow
// code-verify off
/** @brief Returns a step-by-step recipe for one of the canned how-to tasks. */
QString AI::ContextBuilder::howToRecipe(const QString& task)
{
  if (task == QStringLiteral("add_painter"))
    return QStringLiteral("PAINTER WIDGET (Pro)\n"
                          "1. Create the host group: project.group.add "
                          "{title, widgetType: 8}. widgetType=8 is GroupWidget::Painter.\n"
                          "2. Call meta.fetchScriptingDocs('painter_js') BEFORE writing "
                          "any code. Painter API is distinct from frame-parser/transform "
                          "JS -- do not invent function names from another context.\n"
                          "3. Setting the painter program from the API is not yet exposed "
                          "(painterCode lives on the Group struct but no API command "
                          "writes it as of v3.3). Tell the user to open Project Editor -> "
                          "the new group -> Painter tab and paste the code there. Provide "
                          "the code in chat ready to copy.\n"
                          "4. Pin to a workspace: project.workspaces.widgets.add with "
                          "widgetType=18 (DashboardPainter, Pro), groupId, "
                          "relativeIndex=0. Customize must be enabled "
                          "(project.workspaces.customize.set {enabled: true}).\n"
                          "5. project.file.save.\n");

  if (task == QStringLiteral("add_workspace"))
    return QStringLiteral("ADD A WORKSPACE\n"
                          "1. project.workspaces.customize.set {enabled: true} -- "
                          "workspace edits require customize mode on.\n"
                          "2. project.workspaces.add {title, icon} -> returns "
                          "workspaceId (>= 1000). Always provide an icon path "
                          "(e.g. 'qrc:/icons/panes/overview.svg'); without one the "
                          "taskbar tile renders blank.\n"
                          "3. Pin widgets with project.workspaces.widgets.add. See "
                          "meta.howTo('add_widget_to_workspace') for the exact rules.\n"
                          "4. project.file.save.\n");

  if (task == QStringLiteral("add_widget_to_workspace"))
    return QStringLiteral("PIN A WIDGET ONTO A WORKSPACE\n"
                          "1. project.groups.list -> for each candidate group read its "
                          "compatibleWidgetTypes array. That tells you which "
                          "DashboardWidget enum values will actually render for that "
                          "group's data.\n"
                          "2. project.workspaces.list -> grab the target workspaceId "
                          "(>= 1000).\n"
                          "3. project.workspaces.customize.set {enabled: true}.\n"
                          "4. project.workspaces.widgets.add {workspaceId, widgetType, "
                          "groupId, relativeIndex: 0}. NEVER pass widgetType=0 -- that is "
                          "DashboardTerminal, not a tile. Pick from the group's "
                          "compatibleWidgetTypes.\n"
                          "5. relativeIndex is 0 unless you are intentionally adding a "
                          "second tile of the same widgetType+groupId to the same "
                          "workspace. It is NOT a dataset index.\n"
                          "6. project.file.save.\n");

  if (task == QStringLiteral("add_output_widget"))
    return QStringLiteral("ADD AN OUTPUT WIDGET (Pro)\n"
                          "1. Output widgets attach to a group's output panel. "
                          "project.outputWidget.add adds one to the currently selected "
                          "group. project.group.select {groupId} first if needed.\n"
                          "2. Call meta.fetchScriptingDocs('output_widget_js') BEFORE "
                          "writing the JS that converts UI state into device bytes.\n"
                          "3. Pin to a workspace with widgetType=15 "
                          "(DashboardOutputPanel) via project.workspaces.widgets.add.\n"
                          "4. project.file.save.\n");

  if (task == QStringLiteral("add_executive_dashboard"))
    return QStringLiteral("EXECUTIVE / OVERVIEW DASHBOARD\n"
                          "1. Read project.groups.list THOROUGHLY. For each group inspect "
                          "its title, datasetSummary (titles + units), and "
                          "compatibleWidgetTypes. DO NOT pick groups by array index -- "
                          "use the groupId field.\n"
                          "2. Pick a small set (4-8) of groups whose data is genuinely "
                          "summary-relevant: speed, RPM, temperature, fuel, voltage, "
                          "state-of-charge, primary alarms. Skip raw-flag groups (door "
                          "open, individual lights, individual brake pressures) -- those "
                          "belong on dedicated diagnostic workspaces, not the overview.\n"
                          "3. For each picked group, choose the most readable widgetType "
                          "from compatibleWidgetTypes. Heuristics: Gauge (11) for single "
                          "scalars with min/max, MultiPlot (2) for related time-series, "
                          "DataGrid (1) for tabular reads, Compass (12) for headings, "
                          "Bar (10) for bounded levels, LED (8) for booleans/alarms. "
                          "NEVER widgetType=0.\n"
                          "4. project.workspaces.customize.set {enabled: true}, then "
                          "project.workspaces.add {title: 'Overview', icon: "
                          "'qrc:/icons/panes/overview.svg'} -- always provide an icon.\n"
                          "5. For each pick, project.workspaces.widgets.add "
                          "{workspaceId, widgetType, groupId, relativeIndex: 0}. The "
                          "server validates widgetType against the group's "
                          "compatibleWidgetTypes and rejects mismatches with a clear "
                          "error.\n"
                          "6. Show the user the curated list in chat BEFORE saving. Let "
                          "them confirm or redirect. Then project.file.save.\n");

  if (task == QStringLiteral("add_dataset"))
    return QStringLiteral("ADD A DATASET TO A GROUP\n"
                          "1. project.groups.list -> pick the target groupId, then "
                          "project.group.select {groupId}.\n"
                          "2. project.dataset.add {options: <bitflags>}. Visualization "
                          "options are bit flags: 1=Plot, 2=FFT, 4=Bar, 8=Gauge, "
                          "16=Compass, 32=LED, 64=Waterfall (Pro). Combine with bitwise "
                          "OR (e.g. 1|8 = 9 for plot+gauge).\n"
                          "3. Use project.dataset.setOption {option, enabled} to toggle "
                          "individual flags after creation.\n"
                          "4. project.file.save.\n");

  if (task == QStringLiteral("add_transform"))
    return QStringLiteral("PER-DATASET VALUE TRANSFORM\n"
                          "1. Call meta.fetchScriptingDocs with kind matching the "
                          "source's language: 'transform_js' or 'transform_lua'.\n"
                          "2. Write a function transform(value) that returns a number. "
                          "Top-level local declarations (Lua) and var declarations (JS) "
                          "become per-dataset state across calls -- safe for EMAs, "
                          "running averages, etc.\n"
                          "3. project.group.select {groupId}, then "
                          "project.dataset.setTransformCode {datasetIndex, code} on the "
                          "selected dataset.\n"
                          "4. project.file.save.\n");

  return QString();
}

// code-verify on

/** @brief Returns a single scripting reference doc body by kind, or empty. */
QString AI::ContextBuilder::scriptingDocFor(const QString& kind)
{
  static const QSet<QString> kAllowed = {QStringLiteral("frame_parser_js"),
                                         QStringLiteral("frame_parser_lua"),
                                         QStringLiteral("transform_js"),
                                         QStringLiteral("transform_lua"),
                                         QStringLiteral("output_widget_js"),
                                         QStringLiteral("painter_js")};

  if (!kAllowed.contains(kind))
    return {};

  return readResource(QStringLiteral(":/ai/docs/%1.md").arg(kind));
}

/** @brief Returns the concatenation of all scripting reference docs. */
QString AI::ContextBuilder::scriptingDocsBlock()
{
  static const QStringList kKinds = {
    QStringLiteral("frame_parser_js"),
    QStringLiteral("frame_parser_lua"),
    QStringLiteral("transform_js"),
    QStringLiteral("transform_lua"),
    QStringLiteral("output_widget_js"),
    QStringLiteral("painter_js"),
  };

  QString out;
  out.reserve(48 * 1024);
  out += QStringLiteral("# Scripting reference\n\n");
  for (const auto& kind : kKinds) {
    const auto path = QStringLiteral(":/ai/docs/%1.md").arg(kind);
    const auto body = readResource(path);
    if (body.isEmpty())
      continue;

    out += QStringLiteral("\n---\n");
    out += body;
    out += QStringLiteral("\n");
  }
  return out;
}

/** @brief Returns the current project state assembled from safe list commands. */
QString AI::ContextBuilder::liveProjectStateBlock()
{
  ToolDispatcher dispatcher;
  const auto state  = dispatcher.getProjectState();
  const auto pretty = QJsonDocument(state).toJson(QJsonDocument::Indented);

  QString out;
  out += QStringLiteral("# Current project state\n\n");
  out += QStringLiteral("```json\n");
  out += QString::fromUtf8(pretty);
  out += QStringLiteral("\n```\n");
  return out;
}

//--------------------------------------------------------------------------------------------------
// Composer
//--------------------------------------------------------------------------------------------------

/** @brief Returns the array of system blocks for the Anthropic Messages API. */
QJsonArray AI::ContextBuilder::buildSystemArray(bool includeScriptingDocs)
{
  // role + scripting docs + project state under one ephemeral cache breakpoint
  QJsonObject ephemeral;
  ephemeral[QStringLiteral("type")] = QStringLiteral("ephemeral");

  QJsonArray system;

  QJsonObject role;
  role[QStringLiteral("type")] = QStringLiteral("text");
  role[QStringLiteral("text")] = roleBlock();
  system.append(role);

  if (includeScriptingDocs) {
    QJsonObject docs;
    docs[QStringLiteral("type")] = QStringLiteral("text");
    docs[QStringLiteral("text")] = scriptingDocsBlock();
    system.append(docs);
  }

  // Live project state is the LAST block: only this turn pays cache-write
  QJsonObject live;
  live[QStringLiteral("type")]          = QStringLiteral("text");
  live[QStringLiteral("text")]          = liveProjectStateBlock();
  live[QStringLiteral("cache_control")] = ephemeral;
  system.append(live);

  return system;
}

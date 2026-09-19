# Codex / Unreal integration

## Connections

- `unreal_epic`: Epic Unreal MCP, local HTTP `http://127.0.0.1:8000/mcp`.
- `ue_mcp_lyon`: David Lyon's `ue-mcp` **1.3.8**, local stdio Node server and the project `UE_MCP_Bridge` plugin. Installed package and lockfile are in ignored `.tools/ue-mcp/`; the plugin source is tracked in `Plugins/UE_MCP_Bridge/`.
- Local Codex configuration: `.codex/config.toml` (ignored, machine-specific). Both entries are project-scoped.
- Epic toolsets: EditorToolset, LiveCodingToolset, UMGToolSet, SlateInspectorToolset and ToolsetRegistry. MCP auto-start and lazy tool discovery are in `Config/DefaultEditorPerProjectUserSettings.ini`.
- `ue-mcp.yml` disables duplicate native-tool publication on the Lyon server and its feedback submission tool. Its additional HTTP server is disabled. Native tools remain discoverable through the Epic gateway.
- The third-party bridge depends on editor facilities including PCG, Niagara, GAS, animation and audio plugins. These do not replace this project's Mass ECS gameplay architecture. Runtime modules do not depend on either MCP implementation.

Use Epic tools first for editor state, logs and Live Coding; use Lyon tools for additional authoring operations. Serialize all editor calls, including calls across the two servers. Do not apply the same change through both. Confirm the connected project before mutations. Do not run Play, automation tests, or gameplay performance captures unless requested. Do not start normal builds against an open editor.

## ECS snapshot

`MazeDiagnosticsToolset.ReadSnapshots` reads all existing game/PIE worlds through `UMazeECSSubsystem::ReadDiagnostics`. Returns detached values: world path/type, authority, frame/time, session/menu state, active maze seed/size/revision and per-player vitals, items, position, locomotion and progress. Missing gameplay worlds return an explicit status without starting Play. Client values are local observations/mirrors, not server authority. Entity descriptions are scoped to that world and must not be reused as persistent identifiers.

## Unreal Insights

Installed tool: `C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe`.
CPU scopes: `Maze_ECS_Tick`, `Maze_GeneratePending`, `Maze_GenerateTopology`, `Maze_BuildSurface`, `Maze_WorldBuild`, `Maze_ReadDiagnostics`.
`Maze_WorldBuild` includes generation and adapter work; compare exclusive time/nested scopes rather than adding inclusive totals. Asynchronous collision work may finish outside that scope.

For an explicitly requested capture, start from the editor's Trace menu or execute in the editor console:

```text
Trace.File C:/ue_prj/laby/Saved/Profiling/maze.utrace cpu,gpu,frame,bookmark,log
Trace.Stop
```

Create `Saved/Profiling` first if needed and choose a new trace filename for each capture. Record the seed, maze size, graphics settings and build; compare identical scenarios. Separate generation/loading from steady gameplay. Editor timings include editor overhead. Memory profiling is a separate targeted capture, not enabled by default.

Export an existing capture without launching gameplay:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Export-Insights.ps1 -TraceFile C:/path/capture.utrace
```

Add `-IncludeEvents` when individual timing events are needed. Reports and exporter logs go to `Saved/Profiling/Reports/`. No automatic captures, tests or gameplay are configured.

## Verification on 2026-09-20

- Development Editor build succeeded against installed UE 5.8.2, including both project modules and UE_MCP_Bridge 1.3.8.
- Editor opened successfully. Epic MCP initialized on port 8000; its tool schemas include `labyEditor.MazeDiagnosticsToolset.ReadSnapshots`.
- Calling ReadSnapshots without Play returned `No active game/PIE ECS world. Play was not started.` Live gameplay snapshot contents remain unverified.
- Lyon MCP handshake reported version 1.3.8, 25 tool groups, live editor connection to laby and `pluginBuildStale: false`.
- Codex CLI reads both project MCP entries. An already-running Codex session may need reconnection/restart to expose newly configured tools.
- Export-Insights.ps1 passed PowerShell syntax validation. No performance trace was captured/exported, and no gameplay or automation tests were run.

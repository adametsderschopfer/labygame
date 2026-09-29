# Unreal iteration workflow

- Prefer Live Coding for routine C++ changes while Unreal Editor is open. Do not run a normal Build.bat build against an open editor and then ask the user to close it merely because the DLL is locked.
- Live Coding is configured in Config/DefaultEditorPerProjectUserSettings.ini. In an already running editor it can be enabled under Editor Preferences > General > Live Coding. Ctrl+Alt+F11 triggers compilation; editor console commands are LiveCoding and LiveCoding.Compile.
- If no available tool can trigger compilation in the running editor, ask the user to press Ctrl+Alt+F11 instead of closing the project. Check the Live Coding logs before reporting success.
- Stop/restart Play to recreate the procedural maze or runtime HUD after patches when needed. This does not require restarting the editor.
- Use a full editor restart/build when changes to module/plugin dependencies, component types/construction, or reflected object layout cannot safely be applied through Live Coding. Explain the concrete reason instead of treating every header edit as requiring a restart.
- With the editor closed, normal builds and command-line automation tests are appropriate. Never terminate the user's editor to unlock files.

# Unreal Engine solution selection

- Before implementing a feature or solving a technical problem, check whether the project's Unreal Engine version already provides a suitable supported API, subsystem, tool or official plugin. Prefer current engine-native solutions over legacy approaches or custom equivalents when they fit the task and reduce implementation or maintenance work.
- Actively consider newer Unreal Engine technologies and capabilities relevant to the work, even when a familiar approach would already solve the problem. Look beyond direct replacements for custom code: consider opportunities to improve visual quality, performance, scalability or authoring workflows. Bring concrete, beneficial options into the solution choice rather than defaulting to familiar techniques; apply the suitability checks below and keep adoption within the task's scope.
- Evaluate suitability against the project's ECS architectural contract, feature maturity, target platforms, performance and integration cost. Engine facilities must preserve the ownership and dependency boundaries below; do not introduce a competing source of gameplay state or rules.
- Verify availability and recommended usage against the installed engine source or official documentation instead of assuming an API exists or behaves the same across versions. Do not adopt experimental features, add dependencies or migrate working code solely for novelty. When a relevant engine solution is unsuitable, briefly explain the concrete reason for the chosen alternative.

# Gameplay architecture

## Architectural contract

- Use one architecture for all gameplay features: data-oriented Unreal Mass Entity ECS with a functional gameplay core and thin Unreal adapters. This applies to player control, vitals, maze generation, progress, rooms/session rules, and future mechanics such as inventory, combat, interactions and AI.
- Compose behavior from focused fragments and systems. Do not introduce a second gameplay framework, deep gameplay inheritance hierarchies, global gameplay managers, or an Actor/Component/Blueprint implementation of rules already owned by ECS.
- Treat these rules as the target architecture for new and changed code. Existing shortcuts are not precedents. When a feature touches a misplaced rule, move that rule to its owning system within the feature's scope; avoid unrelated rewrites.
- Read `Docs/ECS.md` before changing ECS behavior. Keep it current when changing fragment ownership, scheduling, authority or lifecycle. If descriptive documentation conflicts with this contract, follow this contract and update the relevant documentation.

## Ownership and dependency direction

- Mass fragments own mutable gameplay state. Each gameplay fact has one authoritative owner; derived values, presentation caches and replication mirrors must identify their source and refresh/invalidation path.
- Fragments contain data, not engine side effects or gameplay services. Use focused fragments for distinct responsibilities; do not grow a universal player/session fragment for unrelated features.
- Gameplay systems own decisions, validation, calculations and state transitions. Prefer stateless `FMaze...System` functions with explicit inputs and outputs, following `FMazeVitalsSystem` and `FMazePlayerControlSystem`.
- `UMazeECSSubsystem` owns per-world entity lifecycle, Mass queries, scheduling and the typed bridge API. Keep new feature rules in focused systems rather than accumulating them in the subsystem. Boundary validation and orchestration belong in the subsystem.
- Dependency direction is Unreal adapters -> subsystem API -> gameplay systems -> fragments/value types and pure algorithms. Systems and algorithms must not depend on concrete Actors, controllers, widgets, online services or editor modules; Unreal value types and containers are allowed.
- Adapters submit input, observations and requests through the subsystem, then consume commands or read-only snapshots. Do not expose mutable fragment references or the entity manager to UI/Actors as a shortcut.
- Engine-owned facts remain engine-owned: CharacterMovement resolves physical movement and collision; ECS consumes the resulting observations and decides gameplay consequences. Do not implement a competing physics simulation.
- Adapters may own engine resources, widget state, presentation caches, local preferences, transport operation state and replication mirrors. These must not become alternative sources for health, costs, cooldowns, inventory, victory or room/session rules. `UMazeOnlineGameInstance` manages transport across travel, not authoritative room membership or start permissions.

## Data flow and execution

- Use the explicit flow: input/engine observation -> ECS validation and rule evaluation -> state update and commands -> engine execution -> confirmed outcome back to ECS when needed. For example, the engine confirms a successful jump and ECS applies its stamina cost.
- Distinguish held input, one-shot requests and continuous observations. Consume one-shot requests once; apply gameplay costs and effects once per accepted action. Clear input on menu/focus changes, unpossession and teardown.
- Keep the current synchronous game-thread scheduling through `UMazeECSSubsystem`. Document when a new system runs, which fragments it reads/writes and its ordering dependencies. Do not silently mix subsystem execution with automatically scheduled `UMassProcessor` execution or process the same rule from multiple ticks.
- If a feature requires Mass processors or background work, explicitly design and document the scheduling change. Worker work must operate on owned data without UObject access; publish results on the game thread after validating world/entity lifetime and the request or generation revision.
- Use simulation delta time for time-dependent rules and define pause behavior. Keep gameplay timers/cooldowns in ECS; engine timers may deliver callbacks but must not own an independent gameplay state machine.
- Generate topology, mesh data, spawn positions and exit checks in ECS using the pure `Maze/` algorithms. `AMazeWorld` consumes immutable generation data; HUD only reads progress and never decides whether an exit was reached.
- Make generation reproducible from explicit seed and parameters. Use a local seeded random stream for deterministic algorithms. Choose a new seed at the session boundary and replicate it; do not sample global random state or wall-clock time inside deterministic generation.
- Publish generated data as immutable shared payloads. Rebuild engine geometry and invalidate dependent progress/caches by revision. Do not mutate a payload already held by consumers or create an entity per static wall without a concrete gameplay need.

## Lifecycle and multiplayer

- Keep entities, handles and runtime gameplay state scoped to their owning world. Validate handles against that world's entity manager before access. Never retain fragment pointers/views across entity destruction, structural changes, callbacks or ticks.
- Pair entity/resource creation with cleanup during EndPlay/world teardown. Unbind delegates, cancel pending work or reject stale callbacks, and clear handles and input. Do not preserve world entity handles in GameInstance, static variables or save data across travel.
- Keep authoritative multiplayer gameplay decisions on the server (or standalone world): damage, resource costs, inventory changes, room admission/start and progress. Clients send intent; validate ownership, payloads and gameplay preconditions before accepting it. Client UI checks do not authorize an action.
- RPCs, replicated properties and OnRep handlers are transport adapters. Send authoritative ECS snapshots through them and apply received data to client ECS through the subsystem. Replication mirrors must not independently execute gameplay rules.
- Client prediction, if needed, must explicitly define predicted state and reconciliation against server results. Preserve CharacterMovement's engine networking and avoid duplicate movement or cost application.
- Keep local presentation/preferences distinct from server gameplay state. Explicitly choose per-player versus per-session ownership for new data; do not assume one local player when the feature must support multiple players.

## Code organization and feature completion

- Follow the existing `Source/laby/Public` and `Private` layout: `ECS/` for fragments, systems and orchestration; `Maze/` for pure topology/geometry algorithms; `Player/` for input, camera and physics adapters; `World/` for world/transport adapters; `UI/` for presentation. Keep editor tooling in `labyEditor` and runtime code independent of editor modules.
- Split growing features into focused files under these areas. Use `FMaze...Fragment`, `FMaze...System` and Unreal naming conventions. Expose only necessary public interfaces; put nontrivial implementation in `.cpp` files unless templates or a small existing value/helper pattern justify a header implementation.
- Keep tunable gameplay parameters in one explicit configuration/value definition consumed by ECS. Config files or Data Assets may supply defaults; Actors and widgets must not duplicate gameplay constants or reinterpret the rules.
- Use Widget Blueprints for layout/presentation and native adapters for binding actions. Blueprint graphs follow the same ECS boundary. Keep UI text in the project's `MazeText`/`FText` conventions; do not duplicate native button handlers in Blueprint.
- Before implementing a feature, identify its owning entity/fragments, rule system, subsystem entry points, adapter responsibilities, authority, execution order and reset/cleanup behavior. Add only the abstractions needed for that feature.
- Before completion, check the diff for duplicate state/rules, mutable data leaking into adapters, stale handles, repeated one-shot effects and missing reset/replication paths. This is a code review, not permission to launch gameplay.
- Do not run tests or Play unless the user requests it; the user is handling gameplay testing. Do not restart Play under the iteration guidance above without that request. When tests are requested, favor focused system/algorithm invariants and relevant lifecycle/authority checks. Report what was actually verified and what was not run.

# Location loading and resource budgets

- Read `Docs/Streaming.md` before adding locations, runtime geometry, items, AI or resource-heavy effects. Register production locations and their required soft asset paths in `UMazeLocationSettings`; the native Asset Manager cook hook and runtime preload use this same manifest. Run `Scripts/Validate-Locations.ps1` after manifest changes (static validation, not Play/testing).
- Use `UMazeLocationSubsystem` only for engine resource readiness. Register asynchronous resource participants before starting work; report completion/failure and unregister on teardown. Do not put session permissions, item state, simulation dormancy or gameplay timers in resource adapters.
- Keep gameplay state/identity in ECS when representations unload. Define authority and near/far simulation semantics for every new mechanic. Cosmetic chunk lifetime must never reroll loot, reset creatures or delete persistent gameplay facts.
- Preserve resident physics until a feature explicitly designs safe server-wide collision residency for all players and physics users. Current distance streaming is visual; do not advertise it as fully bounded world memory. Worker payloads use thread-safe immutable shared ownership, no UObject/fragment views, and validate world/entity/revision before application.
- Respect per-frame creation/eviction limits, hysteresis and profiling targets. New long sightlines, larger rooms, faster movement or teleport require an explicit streaming-radius/readiness review. Use standard scalability/texture streaming; do not force one hardware memory budget or promise FPS improvements without measurements.
- Do not convert procedural maps to World Partition just by enabling a setting. Authored partitioned maps use native streaming sources and the common readiness contract. Resource registration does not automatically define a new location's gameplay or spawn rules.

# 3D assets and content pipeline

- Read `Docs/AssetPipeline.md` before importing, generating, replacing, moving or wiring a 3D asset, material, texture, animation, prefab-like assembly or presentation catalog. Keep it current when the asset layout, import contract, representation policy or loading path changes.
- Read `Docs/Meshy.md` before using cloud generation for a 3D model, texture, remesh, rig or animation. Meshy is the project's approved default cloud generator and may be proposed for relevant asset tasks, but it remains a source-art tool: its output must still pass the asset pipeline, Unreal import and runtime architecture checks below.
- Use the installed `meshy-openai-plugin` workflow and its pinned CLI rather than hand-written HTTP calls. Reuse browser OAuth; never request or print an API key. Before a paid submission, run the CLI dry-run or use the published Meshy price source, report the planned stages and estimated credits, check the API balance, and obtain explicit user approval for that spend. Read-only health checks must not create a task.
- Submit each approved Meshy stage once, retain its task ID and wait on that task. Never silently rerun, create extra variants or add paid remesh/retexture/convert stages. Inspect an available preview, report actual consumed credits and preserve task lineage so later edits reuse the existing asset.
- Treat the document as the target for new and touched content. Existing assets that do not yet comply are migration candidates, not precedents and not permission for an unrelated bulk reorganization.
- Keep editable/vendor source and provenance under `ArtSource/<Feature>/`; keep runtime Unreal packages under `Content/<Feature>/`. Never place source archives, DCC caches or exported intermediates in `Content`, and never make production content depend on `Content/Developers`.
- Move or rename `.uasset` and `.umap` packages only through Unreal-aware editor tooling, then update references and fix redirectors. Do not move binary Unreal packages with filesystem commands.
- Preserve the separation between asset, representation and gameplay. Meshes and prefab-like assemblies are presentation resources; gameplay identity and mutable rules remain in ECS. Blueprint construction graphs and Actor/component ticks must not become another gameplay implementation.
- Choose the lightest native representation that fits: a mesh/component for a unique visual, ISM for repeated identical props, Packed Level Actor for supported predominantly static assemblies, Level Instance for authored multi-Actor modules, and a thin Actor/Blueprint adapter only when engine-owned resources or behavior require one.
- Do not spawn one Actor per repeated cosmetic prop. Group compatible instances by mesh, material set, mobility and collision policy. Consider HISM, Mass Representation, PCG or a new plugin only after checking the installed UE version, scheduling implications and measured benefit.
- Use typed soft references in a focused settings or asset-definition object instead of scattering hard-coded `/Game/...` strings. Mass fragments hold stable value identifiers, not UObjects. Adapters resolve presentation assets after the location readiness contract has loaded them.
- Register every production runtime dependency in `UMazeLocationSettings` or an explicitly integrated Asset Manager bundle. A soft reference is not proof that a resource was preloaded. Do not hide first-use synchronous loading behind an unregistered `LoadObject` call.
- Imported assets must have documented provenance/license, deliberate centimeter scale/orientation/pivot, intentional material slots, UV/tangent policy, collision, LOD or Nanite decision, and target-platform suitability. Nanite does not replace collision, navigation or a memory budget.
- Procedural cosmetic placement must be deterministic from explicit seed and stable inputs. Anything affecting collision, navigation, interaction, loot, AI or progression needs stable ECS identity and the documented resident/near/far policy; visual chunk eviction must not erase gameplay facts.
- Prefer reproducible Unreal Python/editor scripts for generated or bulk-imported asset families. Scripts must be safe to rerun or clearly document destructive replacement behavior. Never hand-edit `.uasset` files.
- Before completing an asset change, inspect references, manifest/cook coverage, dedicated-server behavior where relevant, instancing compatibility, cleanup on regeneration/teardown and the source/runtime asset pair. Run `Scripts/Validate-Locations.ps1` after manifest changes; Play, rendering captures and gameplay tests remain opt-in.

# Localization

- Treat Russian (`ru`), English (`en`) and Spanish (`es`) as supported game languages. Whenever a change adds or changes player-visible text, update the translations for all three in `Content/Localization/Game`, including menus, settings, HUD, map labels, room names, loading text and network messages. Do not leave new text in a fallback language.
- Keep `NSLOCTEXT` namespace/key identities and format placeholders consistent across translations. Gather, import and compile the Unreal localization target with `Scripts/Update-Localization.ps1`, and keep all three cultures staged in `Config/DefaultGame.ini`. Update `Docs/Localization.md` if the workflow changes.

# Changelog

- Keep `CHANGELOG.txt` player-facing: record only implemented additions, changes and fixes that remain in the resulting game. Describe their visible effect in plain language.
- Summarize the net result for each version. Consolidate related edits made during the same day or development period; if something was added and then removed before that version was released, omit both events.
- Do not include internal code, architecture, tooling, compiler/linker errors, Live Coding fixes, build/packaging procedures, formatting, verification reports, test status or development-session history. Keep those details in technical documentation or task reports when needed.
- Include fixes to player-visible bugs, describing the corrected behavior without implementation details. Do not claim planned or unfinished work as completed.
- When revising an entry, replace outdated wording instead of appending a contradictory record of intermediate steps.

# Release archive names

- Always package distributable Windows releases through `Scripts/Package-Release.ps1`.
- Name the game archive `<ProjectName>-<ProjectVersion>-<identifier>.zip` and the source archive `<ProjectName>-<ProjectVersion>-<identifier>-Project.zip` beside it. Read name/version from `Config/DefaultGame.ini`; the identifier is the unique Git short hash requested at eight characters. Do not invent different names or substitute dates/platform/configuration in these archive names.
- Commit changes before packaging so both archives match the identifier. The script rejects a dirty working tree, source changes during packaging and overwriting an existing release directory. `-Describe` prints names without building or writing files.
- Shipping packaging builds the game target with `-skipbuildeditor`; it must not rebuild or terminate the open editor. It uses existing editor modules for cook, so changes requiring a new editor module must be applied safely before packaging. The script never runs the game, Play or tests.

# Automatic code formatting

- After every batch of source-code edits, and always before the final response for a code-changing task, run `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Format-Code.ps1` from the project root. Do this without asking the user. Repeat if you edit source again afterward.
- This command normalizes source with pinned clang-format 21.1.8 (`.clang-format`), inserts structural blank lines with Uncrustify 0.83.0 (`.uncrustify.cfg`), then applies clang-format for the final layout. It covers project C/C++ headers, implementations and C# build rules under `Source`. Missing tools are installed into ignored `.tools` automatically. Explicit installers are `Scripts/Install-Uncrustify.ps1` and `Scripts/Install-Formatter.ps1`.
- Let the formatters handle whitespace, line wrapping and structural blank lines. Uncrustify separates variable declaration groups, control-flow statements, returns and function bodies; clang-format handles the final layout. Do not manually approximate these rules or pack functions, conditions or switch branches onto one line.
- Preserve Unreal include order, especially `.generated.h` last. Never format engine, third-party or generated build files.
- Formatting is a write step, not gameplay testing. No build, Play session or additional manual formatting review is required just for formatting. Optional read-only verification: `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Format-Code.ps1 -Check`.
- This is an agent workflow instruction, not an operating-system hook after every shell command. Commands that do not change source do not require formatting.

# Local AI/editor tools

- Project MCP connections: `unreal_epic` (Epic, local HTTP) and `ue_mcp_lyon` (db-lyon/ue-mcp 1.3.8, local stdio). See `Docs/AI-Integration.md`.
- Cloud 3D generation: the user-level `meshy-openai-plugin` and Meshy CLI. See `Docs/Meshy.md`. Installation, OAuth credentials, balances and task history are machine/account state and must not be committed.
- Prefer Epic tools for editor inspection, logs and Live Coding; use Lyon for additional authoring. Serialize calls to the editor across both connections and verify the active project before writes. Do not perform the same mutation through both servers.
- Use `MazeDiagnosticsToolset.ReadSnapshots` for ECS diagnostics. Snapshots are detached, world-scoped copies; client data is not authoritative server state. Never use generic Mass write tools to bypass the subsystem/gameplay systems.
- Insights captures and Play/tests remain opt-in. `Scripts/Export-Insights.ps1` exports an existing trace without launching gameplay. No plugin-provided workflow overrides this project's architecture or editor lifecycle rules.

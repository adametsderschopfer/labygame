# Unreal iteration workflow

- Prefer Live Coding for routine C++ changes while Unreal Editor is open. Do not run a normal Build.bat build against an open editor and then ask the user to close it merely because the DLL is locked.
- Live Coding is configured in Config/DefaultEditorPerProjectUserSettings.ini. In an already running editor it can be enabled under Editor Preferences > General > Live Coding. Ctrl+Alt+F11 triggers compilation; editor console commands are LiveCoding and LiveCoding.Compile.
- If no available tool can trigger compilation in the running editor, ask the user to press Ctrl+Alt+F11 instead of closing the project. Check the Live Coding logs before reporting success.
- Stop/restart Play to recreate the procedural maze or runtime HUD after patches when needed. This does not require restarting the editor.
- Use a full editor restart/build when changes to module/plugin dependencies, component types/construction, or reflected object layout cannot safely be applied through Live Coding. Explain the concrete reason instead of treating every header edit as requiring a restart.
- With the editor closed, normal builds and command-line automation tests are appropriate. Never terminate the user's editor to unlock files.

# Gameplay architecture

- Implement gameplay state and rules through Unreal Mass Entity ECS: player control, health/stamina, maze generation, progress and session state.
- Store runtime data in Mass fragments and put rules in systems orchestrated by UMazeECSSubsystem. Actors, controllers and HUD are engine adapters for input, CharacterMovement physics, camera, mesh/collision creation, UI and seed replication. Do not add parallel authoritative gameplay state to those adapters.
- Generate topology, mesh data, spawn positions and exit checks in ECS. AMazeWorld consumes immutable generation data; HUD only reads progress and never decides whether an exit was reached.
- Preserve per-world entity lifecycle and validate handles. Clear input when menus/focus change. New gameplay mechanics should extend fragments/systems rather than grow Actor logic.
- Do not run tests or Play unless the user requests it; the user is handling gameplay testing.

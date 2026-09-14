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

# Automatic code formatting

- After every batch of source-code edits, and always before the final response for a code-changing task, run `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Format-Code.ps1` from the project root. Do this without asking the user. Repeat if you edit source again afterward.
- This command normalizes source with pinned clang-format 21.1.8 (`.clang-format`), inserts structural blank lines with Uncrustify 0.83.0 (`.uncrustify.cfg`), then applies clang-format for the final layout. It covers project C/C++ headers, implementations and C# build rules under `Source`. Missing tools are installed into ignored `.tools` automatically. Explicit installers are `Scripts/Install-Uncrustify.ps1` and `Scripts/Install-Formatter.ps1`.
- Let the formatters handle whitespace, line wrapping and structural blank lines. Uncrustify separates variable declaration groups, control-flow statements, returns and function bodies; clang-format handles the final layout. Do not manually approximate these rules or pack functions, conditions or switch branches onto one line.
- Preserve Unreal include order, especially `.generated.h` last. Never format engine, third-party or generated build files.
- Formatting is a write step, not gameplay testing. No build, Play session or additional manual formatting review is required just for formatting. Optional read-only verification: `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Format-Code.ps1 -Check`.
- This is an agent workflow instruction, not an operating-system hook after every shell command. Commands that do not change source do not require formatting.

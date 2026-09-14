# Unreal iteration workflow

- Prefer Live Coding for routine C++ changes while Unreal Editor is open. Do not run a normal Build.bat build against an open editor and then ask the user to close it merely because the DLL is locked.
- Live Coding is configured in Config/DefaultEditorPerProjectUserSettings.ini. In an already running editor it can be enabled under Editor Preferences > General > Live Coding. Ctrl+Alt+F11 triggers compilation; editor console commands are LiveCoding and LiveCoding.Compile.
- If no available tool can trigger compilation in the running editor, ask the user to press Ctrl+Alt+F11 instead of closing the project. Check the Live Coding logs before reporting success.
- Stop/restart Play to recreate the procedural maze or runtime HUD after patches when needed. This does not require restarting the editor.
- Use a full editor restart/build when changes to module/plugin dependencies, component types/construction, or reflected object layout cannot safely be applied through Live Coding. Explain the concrete reason instead of treating every header edit as requiring a restart.
- With the editor closed, normal builds and command-line automation tests are appropriate. Never terminate the user's editor to unlock files.

# Localization

English (`en`) is the native target and default fallback for LABY. Russian (`ru`)
and Spanish (`es`) are also supported and staged in packaged builds. The game
settings page lets each player switch culture immediately and persists the choice
in GameUserSettings. In editor Play, the project uses Unreal's game localization
preview and reloads the selected compiled game resource. It does not change the
editor's process-wide language. PIE restores the editor's game preview when it
ends. The editor copy of the local game preference is saved explicitly because
Unreal does not write it through game localization preview.
Runtime UI, network messages, HUD and world labels use `FText` with stable
`NSLOCTEXT` namespace/key identities. Player names, room codes and seed identifiers
are culture-invariant data. Numeric UI values use Unreal's locale-aware formatting;
in editor Play, their number format follows the editor locale.

## Source conventions

- Author new visible text in English with `NSLOCTEXT("Maze.Feature", "StableKey", "Source text")`.
- Keep namespace/key pairs stable. Change the source text when wording changes;
  the gather pipeline will mark obsolete translations as needing an update.
- Use `FText::Format` with named arguments, for example `{Count}` or `{Name}`.
  Preserve these argument names in translations; translators may reorder them.
- Do not concatenate localized fragments or round-trip visible text through `FString`.
- Use `FText::AsCultureInvariant` only for external identifiers/names, not UI sentences.
- `Source/laby/Public/UI/MazeText.h` owns default Widget Blueprint labels.
  Runtime widgets apply these texts to existing assets. Editor asset repair updates
  the same labels when opening the project, preserving geometry and existing fonts.
- The gather target reads native source including editor-generated UI definitions.
  It intentionally does not gather duplicate, potentially outdated text from generated
  `/Game/UI` assets. If future content authors add independent Blueprint text, extend
  `Game_Gather.ini` with `GatherTextFromAssets` for those content folders.

## Update the catalogs

From the project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Update-Localization.ps1 -Action Update -Cultures en,ru,es
```

This runs Gather, Export and Compile through Unreal's GatherText commandlet.
Update preserves existing Russian and Spanish translations by stable key when the
original source text has not changed. Import aligns archive source text with the
manifest so compiled resources match the runtime `FText` identities. Update and
Compile report an error if Russian or Spanish has empty translations.
The commandlet disables the editor's ModelContextProtocol plugin so it does not
compete with the running editor for the same local HTTP port.
It does not start Play or gameplay tests. Pass `-EngineRoot` if Unreal is installed
elsewhere. Generated commandlet configs/logs live under ignored `Saved/Localization`.
Version the target configs and `Content/Localization/Game` data, including `.po`,
`.manifest`, `.archive`, `.locmeta` and `.locres` files.

## When player-visible text changes

Gather and export all supported cultures, translate every new or changed entry in
`en`, `ru` and `es`, then import and compile all three. Preserve `msgctxt` and
format placeholders. Import edited PO files before another export, since export
regenerates PO files from the archives. New text must not rely on a fallback
language in any of the three supported cultures. The map compass uses separate
localized short direction labels on the mini-map and full names on the large map.
When the original source changes for an existing key, check its Russian and Spanish
PO entries: Update leaves their `msgstr` empty for review. Translate them, then
run Import and Compile.

## Add another language (example: German)

1. Run `Scripts/Update-Localization.ps1 -Action Update -Cultures en,ru,es,de` from
   PowerShell (array arguments should be passed in the PowerShell session).
   Existing target culture directories are automatically included on later runs.
2. Translate `Content/Localization/Game/de/Game.po`; keep each `msgctxt` and all
   format argument names unchanged. Do not edit the English source strings in PO.
3. Import **before exporting again**, then compile:

   ```powershell
   ./Scripts/Update-Localization.ps1 -Action Import -Cultures en,ru,es,de
   ./Scripts/Update-Localization.ps1 -Action Compile -Cultures en,ru,es,de
   ```

   Export regenerates PO files from archives, so it can overwrite unimported PO edits.
4. Add `+CulturesToStage=de` under `[/Script/UnrealEd.ProjectPackagingSettings]`
   in `Config/DefaultGame.ini`. The ICU preset is already `All`.
5. Add the new culture to the selector in `MazeMenuSettings.cpp` and the Ward
   settings authoring code, then rebuild `WBP_WardSettings` in the editor.
6. Check long text, line wrapping and font coverage in the actual target language.
   Asian scripts may need additional font assets; right-to-left languages also
   need layout review. Localization-ready text does not guarantee font coverage.

The native English localization resource is loaded as the fallback for entries
missing in Russian or Spanish. A new entry absent from every catalog still uses
its source text, so author new source strings in English and keep all three
catalogs complete. An unavailable language choice also falls back to English.
UI text histories retain their localization identity when formatted, so
translations can reorder arguments without changing gameplay or server data.

Official references:
- https://dev.epicgames.com/documentation/unreal-engine/localization-overview-for-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/managing-the-active-culture-at-runtime

Development-tool exception: the backslash debug panel keeps en/ru/es labels inside
development-only compilation guards in the MazeDevelopment UI/adapter files. These labels
are not gathered into Game or staged locres; this satisfies the requirement that
no part of the debug menu enters the final game. Production UI continues using
the normal Game target. Update-Localization.ps1 is still run after removal of the
old NewMaze key label, refreshing all three production catalogs.

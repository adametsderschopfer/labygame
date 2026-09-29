# Localization

English (`en`) is the native target and default fallback for LABY. Russian (`ru`)
and Spanish (`es`) are also supported and staged in packaged builds. The game
settings page lets each player switch culture immediately and persists the choice
in GameUserSettings. In editor Play, the project also enables Unreal's game
localization preview for the selected culture; the engine does not load game
translations there from `SetCurrentCulture` alone. The local preference is saved
explicitly because Unreal skips `SetCurrentCulture`'s config write in the editor.
Runtime UI, network messages, HUD and world labels use `FText` with stable
`NSLOCTEXT` namespace/key identities. Player names, room codes and seed identifiers
are culture-invariant data. Numeric UI values use Unreal's locale-aware formatting.

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

Untranslated entries fall back to their source text, which is still Russian in
some older widgets. All supported cultures therefore need explicit catalog
coverage. UI text histories retain their localization identity when formatted,
so translations can reorder arguments without changing gameplay or server data.

Official references:
- https://dev.epicgames.com/documentation/unreal-engine/localization-overview-for-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/managing-the-active-culture-at-runtime

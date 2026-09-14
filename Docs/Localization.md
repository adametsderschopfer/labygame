# Localization

English (`en`) is the native language and default fallback for LABY.
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

## Update the English catalog

From the project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Update-Localization.ps1
```

This runs Gather, Export and Compile through Unreal's GatherText commandlet.
It does not start Play or gameplay tests. Pass `-EngineRoot` if Unreal is installed
elsewhere. Generated commandlet configs/logs live under ignored `Saved/Localization`.
Version the target configs and `Content/Localization/Game` data, including `.po`,
`.manifest`, `.archive`, `.locmeta` and `.locres` files.

## Add a language (example: German)

1. Run `Scripts/Update-Localization.ps1 -Action Update -Cultures en,de` from
   PowerShell (array arguments should be passed in the PowerShell session).
   Existing target culture directories are automatically included on later runs.
2. Translate `Content/Localization/Game/de/Game.po`; keep each `msgctxt` and all
   format argument names unchanged. Do not edit the English source strings in PO.
3. Import **before exporting again**, then compile:

   ```powershell
   ./Scripts/Update-Localization.ps1 -Action Import -Cultures en,de
   ./Scripts/Update-Localization.ps1 -Action Compile -Cultures en,de
   ```

   Export regenerates PO files from archives, so it can overwrite unimported PO edits.
4. Add `+CulturesToStage=de` under `[/Script/UnrealEd.ProjectPackagingSettings]`
   in `Config/DefaultGame.ini`. The ICU preset is already `All` to support future
   cultures. Only English translations are staged until other cultures are added.
5. Launch a standalone/packaged game with `-culture=de` to select it.
   For a future language selector, call
   `UKismetInternationalizationLibrary::SetCurrentCulture(LanguageTag, true)`;
   the second argument saves the preference. A selector is intentionally not
   shown while English is the only supported language.
6. Check long text, line wrapping and font coverage in the actual target language.
   Asian scripts may need additional font assets; right-to-left languages also
   need layout review. Localization-ready text does not guarantee font coverage.

Untranslated entries fall back to the native English source. UI text histories
retain their localization identity when formatted, so translations can reorder
arguments without changing gameplay or server data.

Official references:
- https://dev.epicgames.com/documentation/unreal-engine/localization-overview-for-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/managing-the-active-culture-at-runtime

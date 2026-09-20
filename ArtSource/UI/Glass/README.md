# Glass interface artwork

`MenuBackdrop.png` is the background-only menu plate, generated with the built-in
imagegen tool from the user's approved green laboratory HUD concept on 2026-09-20.
Live text, controls, health, stamina and maps are rendered by Unreal, not baked into this image.
Imported as `/Game/UI/Glass/T_MenuBackdrop` (sRGB, UI group, EditorIcon, never stream).

The two short WAV files are original deterministic synthesized interface effects,
created by `Scripts/create_ui_sounds.py`: a quiet air-like hover and a soft confirmation.
They contain no speech or third-party samples. Imported as `S_UIHover` and `S_UIPress`.

## Final background prompt

Use case: background-extraction.
Create a production game MAIN MENU BACKGROUND image, 16:9, ideally 2560x1440. Use the attached approved laboratory corridor HUD concept as visual source. Rebuild ONLY its environment as a cinematic calm background plate. Remove EVERY UI element: no HUD, minimap, crosshair, text, logos, labels, signs, letters or numbers anywhere.
Preserve the eye-level corridor intersection, warm-white small rectangular ceramic tiles, muted grey-green satin marbled linoleum floor, white mineral suspended ceiling and restrained rectangular fluorescent lights. Maintain the original scene's composition and colour, subtle clinical unease, sophisticated atmospheric lighting, no people, weapons, debris or monsters. Reflections subdued, not a flooded wet floor. Soft optical defocus across the image, like looking through lightly frosted glass, still recognisable architecture but no sharp tile noise. Near-black green shadows, grey-green highlights, no amber/orange/yellow. Make the left third and edges considerably darker for live text overlay, while central and right corridor remain softly discernible. This is a background-only render for a real interactive Unreal menu; absolutely no baked-in lettering or interface.

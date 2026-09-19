"""Import WASD Sound Stone steps as individual, non-looping Unreal SoundWaves."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()) / "SourceAudio" / "Footsteps"
tasks = []
for action in ("Walk", "Run", "Sneak"):
    for variant in range(1, 7):
        source = root / f"WASD Sound Stone {action} {variant:02}.wav"
        if not source.is_file():
            raise RuntimeError(f"Missing source: {source}")
        task = unreal.AssetImportTask()
        task.filename = str(source.resolve())
        task.destination_path = "/Game/Audio/Footsteps"
        task.destination_name = f"S_Stone_{action}_{variant:02}"
        task.automated = True
        task.replace_existing = True
        task.save = False
        task.factory = unreal.SoundFactory()
        tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    sound = unreal.load_asset(f"{task.destination_path}/{task.destination_name}")
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError(f"Import failed: {task.destination_name}")
    sound.set_editor_property("looping", False)
    sound.set_editor_property("volume", 1.0)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
        raise RuntimeError(f"Save failed: {task.destination_name}")
unreal.log("LABY_FOOTSTEPS_IMPORTED: 18 non-looping stone footsteps")

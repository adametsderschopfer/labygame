import unreal

# Run once with UnrealEditor-Cmd -run=pythonscript -script=...
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if not world:
    raise RuntimeError("Failed to create blank maze map")
if not unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/Maze"):
    raise RuntimeError("Failed to save maze map")
unreal.log("LABY_MAZE_MAP_SAVED")

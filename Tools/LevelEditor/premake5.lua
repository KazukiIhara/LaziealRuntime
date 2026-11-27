return function(runtimeDirectory, projectDirectory, outputDirectory, levelDataDirectory)
    local defineEditor = dofile(runtimeDirectory .. "/Tools/EditorProject.lua")
    defineEditor("LevelEditor", runtimeDirectory, projectDirectory, outputDirectory, "--level-data-directory", levelDataDirectory)

    includedirs { runtimeDirectory .. "/Tools/LevelEditor/Externals/ImGuizmo" }

    filter "files:**/ImGuizmo.cpp"
        disablewarnings { "4245" }
    filter {}
end

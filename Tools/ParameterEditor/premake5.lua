return function(runtimeDirectory, projectDirectory, outputDirectory, parameterDirectory)
    local defineEditor = dofile(runtimeDirectory .. "/Tools/EditorProject.lua")
    defineEditor("ParameterEditor", runtimeDirectory, projectDirectory, outputDirectory, "--parameter-directory", parameterDirectory)
end

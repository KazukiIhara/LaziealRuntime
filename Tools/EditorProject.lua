return function(editorName, runtimeDirectory, projectDirectory, outputDirectory, fileArgument, argumentPath)
    local editorDirectory = runtimeDirectory .. "/Tools/" .. editorName
    local graphicsProject = runtimeDirectory .. "/Dependencies/LaziealGraphicsFramework/Project"
    local sdlRoot = runtimeDirectory .. "/Dependencies/SDL3"
    local effekseerInstall = graphicsProject .. "/Externals/Effekseer/install_" ..
        ((_ACTION == "vs2022") and "msvc2022_x64" or "msvc2026_x64")

    local function projectPath(filePath)
        return "$(ProjectDir)" .. path.translate(path.getrelative(projectDirectory, filePath), "\\")
    end

    project ("Lazieal" .. editorName)
        kind "WindowedApp"
        language "C++"
        cppdialect "C++20"
        location (projectDirectory)
        characterset "Unicode"
        targetdir (outputDirectory .. "/" .. editorName)

        files {
            editorDirectory .. "/**.cpp",
            editorDirectory .. "/**.h",
            editorDirectory .. "/RuntimeSetting.ini",
        }

        includedirs {
            runtimeDirectory .. "/Project/Include",
            graphicsProject .. "/Include",
            graphicsProject .. "/Externals/imgui",
            graphicsProject .. "/Externals",
        }

        dependson {
            "LaziealRuntime",
            "LaziealGraphicsFramework",
            "DirectXTex",
            "DirectXMesh",
            "ImGui",
        }

        links {
            "LaziealRuntime",
            "LaziealGraphicsFramework",
            "DirectXTex",
            "DirectXMesh",
            "ImGui",
            "d3d12",
            "dxgi",
            "dxguid",
            "dxcompiler",
            "winmm",
            "ole32",
            "shell32",
            "SDL3",
        }

        libdirs {
            sdlRoot .. "/lib/x64",
            effekseerInstall .. "/lib",
        }

        defines { "NOMINMAX" }
        warnings "High"
        buildoptions { "/utf-8" }
        staticruntime "On"
        multiprocessorcompile "On"
        debugdir "%{cfg.targetdir}"
        if argumentPath then
            debugargs { fileArgument, '"' .. projectPath(argumentPath) .. '"' }
        end

        postbuildcommands {
            'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "$(TargetDir)dxcompiler.dll"',
            'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "$(TargetDir)dxil.dll"',
            'copy /Y "' .. projectPath(sdlRoot .. "/lib/x64/SDL3.dll") .. '" "$(TargetDir)SDL3.dll"',
            'copy /Y "' .. projectPath(editorDirectory .. "/RuntimeSetting.ini") .. '" "$(TargetDir)RuntimeSetting.ini"',
            'xcopy /E /I /Y /D "' .. projectPath(graphicsProject .. "/Assets") .. '" "$(TargetDir)Assets\\Framework"'
        }

        filter "configurations:Debug"
            defines { "DEBUG" }
            symbols "On"
            debugformat "C7"
            fatalwarnings { "All" }
            runtime "Debug"
            linkoptions { "/IGNORE:4049", "/IGNORE:4099" }
            libdirs { graphicsProject .. "/Externals/assimp/lib/Debug" }
            links { "assimp-vc143-mtd" }
            links {
                "EffekseerRendererDX12d",
                "EffekseerRendererLLGId",
                "EffekseerRendererCommond",
                "Effekseerd",
                "LLGId",
            }

        filter "configurations:Develop"
            defines { "DEBUG", "DEVELOP" }
            symbols "On"
            debugformat "C7"
            optimize "Debug"
            runtime "Release"
            linkoptions { "/IGNORE:4049", "/IGNORE:4099" }
            libdirs { graphicsProject .. "/Externals/assimp/lib/Release" }
            links { "assimp-vc143-mt" }
            links {
                "EffekseerRendererDX12",
                "EffekseerRendererLLGI",
                "EffekseerRendererCommon",
                "Effekseer",
                "LLGI",
            }

        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "On"
            runtime "Release"
            linkoptions { "/IGNORE:4049", "/IGNORE:4099" }
            libdirs { graphicsProject .. "/Externals/assimp/lib/Release" }
            links { "assimp-vc143-mt" }
            links {
                "EffekseerRendererDX12",
                "EffekseerRendererLLGI",
                "EffekseerRendererCommon",
                "Effekseer",
                "LLGI",
            }

        filter {}
end

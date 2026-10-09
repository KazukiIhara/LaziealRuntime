workspace "LaziealRuntime"
    configurations { "Debug", "Develop", "Release" }
    platforms { "x64" }
    startproject "LaziealParameterEditor"
    location "../"

    targetdir "../../generated/outputs/%{cfg.buildcfg}/%{cfg.platform}"
    objdir "../../generated/obj/%{prj.name}/%{cfg.buildcfg}"

local graphicsProject = "../../Dependencies/LaziealGraphicsFramework/Project"
local sdlRoot = "../../Dependencies/SDL3"

externalproject "DirectXTex"
    location (graphicsProject)
    uuid "A90EE592-95C6-26E0-FECD-FF3BEAA4C1D0"
    kind "StaticLib"
    language "C++"

externalproject "ImGui"
    location (graphicsProject)
    uuid "C0FF640D-2C14-8DBE-F595-301E616989EF"
    kind "StaticLib"
    language "C++"

externalproject "DirectXMesh"
    location (graphicsProject)
    uuid "E50C83EF-51C2-FBE4-DAB6-F5BB466BF2E8"
    kind "StaticLib"
    language "C++"

externalproject "LaziealGraphicsFramework"
    location (graphicsProject)
    uuid "068AFE72-F2AE-4DF3-1BFA-3283077E4C11"
    kind "StaticLib"
    language "C++"

project "LaziealRuntime"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "../"
    characterset "Unicode"

    files {
        "../Src/**.cpp",
        "../Src/**.h",
        "../Include/**.h",
    }

    includedirs {
        "../Src",
        "../Include",
        "../../Dependencies/LaziealGraphicsFramework/Project/Include",
        "../../Dependencies/LaziealGraphicsFramework/Project/Externals",
        "../../Dependencies/SDL3/include",
    }

    defines { "NOMINMAX" }
    warnings "High"
    buildoptions { "/utf-8" }
    staticruntime "On"
    multiprocessorcompile "On"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        fatalwarnings { "All" }
        runtime "Debug"

    filter "configurations:Develop"
        defines { "DEBUG", "DEVELOP" }
        symbols "On"
        optimize "Debug"
        runtime "Release"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        runtime "Release"

    filter {}

project "LaziealParameterEditor"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++20"
    location "../"
    characterset "Unicode"
    targetdir "../../generated/outputs/%{cfg.buildcfg}/%{cfg.platform}/ParameterEditor"

    files {
        "../../Tools/ParameterEditor/**.cpp",
        "../../Tools/ParameterEditor/**.h",
        "../../Tools/ParameterEditor/RuntimeSetting.ini",
    }

    includedirs {
        "../Include",
        graphicsProject .. "/Include",
        graphicsProject .. "/Externals/imgui",
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
        "SDL3",
    }

    libdirs { sdlRoot .. "/lib/x64" }

    defines { "NOMINMAX" }
    warnings "High"
    buildoptions { "/utf-8" }
    staticruntime "On"
    multiprocessorcompile "On"
    debugdir "%{cfg.targetdir}"

    postbuildcommands {
        'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "$(TargetDir)dxcompiler.dll"',
        'copy "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "$(TargetDir)dxil.dll"',
        'copy /Y "$(ProjectDir)..\\Dependencies\\SDL3\\lib\\x64\\SDL3.dll" "$(TargetDir)SDL3.dll"',
        'copy /Y "$(ProjectDir)..\\Tools\\ParameterEditor\\RuntimeSetting.ini" "$(TargetDir)RuntimeSetting.ini"',
        'xcopy /E /I /Y /D "$(ProjectDir)..\\Dependencies\\LaziealGraphicsFramework\\Project\\Assets" "$(TargetDir)Assets\\Framework"'
    }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        fatalwarnings { "All" }
        runtime "Debug"
        linkoptions { "/IGNORE:4049", "/IGNORE:4099" }
        libdirs { graphicsProject .. "/Externals/assimp/lib/Debug" }
        links { "assimp-vc143-mtd" }

    filter "configurations:Develop"
        defines { "DEBUG", "DEVELOP" }
        symbols "On"
        optimize "Debug"
        runtime "Release"
        linkoptions { "/IGNORE:4049", "/IGNORE:4099" }
        libdirs { graphicsProject .. "/Externals/assimp/lib/Release" }
        links { "assimp-vc143-mt" }

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        runtime "Release"
        linkoptions { "/IGNORE:4049", "/IGNORE:4099" }
        libdirs { graphicsProject .. "/Externals/assimp/lib/Release" }
        links { "assimp-vc143-mt" }

    filter {}

workspace "LaziealRuntime"
    configurations { "Debug", "Develop", "Release" }
    platforms { "x64" }
    startproject "LaziealParameterEditor"
    location "../"

    targetdir "../../generated/outputs/%{cfg.buildcfg}/%{cfg.platform}"
    objdir "../../generated/obj/%{prj.name}/%{cfg.buildcfg}"

local graphicsProject = "../../Dependencies/LaziealGraphicsFramework/Project"

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
        debugformat "C7"
        fatalwarnings { "All" }
        runtime "Debug"

    filter "configurations:Develop"
        defines { "DEBUG", "DEVELOP" }
        symbols "On"
        debugformat "C7"
        optimize "Debug"
        runtime "Release"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        runtime "Release"

    filter {}

local runtimeDirectory = path.getabsolute("../..")
local defineParameterEditor = dofile(runtimeDirectory .. "/Tools/ParameterEditor/premake5.lua")
defineParameterEditor(
    runtimeDirectory,
    path.getabsolute(".."),
    "../../generated/outputs/%{cfg.buildcfg}/%{cfg.platform}")

local defineLevelEditor = dofile(runtimeDirectory .. "/Tools/LevelEditor/premake5.lua")
defineLevelEditor(
    runtimeDirectory,
    path.getabsolute(".."),
    "../../generated/outputs/%{cfg.buildcfg}/%{cfg.platform}")

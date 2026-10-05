workspace "LaziealRuntime"
    configurations { "Debug", "Develop", "Release" }
    platforms { "x64" }
    startproject "LaziealRuntime"
    location "../"

    targetdir "../../generated/outputs/%{cfg.buildcfg}/%{cfg.platform}"
    objdir "../../generated/obj/%{prj.name}/%{cfg.buildcfg}"

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

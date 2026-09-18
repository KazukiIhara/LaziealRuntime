# LaziealRuntime

LaziealRuntime is the application layer for LaziealGraphicsFramework. It owns the
window, input, frame loop, timing, and the entry point while the graphics backend
is supplied as a Git submodule.

## Clone

```powershell
git clone --recursive <repository-url>
```

If the repository was cloned without `--recursive`, initialize dependencies with:

```powershell
git submodule update --init --recursive
```

## Generate and build

Run `Project/Premake.bat`, then build `Project/LaziealRuntime.slnx` for x64.
The default development configuration is `Develop`.

## Public API

Include `LGF/LGF.h` from `Project/Include`. Applications provide
`ConfigureRuntime()` and `Main()`; the runtime owns `WinMain` and calls them.

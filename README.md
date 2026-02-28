# FastMagStream

Real-time magnified screen capture for Windows using the DXGI Desktop Duplication API (`D3D11`/`DXGI`) with `GDI` presentation. The project is structured around a reusable core library that owns capture, frame transfer, and present flow, while allowing optional extension points.

## Library Approach

The core engine exposes a single capture loop and a callback hook for optional post-processing. The hook runs after frame data is prepared in memory and before the frame is presented, so custom drawing or processing can be injected without changing the default render path.

## Architecture Diagram

```mermaid
flowchart LR
    UI["Win32 UI Thread<br/>Window + Message Loop"]
    CAP["Capture Thread"]

    D3D["D3D11<br/>Device + DeviceContext"]
    DXGI["DXGI Output Duplication<br/>AcquireNextFrame"]
    STAGE["Staging Texture<br/>GPU -> CPU Readback"]
    HOOK["Optional Hook<br/>Overlay Callback"]
    GDI["GDI<br/>DIB + StretchBlt"]
    WIN["FastMagStream Window"]

    UI --> CAP
    CAP --> D3D --> DXGI --> STAGE --> HOOK --> GDI --> WIN
    UI -."stop signal".-> CAP
```

## Configuration (TOML)

Configuration is mandatory and must be passed using `--config <path>`.

Required keys:

- `display_width`
- `display_height`
- `record_width`
- `record_height`
- `zoom_factor`
- `frames_per_second`

Example `fastmagstream.toml`:

```toml
display_width = 1920
display_height = 1080
record_width = 2560
record_height = 1440
zoom_factor = 2.0
frames_per_second = 60
```

Run:

```powershell
.\FastMagStream.exe --config .\fastmagstream.toml
```

## Build

- Solution: `FastMagStream.slnx` (includes executable + core library projects)
- Dependencies: Windows SDK (`d3d11.lib`, `dxgi.lib`) and vendored `toml++` header
- Platform: Windows (Desktop Duplication requires Windows 8+)

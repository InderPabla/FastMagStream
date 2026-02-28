# FastMagStream

Real-time magnified screen capture for Windows using the DXGI Desktop Duplication API (`D3D11`/`DXGI`) with `GDI` presentation. The project is structured around a reusable core library that owns capture, frame transfer, and present flow, while allowing optional extension points.

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

Optional:

- `behaviour`: overlay mode, e.g. `"crosshairs"` to draw centre crosshairs; omit or leave empty for no overlay.

Example `fastmagstream.toml`:

```toml
display_width = 1920
display_height = 1080
record_width = 2560
record_height = 1440
zoom_factor = 2.0
frames_per_second = 60
# behaviour = "crosshairs"
```

Run:

```powershell
.\FastMagStream.exe --config .\fastmagstream.toml
```

## Zoom Showcase

| Normal Zoom | 2x Zoom |
|---|---|
| <img src="./Example/NormalZoom.png" alt="Normal Zoom" width="360" /> | <img src="./Example/Zoom2x.png" alt="2x Zoom" width="360" /> |

| Bino Normal Zoom | Bino 2x Zoom |
|---|---|
| <img src="./Example/Bino_NormalZoom.png" alt="Bino Normal Zoom" width="360" /> | <img src="./Example/Bino_Zoom2x.png" alt="Bino 2x Zoom" width="360" /> |

| Bino Normal Zoom | Bino 2x Zoom |
|---|---|
| <img src="./Example/Bino_NormalZoom.gif" alt="Bino Normal Zoom GIF" width="360" /> | <img src="./Example/Bino_Zoom2x.gif" alt="Bino 2x Zoom GIF" width="360" /> |

| Non affected frame rate |
|---|
| <img src="./Example/NonAffectedSourceFramerate.png" alt="Non affected frame rate" width="360" /> |

## Build

- Solution: `FastMagStream.slnx` (includes executable + core library projects)
- Dependencies: Windows SDK (`d3d11.lib`, `dxgi.lib`) and vendored `toml++` header
- Platform: Windows (Desktop Duplication requires Windows 8+)

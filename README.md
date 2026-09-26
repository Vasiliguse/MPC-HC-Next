# MPC-HC Next

**MPC-HC Next** is a Windows audio and video player project focused on a modern interface, native hardware-accelerated playback, HDR-aware rendering and a clean, low-latency playback pipeline.

The project is a modified and actively developed continuation of the Media Player Classic codebase. It retains the upstream GPLv3 licensing and attribution while introducing the **MPC-HC Next** product identity and new renderer/UI work.

## Project status

MPC-HC Next is currently under active development. The repository should be treated as a development tree rather than a final stable release.

### Current development areas

- DirectShow playback architecture inherited from the Media Player Classic codebase
- FFmpeg-based media processing
- DXVA2 and hardware-decoding paths
- Native D3D11 decoder/renderer integration
- D3D11 video processing and presentation
- HDR10 signalling and metadata handling
- 10/12-bit video formats and GPU surfaces
- ASS/SSA and other subtitle infrastructure
- D3D11 subtitle composition pipeline
- Modern MPC-HC Next/N Play-oriented interface work
- Windows 10/11 compatibility and x64 builds

### Renderer work

The native D3D11 renderer is being developed as a first-class playback path. Current work includes:

- D3D11 device and adapter management
- DXGI swap-chain creation
- SDR and HDR10 output paths
- D3D11 video processor integration
- native D3D11 decoder sample presentation
- device-loss handling
- frame synchronization and pending-frame management
- subtitle overlay integration
- HDR metadata propagation

HLG, HDR-aware subtitle composition, advanced scaling/shader processing, software-surface upload fallback and full playback validation remain development areas.

## Supported platform

The current project configuration targets modern Windows systems and provides Win32/x64 build configurations.

For development, use:

- Visual Studio 2022 or newer
- Windows SDK compatible with the selected Visual Studio toolset
- Git submodules initialized
- a Windows development environment capable of building the supplied Visual Studio solutions

## Repository layout

| Directory | Purpose |
|---|---|
| `src/apps` | Player application and Windows UI |
| `src/filters` | DirectShow filters, decoders, parsers and renderers |
| `src/SubPic` | Subtitle rendering infrastructure |
| `src/Subtitles` | Subtitle formats and processing |
| `src/ExtLib` | Third-party and supporting libraries |
| `src/Shaders` | Pixel/shader resources |
| `include` | Public/shared headers |
| `distrib` | Installer and distribution resources |
| `docs` | Project documentation |

## Building

Initialize the repository submodules and open the appropriate Visual Studio solution for the desired configuration.

Typical configurations include:

- Debug Win32
- Debug x64
- Release Win32
- Release x64
- Release Filter Win32
- Release Filter x64

The CI configuration in `.github/workflows` is the authoritative automated build configuration for the repository.

## Branding

The application-facing identity is:

**MPC-HC Next**

The legacy MPC-BE name may still occur in internal filenames, project paths or preserved upstream/legal material. These identifiers are not used as the product's user-facing branding.

## Third-party software

MPC-HC Next uses a number of third-party components, including FFmpeg and other libraries retained from the upstream codebase. Their individual licenses and attribution requirements remain applicable.

See the repository license files and the corresponding third-party source directories for details.

## License

MPC-HC Next is distributed under the **GNU General Public License version 3**.

The project is a modified work based on an existing Media Player Classic codebase. Upstream copyright and attribution notices are intentionally retained where required.

## Development

The main development branch currently used for renderer work is:

`next/phase-2-renderer-foundation`

The project is developed incrementally: branding/UI, renderer foundations, playback stability, HDR/color handling, subtitle composition and final release validation are treated as separate workstreams.

## Project

GitHub: https://github.com/Vasiliguse/MPC-HC-Next

# Changelog

All notable public BoatInfo changes are documented here.

## 1.1.0 — 2026-09-01

First public BoatInfo release.

### Added

- Responsive bottom-docked own-vessel information strip for OpenCPN.
- Automatic discovery of scalar Signal K own-vessel values observed through OpenCPN plugin messaging.
- OpenCPN navigation values including position, COG, SOG and heading.
- Signal K categories derived directly from provider paths without a BoatInfo-specific taxonomy.
- User-selectable visibility and `Primary` / `Secondary` / `Detail` responsive priority.
- Editable user-facing labels.
- `Text` presentation as the stable 1.1.0 display mode.
- Apply/Anwenden action for live preference updates.
- Per-vessel profiles keyed by Signal K `self`.
- Signal K vessel identity for name, MMSI and VHF call sign with manual fallback.
- Robust Signal K identity parsing for leaf updates, pathless/root identity objects and full root vessel models.
- Conservative own-vessel context handling so foreign vessel identity cannot replace own-vessel identity.
- Focused Signal K parser tests.
- DAY/DUSK/NIGHT host color integration and DPI-aware rendering.

### Normalization

- Speed to knots.
- Angular quantities to degrees.
- Kelvin to degrees Celsius.
- State-of-charge and tank ratios to percent.
- Remaining time to hours.
- Coulomb charge/discharge values to ampere-hours.
- Revolutions per second to rpm.
- OpenCPN latitude/longitude through the nautical coordinate formatter.

### Design

- Text-first maritime HMI rather than simulated analogue instruments.
- Stable fixed-width value cells per responsive density level.
- Category divider lines and coherent category blocks.
- Full / Compact / Minimal responsive modes.
- Detail values are omitted before Secondary values when space is limited.
- No scrolling required in the live navigation view.

### Validated

- OpenCPN 5.14.0 on Windows.
- MSVC Win32/x86 package build.
- Live and synthetic Signal K data.
- Single-chart and OpenCPN split-screen layouts.
- Wide, narrow and portrait-like OpenCPN window sizes.

### Known limitations

- The supported public 1.1.0 package is currently the validated Windows/MSVC x86 build.
- Linux, macOS, Android and Flatpak scaffolding inherited from the original FrontEnd2 template is not currently release-validated.
- BoatInfo discovers Signal K values from the live stream; it does not query a Signal K server for a complete path/metadata catalog.
- `Digital` and `Digital + Analog` presentation modes are reserved for later releases and are not implemented in 1.1.0.
- BoatInfo is not type-approved navigation equipment and is not a replacement for mandatory instruments or alarms.

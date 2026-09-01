# BoatInfo

BoatInfo is a compact OpenCPN plugin for **own-vessel information**. It generalizes the original BenchyNav prototype so the plugin is useful on any vessel rather than being tied to one boat.

## Stable 1.1.0 product shape

BoatInfo 1.1.0 uses a deliberately **text-first responsive own-vessel status strip** docked below the OpenCPN chart area.

The live view is intentionally simple:

- no simulated analogue gauges;
- no circular dials or needles;
- no decorative Level/Tape/Trend graphics in 1.1.0;
- selected measurements are rendered as label + value + explicit unit;
- categories remain spatially coherent and use full-width divider lines;
- value cells use fixed widths per responsive density level so live updates and resizing do not continuously reflow columns;
- category blocks may wrap to additional rows, but values remain left-aligned inside a stable grid;
- the live navigation view never requires scrolling;
- lower-priority values are omitted when the available pane becomes too small.

Future presentation modes are intentionally outside the 1.1.0 scope:

1. `Text` — stable baseline;
2. `Digital` — later DevKit-native Value/Level/Tape/Trend presentation where the information task justifies it;
3. `Digital + Analog` — optional later presentation only where an analogue relationship adds real operational value.

## Configurable vessel values

BoatInfo builds a catalog from scalar own-vessel values it actually observes. The current implementation uses:

- OpenCPN position-fix values such as latitude, longitude, COG, SOG and heading;
- Signal K own-vessel delta values received through OpenCPN plugin messaging.

Previously unknown numeric, text and boolean Signal K paths can therefore appear in BoatInfo preferences without adding path-specific UI code.

For Signal K values, the provider path is authoritative for the information structure:

- the first path component becomes the category;
- remaining path components become the suggested human-readable name;
- camelCase components are humanized without inventing an additional BoatInfo taxonomy.

Examples:

- `environment.wind.angleApparent` -> category `ENVIRONMENT`, suggested name `Wind Angle Apparent`;
- `environment.wind.speedTrue` -> category `ENVIRONMENT`, suggested name `Wind Speed True`;
- `environment.water.temperature` -> category `ENVIRONMENT`, suggested name `Water Temperature`;
- `environment.depth.belowTransducer` -> category `ENVIRONMENT`, suggested name `Depth Below Transducer`.

In **BoatInfo preferences** the user controls:

- **Show** — whether the value belongs to the live BoatInfo view;
- **Priority** — `Primary`, `Secondary` or `Detail` for responsive omission;
- **Name** — editable suggested label;
- **Presentation** — `Text` for the stable 1.1.0 dashboard;
- source/path information for diagnosis and source selection.

The stable live renderer and the visible Preferences presentation are text-only. Legacy `Value / Level / Tape / Trend / No display` primitive profile fields created during development remain readable only for migration compatibility and do not control the live renderer or visible settings. The model reserves `Digital` and `Digital + Analog` for later dedicated work, but neither mode is exposed or implemented in 1.1.0.

The **Apply / Anwenden** action updates the live view without closing preferences.

## Vessel identity and profiles

Vessel identity is treated separately from ordinary measurements.

BoatInfo uses Signal K own-vessel identity when available:

- `name`;
- `mmsi`;
- `communication.callsignVhf`.

Manual name, MMSI and call sign are fallback values only. Signal K remains authoritative when it supplies the corresponding identity.

Configuration is stored per Signal K `self` vessel identifier. The same OpenCPN installation can therefore move between vessels and restore the appropriate visibility, labels and priorities for each vessel profile.

Identity input accepts the normal Signal K leaf paths as well as pathless
(`path: ""`) vessel objects and full root models containing the own vessel.
BoatInfo ignores identity values from a foreign vessel `context`. An MMSI may be
used from an explicit string or numeric `mmsi` value; as a fallback it is
derived from `self` only when that identifier contains an unambiguous complete
nine-digit MMSI component. UUID digits are never treated as an MMSI.

The focused parser checks cover these representative inputs:

- standard `name`, `mmsi` and `communication.callsignVhf` leaf deltas;
- a pathless root identity delta;
- a nested `communication.callsignVhf` value;
- a foreign vessel context, which is rejected;
- a `self` identifier with an exact nine-digit MMSI fallback;
- a UUID-based `self`, which does not produce a synthetic MMSI.

## Responsive layout

BoatInfo is bottom-docked because the displayed data belongs to the vessel rather than to one chart canvas. OpenCPN single- and split-screen chart layouts therefore remain above the same vessel-global status strip.

The renderer chooses between three density levels according to the available width and height:

- **Full**;
- **Compact**;
- **Minimal**.

Each level has a fixed value-cell width. Categories determine block geometry from their contained value count; the final category boundary on a row may extend to the right edge, while individual value cells stay fixed-width and left-aligned.

If all selected values cannot fit, `Detail` values are omitted first, then `Secondary`; `Primary` values are protected as long as possible. The live view shows a small `+N hidden` indication when selected values are omitted because of space.

## Units and normalization

Signal K values use SI semantics where recognized. BoatInfo normalizes common measurements for presentation, including speed to knots, angular quantities to degrees, temperature from Kelvin to degrees Celsius, state-of-charge and tank-level ratios to percent, remaining time from seconds to hours, battery cumulative charge/discharge from Coulomb to ampere-hours and revolutions per second to rpm.

OpenCPN latitude and longitude are formatted through OpenCPN's nautical coordinate formatter rather than shown as bare decimal degrees. A numeric zero is a valid measurement and is never treated as missing data.

## Validity

BoatInfo never presents a stale or unusable measurement as a current numeric value. A current valid measurement shows label, value and unit. When a measurement is no longer usable/current, its fixed cell remains in place, while the numeric value and unit disappear so the surrounding dashboard geometry does not jump.

The normalized model reserves the DevKit validity vocabulary `valid`, `stale`, `no data`, `invalid`, `out of range` and `unknown`. Provider-specific classification can be extended without changing the dashboard layout contract.

## Maritime HMI and DevKit alignment

BoatInfo follows the shared `jkuhnen/opencpn-plugin-devkit` design guidance:

- present vessel data as information rather than pictures of instruments;
- keep chart/navigation surfaces primary;
- use compact persistent vessel context rather than covering chart hazards or controls;
- keep value, unit and reference qualifier explicit;
- keep related values spatially grouped;
- maintain stable geometry during live updates;
- preserve information priority under resize;
- use OpenCPN host colors and respond to DAY/DUSK/NIGHT scheme changes;
- use DPI-aware logical dimensions;
- keep normal measurements neutral;
- never infer alarm/warning semantics merely from a numeric value;
- never treat zero as missing data;
- never leave an old value visually indistinguishable from current data.

The OpenCPN host owns the surrounding AUI/chart/status-bar chrome. BoatInfo does not draw over host separators merely to force visual symmetry with the status bar.

These maritime HMI references are **design guidance only**. BoatInfo is not claimed to be ECDIS, type-approved, IMO/IEC compliant, or suitable as mandatory carriage equipment.

## Architecture

The implementation keeps major responsibilities separable:

- OpenCPN/plugin lifecycle, data normalization and configuration in `src/boatinfo_core.cpp`;
- vessel identity/plugin metadata in `src/boatinfo_identity.cpp`;
- responsive rendering in `src/boatinfo_dashboard.cpp`.

The dashboard receives normalized presentation values rather than parsing Signal K in its paint path.

Discovery currently means **values observed in the live data stream**. BoatInfo does not query a Signal K server separately for its complete metadata/path catalog.

## Build identity

Plugin name: `BoatInfo`

Package identifier: `boatinfo_pi`

Version: `1.1.0`

Target OpenCPN plugin API: `1.18`

### Windows local package build

Run from a Visual Studio Developer Command Prompt with the OpenCPN wxWidgets build available to CMake:

```bat
bld.bat
```

Equivalent explicit commands:

```bat
rmdir /s /q build-test 2>nul
cmake -S . -B build-test -G "Visual Studio 18 2026" -A Win32 -DOCPN_TARGET=MSVC
cmake --build build-test --target package --config RelWithDebInfo
```

No machine-specific path is stored in the build script.

## 1.1.0 runtime validation

The stable text dashboard has been exercised in real OpenCPN with live/synthetic Signal K data for:

- automatic discovery of previously unknown scalar Signal K paths;
- own-vessel identity and per-vessel profile behavior;
- mixed OpenCPN + Signal K sources;
- preferences and Apply/Anwenden interaction;
- bottom docking;
- OpenCPN split-screen behavior;
- wide, narrow and portrait-like window geometries;
- Full / Compact / Minimal responsive transitions;
- fixed value-cell geometry and category wrapping;
- category lines extending to the right edge;
- text-only display of navigation, propulsion, electrical and environment values;
- common SI conversions and explicit units.

## Legacy build infrastructure

The repository still contains inherited FrontEnd2-era CI/platform scaffolding for Android, Debian, Flatpak, macOS and related packaging. It is **not part of the currently validated BoatInfo 1.1.0 Windows build path** and should not be interpreted as a statement that those targets are currently supported or tested. Cleanup or re-validation of that infrastructure should happen in a separate dedicated change.

## Development workflow

Changes start from a GitHub issue where appropriate, use a dedicated branch and arrive through a pull request. See the repository `AGENTS.md` and the shared DevKit before editing.

## Repository history

This repository started as BenchyNav on top of the community `testplugin`/FrontEnd2 scaffold. BoatInfo was refactored in place so the proven OpenCPN integration could be retained while the active plugin code and product identity were modernized. Some inherited build/CI scaffolding remains intentionally isolated pending a separate cleanup decision.

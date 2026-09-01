# BoatInfo

BoatInfo is a compact OpenCPN plugin for **own-vessel information**. It generalizes the original BenchyNav prototype so the plugin is useful on any vessel rather than being tied to one boat.

## Stable v1 product shape

The first stable BoatInfo presentation is deliberately a **text-first responsive own-vessel status strip** docked below the OpenCPN chart area.

The live view is intentionally simple:

- no simulated analogue gauges;
- no circular dials or needles;
- no decorative Level/Tape/Trend graphics in v1;
- selected measurements are rendered as label + value + explicit unit;
- categories remain spatially coherent and use full-width divider lines;
- value cells use fixed widths per responsive density level so live updates and resizing do not continuously reflow columns;
- category blocks may wrap to additional rows, but values remain left-aligned inside a stable grid;
- the live navigation view never requires scrolling;
- lower-priority values are omitted when the available pane becomes too small.

Future presentation modes are intentionally outside the stable v1 scope. The product direction is:

1. `Text` — the stable v1 baseline;
2. `Digital` — later DevKit-native Value/Level/Tape/Trend presentation where the information task justifies it;
3. `Digital + Analog` — optional later presentation only where an analogue relationship adds real operational value.

The stable text baseline is retained even after richer modes are introduced.

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
- source/path information for diagnosis and source selection.

The current live renderer is text-only. Legacy `Value / Level / Tape / Trend` profile fields created during PR development remain readable for migration compatibility but do not control the stable v1 live renderer. They will be replaced by the explicit `Text / Digital / Digital + Analog` presentation selector in a later dedicated change.

The **Apply / Anwenden** action updates the live view without closing preferences.

## Vessel identity and profiles

Vessel identity is treated separately from ordinary measurements.

BoatInfo uses Signal K own-vessel identity when available:

- `name`;
- `mmsi`;
- `communication.callsignVhf`.

Manual name, MMSI and call sign are fallback values only. Signal K remains authoritative when it supplies the corresponding identity.

Configuration is stored per Signal K `self` vessel identifier. The same OpenCPN/Surface installation can therefore move between vessels and restore the appropriate visibility, labels and priorities for each vessel profile.

## Responsive layout

BoatInfo is bottom-docked because the displayed data belongs to the vessel rather than to one chart canvas. OpenCPN single- and split-screen chart layouts therefore remain above the same vessel-global status strip.

The renderer chooses between three density levels according to the available width and height:

- **Full**;
- **Compact**;
- **Minimal**.

Each level has a fixed value-cell width. Categories determine block geometry from their contained value count; the final category boundary on a row may extend to the right edge, while individual value cells stay fixed-width and left-aligned.

If all selected values cannot fit:

1. `Detail` values are omitted first;
2. then `Secondary` values;
3. `Primary` values are protected as long as possible.

The live view shows a small `+N hidden` indication when selected values are omitted because of space.

## Units and normalization

Signal K values use SI semantics where recognized. BoatInfo normalizes common measurements for presentation, including:

- speed to knots;
- angular quantities to degrees;
- temperature from Kelvin to degrees Celsius;
- state-of-charge and tank-level ratios to percent;
- remaining time from seconds to hours;
- battery cumulative charge/discharge from Coulomb to ampere-hours;
- revolutions per second to rpm.

OpenCPN latitude and longitude are formatted through OpenCPN's nautical coordinate formatter rather than shown as bare decimal degrees.

A numeric zero is a valid measurement and is never treated as missing data.

## Validity

BoatInfo never presents a stale or unusable measurement as a current numeric value.

For the stable text renderer:

- a current valid measurement shows label, value and unit;
- when the measurement is no longer usable/current, its fixed cell remains in place so the dashboard geometry does not jump;
- the numeric value and unit disappear;
- the remaining label changes visual treatment, so loss of the current value is signalled both by the missing value and by styling rather than by color alone.

The normalized model reserves the DevKit validity vocabulary `valid`, `stale`, `no data`, `invalid`, `out of range` and `unknown`. The stable acquisition path already distinguishes current/stale/unavailable operationally; finer provider-specific invalid/out-of-range/unknown classification can be extended without changing the dashboard layout contract.

## Maritime HMI and DevKit alignment

BoatInfo follows the shared `jkuhnen/opencpn-plugin-devkit` design guidance.

The stable v1 baseline follows these principles:

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

The implementation keeps the major responsibilities separable:

- OpenCPN/plugin lifecycle, data normalization and configuration in the plugin/core layer;
- vessel identity handling separately;
- responsive rendering in the dashboard layer.

The dashboard receives normalized presentation values rather than parsing Signal K in its paint path.

Discovery currently means **values observed in the live data stream**. BoatInfo does not yet query a Signal K server separately for its complete metadata/path catalog.

## Build identity

Plugin name: `BoatInfo`

Package/common identifier: `boatinfo`

Version: `1.1.0`

Target OpenCPN plugin API: `1.18`

The existing FrontEnd2-compatible build infrastructure is retained to avoid mixing product work with an unrelated build-system migration.

## Stable-v1 runtime validation

The PR development cycle has already been exercised in real OpenCPN with live/synthetic Signal K data for:

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

Before merging/releasing the stable version, perform one final Windows package build and smoke test of the current head, including DAY/DUSK/NIGHT and a stale/no-data transition after the final DevKit-alignment changes.

## Development workflow

Changes start from a GitHub issue, use a dedicated branch and arrive through a pull request. See the repository `AGENTS.md` and the shared DevKit before editing.

## Repository history

This repository started as BenchyNav on top of the community `testplugin`/FrontEnd2 scaffold. BoatInfo was refactored in place so the proven OpenCPN integration could be retained while obsolete scaffold code and resources were removed.

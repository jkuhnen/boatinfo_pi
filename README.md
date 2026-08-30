# BoatInfo

BoatInfo is a compact OpenCPN sidebar plugin for **own-vessel information**. It generalizes the original BenchyNav prototype so the plugin is useful on any boat rather than being tied to one vessel.

## Configurable vessel values

BoatInfo builds a catalog from scalar own-vessel values it actually observes. The first implementation uses:

- OpenCPN position-fix values such as latitude, longitude, COG, SOG and heading;
- Signal K own-vessel delta values received through OpenCPN plugin messaging.

Previously unknown numeric, text and boolean Signal K paths can therefore appear in BoatInfo preferences without adding path-specific UI code.

In **BoatInfo preferences** each observed value has two independent presentation choices:

- **Show** decides whether the value appears in the BoatInfo panel at all;
- **Display** selects `Value`, `Level`, `Tape`, `Trend` or `No display`.

`No display` keeps the selected value as a compact text row without a graphical instrument. The automatically suggested name remains editable in every mode.

Observed values are grouped into navigation, electrical, tanks, propulsion, environment, vessel and technical categories. Known paths receive semantic names such as `Service battery voltage` and `Starter battery voltage` rather than generic leaf names like `Voltage`.

When OpenCPN and Signal K expose the same primary navigation quantity, BoatInfo treats the Signal K path as an alternative source and keeps it hidden by default while leaving it available in preferences.

The selection is stored in the OpenCPN plugin configuration and restored after restart. User-edited labels are preserved while automatically generated labels can improve across BoatInfo versions.

## Units and validity

Signal K values use SI semantics where the path is recognized. BoatInfo converts common values for presentation, including:

- speed to knots;
- angular quantities to degrees;
- temperature from Kelvin to degrees Celsius;
- state-of-charge and tank-level ratios to percent;
- remaining time from seconds to hours;
- battery cumulative charge/discharge from Coulomb to ampere-hours;
- revolutions per second to rpm.

OpenCPN latitude and longitude are formatted through OpenCPN's nautical coordinate formatter rather than shown as a bare decimal-degree value.

Operational values record their last update time. If a previously valid source stops updating, BoatInfo keeps the last value only with an explicit `STALE` indication; unavailable/unsupported input renders as `NO DATA`. A numeric zero remains a valid value.

Discovery currently means **values observed in the live data stream**. BoatInfo does not yet query a Signal K server separately for the complete metadata/path catalog.

## Digital HMI design

BoatInfo follows the shared `jkuhnen/opencpn-plugin-devkit` digital-instrument grammar:

- `Value` for a current scalar/text value;
- `Level` for bounded quantities;
- `Tape` for cyclic/ordered context such as heading;
- `Trend` for recent history;
- explicit state handling for unavailable/invalid/stale information.

The default visual language intentionally avoids simulated analogue gauges, needles, fake LCDs, gloss, bezels and decorative arcs. Values, units and operational meaning take priority over instrument imitation. The sidebar layout uses a compact information density suitable for a docked OpenCPN panel.

## Design principles

- BoatInfo is an OpenCPN plugin; it does not replace OpenCPN navigation logic.
- The UI is vessel-neutral and does not contain Benchy-specific assumptions.
- OpenCPN host colors are used so the panel follows DAY/DUSK/NIGHT changes.
- Layout dimensions use DPI-aware wxWidgets logical sizing.
- Missing, stale and invalid values remain explicit rather than being guessed.
- Signal K updates are filtered to the server-declared own-vessel context when that context is available.
- Provider acquisition and presentation semantics are kept separate so additional data sources can later normalize into the same instrument model.
- The footer summarizes active data families (`OpenCPN`, `Signal K`, `NMEA XDR`) rather than implying that the most recently received message is the sole source of all displayed values.

The project follows the conventions in `jkuhnen/opencpn-plugin-devkit`. Maritime HMI references in that DevKit are design guidance only; BoatInfo is **not** claimed to be ECDIS, type-approved or otherwise certified navigation equipment.

## Build identity

Plugin name: `BoatInfo`

Package/common identifier: `boatinfo`

Target OpenCPN plugin API: 1.18

The existing FrontEnd2 build infrastructure is retained for now to avoid mixing the product work with an unrelated build-system migration.

## Development workflow

Changes should start from a GitHub issue, use a dedicated branch and arrive through a pull request. See the repository `AGENTS.md` and the shared DevKit before editing.

For Windows/runtime validation, use the proven build procedure documented by the DevKit and verify the resulting package in a real OpenCPN installation. At minimum test plugin enable/disable, dock/close behavior, preferences apply/cancel and restart persistence, DAY/DUSK/NIGHT, Windows DPI scaling, live OpenCPN position fixes, live Signal K own-vessel values, stale-data behavior and discovery of a previously unknown scalar path.

## Repository history

This repository started as BenchyNav on top of the community `testplugin`/FrontEnd2 scaffold. BoatInfo was refactored in place so the proven OpenCPN integration could be retained while obsolete scaffold code and resources were removed.

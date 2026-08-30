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

The selection is stored in the OpenCPN plugin configuration and restored after restart. Known marine semantics receive useful defaults where possible, for example battery state of charge as a linear `Level`, angular values as a `Tape`, and scalar measurements as `Value`.

Signal K values use standard SI semantics where the path is recognized. BoatInfo converts common values for presentation, including speed to knots, angles to degrees, temperatures to degrees Celsius, state-of-charge/level ratios to percent, remaining time to hours and revolutions to rpm.

Discovery currently means **values observed in the live data stream**. BoatInfo does not yet query a Signal K server separately for the complete metadata/path catalog.

## Digital HMI design

BoatInfo follows the shared `jkuhnen/opencpn-plugin-devkit` digital-instrument grammar:

- `Value` for a current scalar/text value;
- `Level` for bounded quantities;
- `Tape` for cyclic/ordered context such as heading;
- `Trend` for recent history;
- explicit state handling for unavailable/invalid information.

The default visual language intentionally avoids simulated analogue gauges, needles, fake LCDs, gloss, bezels and decorative arcs. Values, units and operational meaning take priority over instrument imitation.

## Design principles

- BoatInfo is an OpenCPN plugin; it does not replace OpenCPN navigation logic.
- The UI is vessel-neutral and does not contain Benchy-specific assumptions.
- OpenCPN host colors are used so the panel follows DAY/DUSK/NIGHT changes.
- Layout dimensions use DPI-aware wxWidgets logical sizing.
- Missing or invalid values remain explicitly unavailable rather than being guessed.
- Signal K updates are filtered to the server-declared own-vessel context when that context is available.
- Provider acquisition and presentation semantics are kept separate so additional data sources can later normalize into the same instrument model.

The project follows the conventions in `jkuhnen/opencpn-plugin-devkit`. Maritime HMI references in that DevKit are design guidance only; BoatInfo is **not** claimed to be ECDIS, type-approved or otherwise certified navigation equipment.

## Build identity

Plugin name: `BoatInfo`

Package/common identifier: `boatinfo`

Target OpenCPN plugin API: 1.18

The existing FrontEnd2 build infrastructure is retained for now to avoid mixing the product work with an unrelated build-system migration.

## Development workflow

Changes should start from a GitHub issue, use a dedicated branch and arrive through a pull request. See the repository `AGENTS.md` and the shared DevKit before editing.

For Windows/runtime validation, use the proven build procedure documented by the DevKit and verify the resulting package in a real OpenCPN installation. At minimum test plugin enable/disable, dock/close behavior, preferences apply/cancel and restart persistence, DAY/DUSK/NIGHT, Windows DPI scaling, live OpenCPN position fixes, live Signal K own-vessel values and discovery of a previously unknown scalar path.

## Repository history

This repository started as BenchyNav on top of the community `testplugin`/FrontEnd2 scaffold. BoatInfo was refactored in place so the proven OpenCPN integration could be retained while obsolete scaffold code and resources were removed.

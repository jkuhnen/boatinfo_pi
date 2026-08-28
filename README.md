# BoatInfo

BoatInfo is a compact OpenCPN sidebar plugin for **own-vessel information**. It generalizes the original BenchyNav prototype so the plugin is useful on any boat rather than being tied to one vessel.

## Current information

BoatInfo currently presents three groups:

- **Vessel:** name, MMSI and VHF call sign from Signal K own-vessel data.
- **Navigation:** latitude, longitude, COG, SOG, UTC date and UTC time from OpenCPN position-fix callbacks.
- **Electrical:** service-battery state of charge, estimated time remaining, current, voltage and power plus starter-battery voltage from Signal K paths.

The panel also indicates whether relevant data was most recently seen through Signal K or NMEA XDR.

## Design principles

- BoatInfo is an OpenCPN plugin; it does not replace OpenCPN navigation logic.
- The UI is vessel-neutral and does not contain Benchy-specific assumptions.
- OpenCPN host colors are used so the panel follows DAY/DUSK/NIGHT changes.
- Layout dimensions use DPI-aware wxWidgets logical sizing.
- Missing or invalid values remain explicitly unavailable rather than being guessed.
- Signal K updates are filtered to the server-declared own-vessel context when that context is available.

The project follows the conventions in `jkuhnen/opencpn-plugin-devkit`. Maritime HMI references in that DevKit are design guidance only; BoatInfo is **not** claimed to be ECDIS, type-approved or otherwise certified navigation equipment.

## Build identity

Plugin name: `BoatInfo`

Package/common identifier: `boatinfo`

Target OpenCPN plugin API: 1.18

The existing FrontEnd2 build infrastructure is retained for now to avoid mixing a product refactor with an unrelated build-system migration.

## Development workflow

Changes should start from a GitHub issue, use a dedicated branch and arrive through a pull request. See the repository `AGENTS.md` and the shared DevKit before editing.

For Windows/runtime validation, use the proven build procedure documented by the DevKit and verify the resulting package in a real OpenCPN installation. At minimum test plugin enable/disable, dock/close behavior, DAY/DUSK/NIGHT, Windows DPI scaling, live OpenCPN position fixes, Signal K own-vessel identity and service/starter battery values.

## Repository history

This repository started as BenchyNav on top of the community `testplugin`/FrontEnd2 scaffold. The working implementation is being refactored in place because preserving tested OpenCPN integration is safer and more reviewable than starting a second plugin from scratch.

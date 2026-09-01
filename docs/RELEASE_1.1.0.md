# BoatInfo 1.1.0

BoatInfo 1.1.0 is the first public release of the configurable own-vessel information plugin for OpenCPN.

BoatInfo adds a compact, responsive status strip below the chart and presents selected navigation, electrical, tank, propulsion and environmental information without imitating traditional analogue instruments.

## Highlights

- Compact vessel-global bottom strip which remains independent of OpenCPN's one- or two-chart layout.
- Automatic discovery of scalar own-vessel Signal K values seen by OpenCPN.
- OpenCPN navigation data plus Signal K onboard-system data in one coherent view.
- User-configurable Show, Priority and Name settings.
- Responsive Full / Compact / Minimal layouts with priority-based omission when space becomes limited.
- Provider-path categories such as NAVIGATION, ELECTRICAL, TANKS, PROPULSION and ENVIRONMENT.
- Common nautical/unit normalization including knots, degrees, degrees Celsius, percent, hours, ampere-hours and rpm.
- Signal K vessel identity and per-vessel profiles.
- Robust own-vessel identity handling for normal leaf deltas and root/pathless Signal K messages.
- OpenCPN DAY/DUSK/NIGHT integration.
- Explicit stale/no-current-value behavior which does not silently leave an old measurement looking current.

## Presentation philosophy

The 1.1.0 release deliberately uses one presentation mode: **Text**.

BoatInfo presents vessel data as information rather than pictures of instruments. Simulated needles, decorative gauges and the development-era Level/Tape/Trend selectors are not part of the public 1.1.0 interface.

Future `Digital` and `Digital + Analog` modes may add other representations where they provide operational value, but they are not part of this release.

## Compatibility

Validated release target:

- OpenCPN 5.14.0;
- Windows;
- MSVC Win32/x86;
- OpenCPN plugin API 1.18.

The repository still contains inherited multi-platform FrontEnd2 build scaffolding. These targets are not claimed as supported until they are separately built and runtime-tested.

## Installation

Import the supplied BoatInfo `.tar.gz` package using **OpenCPN → Options → Plugins → Import Plugin**, then enable BoatInfo. The archive should be imported as-is and not unpacked first.

See `docs/INSTALL.md` for setup and data-source details.

## Validation

The release has been exercised in OpenCPN with live/synthetic Signal K data including navigation, battery/electrical, tanks, propulsion and environmental values. The Signal K identity parser also has focused automated tests for standard leaf deltas, root/pathless identity data, nested call sign data, foreign contexts and conservative MMSI fallback behavior.

## Safety

BoatInfo is supplementary navigation/onboard-information software. It is not type-approved equipment and is not intended to replace mandatory navigation, machinery, alarm or safety systems.

# BoatInfo installation

## BoatInfo 1.1.0 supported package

The first public BoatInfo release is validated for:

- Windows;
- OpenCPN 5.14.0;
- OpenCPN plugin API 1.18;
- MSVC Win32/x86 package build.

Other platform build scaffolding exists in the repository but is not release-validated for BoatInfo 1.1.0.

## Install a release package manually

BoatInfo uses the standard OpenCPN plugin package format. For the initial public release, download the BoatInfo `.tar.gz` package supplied with the GitHub release.

In OpenCPN:

1. Open **Options**.
2. Open **Plugins**.
3. Choose **Import Plugin**.
4. Select the downloaded BoatInfo `.tar.gz` package.
5. Enable **BoatInfo** in the plugin list.
6. Restart OpenCPN if requested or if the pane is not immediately available.

Do not unpack the `.tar.gz` file before importing it.

## First setup

BoatInfo appears as a vessel-global strip below the chart area.

Open BoatInfo preferences to choose which observed values should be shown. Each discovered value can be configured with:

- **Show**;
- **Priority** (`Primary`, `Secondary`, `Detail`);
- **Name**;
- **Presentation** (`Text` in 1.1.0);
- source/path information.

Use **Apply / Anwenden** to update the live strip without closing the preferences dialog.

## Data sources

BoatInfo currently receives data from two OpenCPN-side sources:

- OpenCPN navigation/fix data;
- Signal K own-vessel values delivered through OpenCPN plugin messaging.

Signal K values become available in BoatInfo after they have been observed in the live stream. BoatInfo does not connect directly to the Signal K server to enumerate every available path.

For vessel identity, BoatInfo recognizes Signal K `name`, `mmsi` and `communication.callsignVhf`. Manual identity values are fallbacks when Signal K does not provide them.

## Updating

When installing a newer BoatInfo package, keep the existing OpenCPN configuration unless the release notes explicitly request a reset. BoatInfo 1.1.0 retains compatibility with legacy development profile fields while exposing only the supported text presentation in Preferences.

## Safety

BoatInfo is supplementary software for presenting own-vessel information. It is not type-approved navigation equipment, an ECDIS, an alarm management system or a substitute for required onboard instruments. Verify critical navigation and machinery information using the appropriate primary source.

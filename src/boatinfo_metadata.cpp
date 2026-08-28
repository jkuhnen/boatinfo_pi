#include "boatinfo_pi.h"

int boatinfo_pi::GetPlugInVersionMajor() { return 1; }

int boatinfo_pi::GetPlugInVersionMinor() { return 1; }

int boatinfo_pi::GetAPIVersionMajor() { return 1; }

int boatinfo_pi::GetAPIVersionMinor() { return 18; }

wxBitmap* boatinfo_pi::GetPlugInBitmap() {
  return opencpn_plugin::GetPlugInBitmap();
}

wxString boatinfo_pi::GetCommonName() { return wxT("BoatInfo"); }

wxString boatinfo_pi::GetShortDescription() {
  return wxT("Own-vessel navigation and onboard system information");
}

wxString boatinfo_pi::GetLongDescription() {
  return wxT("Displays own-vessel identity, navigation and electrical information from OpenCPN data sources in a compact docked panel.");
}

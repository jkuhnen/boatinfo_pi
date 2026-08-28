#include "boatinfo_pi.h"

#include <wx/bmpbndl.h>

#include "boatinfo_icons.h"

namespace {
const wxSize kPluginIconSize(32, 32);

wxBitmap RenderEmbeddedSvg(const char* svg) {
  return wxBitmapBundle::FromSVG(svg, kPluginIconSize)
      .GetBitmap(kPluginIconSize);
}
}  // namespace

boatinfo_pi::boatinfo_pi(void* ppimgr)
    : opencpn_plugin_118(ppimgr),
      m_pluginBitmap(RenderEmbeddedSvg(boatinfo_icon_data::kDaySvg)) {}

int boatinfo_pi::GetPlugInVersionMajor() { return BOATINFO_VERSION_MAJOR; }

int boatinfo_pi::GetPlugInVersionMinor() { return BOATINFO_VERSION_MINOR; }

int boatinfo_pi::GetAPIVersionMajor() { return BOATINFO_API_VERSION_MAJOR; }

int boatinfo_pi::GetAPIVersionMinor() { return BOATINFO_API_VERSION_MINOR; }

wxBitmap* boatinfo_pi::GetPlugInBitmap() { return &m_pluginBitmap; }

wxString boatinfo_pi::GetCommonName() { return wxT("BoatInfo"); }

wxString boatinfo_pi::GetShortDescription() {
  return wxT("Own-vessel navigation and onboard system information");
}

wxString boatinfo_pi::GetLongDescription() {
  return wxT(
      "BoatInfo displays own-vessel identity, navigation and electrical "
      "information from OpenCPN data sources in a compact docked "
      "panel.");
}

#include "testplugin_pi.h"
#include "version.h"

#include <cmath>
#include <wx/sizer.h>
#include <wx/stattext.h>

#ifdef _WIN32
#define BENCHYNAV_EXPORT __declspec(dllexport)
#else
#define BENCHYNAV_EXPORT
#endif

namespace {
wxString FormatCoordinate(double value, wxChar positiveHemisphere,
                          wxChar negativeHemisphere) {
  if (!std::isfinite(value)) {
    return wxT("---");
  }

  wxString result = wxString::Format(wxT("%.5f"), std::fabs(value));
  result += wxT(" ");
  result += value >= 0.0 ? positiveHemisphere : negativeHemisphere;
  return result;
}
}  // namespace

extern "C" BENCHYNAV_EXPORT opencpn_plugin* create_pi(void* ppimgr) {
  return new testplugin_pi(ppimgr);
}

extern "C" BENCHYNAV_EXPORT void destroy_pi(opencpn_plugin* p) { delete p; }

testplugin_pi::testplugin_pi(void* ppimgr) : opencpn_plugin_118(ppimgr) {}

testplugin_pi::~testplugin_pi() {}

int testplugin_pi::Init() {
  m_auiManager = GetFrameAuiManager();

  wxWindow* auiParent = m_auiManager->GetManagedWindow();

  m_helloPanel = new wxWindow(auiParent, wxID_ANY, wxDefaultPosition,
                              wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);

  wxColour panelBackground;
  GetGlobalColor(wxT("DILG1"), &panelBackground);
  m_helloPanel->SetBackgroundColour(panelBackground);

  wxColour panelText;
  GetGlobalColor(wxT("DILG3"), &panelText);
  m_helloPanel->SetForegroundColour(panelText);

  wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText* title =
      new wxStaticText(m_helloPanel, wxID_ANY, wxT("BenchyNav"));
  title->SetForegroundColour(panelText);
  wxFont titleFont = title->GetFont();
  titleFont.SetWeight(wxFONTWEIGHT_BOLD);
  titleFont.SetPointSize(titleFont.GetPointSize() + 2);
  title->SetFont(titleFont);
  sizer->Add(title, 0, wxALL, 15);

  auto addSectionTitle = [&](const wxString& text) {
    wxStaticText* section =
        new wxStaticText(m_helloPanel, wxID_ANY, text);
    section->SetForegroundColour(panelText);
    wxFont sectionFont = section->GetFont();
    sectionFont.SetWeight(wxFONTWEIGHT_BOLD);
    section->SetFont(sectionFont);
    sizer->Add(section, 0, wxLEFT | wxRIGHT | wxTOP, 15);
  };

  auto addRow = [&](wxFlexGridSizer* grid, const wxString& label,
                    const wxString& value) -> wxStaticText* {
    wxStaticText* labelText =
        new wxStaticText(m_helloPanel, wxID_ANY, label);
    labelText->SetForegroundColour(panelText);
    grid->Add(labelText, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* valueText =
        new wxStaticText(m_helloPanel, wxID_ANY, value);
    valueText->SetForegroundColour(panelText);
    grid->Add(valueText, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    return valueText;
  };

  addSectionTitle(wxT("VESSEL"));
  wxFlexGridSizer* vesselGrid = new wxFlexGridSizer(2, 8, 20);
  vesselGrid->AddGrowableCol(1, 1);
  addRow(vesselGrid, wxT("MMSI"), wxT("---"));
  addRow(vesselGrid, wxT("Call Sign"), wxT("---"));
  sizer->Add(vesselGrid, 0, wxEXPAND | wxALL, 15);

  addSectionTitle(wxT("NAVIGATION"));
  wxFlexGridSizer* navGrid = new wxFlexGridSizer(2, 8, 20);
  navGrid->AddGrowableCol(1, 1);
  m_latitudeValue = addRow(navGrid, wxT("Latitude"), wxT("---"));
  m_longitudeValue = addRow(navGrid, wxT("Longitude"), wxT("---"));
  addRow(navGrid, wxT("COG"), wxT("--- deg"));
  addRow(navGrid, wxT("SOG"), wxT("--- kn"));
  addRow(navGrid, wxT("Date"), wxT("--.--.----"));
  addRow(navGrid, wxT("Time"), wxT("--:--:--"));
  sizer->Add(navGrid, 0, wxEXPAND | wxALL, 15);

  addSectionTitle(wxT("BATTERY"));
  wxFlexGridSizer* batteryGrid = new wxFlexGridSizer(2, 8, 20);
  batteryGrid->AddGrowableCol(1, 1);
  addRow(batteryGrid, wxT("State"), wxT("--- %"));
  addRow(batteryGrid, wxT("Remaining"), wxT("---"));
  addRow(batteryGrid, wxT("Current"), wxT("--- A"));
  sizer->Add(batteryGrid, 0, wxEXPAND | wxALL, 15);

  m_helloPanel->SetSizer(sizer);

  wxAuiPaneInfo pane;
  pane.Name(wxT("BenchyNavPanel"))
      .Caption(wxT("BenchyNav"))
      .Right()
      .BestSize(300, -1)
      .MinSize(200, -1)
      .Floatable(false)
      .RightDockable(true)
      .LeftDockable(false)
      .TopDockable(false)
      .BottomDockable(false)
      .CloseButton(true)
      .Show(true);

  m_auiManager->AddPane(m_helloPanel, pane);
  m_auiManager->Update();

  return USES_AUI_MANAGER | WANTS_NMEA_EVENTS;
}

void testplugin_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  if (m_latitudeValue) {
    m_latitudeValue->SetLabel(FormatCoordinate(pfix.Lat, wxT('N'), wxT('S')));
  }

  if (m_longitudeValue) {
    m_longitudeValue->SetLabel(FormatCoordinate(pfix.Lon, wxT('E'), wxT('W')));
  }

  if (m_helloPanel) {
    m_helloPanel->Layout();
  }
}

bool testplugin_pi::DeInit() {
  m_latitudeValue = nullptr;
  m_longitudeValue = nullptr;

  if (m_helloPanel && m_auiManager) {
    m_auiManager->DetachPane(m_helloPanel);
    m_helloPanel->Destroy();
    m_helloPanel = nullptr;

    m_auiManager->Update();
  }

  m_auiManager = nullptr;

  return true;
}

int testplugin_pi::GetAPIVersionMajor() { return OCPN_API_VERSION_MAJOR; }

int testplugin_pi::GetAPIVersionMinor() { return OCPN_API_VERSION_MINOR; }

int testplugin_pi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }

int testplugin_pi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }

wxBitmap* testplugin_pi::GetPlugInBitmap() {
  static wxBitmap bitmap(32, 32);
  return &bitmap;
}

wxString testplugin_pi::GetCommonName() { return wxT("BenchyNav"); }

wxString testplugin_pi::GetShortDescription() {
  return wxT("Benchy vessel information and navigation data");
}

wxString testplugin_pi::GetLongDescription() {
  return wxT(
      "Displays Benchy vessel information and navigation data "
      "in a docked OpenCPN panel.");
}

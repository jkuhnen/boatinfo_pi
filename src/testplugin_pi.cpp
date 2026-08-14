#include "testplugin_pi.h"
#include "version.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>

#include <wx/jsonreader.h>
#include <wx/jsonval.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/url.h>

#ifdef _WIN32
#define BENCHYNAV_EXPORT __declspec(dllexport)
#else
#define BENCHYNAV_EXPORT
#endif

namespace {
const wxString kSignalKBaseUrl = wxT("http://localhost:3000");
const int kSignalKRefreshMs = 2000;

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

wxJSONValue GetPath(const wxJSONValue& root,
                    std::initializer_list<const char*> path) {
  wxJSONValue current = root;
  for (const char* key : path) {
    const wxString wxKey = wxString::FromUTF8(key);
    if (!current.IsObject() || !current.HasMember(wxKey)) {
      return wxJSONValue();
    }
    current = current.ItemAt(wxKey);
  }
  return current;
}

wxJSONValue GetSignalKValue(const wxJSONValue& root,
                            std::initializer_list<const char*> path) {
  wxJSONValue value = GetPath(root, path);
  if (value.IsObject() && value.HasMember(wxT("value"))) {
    value = value.ItemAt(wxT("value"));
  }
  return value;
}

bool JsonNumber(const wxJSONValue& value, double& result) {
  switch (value.GetType()) {
    case wxJSONTYPE_DOUBLE:
      result = value.AsDouble();
      return true;
    case wxJSONTYPE_INT:
      result = static_cast<double>(value.AsInt());
      return true;
    case wxJSONTYPE_UINT:
      result = static_cast<double>(value.AsUInt());
      return true;
    case wxJSONTYPE_LONG:
      result = static_cast<double>(value.AsLong());
      return true;
    case wxJSONTYPE_ULONG:
      result = static_cast<double>(value.AsULong());
      return true;
    case wxJSONTYPE_SHORT:
      result = static_cast<double>(value.AsShort());
      return true;
    case wxJSONTYPE_USHORT:
      result = static_cast<double>(value.AsUShort());
      return true;
    default:
      return false;
  }
}

bool JsonString(const wxJSONValue& value, wxString& result) {
  if (!value.IsString() && !value.IsCString()) {
    return false;
  }
  result = value.AsString();
  return true;
}

wxString FormatRemaining(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0.0) {
    return wxT("---");
  }

  const long totalMinutes = static_cast<long>(std::lround(seconds / 60.0));
  const long hours = totalMinutes / 60;
  const long minutes = totalMinutes % 60;
  return wxString::Format(wxT("%ld h %02ld min"), hours, minutes);
}

void SetLabel(wxStaticText* control, const wxString& value) {
  if (control) {
    control->SetLabel(value);
  }
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
  m_nameValue = addRow(vesselGrid, wxT("Name"), wxT("---"));
  m_mmsiValue = addRow(vesselGrid, wxT("MMSI"), wxT("---"));
  m_callSignValue = addRow(vesselGrid, wxT("Call Sign"), wxT("---"));
  sizer->Add(vesselGrid, 0, wxEXPAND | wxALL, 15);

  addSectionTitle(wxT("NAVIGATION"));
  wxFlexGridSizer* navGrid = new wxFlexGridSizer(2, 8, 20);
  navGrid->AddGrowableCol(1, 1);
  m_latitudeValue = addRow(navGrid, wxT("Latitude"), wxT("---"));
  m_longitudeValue = addRow(navGrid, wxT("Longitude"), wxT("---"));
  m_cogValue = addRow(navGrid, wxT("COG"), wxT("--- deg"));
  m_sogValue = addRow(navGrid, wxT("SOG"), wxT("--- kn"));
  m_dateValue = addRow(navGrid, wxT("Date"), wxT("--.--.----"));
  m_timeValue = addRow(navGrid, wxT("Time UTC"), wxT("--:--:--"));
  sizer->Add(navGrid, 0, wxEXPAND | wxALL, 15);

  addSectionTitle(wxT("BATTERY"));
  wxFlexGridSizer* batteryGrid = new wxFlexGridSizer(2, 8, 20);
  batteryGrid->AddGrowableCol(1, 1);
  m_socValue = addRow(batteryGrid, wxT("State"), wxT("--- %"));
  m_remainingValue = addRow(batteryGrid, wxT("Remaining"), wxT("---"));
  m_currentValue = addRow(batteryGrid, wxT("Current"), wxT("--- A"));
  m_voltageValue = addRow(batteryGrid, wxT("Voltage"), wxT("--- V"));
  m_powerValue = addRow(batteryGrid, wxT("Power"), wxT("--- W"));
  m_starterVoltageValue =
      addRow(batteryGrid, wxT("Starter"), wxT("--- V"));
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

  m_signalKTimer = new wxTimer(m_helloPanel);
  m_helloPanel->Bind(wxEVT_TIMER, &testplugin_pi::OnSignalKTimer, this,
                     m_signalKTimer->GetId());
  m_signalKTimer->Start(kSignalKRefreshMs);

  return USES_AUI_MANAGER | WANTS_NMEA_EVENTS;
}

void testplugin_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  SetLabel(m_latitudeValue,
           FormatCoordinate(pfix.Lat, wxT('N'), wxT('S')));
  SetLabel(m_longitudeValue,
           FormatCoordinate(pfix.Lon, wxT('E'), wxT('W')));

  if (m_helloPanel) {
    m_helloPanel->Layout();
  }
}

void testplugin_pi::OnSignalKTimer(wxTimerEvent&) { UpdateSignalK(); }

bool testplugin_pi::UpdateSignalK() {
  wxLogNull suppressNetworkErrors;

  wxURL url(kSignalKBaseUrl + wxT("/signalk/v1/api/vessels/self"));
  if (!url.IsOk()) {
    return false;
  }

  url.GetProtocol().SetTimeout(1);
  std::unique_ptr<wxInputStream> stream(url.GetInputStream());
  if (!stream || !stream->IsOk()) {
    return false;
  }

  wxJSONValue root;
  wxJSONReader reader(wxJSONREADER_STRICT);
  if (reader.Parse(*stream, &root) != 0 || !root.IsObject()) {
    return false;
  }

  wxString text;
  if (JsonString(GetSignalKValue(root, {"name"}), text)) {
    SetLabel(m_nameValue, text);
  }
  if (JsonString(GetSignalKValue(root, {"mmsi"}), text)) {
    SetLabel(m_mmsiValue, text);
  }
  if (JsonString(GetSignalKValue(root, {"communication", "callsignVhf"}),
                 text)) {
    SetLabel(m_callSignValue, text);
  }

  wxJSONValue position = GetSignalKValue(root, {"navigation", "position"});
  double number = 0.0;
  if (position.IsObject() && position.HasMember(wxT("latitude")) &&
      JsonNumber(position.ItemAt(wxT("latitude")), number)) {
    SetLabel(m_latitudeValue,
             FormatCoordinate(number, wxT('N'), wxT('S')));
  }
  if (position.IsObject() && position.HasMember(wxT("longitude")) &&
      JsonNumber(position.ItemAt(wxT("longitude")), number)) {
    SetLabel(m_longitudeValue,
             FormatCoordinate(number, wxT('E'), wxT('W')));
  }

  if (JsonNumber(GetSignalKValue(root, {"navigation", "courseOverGroundTrue"}),
                 number)) {
    const double degrees =
        std::fmod(number * 180.0 / 3.14159265358979323846 + 360.0, 360.0);
    SetLabel(m_cogValue, wxString::Format(wxT("%.1f deg"), degrees));
  }

  if (JsonNumber(GetSignalKValue(root, {"navigation", "speedOverGround"}),
                 number)) {
    SetLabel(m_sogValue,
             wxString::Format(wxT("%.1f kn"), number * 1.94384));
  }

  if (JsonString(GetSignalKValue(root, {"navigation", "datetime"}), text) &&
      text.length() >= 19) {
    const wxString date = text.Mid(8, 2) + wxT(".") + text.Mid(5, 2) +
                          wxT(".") + text.Mid(0, 4);
    const wxString time = text.Mid(11, 8);
    SetLabel(m_dateValue, date);
    SetLabel(m_timeValue, time);
  }

  if (JsonNumber(GetSignalKValue(root, {"electrical", "batteries", "service",
                                        "capacity", "stateOfCharge"}),
                 number)) {
    SetLabel(m_socValue,
             wxString::Format(wxT("%.0f %%"), number * 100.0));
  }

  if (JsonNumber(GetSignalKValue(root, {"electrical", "batteries", "service",
                                        "capacity", "timeRemaining"}),
                 number)) {
    SetLabel(m_remainingValue, FormatRemaining(number));
  }

  if (JsonNumber(GetSignalKValue(root, {"electrical", "batteries", "service",
                                        "current"}),
                 number)) {
    SetLabel(m_currentValue, wxString::Format(wxT("%.2f A"), number));
  }

  if (JsonNumber(GetSignalKValue(root, {"electrical", "batteries", "service",
                                        "voltage"}),
                 number)) {
    SetLabel(m_voltageValue, wxString::Format(wxT("%.2f V"), number));
  }

  if (JsonNumber(GetSignalKValue(root, {"electrical", "batteries", "service",
                                        "power"}),
                 number)) {
    SetLabel(m_powerValue, wxString::Format(wxT("%.1f W"), number));
  }

  if (JsonNumber(GetSignalKValue(root, {"electrical", "batteries", "starter",
                                        "voltage"}),
                 number)) {
    SetLabel(m_starterVoltageValue,
             wxString::Format(wxT("%.2f V"), number));
  }

  if (m_helloPanel) {
    m_helloPanel->Layout();
  }

  return true;
}

bool testplugin_pi::DeInit() {
  if (m_signalKTimer) {
    m_signalKTimer->Stop();
    delete m_signalKTimer;
    m_signalKTimer = nullptr;
  }

  m_nameValue = nullptr;
  m_mmsiValue = nullptr;
  m_callSignValue = nullptr;
  m_latitudeValue = nullptr;
  m_longitudeValue = nullptr;
  m_cogValue = nullptr;
  m_sogValue = nullptr;
  m_dateValue = nullptr;
  m_timeValue = nullptr;
  m_socValue = nullptr;
  m_remainingValue = nullptr;
  m_currentValue = nullptr;
  m_voltageValue = nullptr;
  m_powerValue = nullptr;
  m_starterVoltageValue = nullptr;

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

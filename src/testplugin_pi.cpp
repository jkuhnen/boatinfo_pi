#include "testplugin_pi.h"
#include "version.h"

#include <cmath>

#include <wx/datetime.h>
#include <wx/jsonreader.h>
#include <wx/jsonval.h>
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

void SetLabel(wxStaticText* control, const wxString& value) {
  if (control) {
    control->SetLabel(value);
  }
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

wxString FormatRemaining(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0.0) {
    return wxT("---");
  }

  const long totalMinutes = static_cast<long>(std::lround(seconds / 60.0));
  const long hours = totalMinutes / 60;
  const long minutes = totalMinutes % 60;
  return wxString::Format(wxT("%ld h %02ld min"), hours, minutes);
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
    wxStaticText* section = new wxStaticText(m_helloPanel, wxID_ANY, text);
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
  m_starterVoltageValue = addRow(batteryGrid, wxT("Starter"), wxT("--- V"));
  m_signalKStatusValue = addRow(batteryGrid, wxT("Source"), wxT("waiting"));
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

  return USES_AUI_MANAGER | WANTS_NMEA_EVENTS | WANTS_NMEA_SENTENCES |
         WANTS_PLUGIN_MESSAGING;
}

void testplugin_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  SetLabel(m_latitudeValue,
           FormatCoordinate(pfix.Lat, wxT('N'), wxT('S')));
  SetLabel(m_longitudeValue,
           FormatCoordinate(pfix.Lon, wxT('E'), wxT('W')));

  if (std::isfinite(pfix.Cog)) {
    double cog = std::fmod(pfix.Cog, 360.0);
    if (cog < 0.0) {
      cog += 360.0;
    }
    SetLabel(m_cogValue, wxString::Format(wxT("%.1f deg"), cog));
  } else {
    SetLabel(m_cogValue, wxT("--- deg"));
  }

  if (std::isfinite(pfix.Sog) && pfix.Sog >= 0.0) {
    SetLabel(m_sogValue, wxString::Format(wxT("%.1f kn"), pfix.Sog));
  } else {
    SetLabel(m_sogValue, wxT("--- kn"));
  }

  if (pfix.FixTime > 0) {
    wxDateTime utc(static_cast<time_t>(pfix.FixTime));
    utc = utc.ToUTC();
    SetLabel(m_dateValue, utc.Format(wxT("%d.%m.%Y")));
    SetLabel(m_timeValue, utc.Format(wxT("%H:%M:%S")));
  } else {
    SetLabel(m_dateValue, wxT("--.--.----"));
    SetLabel(m_timeValue, wxT("--:--:--"));
  }

  if (m_helloPanel) {
    m_helloPanel->Layout();
  }
}

void testplugin_pi::SetNMEASentence(wxString& sentence) {
  if (sentence.Find(wxT("XDR")) != wxNOT_FOUND) {
    SetLabel(m_signalKStatusValue, wxT("NMEA XDR"));
  }
}

void testplugin_pi::SetPluginMessage(wxString& message_id,
                                     wxString& message_body) {
  if (message_id == wxT("OCPN_CORE_SIGNALK")) {
    SetLabel(m_signalKStatusValue, wxT("Signal K"));
    ParseSignalK(message_body);
  }
}

void testplugin_pi::ParseSignalK(const wxString& message) {
  wxJSONValue root;
  wxJSONReader reader;
  if (reader.Parse(message, &root) != 0 || !root.IsObject()) {
    return;
  }

  if (root.HasMember(wxT("self")) && root[wxT("self")].IsString()) {
    m_signalKSelf = wxT("vessels.") + root[wxT("self")].AsString();
  }

  if (root.HasMember(wxT("context")) && root[wxT("context")].IsString() &&
      !m_signalKSelf.IsEmpty() &&
      root[wxT("context")].AsString() != m_signalKSelf) {
    return;
  }

  if (!root.HasMember(wxT("updates")) || !root[wxT("updates")].IsArray()) {
    return;
  }

  wxJSONValue& updates = root[wxT("updates")];
  for (int i = 0; i < updates.Size(); ++i) {
    wxJSONValue& update = updates[i];
    if (!update.IsObject() || !update.HasMember(wxT("values")) ||
        !update[wxT("values")].IsArray()) {
      continue;
    }

    wxJSONValue& values = update[wxT("values")];
    for (int j = 0; j < values.Size(); ++j) {
      wxJSONValue& item = values[j];
      if (!item.IsObject() || !item.HasMember(wxT("path")) ||
          !item[wxT("path")].IsString() || !item.HasMember(wxT("value"))) {
        continue;
      }
      UpdateSignalKPath(item[wxT("path")].AsString(), item[wxT("value")]);
    }
  }

  if (m_helloPanel) {
    m_helloPanel->Layout();
  }
}

void testplugin_pi::UpdateSignalKPath(const wxString& path,
                                      const wxJSONValue& value) {
  if (path == wxT("name") && value.IsString()) {
    SetLabel(m_nameValue, value.AsString());
    return;
  }
  if (path == wxT("mmsi")) {
    if (value.IsString()) {
      SetLabel(m_mmsiValue, value.AsString());
    } else {
      double number = 0.0;
      if (JsonNumber(value, number)) {
        SetLabel(m_mmsiValue, wxString::Format(wxT("%.0f"), number));
      }
    }
    return;
  }
  if (path == wxT("communication.callsignVhf") && value.IsString()) {
    SetLabel(m_callSignValue, value.AsString());
    return;
  }

  double number = 0.0;
  if (!JsonNumber(value, number)) {
    return;
  }

  if (path == wxT("electrical.batteries.service.capacity.stateOfCharge")) {
    SetLabel(m_socValue, wxString::Format(wxT("%.0f %%"), number * 100.0));
  } else if (path ==
             wxT("electrical.batteries.service.capacity.timeRemaining")) {
    SetLabel(m_remainingValue, FormatRemaining(number));
  } else if (path == wxT("electrical.batteries.service.current")) {
    SetLabel(m_currentValue, wxString::Format(wxT("%.2f A"), number));
  } else if (path == wxT("electrical.batteries.service.voltage")) {
    SetLabel(m_voltageValue, wxString::Format(wxT("%.2f V"), number));
  } else if (path == wxT("electrical.batteries.service.power")) {
    SetLabel(m_powerValue, wxString::Format(wxT("%.1f W"), number));
  } else if (path == wxT("electrical.batteries.starter.voltage")) {
    SetLabel(m_starterVoltageValue,
             wxString::Format(wxT("%.2f V"), number));
  }
}

bool testplugin_pi::DeInit() {
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
  m_signalKStatusValue = nullptr;

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

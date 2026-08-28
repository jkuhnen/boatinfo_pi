#include "boatinfo_pi.h"

#include <cmath>

#include <wx/datetime.h>
#include <wx/jsonreader.h>
#include <wx/jsonval.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#ifdef _WIN32
#define BOATINFO_EXPORT __declspec(dllexport)
#else
#define BOATINFO_EXPORT
#endif

namespace {
wxString FormatCoordinate(double value, double maximumMagnitude,
                          wxChar positiveHemisphere,
                          wxChar negativeHemisphere) {
  if (!std::isfinite(value) || std::fabs(value) > maximumMagnitude) {
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
      break;
    case wxJSONTYPE_INT:
      result = static_cast<double>(value.AsInt());
      break;
    case wxJSONTYPE_UINT:
      result = static_cast<double>(value.AsUInt());
      break;
    case wxJSONTYPE_LONG:
      result = static_cast<double>(value.AsLong());
      break;
    case wxJSONTYPE_ULONG:
      result = static_cast<double>(value.AsULong());
      break;
    case wxJSONTYPE_SHORT:
      result = static_cast<double>(value.AsShort());
      break;
    case wxJSONTYPE_USHORT:
      result = static_cast<double>(value.AsUShort());
      break;
    default:
      return false;
  }

  return std::isfinite(result);
}

wxString NormalizeSignalKSelf(const wxString& self) {
  if (self.IsEmpty() || self.StartsWith(wxT("vessels."))) {
    return self;
  }
  return wxT("vessels.") + self;
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

extern "C" BOATINFO_EXPORT opencpn_plugin* create_pi(void* ppimgr) {
  return new boatinfo_pi(ppimgr);
}

extern "C" BOATINFO_EXPORT void destroy_pi(opencpn_plugin* plugin) {
  delete plugin;
}

boatinfo_pi::~boatinfo_pi() = default;

int boatinfo_pi::Init() {
  m_auiManager = GetFrameAuiManager();
  if (!m_auiManager || !m_auiManager->GetManagedWindow()) {
    return 0;
  }

  wxWindow* auiParent = m_auiManager->GetManagedWindow();
  m_panel = new wxWindow(auiParent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                         wxFULL_REPAINT_ON_RESIZE);

  const int gap = m_panel->FromDIP(8);
  const int sectionGap = m_panel->FromDIP(12);
  const int outer = m_panel->FromDIP(14);
  const int labelGap = m_panel->FromDIP(18);

  wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText* title = new wxStaticText(m_panel, wxID_ANY, wxT("BoatInfo"));
  wxFont titleFont = title->GetFont();
  titleFont.SetWeight(wxFONTWEIGHT_BOLD);
  titleFont.SetPointSize(titleFont.GetPointSize() + 2);
  title->SetFont(titleFont);
  sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, outer);
  sizer->AddSpacer(sectionGap);

  auto addSectionTitle = [&](const wxString& text) {
    wxStaticText* section = new wxStaticText(m_panel, wxID_ANY, text);
    wxFont sectionFont = section->GetFont();
    sectionFont.SetWeight(wxFONTWEIGHT_BOLD);
    section->SetFont(sectionFont);
    sizer->Add(section, 0, wxLEFT | wxRIGHT | wxTOP, outer);
  };

  auto addRow = [&](wxFlexGridSizer* grid, const wxString& label,
                    const wxString& value) -> wxStaticText* {
    wxStaticText* labelText = new wxStaticText(m_panel, wxID_ANY, label);
    grid->Add(labelText, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

    wxStaticText* valueText = new wxStaticText(m_panel, wxID_ANY, value);
    grid->Add(valueText, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    return valueText;
  };

  addSectionTitle(wxT("Vessel"));
  wxFlexGridSizer* vesselGrid = new wxFlexGridSizer(2, gap, labelGap);
  vesselGrid->AddGrowableCol(1, 1);
  m_nameValue = addRow(vesselGrid, wxT("Name"), wxT("---"));
  m_mmsiValue = addRow(vesselGrid, wxT("MMSI"), wxT("---"));
  m_callSignValue = addRow(vesselGrid, wxT("Call sign"), wxT("---"));
  sizer->Add(vesselGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, outer);

  addSectionTitle(wxT("Navigation"));
  wxFlexGridSizer* navGrid = new wxFlexGridSizer(2, gap, labelGap);
  navGrid->AddGrowableCol(1, 1);
  m_latitudeValue = addRow(navGrid, wxT("Latitude"), wxT("---"));
  m_longitudeValue = addRow(navGrid, wxT("Longitude"), wxT("---"));
  m_cogValue = addRow(navGrid, wxT("COG"), wxT("--- deg"));
  m_sogValue = addRow(navGrid, wxT("SOG"), wxT("--- kn"));
  m_dateValue = addRow(navGrid, wxT("Date"), wxT("--.--.----"));
  m_timeValue = addRow(navGrid, wxT("Time UTC"), wxT("--:--:--"));
  sizer->Add(navGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, outer);

  addSectionTitle(wxT("Electrical"));
  wxFlexGridSizer* electricalGrid = new wxFlexGridSizer(2, gap, labelGap);
  electricalGrid->AddGrowableCol(1, 1);
  m_socValue = addRow(electricalGrid, wxT("Service SOC"), wxT("--- %"));
  m_remainingValue =
      addRow(electricalGrid, wxT("Time remaining"), wxT("---"));
  m_currentValue = addRow(electricalGrid, wxT("Service current"), wxT("--- A"));
  m_voltageValue = addRow(electricalGrid, wxT("Service voltage"), wxT("--- V"));
  m_powerValue = addRow(electricalGrid, wxT("Service power"), wxT("--- W"));
  m_starterVoltageValue =
      addRow(electricalGrid, wxT("Starter voltage"), wxT("--- V"));
  m_dataSourceValue = addRow(electricalGrid, wxT("Data source"), wxT("Waiting"));
  sizer->Add(electricalGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, outer);

  m_panel->SetSizer(sizer);
  ApplyHostStyle();

  wxAuiPaneInfo pane;
  pane.Name(wxT("BoatInfoPanel"))
      .Caption(wxT("BoatInfo"))
      .Right()
      .BestSize(m_panel->FromDIP(320), -1)
      .MinSize(m_panel->FromDIP(220), -1)
      .Floatable(false)
      .RightDockable(true)
      .LeftDockable(true)
      .TopDockable(false)
      .BottomDockable(false)
      .CloseButton(false)
      .Show(true);

  if (!m_auiManager->AddPane(m_panel, pane)) {
    m_panel->Destroy();
    m_panel = nullptr;
    ClearControlPointers();
    m_auiManager = nullptr;
    return 0;
  }
  m_auiManager->Update();

  return USES_AUI_MANAGER | WANTS_NMEA_EVENTS | WANTS_NMEA_SENTENCES |
         WANTS_PLUGIN_MESSAGING;
}

void boatinfo_pi::ApplyHostStyle() {
  if (!m_panel) {
    return;
  }

  wxColour background;
  wxColour text;
  if (!GetGlobalColor(wxT("DILG1"), &background)) {
    background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  }
  if (!GetGlobalColor(wxT("DILG3"), &text)) {
    text = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
  }

  m_panel->SetBackgroundColour(background);
  m_panel->SetForegroundColour(text);

  const wxWindowList& children = m_panel->GetChildren();
  for (wxWindowList::compatibility_iterator node = children.GetFirst(); node;
       node = node->GetNext()) {
    wxWindow* child = node->GetData();
    child->SetBackgroundColour(background);
    child->SetForegroundColour(text);
  }

  m_panel->Refresh();
}

void boatinfo_pi::SetColorScheme(PI_ColorScheme cs) {
  m_colorScheme = cs;
  ApplyHostStyle();
}

void boatinfo_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  SetLabel(m_latitudeValue,
           FormatCoordinate(pfix.Lat, 90.0, wxT('N'), wxT('S')));
  SetLabel(m_longitudeValue,
           FormatCoordinate(pfix.Lon, 180.0, wxT('E'), wxT('W')));

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

  if (m_panel) {
    m_panel->Layout();
  }
}

void boatinfo_pi::SetNMEASentence(wxString& sentence) {
  if (sentence.length() >= 6 && sentence[0] == wxT('$') &&
      sentence.Mid(3, 3) == wxT("XDR")) {
    SetLabel(m_dataSourceValue, wxT("NMEA XDR"));
  }
}

void boatinfo_pi::SetPluginMessage(wxString& message_id,
                                   wxString& message_body) {
  if (message_id == wxT("OCPN_CORE_SIGNALK")) {
    if (ParseSignalK(message_body)) {
      SetLabel(m_dataSourceValue, wxT("Signal K"));
    }
  }
}

bool boatinfo_pi::ParseSignalK(const wxString& message) {
  wxJSONValue root;
  wxJSONReader reader;
  if (reader.Parse(message, &root) != 0 || !root.IsObject()) {
    return false;
  }

  if (root.HasMember(wxT("self")) && root[wxT("self")].IsString()) {
    const wxString self = NormalizeSignalKSelf(root[wxT("self")].AsString());
    if (!self.IsEmpty()) {
      m_signalKSelf = self;
    }
  }

  if (root.HasMember(wxT("context")) && root[wxT("context")].IsString()) {
    if (m_signalKSelf.IsEmpty() ||
        root[wxT("context")].AsString() != m_signalKSelf) {
      return false;
    }
  }

  if (!root.HasMember(wxT("updates")) || !root[wxT("updates")].IsArray()) {
    return false;
  }

  bool handled = false;
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
      handled =
          UpdateSignalKPath(item[wxT("path")].AsString(),
                            item[wxT("value")]) ||
          handled;
    }
  }

  if (handled && m_panel) {
    m_panel->Layout();
  }
  return handled;
}

bool boatinfo_pi::UpdateSignalKPath(const wxString& path,
                                    const wxJSONValue& value) {
  if (path == wxT("name") && value.IsString()) {
    SetLabel(m_nameValue, value.AsString());
    return true;
  }
  if (path == wxT("mmsi")) {
    if (value.IsString()) {
      SetLabel(m_mmsiValue, value.AsString());
      return true;
    } else {
      double number = 0.0;
      if (JsonNumber(value, number)) {
        SetLabel(m_mmsiValue, wxString::Format(wxT("%.0f"), number));
        return true;
      }
    }
    return false;
  }
  if (path == wxT("communication.callsignVhf") && value.IsString()) {
    SetLabel(m_callSignValue, value.AsString());
    return true;
  }

  double number = 0.0;
  if (!JsonNumber(value, number)) {
    return false;
  }

  if (path == wxT("electrical.batteries.service.capacity.stateOfCharge")) {
    if (number < 0.0 || number > 1.0) {
      return false;
    }
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
  } else {
    return false;
  }
  return true;
}

void boatinfo_pi::ClearControlPointers() {
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
  m_dataSourceValue = nullptr;
}

bool boatinfo_pi::DeInit() {
  ClearControlPointers();
  m_signalKSelf.clear();

  if (m_panel && m_auiManager) {
    m_auiManager->DetachPane(m_panel);
    m_panel->Destroy();
    m_panel = nullptr;
    m_auiManager->Update();
  }

  m_auiManager = nullptr;
  return true;
}

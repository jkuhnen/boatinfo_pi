#include "boatinfo_pi.h"

#include <algorithm>
#include <cmath>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/fileconf.h>
#include <wx/jsonreader.h>
#include <wx/jsonval.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#ifdef _WIN32
#define BOATINFO_EXPORT __declspec(dllexport)
#else
#define BOATINFO_EXPORT
#endif

namespace {
const double kRadToDeg = 180.0 / 3.14159265358979323846;
const double kMsToKnots = 1.9438444924406;

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

wxString SuggestedLabelFromPath(const wxString& path) {
  const int dot = path.Find(wxT('.'), true);
  wxString part = dot == wxNOT_FOUND ? path : path.Mid(dot + 1);
  if (part.IsEmpty()) {
    return path;
  }

  wxString out;
  for (size_t i = 0; i < part.length(); ++i) {
    const wxChar c = part[i];
    if (i > 0 && wxIsupper(c) && wxIslower(part[i - 1])) {
      out += wxT(' ');
    }
    out += c;
  }
  if (!out.IsEmpty()) {
    out[0] = wxToupper(out[0]);
  }
  return out;
}

wxString PrimitiveLabel(BoatInfoValue::Primitive primitive) {
  switch (primitive) {
    case BoatInfoValue::PRIMITIVE_LEVEL:
      return wxT("Level");
    case BoatInfoValue::PRIMITIVE_TAPE:
      return wxT("Tape");
    case BoatInfoValue::PRIMITIVE_TREND:
      return wxT("Trend");
    case BoatInfoValue::PRIMITIVE_NONE:
      return wxT("None");
    case BoatInfoValue::PRIMITIVE_VALUE:
    default:
      return wxT("Value");
  }
}

BoatInfoValue::Primitive PrimitiveFromSelection(int selection) {
  switch (selection) {
    case 1:
      return BoatInfoValue::PRIMITIVE_LEVEL;
    case 2:
      return BoatInfoValue::PRIMITIVE_TAPE;
    case 3:
      return BoatInfoValue::PRIMITIVE_TREND;
    case 4:
      return BoatInfoValue::PRIMITIVE_NONE;
    case 0:
    default:
      return BoatInfoValue::PRIMITIVE_VALUE;
  }
}

int SelectionFromPrimitive(BoatInfoValue::Primitive primitive) {
  switch (primitive) {
    case BoatInfoValue::PRIMITIVE_LEVEL:
      return 1;
    case BoatInfoValue::PRIMITIVE_TAPE:
      return 2;
    case BoatInfoValue::PRIMITIVE_TREND:
      return 3;
    case BoatInfoValue::PRIMITIVE_NONE:
      return 4;
    case BoatInfoValue::PRIMITIVE_VALUE:
    default:
      return 0;
  }
}

wxString FormatNumber(const BoatInfoValue& value) {
  if (!value.valid || !value.hasNumericValue) {
    return wxT("—");
  }
  if (value.unit == wxT("%") || value.unit == wxT("rpm")) {
    return wxString::Format(wxT("%.0f"), value.displayValue);
  }
  if (value.unit == wxT("°") || value.unit == wxT("kn") ||
      value.unit == wxT("m") || value.unit == wxT("h") ||
      value.unit == wxT("W") || value.unit == wxT("°C")) {
    return wxString::Format(wxT("%.1f"), value.displayValue);
  }
  if (value.unit == wxT("V") || value.unit == wxT("A")) {
    return wxString::Format(wxT("%.2f"), value.displayValue);
  }
  return wxString::Format(wxT("%.2f"), value.displayValue);
}

void ResolveHostColours(wxColour& background, wxColour& text,
                        wxColour& secondary, wxColour& accent) {
  if (!GetGlobalColor(wxT("DILG1"), &background)) {
    background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  }
  if (!GetGlobalColor(wxT("DILG3"), &text)) {
    text = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
  }
  if (!GetGlobalColor(wxT("DILG2"), &secondary)) {
    secondary = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
  }
  if (!GetGlobalColor(wxT("UIBCK"), &accent)) {
    accent = text;
  }
}
}  // namespace

class BoatInfoInstrumentPanel : public wxPanel {
public:
  BoatInfoInstrumentPanel(wxWindow* parent, const BoatInfoValue& model)
      : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(108)),
                wxBORDER_NONE),
        m_model(model) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &BoatInfoInstrumentPanel::OnPaint, this);
  }

  void SetModel(const BoatInfoValue& model) {
    m_model = model;
    Refresh(false);
  }

private:
  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    wxColour background, text, secondary, accent;
    ResolveHostColours(background, text, secondary, accent);
    dc.SetBackground(wxBrush(background));
    dc.Clear();

    const wxSize size = GetClientSize();
    const int pad = FromDIP(10);
    const int graphTop = FromDIP(67);
    dc.SetTextForeground(secondary);
    wxFont labelFont = GetFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(labelFont);
    dc.DrawText(m_model.label.IsEmpty() ? m_model.suggestedLabel : m_model.label,
                pad, FromDIP(5));

    wxFont valueFont = GetFont();
    valueFont.SetPointSize(std::max(14, valueFont.GetPointSize() + 8));
    valueFont.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(valueFont);
    dc.SetTextForeground(text);

    wxString rendered = m_model.hasNumericValue ? FormatNumber(m_model)
                                                 : m_model.textValue;
    if (!m_model.valid || rendered.IsEmpty()) {
      rendered = wxT("—");
    }
    dc.DrawText(rendered, pad, FromDIP(24));

    wxCoord valueWidth = 0, valueHeight = 0;
    dc.GetTextExtent(rendered, &valueWidth, &valueHeight);
    if (!m_model.unit.IsEmpty() && m_model.valid) {
      wxFont unitFont = GetFont();
      dc.SetFont(unitFont);
      dc.SetTextForeground(secondary);
      dc.DrawText(m_model.unit, pad + valueWidth + FromDIP(6),
                  FromDIP(38));
    }

    if (!m_model.valid) {
      dc.SetFont(GetFont());
      dc.SetTextForeground(secondary);
      dc.DrawText(wxT("NO DATA"), pad, graphTop);
      return;
    }

    if (m_model.primitive == BoatInfoValue::PRIMITIVE_LEVEL &&
        m_model.hasNumericValue && m_model.maximum > m_model.minimum) {
      const int x = pad;
      const int y = graphTop + FromDIP(5);
      const int w = std::max(1, size.x - 2 * pad);
      const int h = FromDIP(8);
      dc.SetPen(wxPen(secondary));
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.DrawRectangle(x, y, w, h);
      double fraction = (m_model.displayValue - m_model.minimum) /
                        (m_model.maximum - m_model.minimum);
      fraction = std::max(0.0, std::min(1.0, fraction));
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(accent));
      dc.DrawRectangle(x, y, static_cast<int>(w * fraction), h);
    } else if (m_model.primitive == BoatInfoValue::PRIMITIVE_TAPE &&
               m_model.hasNumericValue) {
      const int center = size.x / 2;
      const int y = graphTop + FromDIP(8);
      dc.SetPen(wxPen(secondary));
      dc.DrawLine(pad, y, size.x - pad, y);
      dc.SetPen(wxPen(text, FromDIP(2)));
      dc.DrawLine(center, y - FromDIP(8), center, y + FromDIP(8));
      dc.SetFont(GetFont());
      dc.SetTextForeground(secondary);
      for (int offset = -2; offset <= 2; ++offset) {
        double tick = m_model.displayValue + offset * 10.0;
        while (tick < 0.0) tick += 360.0;
        while (tick >= 360.0) tick -= 360.0;
        const int x = center + offset * FromDIP(42);
        dc.DrawLine(x, y - FromDIP(3), x, y + FromDIP(3));
        dc.DrawText(wxString::Format(wxT("%.0f"), tick), x - FromDIP(9),
                    y + FromDIP(7));
      }
    } else if (m_model.primitive == BoatInfoValue::PRIMITIVE_TREND &&
               m_model.trend.size() > 1) {
      const int left = pad;
      const int right = size.x - pad;
      const int top = graphTop;
      const int bottom = size.y - FromDIP(8);
      double minValue = *std::min_element(m_model.trend.begin(),
                                          m_model.trend.end());
      double maxValue = *std::max_element(m_model.trend.begin(),
                                          m_model.trend.end());
      if (maxValue <= minValue) {
        maxValue = minValue + 1.0;
      }
      dc.SetPen(wxPen(accent));
      wxPoint previous;
      for (size_t i = 0; i < m_model.trend.size(); ++i) {
        const double fx = static_cast<double>(i) /
                          static_cast<double>(m_model.trend.size() - 1);
        const double fy = (m_model.trend[i] - minValue) /
                          (maxValue - minValue);
        wxPoint point(left + static_cast<int>((right - left) * fx),
                      bottom - static_cast<int>((bottom - top) * fy));
        if (i > 0) {
          dc.DrawLine(previous, point);
        }
        previous = point;
      }
    }
  }

  BoatInfoValue m_model;
};

extern "C" BOATINFO_EXPORT opencpn_plugin* create_pi(void* ppimgr) {
  return new boatinfo_pi(ppimgr);
}

extern "C" BOATINFO_EXPORT void destroy_pi(opencpn_plugin* plugin) {
  delete plugin;
}

boatinfo_pi::~boatinfo_pi() = default;

int boatinfo_pi::Init() {
  LoadConfiguration();

  m_auiManager = GetFrameAuiManager();
  if (!m_auiManager || !m_auiManager->GetManagedWindow()) {
    return 0;
  }

  wxWindow* auiParent = m_auiManager->GetManagedWindow();
  m_panel = new wxWindow(auiParent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                         wxFULL_REPAINT_ON_RESIZE);
  BuildMainPanel();

  wxAuiPaneInfo pane;
  pane.Name(wxT("BoatInfoPanel"))
      .Caption(wxT("BoatInfo"))
      .Right()
      .BestSize(m_panel->FromDIP(340), -1)
      .MinSize(m_panel->FromDIP(240), -1)
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
         WANTS_PLUGIN_MESSAGING | WANTS_PREFERENCES | WANTS_CONFIG;
}

void boatinfo_pi::BuildMainPanel() {
  if (!m_panel) return;

  const int outer = m_panel->FromDIP(12);
  const int gap = m_panel->FromDIP(8);
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

  wxStaticText* title = new wxStaticText(m_panel, wxID_ANY, wxT("BoatInfo"));
  wxFont titleFont = title->GetFont();
  titleFont.SetWeight(wxFONTWEIGHT_BOLD);
  titleFont.SetPointSize(titleFont.GetPointSize() + 2);
  title->SetFont(titleFont);
  root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, outer);

  wxStaticText* subtitle =
      new wxStaticText(m_panel, wxID_ANY, wxT("Own-vessel data"));
  root->Add(subtitle, 0, wxLEFT | wxRIGHT | wxTOP, outer);

  m_instrumentGrid = new wxFlexGridSizer(1, gap, gap);
  m_instrumentGrid->AddGrowableCol(0, 1);
  root->Add(m_instrumentGrid, 1, wxEXPAND | wxALL, outer);

  wxBoxSizer* footer = new wxBoxSizer(wxHORIZONTAL);
  footer->Add(new wxStaticText(m_panel, wxID_ANY, wxT("Source:")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, m_panel->FromDIP(5));
  m_dataSourceValue = new wxStaticText(m_panel, wxID_ANY, wxT("Waiting"));
  footer->Add(m_dataSourceValue, 1, wxALIGN_CENTER_VERTICAL);
  root->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, outer);

  m_panel->SetSizer(root);
  RebuildInstrumentGrid();
  ApplyHostStyle();
}

void boatinfo_pi::RebuildInstrumentGrid() {
  if (!m_panel || !m_instrumentGrid) return;

  m_instrumentGrid->Clear(true);
  m_instruments.clear();
  m_emptyHint = nullptr;

  size_t visibleCount = 0;
  for (std::map<wxString, BoatInfoValue>::const_iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    const BoatInfoValue& value = it->second;
    if (!value.visible || value.primitive == BoatInfoValue::PRIMITIVE_NONE) {
      continue;
    }
    BoatInfoInstrumentPanel* panel =
        new BoatInfoInstrumentPanel(m_panel, value);
    m_instrumentGrid->Add(panel, 0, wxEXPAND);
    m_instruments[value.key] = panel;
    ++visibleCount;
  }

  if (visibleCount == 0) {
    m_emptyHint = new wxStaticText(
        m_panel, wxID_ANY,
        wxT("No values selected. Open BoatInfo preferences to choose from "
            "values observed on this vessel."));
    m_emptyHint->Wrap(m_panel->FromDIP(290));
    m_instrumentGrid->Add(m_emptyHint, 0, wxEXPAND | wxALL,
                          m_panel->FromDIP(6));
  }

  ApplyHostStyle();
  m_panel->Layout();
}

void boatinfo_pi::ApplyHostStyle() {
  if (!m_panel) return;

  wxColour background, text, secondary, accent;
  ResolveHostColours(background, text, secondary, accent);
  m_panel->SetBackgroundColour(background);
  m_panel->SetForegroundColour(text);

  const wxWindowList& children = m_panel->GetChildren();
  for (wxWindowList::compatibility_iterator node = children.GetFirst(); node;
       node = node->GetNext()) {
    wxWindow* child = node->GetData();
    child->SetBackgroundColour(background);
    child->SetForegroundColour(text);
    child->Refresh(false);
  }
  m_panel->Refresh(false);
}

void boatinfo_pi::SetColorScheme(PI_ColorScheme cs) {
  m_colorScheme = cs;
  ApplyHostStyle();
}

BoatInfoValue& boatinfo_pi::EnsureValue(const wxString& key,
                                        const wxString& source,
                                        const wxString& path) {
  std::map<wxString, BoatInfoValue>::iterator found = m_values.find(key);
  if (found != m_values.end()) {
    if (found->second.source.IsEmpty()) found->second.source = source;
    if (found->second.path.IsEmpty()) found->second.path = path;
    return found->second;
  }

  BoatInfoValue value;
  value.key = key;
  value.source = source;
  value.path = path;
  value.suggestedLabel = SuggestedLabelFromPath(path);
  value.label = value.suggestedLabel;
  ApplySemanticDefaults(value);
  std::pair<std::map<wxString, BoatInfoValue>::iterator, bool> inserted =
      m_values.insert(std::make_pair(key, value));
  return inserted.first->second;
}

void boatinfo_pi::ApplySemanticDefaults(BoatInfoValue& value) {
  const wxString path = value.path.Lower();
  if (value.suggestedLabel.IsEmpty()) {
    value.suggestedLabel = SuggestedLabelFromPath(value.path);
  }
  if (value.label.IsEmpty()) value.label = value.suggestedLabel;

  if (value.source == wxT("OpenCPN")) {
    if (path == wxT("navigation.speedoverground")) {
      value.suggestedLabel = value.label = wxT("SOG");
      value.unit = wxT("kn");
      value.visible = true;
    } else if (path == wxT("navigation.courseovergroundtrue")) {
      value.suggestedLabel = value.label = wxT("COG");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
      value.visible = true;
    } else if (path == wxT("navigation.headingtrue")) {
      value.suggestedLabel = value.label = wxT("Heading");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
    } else if (path == wxT("navigation.headingmagnetic")) {
      value.suggestedLabel = value.label = wxT("Heading magnetic");
      value.unit = wxT("°");
      value.qualifier = wxT("M");
      value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
    } else if (path == wxT("navigation.position.latitude")) {
      value.suggestedLabel = value.label = wxT("Latitude");
      value.unit = wxT("°");
    } else if (path == wxT("navigation.position.longitude")) {
      value.suggestedLabel = value.label = wxT("Longitude");
      value.unit = wxT("°");
    }
    return;
  }

  if (path.Find(wxT("stateofcharge")) != wxNOT_FOUND) {
    value.suggestedLabel = wxT("Battery state of charge");
    if (path.Find(wxT("service")) != wxNOT_FOUND)
      value.suggestedLabel = wxT("Service battery");
    value.label = value.suggestedLabel;
    value.unit = wxT("%");
    value.primitive = BoatInfoValue::PRIMITIVE_LEVEL;
    value.bounded = true;
    value.minimum = 0.0;
    value.maximum = 100.0;
    value.visible = true;
  } else if (path.Find(wxT("currentlevel")) != wxNOT_FOUND) {
    value.unit = wxT("%");
    value.primitive = BoatInfoValue::PRIMITIVE_LEVEL;
    value.bounded = true;
    value.minimum = 0.0;
    value.maximum = 100.0;
  } else if (path.EndsWith(wxT(".voltage"))) {
    value.unit = wxT("V");
    value.visible = path.Find(wxT("batteries")) != wxNOT_FOUND;
  } else if (path.EndsWith(wxT(".current"))) {
    value.unit = wxT("A");
    value.visible = path.Find(wxT("batteries")) != wxNOT_FOUND;
  } else if (path.EndsWith(wxT(".power"))) {
    value.unit = wxT("W");
  } else if (path.Find(wxT("timeremaining")) != wxNOT_FOUND) {
    value.unit = wxT("h");
  } else if (path.Find(wxT("speed")) != wxNOT_FOUND) {
    value.unit = wxT("kn");
  } else if (path.Find(wxT("heading")) != wxNOT_FOUND ||
             path.Find(wxT("course")) != wxNOT_FOUND ||
             path.Find(wxT("bearing")) != wxNOT_FOUND ||
             path.Find(wxT("angle")) != wxNOT_FOUND) {
    value.unit = wxT("°");
    value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
  } else if (path.Find(wxT("depth")) != wxNOT_FOUND) {
    value.unit = wxT("m");
  } else if (path.Find(wxT("temperature")) != wxNOT_FOUND) {
    value.unit = wxT("°C");
  } else if (path.Find(wxT("revolutions")) != wxNOT_FOUND) {
    value.unit = wxT("rpm");
  }
}

void boatinfo_pi::ObserveNumeric(const wxString& key, const wxString& source,
                                 const wxString& path, double rawValue) {
  BoatInfoValue& value = EnsureValue(key, source, path);
  value.value = rawValue;
  value.displayValue = rawValue;
  value.hasNumericValue = true;
  value.textValue.clear();
  value.valid = std::isfinite(rawValue);
  if (!value.valid) {
    UpdateInstrument(key);
    return;
  }

  const wxString lower = path.Lower();
  if (source == wxT("Signal K")) {
    if (lower.Find(wxT("stateofcharge")) != wxNOT_FOUND ||
        lower.Find(wxT("currentlevel")) != wxNOT_FOUND) {
      value.displayValue = rawValue * 100.0;
      if (rawValue < 0.0 || rawValue > 1.0) value.valid = false;
    } else if (lower.Find(wxT("timeremaining")) != wxNOT_FOUND) {
      value.displayValue = rawValue / 3600.0;
    } else if (lower.Find(wxT("speed")) != wxNOT_FOUND) {
      value.displayValue = rawValue * kMsToKnots;
    } else if (lower.Find(wxT("heading")) != wxNOT_FOUND ||
               lower.Find(wxT("course")) != wxNOT_FOUND ||
               lower.Find(wxT("bearing")) != wxNOT_FOUND ||
               lower.Find(wxT("angle")) != wxNOT_FOUND) {
      value.displayValue = rawValue * kRadToDeg;
      while (value.displayValue < 0.0) value.displayValue += 360.0;
      while (value.displayValue >= 360.0) value.displayValue -= 360.0;
    } else if (lower.Find(wxT("temperature")) != wxNOT_FOUND) {
      value.displayValue = rawValue - 273.15;
    } else if (lower.Find(wxT("revolutions")) != wxNOT_FOUND) {
      value.displayValue = rawValue * 60.0;
    }
  }

  value.trend.push_back(value.displayValue);
  if (value.trend.size() > 60) value.trend.erase(value.trend.begin());
  UpdateInstrument(key);
}

void boatinfo_pi::ObserveText(const wxString& key, const wxString& source,
                              const wxString& path, const wxString& text) {
  BoatInfoValue& value = EnsureValue(key, source, path);
  value.hasNumericValue = false;
  value.textValue = text;
  value.valid = !text.IsEmpty();
  UpdateInstrument(key);
}

void boatinfo_pi::UpdateInstrument(const wxString& key) {
  std::map<wxString, BoatInfoInstrumentPanel*>::iterator widget =
      m_instruments.find(key);
  std::map<wxString, BoatInfoValue>::iterator value = m_values.find(key);
  if (widget != m_instruments.end() && value != m_values.end()) {
    widget->second->SetModel(value->second);
  }
}

void boatinfo_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  if (std::isfinite(pfix.Lat) && std::fabs(pfix.Lat) <= 90.0) {
    ObserveNumeric(wxT("opencpn:navigation.position.latitude"), wxT("OpenCPN"),
                   wxT("navigation.position.latitude"), pfix.Lat);
  }
  if (std::isfinite(pfix.Lon) && std::fabs(pfix.Lon) <= 180.0) {
    ObserveNumeric(wxT("opencpn:navigation.position.longitude"), wxT("OpenCPN"),
                   wxT("navigation.position.longitude"), pfix.Lon);
  }
  if (std::isfinite(pfix.Cog)) {
    double cog = std::fmod(pfix.Cog, 360.0);
    if (cog < 0.0) cog += 360.0;
    ObserveNumeric(wxT("opencpn:navigation.courseOverGroundTrue"),
                   wxT("OpenCPN"), wxT("navigation.courseOverGroundTrue"), cog);
  }
  if (std::isfinite(pfix.Sog) && pfix.Sog >= 0.0) {
    ObserveNumeric(wxT("opencpn:navigation.speedOverGround"), wxT("OpenCPN"),
                   wxT("navigation.speedOverGround"), pfix.Sog);
  }
  if (std::isfinite(pfix.Hdt)) {
    ObserveNumeric(wxT("opencpn:navigation.headingTrue"), wxT("OpenCPN"),
                   wxT("navigation.headingTrue"), pfix.Hdt);
  }
  if (std::isfinite(pfix.Hdm)) {
    ObserveNumeric(wxT("opencpn:navigation.headingMagnetic"), wxT("OpenCPN"),
                   wxT("navigation.headingMagnetic"), pfix.Hdm);
  }
}

void boatinfo_pi::SetNMEASentence(wxString& sentence) {
  if (sentence.length() >= 6 && sentence[0] == wxT('$') &&
      sentence.Mid(3, 3) == wxT("XDR")) {
    if (m_dataSourceValue) m_dataSourceValue->SetLabel(wxT("NMEA XDR"));
  }
}

void boatinfo_pi::SetPluginMessage(wxString& message_id,
                                   wxString& message_body) {
  if (message_id == wxT("OCPN_CORE_SIGNALK") && ParseSignalK(message_body)) {
    if (m_dataSourceValue) m_dataSourceValue->SetLabel(wxT("Signal K"));
  }
}

bool boatinfo_pi::ParseSignalK(const wxString& message) {
  wxJSONValue root;
  wxJSONReader reader;
  if (reader.Parse(message, &root) != 0 || !root.IsObject()) return false;

  if (root.HasMember(wxT("self")) && root[wxT("self")].IsString()) {
    const wxString self = NormalizeSignalKSelf(root[wxT("self")].AsString());
    if (!self.IsEmpty()) m_signalKSelf = self;
  }

  if (root.HasMember(wxT("context")) && root[wxT("context")].IsString()) {
    const wxString context = root[wxT("context")].AsString();
    if (!m_signalKSelf.IsEmpty() && context != m_signalKSelf) return false;
  }

  if (!root.HasMember(wxT("updates")) || !root[wxT("updates")].IsArray())
    return false;

  bool handled = false;
  wxJSONValue& updates = root[wxT("updates")];
  for (int i = 0; i < updates.Size(); ++i) {
    wxJSONValue& update = updates[i];
    if (!update.IsObject() || !update.HasMember(wxT("values")) ||
        !update[wxT("values")].IsArray())
      continue;

    wxJSONValue& values = update[wxT("values")];
    for (int j = 0; j < values.Size(); ++j) {
      wxJSONValue& item = values[j];
      if (!item.IsObject() || !item.HasMember(wxT("path")) ||
          !item[wxT("path")].IsString() || !item.HasMember(wxT("value")))
        continue;
      handled = ObserveSignalKPath(item[wxT("path")].AsString(),
                                   item[wxT("value")]) || handled;
    }
  }
  return handled;
}

bool boatinfo_pi::ObserveSignalKPath(const wxString& path,
                                     const wxJSONValue& jsonValue) {
  const wxString key = wxT("signalk:") + path;
  double number = 0.0;
  if (JsonNumber(jsonValue, number)) {
    const bool existed = m_values.find(key) != m_values.end();
    ObserveNumeric(key, wxT("Signal K"), path, number);
    if (!existed) RebuildInstrumentGrid();
    return true;
  }
  if (jsonValue.IsString()) {
    const bool existed = m_values.find(key) != m_values.end();
    ObserveText(key, wxT("Signal K"), path, jsonValue.AsString());
    if (!existed) RebuildInstrumentGrid();
    return true;
  }
  if (jsonValue.IsBool()) {
    const bool existed = m_values.find(key) != m_values.end();
    ObserveText(key, wxT("Signal K"), path,
                jsonValue.AsBool() ? wxT("ON") : wxT("OFF"));
    if (!existed) RebuildInstrumentGrid();
    return true;
  }
  return false;
}

void boatinfo_pi::ShowPreferencesDialog(wxWindow* parent) {
  wxDialog dialog(parent, wxID_ANY, wxT("BoatInfo preferences"),
                  wxDefaultPosition, wxSize(760, 560),
                  wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

  wxStaticText* intro = new wxStaticText(
      &dialog, wxID_ANY,
      wxT("BoatInfo learns scalar own-vessel values as they are observed. "
          "Choose what is shown, edit the suggested label and select a digital "
          "presentation. Unknown Signal K values appear here automatically."));
  intro->Wrap(dialog.FromDIP(710));
  root->Add(intro, 0, wxEXPAND | wxALL, dialog.FromDIP(12));

  wxScrolledWindow* scroll = new wxScrolledWindow(
      &dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
  scroll->SetScrollRate(0, dialog.FromDIP(10));
  wxFlexGridSizer* grid = new wxFlexGridSizer(5, dialog.FromDIP(6),
                                             dialog.FromDIP(10));
  grid->AddGrowableCol(2, 1);

  wxFont headerFont = dialog.GetFont();
  headerFont.SetWeight(wxFONTWEIGHT_BOLD);
  const wxString headers[] = {wxT("Show"), wxT("Source"), wxT("Name"),
                              wxT("Display"), wxT("Path")};
  for (size_t i = 0; i < 5; ++i) {
    wxStaticText* header = new wxStaticText(scroll, wxID_ANY, headers[i]);
    header->SetFont(headerFont);
    grid->Add(header, 0, wxALIGN_CENTER_VERTICAL);
  }

  std::vector<PreferenceRow> rows;
  for (std::map<wxString, BoatInfoValue>::iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    BoatInfoValue& value = it->second;
    PreferenceRow row;
    row.key = value.key;
    row.visible = new wxCheckBox(scroll, wxID_ANY, wxEmptyString);
    row.visible->SetValue(value.visible);
    grid->Add(row.visible, 0, wxALIGN_CENTER);
    grid->Add(new wxStaticText(scroll, wxID_ANY, value.source), 0,
              wxALIGN_CENTER_VERTICAL);
    row.label = new wxTextCtrl(scroll, wxID_ANY,
                               value.label.IsEmpty() ? value.suggestedLabel
                                                     : value.label);
    grid->Add(row.label, 1, wxEXPAND);
    wxArrayString choices;
    choices.Add(wxT("Value"));
    choices.Add(wxT("Level"));
    choices.Add(wxT("Tape"));
    choices.Add(wxT("Trend"));
    choices.Add(wxT("None"));
    row.primitive = new wxChoice(scroll, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, choices);
    row.primitive->SetSelection(SelectionFromPrimitive(value.primitive));
    grid->Add(row.primitive, 0, wxEXPAND);
    wxStaticText* path = new wxStaticText(scroll, wxID_ANY, value.path);
    path->SetToolTip(value.path);
    grid->Add(path, 0, wxALIGN_CENTER_VERTICAL);
    rows.push_back(row);
  }

  if (rows.empty()) {
    grid->Add(new wxStaticText(
                  scroll, wxID_ANY,
                  wxT("No values observed yet. Leave this dialog open or return "
                      "after OpenCPN/Signal K has delivered vessel data.")),
              0, wxEXPAND | wxALL, dialog.FromDIP(8));
  }

  scroll->SetSizer(grid);
  root->Add(scroll, 1, wxEXPAND | wxLEFT | wxRIGHT, dialog.FromDIP(12));

  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  buttons->AddButton(new wxButton(&dialog, wxID_CANCEL));
  buttons->AddButton(new wxButton(&dialog, wxID_OK));
  buttons->Realize();
  root->Add(buttons, 0, wxEXPAND | wxALL, dialog.FromDIP(12));
  dialog.SetSizer(root);
  dialog.Layout();
  dialog.CentreOnParent();

  if (dialog.ShowModal() == wxID_OK) {
    ApplyPreferenceRows(rows);
    SaveConfiguration();
    RebuildInstrumentGrid();
  }
}

void boatinfo_pi::ApplyPreferenceRows(const std::vector<PreferenceRow>& rows) {
  for (size_t i = 0; i < rows.size(); ++i) {
    std::map<wxString, BoatInfoValue>::iterator found =
        m_values.find(rows[i].key);
    if (found == m_values.end()) continue;
    found->second.visible = rows[i].visible->GetValue();
    found->second.label = rows[i].label->GetValue();
    if (found->second.label.IsEmpty())
      found->second.label = found->second.suggestedLabel;
    found->second.primitive =
        PrimitiveFromSelection(rows[i].primitive->GetSelection());
    found->second.userConfigured = true;
  }
}

void boatinfo_pi::LoadConfiguration() {
  if (m_configLoaded) return;
  m_configLoaded = true;
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  config->SetPath(wxT("/Plugins/BoatInfo"));
  long count = 0;
  config->Read(wxT("InstrumentCount"), &count, 0L);
  for (long i = 0; i < count; ++i) {
    config->SetPath(wxString::Format(wxT("/Plugins/BoatInfo/Instrument%ld"), i));
    wxString key, source, path, label;
    long primitive = 0;
    bool visible = false;
    if (!config->Read(wxT("Key"), &key) || key.IsEmpty()) continue;
    config->Read(wxT("Source"), &source, wxEmptyString);
    config->Read(wxT("Path"), &path, wxEmptyString);
    config->Read(wxT("Label"), &label, wxEmptyString);
    config->Read(wxT("Visible"), &visible, false);
    config->Read(wxT("Primitive"), &primitive, 0L);
    BoatInfoValue& value = EnsureValue(key, source, path);
    if (!label.IsEmpty()) value.label = label;
    value.visible = visible;
    if (primitive >= BoatInfoValue::PRIMITIVE_VALUE &&
        primitive <= BoatInfoValue::PRIMITIVE_NONE) {
      value.primitive = static_cast<BoatInfoValue::Primitive>(primitive);
    }
    value.userConfigured = true;
    value.valid = false;
  }
  config->SetPath(wxT("/Plugins/BoatInfo"));
}

void boatinfo_pi::SaveConfiguration() {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  config->DeleteGroup(wxT("/Plugins/BoatInfo"));
  config->SetPath(wxT("/Plugins/BoatInfo"));
  config->Write(wxT("InstrumentCount"), static_cast<long>(m_values.size()));
  long index = 0;
  for (std::map<wxString, BoatInfoValue>::const_iterator it = m_values.begin();
       it != m_values.end(); ++it, ++index) {
    const BoatInfoValue& value = it->second;
    config->SetPath(
        wxString::Format(wxT("/Plugins/BoatInfo/Instrument%ld"), index));
    config->Write(wxT("Key"), value.key);
    config->Write(wxT("Source"), value.source);
    config->Write(wxT("Path"), value.path);
    config->Write(wxT("Label"), value.label);
    config->Write(wxT("Visible"), value.visible);
    config->Write(wxT("Primitive"), static_cast<long>(value.primitive));
  }
  config->SetPath(wxT("/Plugins/BoatInfo"));
  config->Flush();
}

void boatinfo_pi::ClearControlPointers() {
  m_instrumentGrid = nullptr;
  m_emptyHint = nullptr;
  m_dataSourceValue = nullptr;
  m_instruments.clear();
}

bool boatinfo_pi::DeInit() {
  SaveConfiguration();
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

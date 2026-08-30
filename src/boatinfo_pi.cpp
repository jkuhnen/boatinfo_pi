#include "boatinfo_pi.h"

#include <algorithm>
#include <cmath>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/fileconf.h>
#include <wx/jsonreader.h>
#include <wx/jsonval.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

#ifdef _WIN32
#define BOATINFO_EXPORT __declspec(dllexport)
#else
#define BOATINFO_EXPORT
#endif

namespace {
const double kRadToDeg = 180.0 / 3.14159265358979323846;
const double kMsToKnots = 1.9438444924406;
const double kCoulombToAh = 1.0 / 3600.0;

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

long long NowSeconds() {
  return static_cast<long long>(wxDateTime::Now().GetTicks());
}

wxString NormalizeSignalKSelf(const wxString& self) {
  if (self.IsEmpty() || self.StartsWith(wxT("vessels."))) return self;
  return wxT("vessels.") + self;
}

wxString HumanizeToken(const wxString& token) {
  if (token.IsEmpty()) return token;
  wxString out;
  for (size_t i = 0; i < token.length(); ++i) {
    const wxChar c = token[i];
    const bool upper = c >= wxT('A') && c <= wxT('Z');
    const bool previousLower =
        i > 0 && token[i - 1] >= wxT('a') && token[i - 1] <= wxT('z');
    if (upper && previousLower) out += wxT(' ');
    out += (c == wxT('_') || c == wxT('-')) ? wxT(' ') : c;
  }
  if (!out.IsEmpty() && out[0] >= wxT('a') && out[0] <= wxT('z'))
    out[0] = static_cast<wxChar>(out[0] - wxT('a') + wxT('A'));
  return out;
}

wxString SuggestedLabelFromPath(const wxString& path) {
  const int dot = path.Find(wxT('.'), true);
  return HumanizeToken(dot == wxNOT_FOUND ? path : path.Mid(dot + 1));
}

wxString PathSegmentAfter(const wxString& path, const wxString& prefix) {
  if (!path.StartsWith(prefix)) return wxEmptyString;
  const wxString remainder = path.Mid(prefix.length());
  const int dot = remainder.Find(wxT('.'));
  return dot == wxNOT_FOUND ? remainder : remainder.Left(dot);
}

wxString BatteryName(const wxString& path) {
  const wxString id = PathSegmentAfter(path, wxT("electrical.batteries."));
  const wxString lower = id.Lower();
  if (lower == wxT("service") || lower == wxT("house") ||
      lower == wxT("housebank"))
    return wxT("Service battery");
  if (lower == wxT("starter") || lower == wxT("start"))
    return wxT("Starter battery");
  if (id.IsEmpty()) return wxT("Battery");
  return HumanizeToken(id) + wxT(" battery");
}

wxString TankName(const wxString& path) {
  if (!path.StartsWith(wxT("tanks."))) return wxT("Tank");
  wxString remainder = path.Mid(6);
  const int firstDot = remainder.Find(wxT('.'));
  const wxString type = firstDot == wxNOT_FOUND ? remainder
                                                : remainder.Left(firstDot);
  remainder = firstDot == wxNOT_FOUND ? wxEmptyString
                                      : remainder.Mid(firstDot + 1);
  const int secondDot = remainder.Find(wxT('.'));
  const wxString id = secondDot == wxNOT_FOUND ? remainder
                                               : remainder.Left(secondDot);
  wxString label = HumanizeToken(type);
  if (!id.IsEmpty() && id != wxT("0")) label += wxT(" ") + HumanizeToken(id);
  return label;
}

bool IsLegacyAutoLabel(const wxString& label) {
  const wxString lower = label.Lower();
  return lower == wxT("voltage") || lower == wxT("current") ||
         lower == wxT("power") || lower == wxT("service battery") ||
         lower == wxT("course over ground true") ||
         lower == wxT("speed over ground") ||
         lower == wxT("discharge since full") ||
         lower == wxT("time remaining") ||
         lower == wxT("antenna altitude") || lower == wxT("datetime") ||
         lower == wxT("latitude") || lower == wxT("longitude") ||
         lower == wxT("cog") || lower == wxT("sog");
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
    default:
      return 0;
  }
}

int CategoryRank(const wxString& category) {
  if (category == wxT("Vessel")) return 0;
  if (category == wxT("Navigation")) return 1;
  if (category == wxT("Electrical")) return 2;
  if (category == wxT("Tanks")) return 3;
  if (category == wxT("Propulsion")) return 4;
  if (category == wxT("Environment")) return 5;
  return 6;
}

bool ValueOrder(const BoatInfoValue* a, const BoatInfoValue* b) {
  const int aRank = CategoryRank(a->category);
  const int bRank = CategoryRank(b->category);
  if (aRank != bRank) return aRank < bRank;
  const wxString aLabel = a->label.IsEmpty() ? a->suggestedLabel : a->label;
  const wxString bLabel = b->label.IsEmpty() ? b->suggestedLabel : b->label;
  if (aLabel.CmpNoCase(bLabel) != 0) return aLabel.CmpNoCase(bLabel) < 0;
  return a->key < b->key;
}

wxString FormatNumber(const BoatInfoValue& value) {
  if (!value.valid || !value.hasNumericValue) return wxT("—");
  const wxString lowerPath = value.path.Lower();
  if (lowerPath.EndsWith(wxT("latitude")))
    return toSDMM_PlugIn(1, value.displayValue, true);
  if (lowerPath.EndsWith(wxT("longitude")))
    return toSDMM_PlugIn(2, value.displayValue, true);
  if (value.unit == wxT("%") || value.unit == wxT("rpm"))
    return wxString::Format(wxT("%.0f"), value.displayValue);
  if (value.unit == wxT("V") || value.unit == wxT("A"))
    return wxString::Format(wxT("%.2f"), value.displayValue);
  if (value.unit == wxT("Ah"))
    return wxString::Format(wxT("%.1f"), value.displayValue);
  return wxString::Format(wxT("%.1f"), value.displayValue);
}

void ResolveHostColours(wxColour& background, wxColour& text,
                        wxColour& secondary, wxColour& accent) {
  if (!GetGlobalColor(wxT("DILG1"), &background))
    background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  if (!GetGlobalColor(wxT("DILG3"), &text))
    text = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
  if (!GetGlobalColor(wxT("DILG2"), &secondary))
    secondary = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
  if (!GetGlobalColor(wxT("UIBCK"), &accent)) accent = text;
}

void ApplyColoursRecursive(wxWindow* window, const wxColour& background,
                           const wxColour& text) {
  if (!window) return;
  window->SetBackgroundColour(background);
  window->SetForegroundColour(text);
  const wxWindowList& children = window->GetChildren();
  for (wxWindowList::compatibility_iterator node = children.GetFirst(); node;
       node = node->GetNext())
    ApplyColoursRecursive(node->GetData(), background, text);
  window->Refresh(false);
}

wxString UnitWithQualifier(const BoatInfoValue& value) {
  if (value.qualifier.IsEmpty()) return value.unit;
  if (value.unit.IsEmpty()) return value.qualifier;
  return value.unit + wxT(" ") + value.qualifier;
}
}  // namespace

class BoatInfoInstrumentPanel : public wxPanel {
public:
  BoatInfoInstrumentPanel(wxWindow* parent, const BoatInfoValue& model)
      : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                wxBORDER_NONE),
        m_model(model) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    UpdateMinimumHeight();
    Bind(wxEVT_PAINT, &BoatInfoInstrumentPanel::OnPaint, this);
  }

  void SetModel(const BoatInfoValue& model) {
    m_model = model;
    UpdateMinimumHeight();
    Refresh(false);
  }

private:
  void UpdateMinimumHeight() {
    int height = 44;
    if (m_model.primitive == BoatInfoValue::PRIMITIVE_NONE) height = 28;
    if (m_model.primitive == BoatInfoValue::PRIMITIVE_LEVEL) height = 56;
    if (m_model.primitive == BoatInfoValue::PRIMITIVE_TAPE) height = 72;
    if (m_model.primitive == BoatInfoValue::PRIMITIVE_TREND) height = 62;
    SetMinSize(wxSize(-1, FromDIP(height)));
  }

  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    wxColour background, text, secondary, accent;
    ResolveHostColours(background, text, secondary, accent);
    dc.SetBackground(wxBrush(background));
    dc.Clear();

    const wxSize size = GetClientSize();
    const int pad = FromDIP(5);
    const wxString label =
        m_model.label.IsEmpty() ? m_model.suggestedLabel : m_model.label;
    wxString rendered = m_model.hasNumericValue ? FormatNumber(m_model)
                                                 : m_model.textValue;
    if (!m_model.valid || rendered.IsEmpty()) rendered = wxT("—");
    const wxString unit = UnitWithQualifier(m_model);

    wxFont labelFont = GetFont();
    labelFont.SetPointSize(std::max(8, labelFont.GetPointSize() - 1));
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);

    if (m_model.primitive == BoatInfoValue::PRIMITIVE_NONE) {
      dc.SetFont(labelFont);
      dc.SetTextForeground(text);
      dc.DrawText(label, pad, FromDIP(5));

      wxString compact = rendered;
      if (m_model.valid && !unit.IsEmpty()) compact += wxT(" ") + unit;
      wxFont compactFont = GetFont();
      compactFont.SetPointSize(std::max(8, compactFont.GetPointSize() - 1));
      dc.SetFont(compactFont);
      dc.SetTextForeground(m_model.stale ? secondary : text);
      wxCoord width = 0, height = 0;
      dc.GetTextExtent(compact, &width, &height);
      dc.DrawText(compact, std::max(pad, size.x - pad - static_cast<int>(width)),
                  FromDIP(5));
      return;
    }

    dc.SetFont(labelFont);
    dc.SetTextForeground(text);
    dc.DrawText(label, pad, 0);

    wxFont valueFont = GetFont();
    valueFont.SetPointSize(std::max(12, valueFont.GetPointSize() + 4));
    valueFont.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(valueFont);
    dc.SetTextForeground(m_model.stale ? secondary : text);
    dc.DrawText(rendered, pad, FromDIP(14));

    wxCoord valueWidth = 0, valueHeight = 0;
    dc.GetTextExtent(rendered, &valueWidth, &valueHeight);
    if (m_model.valid && !unit.IsEmpty() &&
        !m_model.path.Lower().EndsWith(wxT("latitude")) &&
        !m_model.path.Lower().EndsWith(wxT("longitude"))) {
      wxFont unitFont = GetFont();
      unitFont.SetPointSize(std::max(8, unitFont.GetPointSize() - 1));
      dc.SetFont(unitFont);
      dc.SetTextForeground(text);
      dc.DrawText(unit, pad + valueWidth + FromDIP(4), FromDIP(22));
    }

    if (!m_model.valid || m_model.stale) {
      wxFont stateFont = GetFont();
      stateFont.SetPointSize(std::max(8, stateFont.GetPointSize() - 1));
      dc.SetFont(stateFont);
      dc.SetTextForeground(secondary);
      dc.DrawText(m_model.stale ? wxT("STALE") : wxT("NO DATA"),
                  std::max(pad, size.x - FromDIP(48)), 0);
    }
    if (!m_model.valid) return;

    const int graphTop = FromDIP(38);
    if (m_model.primitive == BoatInfoValue::PRIMITIVE_LEVEL &&
        m_model.hasNumericValue && m_model.maximum > m_model.minimum) {
      const int y = graphTop + FromDIP(1);
      const int width = std::max(1, size.x - 2 * pad);
      const int height = FromDIP(5);
      dc.SetPen(wxPen(secondary));
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.DrawRectangle(pad, y, width, height);
      double fraction = (m_model.displayValue - m_model.minimum) /
                        (m_model.maximum - m_model.minimum);
      fraction = std::max(0.0, std::min(1.0, fraction));
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(m_model.stale ? secondary : accent));
      dc.DrawRectangle(pad, y, static_cast<int>(width * fraction), height);
    } else if (m_model.primitive == BoatInfoValue::PRIMITIVE_TAPE &&
               m_model.hasNumericValue) {
      const int center = size.x / 2;
      const int y = graphTop + FromDIP(4);
      dc.SetPen(wxPen(secondary));
      dc.DrawLine(pad, y, size.x - pad, y);
      dc.SetPen(wxPen(m_model.stale ? secondary : text,
                      std::max(1, FromDIP(2))));
      dc.DrawLine(center, y - FromDIP(5), center, y + FromDIP(5));
      wxFont tapeFont = GetFont();
      tapeFont.SetPointSize(std::max(8, tapeFont.GetPointSize() - 1));
      dc.SetFont(tapeFont);
      dc.SetTextForeground(text);
      for (int offset = -2; offset <= 2; ++offset) {
        double tick = m_model.displayValue + offset * 10.0;
        while (tick < 0.0) tick += 360.0;
        while (tick >= 360.0) tick -= 360.0;
        const int x = center + offset * FromDIP(42);
        dc.DrawLine(x, y - FromDIP(3), x, y + FromDIP(3));
        dc.DrawText(wxString::Format(wxT("%.0f"), tick), x - FromDIP(8),
                    y + FromDIP(4));
      }
    } else if (m_model.primitive == BoatInfoValue::PRIMITIVE_TREND &&
               m_model.trend.size() > 1) {
      const int left = pad;
      const int right = size.x - pad;
      const int top = graphTop;
      const int bottom = size.y - FromDIP(4);
      double minValue = *std::min_element(m_model.trend.begin(),
                                          m_model.trend.end());
      double maxValue = *std::max_element(m_model.trend.begin(),
                                          m_model.trend.end());
      if (maxValue <= minValue) maxValue = minValue + 1.0;
      dc.SetPen(wxPen(m_model.stale ? secondary : accent));
      wxPoint previous;
      for (size_t i = 0; i < m_model.trend.size(); ++i) {
        const double fx = static_cast<double>(i) /
                          static_cast<double>(m_model.trend.size() - 1);
        const double fy = (m_model.trend[i] - minValue) /
                          (maxValue - minValue);
        const wxPoint point(
            left + static_cast<int>((right - left) * fx),
            bottom - static_cast<int>((bottom - top) * fy));
        if (i > 0) dc.DrawLine(previous.x, previous.y, point.x, point.y);
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

  BoatInfoValue& vesselName =
      EnsureValue(wxT("signalk:name"), wxT("Signal K"), wxT("name"));
  vesselName.visible = true;
  vesselName.primitive = BoatInfoValue::PRIMITIVE_NONE;
  BoatInfoValue& vesselMmsi =
      EnsureValue(wxT("signalk:mmsi"), wxT("Signal K"), wxT("mmsi"));
  vesselMmsi.visible = true;
  vesselMmsi.primitive = BoatInfoValue::PRIMITIVE_NONE;
  BoatInfoValue& vesselCallsign = EnsureValue(
      wxT("signalk:communication.callsignVhf"), wxT("Signal K"),
      wxT("communication.callsignVhf"));
  vesselCallsign.visible = true;
  vesselCallsign.primitive = BoatInfoValue::PRIMITIVE_NONE;

  m_auiManager = GetFrameAuiManager();
  if (!m_auiManager || !m_auiManager->GetManagedWindow()) return 0;

  m_panel = new wxWindow(m_auiManager->GetManagedWindow(), wxID_ANY,
                         wxDefaultPosition, wxDefaultSize,
                         wxFULL_REPAINT_ON_RESIZE);
  BuildMainPanel();

  m_staleTimer = new wxTimer(m_panel);
  m_panel->Bind(wxEVT_TIMER,
                [this](wxTimerEvent&) { RefreshFreshness(); },
                m_staleTimer->GetId());
  m_staleTimer->Start(2000);

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
    m_staleTimer->Stop();
    delete m_staleTimer;
    m_staleTimer = nullptr;
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
  const int outer = m_panel->FromDIP(7);
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

  wxStaticText* title = new wxStaticText(m_panel, wxID_ANY, wxT("BoatInfo"));
  wxFont titleFont = title->GetFont();
  titleFont.SetWeight(wxFONTWEIGHT_BOLD);
  titleFont.SetPointSize(titleFont.GetPointSize() + 1);
  title->SetFont(titleFont);
  root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, outer);

  m_instrumentScroll = new wxScrolledWindow(
      m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxVSCROLL | wxBORDER_NONE);
  m_instrumentScroll->SetScrollRate(0, m_panel->FromDIP(16));
  m_instrumentScroll->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_NEVER);
  m_instrumentGrid = new wxFlexGridSizer(1, m_panel->FromDIP(1), 0);
  m_instrumentGrid->AddGrowableCol(0, 1);
  m_instrumentScroll->SetSizer(m_instrumentGrid);
  root->Add(m_instrumentScroll, 1, wxEXPAND | wxLEFT | wxRIGHT,
            m_panel->FromDIP(3));

  wxBoxSizer* footer = new wxBoxSizer(wxHORIZONTAL);
  footer->Add(new wxStaticText(m_panel, wxID_ANY, wxT("Data:")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, m_panel->FromDIP(4));
  m_dataSourceValue = new wxStaticText(m_panel, wxID_ANY, wxT("Waiting"));
  footer->Add(m_dataSourceValue, 1, wxALIGN_CENTER_VERTICAL);
  root->Add(footer, 0, wxEXPAND | wxALL, outer);

  m_panel->SetSizer(root);
  RebuildInstrumentGrid();
  ApplyHostStyle();
}

void boatinfo_pi::RebuildInstrumentGrid() {
  if (!m_panel || !m_instrumentScroll || !m_instrumentGrid) return;
  m_instrumentGrid->Clear(true);
  m_instruments.clear();
  m_emptyHint = nullptr;

  std::vector<const BoatInfoValue*> visible;
  for (std::map<wxString, BoatInfoValue>::const_iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    if (it->second.visible) visible.push_back(&it->second);
  }
  std::sort(visible.begin(), visible.end(), ValueOrder);

  wxColour background, text, secondary, accent;
  ResolveHostColours(background, text, secondary, accent);
  wxString currentCategory;
  for (size_t i = 0; i < visible.size(); ++i) {
    const BoatInfoValue& value = *visible[i];
    if (value.category != currentCategory) {
      currentCategory = value.category;
      wxStaticText* category = new wxStaticText(
          m_instrumentScroll, wxID_ANY, currentCategory.Upper());
      wxFont categoryFont = category->GetFont();
      categoryFont.SetWeight(wxFONTWEIGHT_BOLD);
      category->SetFont(categoryFont);
      category->SetForegroundColour(text);
      m_instrumentGrid->Add(category, 0,
                            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
                            m_panel->FromDIP(5));

      wxStaticLine* divider =
          new wxStaticLine(m_instrumentScroll, wxID_ANY, wxDefaultPosition,
                           wxDefaultSize, wxLI_HORIZONTAL);
      divider->SetForegroundColour(secondary);
      m_instrumentGrid->Add(divider, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                            m_panel->FromDIP(5));
    }

    BoatInfoInstrumentPanel* panel =
        new BoatInfoInstrumentPanel(m_instrumentScroll, value);
    m_instrumentGrid->Add(panel, 0, wxEXPAND | wxLEFT | wxRIGHT,
                          m_panel->FromDIP(1));
    m_instruments[value.key] = panel;
  }

  if (visible.empty()) {
    m_emptyHint = new wxStaticText(
        m_instrumentScroll, wxID_ANY,
        wxT("No values selected. Open BoatInfo preferences to choose from "
            "values observed on this vessel."));
    m_emptyHint->Wrap(m_panel->FromDIP(290));
    m_instrumentGrid->Add(m_emptyHint, 0, wxEXPAND | wxALL,
                          m_panel->FromDIP(6));
  }

  ApplyHostStyle();
  m_instrumentScroll->Layout();
  m_instrumentScroll->FitInside();
  m_panel->Layout();
}

void boatinfo_pi::ApplyHostStyle() {
  if (!m_panel) return;
  wxColour background, text, secondary, accent;
  ResolveHostColours(background, text, secondary, accent);
  ApplyColoursRecursive(m_panel, background, text);
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
  return m_values.insert(std::make_pair(key, value)).first->second;
}

void boatinfo_pi::ApplySemanticDefaults(BoatInfoValue& value) {
  const wxString path = value.path.Lower();
  const bool configured = value.userConfigured;
  if (value.suggestedLabel.IsEmpty())
    value.suggestedLabel = SuggestedLabelFromPath(value.path);

  value.category = wxT("Technical");
  value.semanticId = path;
  value.alternativeSource = false;
  value.freshnessSeconds = 30;
  wxString suggestion = value.suggestedLabel;

  if (value.source == wxT("OpenCPN")) {
    value.category = wxT("Navigation");
    value.freshnessSeconds = 10;
    if (path == wxT("navigation.speedoverground")) {
      suggestion = wxT("SOG");
      value.semanticId = wxT("navigation.sog");
      value.unit = wxT("kn");
      if (!configured) value.visible = true;
    } else if (path == wxT("navigation.courseovergroundtrue")) {
      suggestion = wxT("COG");
      value.semanticId = wxT("navigation.cog.true");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
        value.visible = true;
      }
    } else if (path == wxT("navigation.headingtrue")) {
      suggestion = wxT("Heading");
      value.semanticId = wxT("navigation.heading.true");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      if (!configured) value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
    } else if (path == wxT("navigation.headingmagnetic")) {
      suggestion = wxT("Heading magnetic");
      value.semanticId = wxT("navigation.heading.magnetic");
      value.unit = wxT("°");
      value.qualifier = wxT("M");
      if (!configured) value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
    } else if (path == wxT("navigation.position.latitude")) {
      suggestion = wxT("Latitude");
      value.semanticId = wxT("navigation.latitude");
      value.unit.clear();
      if (!configured) value.visible = true;
    } else if (path == wxT("navigation.position.longitude")) {
      suggestion = wxT("Longitude");
      value.semanticId = wxT("navigation.longitude");
      value.unit.clear();
      if (!configured) value.visible = true;
    }
    value.suggestedLabel = suggestion;
    if (!value.labelCustomized) value.label = suggestion;
    return;
  }

  if (path == wxT("name")) {
    value.category = wxT("Vessel");
    suggestion = wxT("Vessel name");
    value.freshnessSeconds = 300;
    value.visible = true;
    value.primitive = BoatInfoValue::PRIMITIVE_NONE;
  } else if (path == wxT("mmsi")) {
    value.category = wxT("Vessel");
    suggestion = wxT("MMSI");
    value.freshnessSeconds = 300;
    value.visible = true;
    value.primitive = BoatInfoValue::PRIMITIVE_NONE;
  } else if (path == wxT("communication.callsignvhf")) {
    value.category = wxT("Vessel");
    suggestion = wxT("Call sign");
    value.freshnessSeconds = 300;
    value.visible = true;
    value.primitive = BoatInfoValue::PRIMITIVE_NONE;
  } else if (path.StartsWith(wxT("electrical.batteries."))) {
    value.category = wxT("Electrical");
    const wxString battery = BatteryName(path);
    if (path.Find(wxT("stateofcharge")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" SOC");
      value.unit = wxT("%");
      value.bounded = true;
      value.minimum = 0.0;
      value.maximum = 100.0;
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_LEVEL;
        value.visible = path.Find(wxT("service")) != wxNOT_FOUND ||
                        path.Find(wxT("house")) != wxNOT_FOUND;
      }
    } else if (path.Find(wxT("timeremaining")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" time remaining");
      value.unit = wxT("h");
    } else if (path.Find(wxT("dischargesincefull")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" discharge since full");
      value.unit = wxT("Ah");
    } else if (path.Find(wxT("lifetimedischarge")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" lifetime discharge");
      value.unit = wxT("Ah");
    } else if (path.Find(wxT("lifetimerecharge")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" lifetime recharge");
      value.unit = wxT("Ah");
    } else if (path.EndsWith(wxT(".voltage"))) {
      suggestion = battery + wxT(" voltage");
      value.unit = wxT("V");
      if (!configured)
        value.visible = path.Find(wxT("service")) != wxNOT_FOUND ||
                        path.Find(wxT("starter")) != wxNOT_FOUND;
    } else if (path.EndsWith(wxT(".current"))) {
      suggestion = battery + wxT(" current");
      value.unit = wxT("A");
      if (!configured)
        value.visible = path.Find(wxT("service")) != wxNOT_FOUND;
    } else if (path.EndsWith(wxT(".power"))) {
      suggestion = battery + wxT(" power");
      value.unit = wxT("W");
    } else if (path.EndsWith(wxT(".temperature"))) {
      suggestion = battery + wxT(" temperature");
      value.unit = wxT("°C");
    }
  } else if (path.StartsWith(wxT("tanks."))) {
    value.category = wxT("Tanks");
    const wxString tank = TankName(path);
    if (path.Find(wxT("currentlevel")) != wxNOT_FOUND) {
      suggestion = tank + wxT(" level");
      value.unit = wxT("%");
      value.bounded = true;
      value.minimum = 0.0;
      value.maximum = 100.0;
      if (!configured) value.primitive = BoatInfoValue::PRIMITIVE_LEVEL;
    }
  } else if (path.StartsWith(wxT("propulsion."))) {
    value.category = wxT("Propulsion");
    const wxString engine = PathSegmentAfter(path, wxT("propulsion."));
    const wxString engineName = engine.IsEmpty() ? wxT("Engine")
                                                  : HumanizeToken(engine);
    if (path.Find(wxT("revolutions")) != wxNOT_FOUND) {
      suggestion = engineName + wxT(" RPM");
      value.unit = wxT("rpm");
    } else if (path.Find(wxT("temperature")) != wxNOT_FOUND) {
      suggestion = engineName + wxT(" temperature");
      value.unit = wxT("°C");
    }
  } else if (path.StartsWith(wxT("environment."))) {
    value.category = wxT("Environment");
    if (path.Find(wxT("depth")) != wxNOT_FOUND) value.unit = wxT("m");
    if (path.Find(wxT("temperature")) != wxNOT_FOUND) value.unit = wxT("°C");
    if (path.Find(wxT("speed")) != wxNOT_FOUND) value.unit = wxT("kn");
    if (path.Find(wxT("angle")) != wxNOT_FOUND ||
        path.Find(wxT("direction")) != wxNOT_FOUND) {
      value.unit = wxT("°");
      if (!configured) value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
    }
  } else if (path.StartsWith(wxT("navigation.gnss."))) {
    value.category = wxT("Technical");
    if (path.EndsWith(wxT("antennaaltitude"))) {
      suggestion = wxT("GNSS antenna altitude");
      value.unit = wxT("m");
    }
  } else if (path.StartsWith(wxT("navigation."))) {
    value.category = wxT("Navigation");
    if (path == wxT("navigation.speedoverground")) {
      suggestion = wxT("SOG");
      value.semanticId = wxT("navigation.sog");
      value.unit = wxT("kn");
      value.alternativeSource = true;
      if (!configured) value.visible = false;
    } else if (path == wxT("navigation.courseovergroundtrue")) {
      suggestion = wxT("COG");
      value.semanticId = wxT("navigation.cog.true");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      value.alternativeSource = true;
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
        value.visible = false;
      }
    } else if (path == wxT("navigation.headingtrue")) {
      suggestion = wxT("Heading");
      value.semanticId = wxT("navigation.heading.true");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      value.alternativeSource = true;
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
        value.visible = false;
      }
    } else if (path == wxT("navigation.headingmagnetic")) {
      suggestion = wxT("Heading magnetic");
      value.semanticId = wxT("navigation.heading.magnetic");
      value.unit = wxT("°");
      value.qualifier = wxT("M");
      value.alternativeSource = true;
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
        value.visible = false;
      }
    } else if (path == wxT("navigation.position.latitude")) {
      suggestion = wxT("Latitude");
      value.semanticId = wxT("navigation.latitude");
      value.unit.clear();
      value.alternativeSource = true;
      if (!configured) value.visible = false;
    } else if (path == wxT("navigation.position.longitude")) {
      suggestion = wxT("Longitude");
      value.semanticId = wxT("navigation.longitude");
      value.unit.clear();
      value.alternativeSource = true;
      if (!configured) value.visible = false;
    } else if (path == wxT("navigation.datetime")) {
      value.category = wxT("Technical");
      suggestion = wxT("Navigation datetime");
      value.freshnessSeconds = 60;
    } else if (path.Find(wxT("speed")) != wxNOT_FOUND) {
      value.unit = wxT("kn");
    } else if (path.Find(wxT("heading")) != wxNOT_FOUND ||
               path.Find(wxT("course")) != wxNOT_FOUND ||
               path.Find(wxT("bearing")) != wxNOT_FOUND ||
               path.Find(wxT("angle")) != wxNOT_FOUND) {
      value.unit = wxT("°");
      if (!configured) value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
    }
  }

  value.suggestedLabel = suggestion;
  if (!value.labelCustomized) value.label = suggestion;
}

void boatinfo_pi::ObserveNumeric(const wxString& key, const wxString& source,
                                 const wxString& path, double rawValue) {
  const bool isNew = m_values.find(key) == m_values.end();
  BoatInfoValue& value = EnsureValue(key, source, path);
  value.value = rawValue;
  value.displayValue = rawValue;
  value.hasNumericValue = true;
  value.textValue.clear();
  value.valid = std::isfinite(rawValue);
  value.stale = false;
  value.lastUpdateSeconds = NowSeconds();

  const wxString lower = path.Lower();
  if (value.valid && source == wxT("Signal K")) {
    if (lower.Find(wxT("stateofcharge")) != wxNOT_FOUND ||
        lower.Find(wxT("currentlevel")) != wxNOT_FOUND) {
      value.displayValue = rawValue * 100.0;
      if (rawValue < 0.0 || rawValue > 1.0) value.valid = false;
    } else if (lower.Find(wxT("timeremaining")) != wxNOT_FOUND) {
      value.displayValue = rawValue / 3600.0;
    } else if (lower.Find(wxT("dischargesincefull")) != wxNOT_FOUND ||
               lower.Find(wxT("lifetimedischarge")) != wxNOT_FOUND ||
               lower.Find(wxT("lifetimerecharge")) != wxNOT_FOUND) {
      value.displayValue = rawValue * kCoulombToAh;
    } else if (lower.Find(wxT("speed")) != wxNOT_FOUND) {
      value.displayValue = rawValue * kMsToKnots;
    } else if (lower.Find(wxT("heading")) != wxNOT_FOUND ||
               lower.Find(wxT("course")) != wxNOT_FOUND ||
               lower.Find(wxT("bearing")) != wxNOT_FOUND ||
               lower.Find(wxT("angle")) != wxNOT_FOUND ||
               lower.Find(wxT("direction")) != wxNOT_FOUND) {
      value.displayValue = rawValue * kRadToDeg;
      while (value.displayValue < 0.0) value.displayValue += 360.0;
      while (value.displayValue >= 360.0) value.displayValue -= 360.0;
    } else if (lower.Find(wxT("temperature")) != wxNOT_FOUND) {
      value.displayValue = rawValue - 273.15;
    } else if (lower.Find(wxT("revolutions")) != wxNOT_FOUND) {
      value.displayValue = rawValue * 60.0;
    }
  }

  if (value.valid) {
    value.trend.push_back(value.displayValue);
    if (value.trend.size() > 60) value.trend.erase(value.trend.begin());
  }
  if (source == wxT("OpenCPN")) m_seenOpenCPN = true;
  if (source == wxT("Signal K")) m_seenSignalK = true;
  UpdateSourceSummary();
  if (isNew && m_panel) RebuildInstrumentGrid();
  UpdateInstrument(key);
}

void boatinfo_pi::ObserveText(const wxString& key, const wxString& source,
                              const wxString& path, const wxString& textValue) {
  const bool isNew = m_values.find(key) == m_values.end();
  BoatInfoValue& value = EnsureValue(key, source, path);
  value.hasNumericValue = false;
  value.textValue = textValue;
  value.valid = !textValue.IsEmpty();
  value.stale = false;
  value.lastUpdateSeconds = NowSeconds();
  if (source == wxT("Signal K")) m_seenSignalK = true;
  UpdateSourceSummary();
  if (isNew && m_panel) RebuildInstrumentGrid();
  UpdateInstrument(key);
}

void boatinfo_pi::ObserveNoData(const wxString& key, const wxString& source,
                                const wxString& path) {
  const bool isNew = m_values.find(key) == m_values.end();
  BoatInfoValue& value = EnsureValue(key, source, path);
  value.valid = false;
  value.stale = false;
  value.lastUpdateSeconds = NowSeconds();
  if (isNew && m_panel) RebuildInstrumentGrid();
  UpdateInstrument(key);
}

void boatinfo_pi::UpdateInstrument(const wxString& key) {
  std::map<wxString, BoatInfoInstrumentPanel*>::iterator widget =
      m_instruments.find(key);
  std::map<wxString, BoatInfoValue>::iterator value = m_values.find(key);
  if (widget != m_instruments.end() && value != m_values.end())
    widget->second->SetModel(value->second);
}

void boatinfo_pi::RefreshFreshness() {
  const long long now = NowSeconds();
  for (std::map<wxString, BoatInfoValue>::iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    BoatInfoValue& value = it->second;
    const bool shouldBeStale =
        value.valid && value.lastUpdateSeconds > 0 &&
        now - value.lastUpdateSeconds > value.freshnessSeconds;
    if (value.stale != shouldBeStale) {
      value.stale = shouldBeStale;
      UpdateInstrument(value.key);
    }
  }
}

void boatinfo_pi::UpdateSourceSummary() {
  if (!m_dataSourceValue) return;
  wxString summary;
  if (m_seenOpenCPN) summary += wxT("OpenCPN");
  if (m_seenSignalK) {
    if (!summary.IsEmpty()) summary += wxT(" • ");
    summary += wxT("Signal K");
  }
  if (m_seenNmeaXdr) {
    if (!summary.IsEmpty()) summary += wxT(" • ");
    summary += wxT("NMEA XDR");
  }
  m_dataSourceValue->SetLabel(summary.IsEmpty() ? wxT("Waiting") : summary);
}

void boatinfo_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  if (std::isfinite(pfix.Lat) && std::fabs(pfix.Lat) <= 90.0)
    ObserveNumeric(wxT("opencpn:navigation.position.latitude"), wxT("OpenCPN"),
                   wxT("navigation.position.latitude"), pfix.Lat);
  if (std::isfinite(pfix.Lon) && std::fabs(pfix.Lon) <= 180.0)
    ObserveNumeric(wxT("opencpn:navigation.position.longitude"), wxT("OpenCPN"),
                   wxT("navigation.position.longitude"), pfix.Lon);
  if (std::isfinite(pfix.Cog)) {
    double cog = std::fmod(pfix.Cog, 360.0);
    if (cog < 0.0) cog += 360.0;
    ObserveNumeric(wxT("opencpn:navigation.courseOverGroundTrue"),
                   wxT("OpenCPN"), wxT("navigation.courseOverGroundTrue"), cog);
  }
  if (std::isfinite(pfix.Sog) && pfix.Sog >= 0.0)
    ObserveNumeric(wxT("opencpn:navigation.speedOverGround"), wxT("OpenCPN"),
                   wxT("navigation.speedOverGround"), pfix.Sog);
  if (std::isfinite(pfix.Hdt))
    ObserveNumeric(wxT("opencpn:navigation.headingTrue"), wxT("OpenCPN"),
                   wxT("navigation.headingTrue"), pfix.Hdt);
  if (std::isfinite(pfix.Hdm))
    ObserveNumeric(wxT("opencpn:navigation.headingMagnetic"), wxT("OpenCPN"),
                   wxT("navigation.headingMagnetic"), pfix.Hdm);
}

void boatinfo_pi::SetNMEASentence(wxString& sentence) {
  if (sentence.length() >= 6 && sentence[0] == wxT('$') &&
      sentence.Mid(3, 3) == wxT("XDR")) {
    m_seenNmeaXdr = true;
    UpdateSourceSummary();
  }
}

void boatinfo_pi::SetPluginMessage(wxString& message_id,
                                   wxString& message_body) {
  if (message_id == wxT("OCPN_CORE_SIGNALK") && ParseSignalK(message_body)) {
    m_seenSignalK = true;
    UpdateSourceSummary();
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
    ObserveNumeric(key, wxT("Signal K"), path, number);
    return true;
  }
  if (jsonValue.IsString()) {
    ObserveText(key, wxT("Signal K"), path, jsonValue.AsString());
    return true;
  }
  if (jsonValue.IsBool()) {
    ObserveText(key, wxT("Signal K"), path,
                jsonValue.AsBool() ? wxT("ON") : wxT("OFF"));
    return true;
  }
  ObserveNoData(key, wxT("Signal K"), path);
  return true;
}

void boatinfo_pi::ShowPreferencesDialog(wxWindow* parent) {
  wxDialog dialog(parent, wxID_ANY, wxT("BoatInfo preferences"),
                  wxDefaultPosition, wxSize(840, 600),
                  wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

  wxStaticText* intro = new wxStaticText(
      &dialog, wxID_ANY,
      wxT("BoatInfo learns own-vessel values as they are observed. Choose what "
          "is shown, edit the suggested name and select the digital "
          "presentation. Vessel identity (name, MMSI and call sign) is always "
          "shown first. OpenCPN navigation values are preferred by default "
          "when the same navigation quantity also arrives through Signal K."));
  intro->Wrap(dialog.FromDIP(790));
  root->Add(intro, 0, wxEXPAND | wxALL, dialog.FromDIP(12));

  wxScrolledWindow* scroll = new wxScrolledWindow(
      &dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
  scroll->SetScrollRate(0, dialog.FromDIP(10));
  wxFlexGridSizer* grid = new wxFlexGridSizer(5, dialog.FromDIP(5),
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

  std::vector<BoatInfoValue*> ordered;
  for (std::map<wxString, BoatInfoValue>::iterator it = m_values.begin();
       it != m_values.end(); ++it)
    ordered.push_back(&it->second);
  std::sort(ordered.begin(), ordered.end(), ValueOrder);

  std::vector<PreferenceRow> rows;
  wxString currentCategory;
  for (size_t i = 0; i < ordered.size(); ++i) {
    BoatInfoValue& value = *ordered[i];
    if (value.category != currentCategory) {
      currentCategory = value.category;
      wxStaticText* category =
          new wxStaticText(scroll, wxID_ANY, currentCategory.Upper());
      category->SetFont(headerFont);
      grid->Add(category, 0, wxTOP, dialog.FromDIP(12));
      for (int column = 1; column < 5; ++column) grid->AddSpacer(1);
    }

    PreferenceRow row;
    row.key = value.key;
    row.visible = new wxCheckBox(scroll, wxID_ANY, wxEmptyString);
    row.visible->SetValue(value.category == wxT("Vessel") ? true
                                                          : value.visible);
    if (value.category == wxT("Vessel")) row.visible->Enable(false);
    grid->Add(row.visible, 0, wxALIGN_CENTER);

    wxString sourceLabel = value.source;
    if (value.alternativeSource) sourceLabel += wxT(" (alt.)");
    wxStaticText* source = new wxStaticText(scroll, wxID_ANY, sourceLabel);
    if (value.alternativeSource)
      source->SetToolTip(wxT("The same navigation quantity is also available "
                            "from OpenCPN. OpenCPN is preferred by default."));
    grid->Add(source, 0, wxALIGN_CENTER_VERTICAL);

    row.label = new wxTextCtrl(scroll, wxID_ANY,
                               value.label.IsEmpty() ? value.suggestedLabel
                                                     : value.label);
    grid->Add(row.label, 1, wxEXPAND);

    wxArrayString choices;
    choices.Add(wxT("Value"));
    choices.Add(wxT("Level"));
    choices.Add(wxT("Tape"));
    choices.Add(wxT("Trend"));
    choices.Add(wxT("No display"));
    row.primitive = new wxChoice(scroll, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, choices);
    row.primitive->SetSelection(value.category == wxT("Vessel")
                                    ? SelectionFromPrimitive(
                                          BoatInfoValue::PRIMITIVE_NONE)
                                    : SelectionFromPrimitive(value.primitive));
    if (value.category == wxT("Vessel")) row.primitive->Enable(false);
    grid->Add(row.primitive, 0, wxEXPAND);

    wxStaticText* pathText = new wxStaticText(scroll, wxID_ANY, value.path);
    pathText->SetToolTip(value.path);
    grid->Add(pathText, 0, wxALIGN_CENTER_VERTICAL);
    rows.push_back(row);
  }

  if (rows.empty()) {
    grid->Add(new wxStaticText(
                  scroll, wxID_ANY,
                  wxT("No values observed yet. Return after OpenCPN or Signal K "
                      "has delivered own-vessel data.")),
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
    BoatInfoValue& value = found->second;
    if (value.category == wxT("Vessel")) {
      value.visible = true;
      value.primitive = BoatInfoValue::PRIMITIVE_NONE;
    } else {
      value.visible = rows[i].visible->GetValue();
      value.primitive =
          PrimitiveFromSelection(rows[i].primitive->GetSelection());
    }
    const wxString enteredLabel = rows[i].label->GetValue();
    value.label = enteredLabel.IsEmpty() ? value.suggestedLabel : enteredLabel;
    value.labelCustomized = value.label != value.suggestedLabel;
    value.userConfigured = true;
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
    wxString key, source, path, label, savedSuggestion;
    long primitive = 0;
    bool visible = false;
    bool labelCustomized = false;
    if (!config->Read(wxT("Key"), &key) || key.IsEmpty()) continue;
    config->Read(wxT("Source"), &source, wxEmptyString);
    config->Read(wxT("Path"), &path, wxEmptyString);
    config->Read(wxT("Label"), &label, wxEmptyString);
    config->Read(wxT("SuggestedLabel"), &savedSuggestion, wxEmptyString);
    config->Read(wxT("LabelCustomized"), &labelCustomized, false);
    config->Read(wxT("Visible"), &visible, false);
    config->Read(wxT("Primitive"), &primitive, 0L);

    BoatInfoValue& value = EnsureValue(key, source, path);
    value.userConfigured = true;
    value.visible = visible;
    if (primitive >= BoatInfoValue::PRIMITIVE_VALUE &&
        primitive <= BoatInfoValue::PRIMITIVE_NONE)
      value.primitive = static_cast<BoatInfoValue::Primitive>(primitive);

    if (!savedSuggestion.IsEmpty()) {
      value.labelCustomized = labelCustomized;
    } else {
      value.labelCustomized = !label.IsEmpty() && !IsLegacyAutoLabel(label);
    }
    ApplySemanticDefaults(value);
    if (value.labelCustomized && !label.IsEmpty()) value.label = label;
    value.valid = false;
    value.stale = false;
  }
  config->SetPath(wxT("/Plugins/BoatInfo"));
}

void boatinfo_pi::SaveConfiguration() {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  config->SetPath(wxT("/Plugins"));
  config->DeleteGroup(wxT("BoatInfo"));
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
    config->Write(wxT("SuggestedLabel"), value.suggestedLabel);
    config->Write(wxT("LabelCustomized"), value.labelCustomized);
    config->Write(wxT("Visible"), value.visible);
    config->Write(wxT("Primitive"), static_cast<long>(value.primitive));
  }
  config->SetPath(wxT("/Plugins/BoatInfo"));
  config->Flush();
}

void boatinfo_pi::ClearControlPointers() {
  m_instrumentScroll = nullptr;
  m_instrumentGrid = nullptr;
  m_emptyHint = nullptr;
  m_dataSourceValue = nullptr;
  m_instruments.clear();
}

bool boatinfo_pi::DeInit() {
  SaveConfiguration();
  if (m_staleTimer) {
    m_staleTimer->Stop();
    delete m_staleTimer;
    m_staleTimer = nullptr;
  }
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

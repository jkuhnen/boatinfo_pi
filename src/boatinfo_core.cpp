#include "boatinfo_pi.h"
#include "boatinfo_dashboard.h"
#include "boatinfo_signalk.h"

#include <algorithm>
#include <cmath>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/datetime.h>
#include <wx/dialog.h>
#include <wx/fileconf.h>
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
const wxString kIdentityNameKey = wxT("signalk:name");
const wxString kIdentityMmsiKey = wxT("signalk:mmsi");
const wxString kIdentityCallSignKey = wxT("signalk:communication.callsignVhf");

bool JsonNumber(const wxJSONValue& value, double& result) {
  switch (value.GetType()) {
    case wxJSONTYPE_DOUBLE: result = value.AsDouble(); break;
    case wxJSONTYPE_INT: result = static_cast<double>(value.AsInt()); break;
    case wxJSONTYPE_UINT: result = static_cast<double>(value.AsUInt()); break;
    case wxJSONTYPE_LONG: result = static_cast<double>(value.AsLong()); break;
    case wxJSONTYPE_ULONG: result = static_cast<double>(value.AsULong()); break;
    case wxJSONTYPE_SHORT: result = static_cast<double>(value.AsShort()); break;
    case wxJSONTYPE_USHORT: result = static_cast<double>(value.AsUShort()); break;
    default: return false;
  }
  return std::isfinite(result);
}

long long NowSeconds() {
  return static_cast<long long>(wxDateTime::Now().GetTicks());
}

wxString SafeProfileKey(const wxString& input) {
  if (input.IsEmpty()) return wxT("default");
  wxString result;
  for (size_t i = 0; i < input.length(); ++i) {
    const wxChar c = input[i];
    const bool safe = (c >= wxT('a') && c <= wxT('z')) ||
                      (c >= wxT('A') && c <= wxT('Z')) ||
                      (c >= wxT('0') && c <= wxT('9')) ||
                      c == wxT('_') || c == wxT('-');
    result += safe ? c : wxT('_');
  }
  return result.IsEmpty() ? wxT("default") : result;
}

bool IsIdentityPath(const wxString& path) {
  const wxString lower = path.Lower();
  return lower == wxT("name") || lower == wxT("mmsi") ||
         lower == wxT("communication.callsignvhf");
}

wxString HumanizeToken(const wxString& token) {
  if (token.IsEmpty()) return token;
  wxString out;
  for (size_t i = 0; i < token.length(); ++i) {
    const wxChar c = token[i];
    const bool upper = c >= wxT('A') && c <= wxT('Z');
    const bool previousLower = i > 0 && token[i - 1] >= wxT('a') &&
                               token[i - 1] <= wxT('z');
    if (upper && previousLower) out += wxT(' ');
    out += (c == wxT('_') || c == wxT('-')) ? wxT(' ') : c;
  }
  if (!out.IsEmpty() && out[0] >= wxT('a') && out[0] <= wxT('z'))
    out[0] = static_cast<wxChar>(out[0] - wxT('a') + wxT('A'));
  return out;
}

wxArrayString SplitPath(const wxString& path) {
  wxArrayString parts;
  wxString current;
  for (size_t i = 0; i < path.length(); ++i) {
    if (path[i] == wxT('.')) {
      if (!current.IsEmpty()) parts.Add(current);
      current.clear();
    } else {
      current += path[i];
    }
  }
  if (!current.IsEmpty()) parts.Add(current);
  return parts;
}

wxString CategoryFromPath(const wxString& path) {
  const wxArrayString parts = SplitPath(path);
  if (parts.IsEmpty()) return wxT("Other");
  return HumanizeToken(parts[0]);
}

wxString LabelFromPath(const wxString& path) {
  const wxArrayString parts = SplitPath(path);
  if (parts.IsEmpty()) return path;
  wxString label;
  const size_t start = parts.size() > 1 ? 1 : 0;
  for (size_t i = start; i < parts.size(); ++i) {
    if (!label.IsEmpty()) label += wxT(" ");
    label += HumanizeToken(parts[i]);
  }
  return label.IsEmpty() ? HumanizeToken(parts[0]) : label;
}

bool IsLegacyAutoLabel(const wxString& label) {
  const wxString lower = label.Lower();
  return lower == wxT("voltage") || lower == wxT("current") ||
         lower == wxT("power") || lower == wxT("service battery") ||
         lower == wxT("service battery voltage") ||
         lower == wxT("starter battery voltage") ||
         lower == wxT("service battery soc") ||
         lower == wxT("course over ground true") ||
         lower == wxT("speed over ground") || lower == wxT("heading") ||
         lower == wxT("discharge since full") ||
         lower == wxT("time remaining") || lower == wxT("antenna altitude") ||
         lower == wxT("datetime") || lower == wxT("latitude") ||
         lower == wxT("longitude") || lower == wxT("cog") ||
         lower == wxT("sog") || lower == wxT("angle apparent") ||
         lower == wxT("apparent wind angle") ||
         lower == wxT("apparent wind speed") ||
         lower == wxT("true wind angle") || lower == wxT("true wind speed") ||
         lower == wxT("depth");
}

BoatInfoValue::Presentation PresentationFromSelection(int) {
  // Stable 1.1.0 exposes Text only. Later modes can extend this mapping when
  // their renderers and product behavior are implemented.
  return BoatInfoValue::PRESENTATION_TEXT;
}

int SelectionFromPresentation(BoatInfoValue::Presentation) { return 0; }

BoatInfoValue::Priority PriorityFromSelection(int selection) {
  if (selection == 0) return BoatInfoValue::PRIORITY_PRIMARY;
  if (selection == 2) return BoatInfoValue::PRIORITY_DETAIL;
  return BoatInfoValue::PRIORITY_SECONDARY;
}

int SelectionFromPriority(BoatInfoValue::Priority priority) {
  if (priority == BoatInfoValue::PRIORITY_PRIMARY) return 0;
  if (priority == BoatInfoValue::PRIORITY_DETAIL) return 2;
  return 1;
}

int CategoryRank(const wxString& category) {
  if (category == wxT("Navigation")) return 0;
  if (category == wxT("Electrical")) return 1;
  if (category == wxT("Tanks")) return 2;
  if (category == wxT("Propulsion")) return 3;
  if (category == wxT("Environment")) return 4;
  return 5;
}

int SemanticRank(const BoatInfoValue& value) {
  const wxString path = value.path.Lower();
  if (path.Find(wxT("speedoverground")) != wxNOT_FOUND) return 0;
  if (path.Find(wxT("courseoverground")) != wxNOT_FOUND) return 1;
  if (path.Find(wxT("heading")) != wxNOT_FOUND) return 2;
  if (path.Find(wxT("stateofcharge")) != wxNOT_FOUND) return 0;
  if (path.Find(wxT("voltage")) != wxNOT_FOUND) return 1;
  if (path.Find(wxT("current")) != wxNOT_FOUND) return 2;
  if (path.Find(wxT("revolutions")) != wxNOT_FOUND) return 0;
  if (path.Find(wxT("depth")) != wxNOT_FOUND) return 0;
  return 10;
}

bool ValueOrder(const BoatInfoValue* a, const BoatInfoValue* b) {
  const int aCategory = CategoryRank(a->category);
  const int bCategory = CategoryRank(b->category);
  if (aCategory != bCategory) return aCategory < bCategory;
  if (a->category.CmpNoCase(b->category) != 0)
    return a->category.CmpNoCase(b->category) < 0;
  if (a->priority != b->priority) return a->priority < b->priority;
  const int aSemantic = SemanticRank(*a);
  const int bSemantic = SemanticRank(*b);
  if (aSemantic != bSemantic) return aSemantic < bSemantic;
  const wxString aLabel = a->label.IsEmpty() ? a->suggestedLabel : a->label;
  const wxString bLabel = b->label.IsEmpty() ? b->suggestedLabel : b->label;
  if (aLabel.CmpNoCase(bLabel) != 0) return aLabel.CmpNoCase(bLabel) < 0;
  return a->key < b->key;
}

wxString EffectiveText(const std::map<wxString, BoatInfoValue>& values,
                       const wxString& key) {
  std::map<wxString, BoatInfoValue>::const_iterator found = values.find(key);
  if (found == values.end() || !found->second.valid ||
      found->second.textValue.IsEmpty())
    return wxT("—");
  return found->second.textValue;
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

void ApplyPathSemantics(BoatInfoValue& value, bool configured) {
  const wxString path = value.path.Lower();

  value.category = CategoryFromPath(value.path);
  value.suggestedLabel = LabelFromPath(value.path);
  if (!value.labelCustomized) value.label = value.suggestedLabel;
  value.semanticId = path;
  value.alternativeSource = false;
  value.unit.clear();
  value.qualifier.clear();
  value.bounded = false;
  value.minimum = 0.0;
  value.maximum = 1.0;
  value.freshnessSeconds = 30;
  if (!configured) {
    value.visible = false;
    value.priority = BoatInfoValue::PRIORITY_DETAIL;
    value.primitive = BoatInfoValue::PRIMITIVE_VALUE;
  }

  if (IsIdentityPath(path)) {
    value.category = wxT("Vessel");
    value.visible = true;
    value.priority = BoatInfoValue::PRIORITY_PRIMARY;
    value.primitive = BoatInfoValue::PRIMITIVE_NONE;
    value.freshnessSeconds = 300;
    return;
  }

  if (path.Find(wxT("stateofcharge")) != wxNOT_FOUND ||
      path.Find(wxT("currentlevel")) != wxNOT_FOUND) {
    value.unit = wxT("%");
    value.bounded = true;
    value.minimum = 0.0;
    value.maximum = 100.0;
    if (!configured) value.primitive = BoatInfoValue::PRIMITIVE_LEVEL;
  } else if (path.Find(wxT("timeremaining")) != wxNOT_FOUND) {
    value.unit = wxT("h");
  } else if (path.Find(wxT("dischargesincefull")) != wxNOT_FOUND ||
             path.Find(wxT("lifetimedischarge")) != wxNOT_FOUND ||
             path.Find(wxT("lifetimerecharge")) != wxNOT_FOUND) {
    value.unit = wxT("Ah");
  } else if (path.Find(wxT("temperature")) != wxNOT_FOUND) {
    value.unit = wxT("°C");
  } else if (path.Find(wxT("revolutions")) != wxNOT_FOUND) {
    value.unit = wxT("rpm");
  } else if (path.Find(wxT("speed")) != wxNOT_FOUND) {
    value.unit = wxT("kn");
  } else if (path.Find(wxT("angle")) != wxNOT_FOUND ||
             path.Find(wxT("direction")) != wxNOT_FOUND ||
             path.Find(wxT("heading")) != wxNOT_FOUND ||
             path.Find(wxT("course")) != wxNOT_FOUND ||
             path.Find(wxT("bearing")) != wxNOT_FOUND) {
    value.unit = wxT("°");
    if (!configured) value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
  } else if (path.EndsWith(wxT(".voltage"))) {
    value.unit = wxT("V");
  } else if (path.EndsWith(wxT(".current"))) {
    value.unit = wxT("A");
  } else if (path.EndsWith(wxT(".power"))) {
    value.unit = wxT("W");
  } else if (path.Find(wxT("depth")) != wxNOT_FOUND ||
             path.Find(wxT("altitude")) != wxNOT_FOUND) {
    value.unit = wxT("m");
  }

  if (!configured) {
    if (path == wxT("navigation.speedoverground") ||
        path == wxT("navigation.courseovergroundtrue") ||
        path.Find(wxT("stateofcharge")) != wxNOT_FOUND ||
        path.Find(wxT("currentlevel")) != wxNOT_FOUND ||
        path.Find(wxT("depth")) != wxNOT_FOUND) {
      value.visible = true;
      value.priority = BoatInfoValue::PRIORITY_PRIMARY;
    } else if (path.Find(wxT("revolutions")) != wxNOT_FOUND ||
               path.Find(wxT("headingtrue")) != wxNOT_FOUND ||
               path.EndsWith(wxT(".voltage")) ||
               path.EndsWith(wxT(".current"))) {
      value.visible = true;
      value.priority = BoatInfoValue::PRIORITY_SECONDARY;
    }
  }
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
  LoadConfiguration();
  EnsureIdentityValues();
  ApplyIdentityFallbacks();

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
      .CaptionVisible(false)
      .Bottom()
      .BestSize(-1, m_panel->FromDIP(165))
      .MinSize(-1, m_panel->FromDIP(78))
      .MaxSize(-1, m_panel->FromDIP(240))
      .Floatable(false)
      .Movable(false)
      .LeftDockable(false)
      .RightDockable(false)
      .TopDockable(false)
      .BottomDockable(true)
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
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  m_dashboard = new BoatInfoDashboardPanel(m_panel);
  root->Add(m_dashboard, 1, wxEXPAND);
  m_panel->SetSizer(root);
  RebuildDashboard();
  ApplyHostStyle();
}

void boatinfo_pi::RebuildDashboard() {
  if (!m_dashboard) return;
  std::vector<const BoatInfoValue*> ordered;
  for (std::map<wxString, BoatInfoValue>::const_iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    if (it->second.visible && !IsIdentityPath(it->second.path))
      ordered.push_back(&it->second);
  }
  std::sort(ordered.begin(), ordered.end(), ValueOrder);
  std::vector<BoatInfoValue> values;
  values.reserve(ordered.size());
  for (size_t i = 0; i < ordered.size(); ++i) values.push_back(*ordered[i]);

  m_dashboard->SetState(values, EffectiveText(m_values, kIdentityNameKey),
                        EffectiveText(m_values, kIdentityMmsiKey),
                        EffectiveText(m_values, kIdentityCallSignKey),
                        m_sourceSummary);
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
  RebuildDashboard();
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
  value.suggestedLabel = LabelFromPath(path);
  value.label = value.suggestedLabel;
  ApplySemanticDefaults(value);
  return m_values.insert(std::make_pair(key, value)).first->second;
}

void boatinfo_pi::EnsureIdentityValues() {
  BoatInfoValue& name =
      EnsureValue(kIdentityNameKey, wxT("Signal K"), wxT("name"));
  BoatInfoValue& mmsi =
      EnsureValue(kIdentityMmsiKey, wxT("Signal K"), wxT("mmsi"));
  BoatInfoValue& callSign = EnsureValue(
      kIdentityCallSignKey, wxT("Signal K"), wxT("communication.callsignVhf"));
  name.visible = mmsi.visible = callSign.visible = true;
  name.primitive = mmsi.primitive = callSign.primitive =
      BoatInfoValue::PRIMITIVE_NONE;
}

void boatinfo_pi::ApplyIdentityFallbacks() {
  EnsureIdentityValues();
  struct Fallback {
    const wxString* key;
    const wxString* text;
  } fallbacks[] = {{&kIdentityNameKey, &m_manualVesselName},
                   {&kIdentityMmsiKey, &m_manualMmsi},
                   {&kIdentityCallSignKey, &m_manualCallSign}};
  for (size_t i = 0; i < 3; ++i) {
    BoatInfoValue& value = m_values[*fallbacks[i].key];
    if (!value.valid && !fallbacks[i].text->IsEmpty()) {
      value.textValue = *fallbacks[i].text;
      value.hasNumericValue = false;
      value.valid = true;
      value.stale = false;
      value.manualFallback = true;
      value.lastUpdateSeconds = 0;
    }
  }
  RebuildDashboard();
}

bool boatinfo_pi::ApplySignalKIdentity(const wxJSONValue& model) {
  BoatInfoSignalKIdentity identity;
  if (!ExtractBoatInfoSignalKIdentity(model, m_signalKSelf, identity))
    return false;

  if (identity.hasName)
    ObserveText(kIdentityNameKey, wxT("Signal K"), wxT("name"),
                identity.name);
  if (identity.hasMmsi)
    ObserveText(kIdentityMmsiKey, wxT("Signal K"), wxT("mmsi"),
                identity.mmsi);
  if (identity.hasCallSign)
    ObserveText(kIdentityCallSignKey, wxT("Signal K"),
                wxT("communication.callsignVhf"), identity.callSign);
  return true;
}

void boatinfo_pi::ApplySignalKSelfMmsiFallback() {
  EnsureIdentityValues();
  BoatInfoValue& value = m_values[kIdentityMmsiKey];
  if (value.valid && !value.manualFallback) return;

  wxString mmsi;
  if (DeriveBoatInfoMmsiFromSelf(m_signalKSelf, mmsi))
    ObserveText(kIdentityMmsiKey, wxT("Signal K"), wxT("mmsi"), mmsi);
}

void boatinfo_pi::ApplySemanticDefaults(BoatInfoValue& value) {
  const bool configured = value.userConfigured;
  ApplyPathSemantics(value, configured);

  if (value.source == wxT("OpenCPN")) {
    value.category = wxT("Navigation");
    value.freshnessSeconds = 10;
    const wxString path = value.path.Lower();
    if (path == wxT("navigation.speedoverground")) {
      value.suggestedLabel = wxT("SOG");
      value.semanticId = wxT("navigation.sog");
      value.unit = wxT("kn");
      if (!configured) {
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_PRIMARY;
      }
    } else if (path == wxT("navigation.courseovergroundtrue")) {
      value.suggestedLabel = wxT("COG");
      value.semanticId = wxT("navigation.cog.true");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      if (!configured) {
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_PRIMARY;
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
      }
    } else if (path == wxT("navigation.headingtrue")) {
      value.suggestedLabel = wxT("Heading");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      if (!configured) {
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_SECONDARY;
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
      }
    } else if (path == wxT("navigation.headingmagnetic")) {
      value.suggestedLabel = wxT("Heading magnetic");
      value.unit = wxT("°");
      value.qualifier = wxT("M");
    } else if (path.Find(wxT("position.latitude")) != wxNOT_FOUND ||
               path.Find(wxT("position.longitude")) != wxNOT_FOUND) {
      value.unit.clear();
      if (!configured) value.visible = false;
    }
    if (!value.labelCustomized) value.label = value.suggestedLabel;
  } else {
    const wxString path = value.path.Lower();
    if (path == wxT("navigation.speedoverground") ||
        path == wxT("navigation.courseovergroundtrue") ||
        path == wxT("navigation.headingtrue") ||
        path == wxT("navigation.headingmagnetic") ||
        path == wxT("navigation.position.latitude") ||
        path == wxT("navigation.position.longitude")) {
      value.alternativeSource = true;
      if (!configured) value.visible = false;
    }
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
  value.stale = false;
  value.manualFallback = false;
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
  RebuildDashboard();
}

void boatinfo_pi::ObserveText(const wxString& key, const wxString& source,
                              const wxString& path, const wxString& textValue) {
  BoatInfoValue& value = EnsureValue(key, source, path);
  value.hasNumericValue = false;
  value.textValue = textValue;
  value.valid = !textValue.IsEmpty();
  value.stale = false;
  value.manualFallback = false;
  value.lastUpdateSeconds = NowSeconds();
  if (source == wxT("Signal K")) m_seenSignalK = true;
  UpdateSourceSummary();
  RebuildDashboard();
}

void boatinfo_pi::ObserveNoData(const wxString& key, const wxString& source,
                                const wxString& path) {
  BoatInfoValue& value = EnsureValue(key, source, path);
  value.valid = false;
  value.stale = false;
  value.manualFallback = false;
  value.lastUpdateSeconds = NowSeconds();
  if (IsIdentityPath(path)) ApplyIdentityFallbacks();
  RebuildDashboard();
}

void boatinfo_pi::RefreshFreshness() {
  const long long now = NowSeconds();
  bool changed = false;
  for (std::map<wxString, BoatInfoValue>::iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    BoatInfoValue& value = it->second;
    if (IsIdentityPath(value.path) || value.manualFallback) continue;
    const bool shouldBeStale = value.valid && value.lastUpdateSeconds > 0 &&
                               now - value.lastUpdateSeconds >
                                   value.freshnessSeconds;
    if (value.stale != shouldBeStale) {
      value.stale = shouldBeStale;
      changed = true;
    }
  }
  if (changed) RebuildDashboard();
}

void boatinfo_pi::UpdateSourceSummary() {
  wxString summary;
  if (m_seenOpenCPN) summary += wxT("OpenCPN");
  if (m_seenSignalK) {
    if (!summary.IsEmpty()) summary += wxT(" · ");
    summary += wxT("Signal K");
  }
  if (m_seenNmeaXdr) {
    if (!summary.IsEmpty()) summary += wxT(" · ");
    summary += wxT("NMEA XDR");
  }
  m_sourceSummary = summary.IsEmpty() ? wxT("Waiting") : summary;
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
    RebuildDashboard();
  }
}

void boatinfo_pi::SetPluginMessage(wxString& message_id,
                                   wxString& message_body) {
  if (message_id == wxT("OCPN_CORE_SIGNALK") && ParseSignalK(message_body)) {
    m_seenSignalK = true;
    UpdateSourceSummary();
    RebuildDashboard();
  }
}

void boatinfo_pi::ActivateVesselProfile(const wxString& signalKSelf) {
  const wxString normalized = NormalizeBoatInfoSignalKSelf(signalKSelf);
  if (normalized.IsEmpty()) return;
  const wxString nextProfile = SafeProfileKey(normalized);
  if (nextProfile == m_profileKey && normalized == m_signalKSelf) return;

  if (m_configLoaded) SaveConfiguration();
  m_signalKSelf = normalized;
  m_profileKey = nextProfile;

  for (std::map<wxString, BoatInfoValue>::iterator it = m_values.begin();
       it != m_values.end();) {
    if (it->second.source == wxT("Signal K") &&
        !IsIdentityPath(it->second.path)) {
      it = m_values.erase(it);
      continue;
    }
    BoatInfoValue& value = it->second;
    if (IsIdentityPath(value.path)) {
      value.valid = false;
      value.stale = false;
      value.manualFallback = false;
      value.textValue.clear();
    }
    value.userConfigured = false;
    value.labelCustomized = false;
    value.visible = false;
    value.primitive = BoatInfoValue::PRIMITIVE_VALUE;
    value.priority = BoatInfoValue::PRIORITY_SECONDARY;
    ApplySemanticDefaults(value);
    ++it;
  }

  m_manualVesselName.clear();
  m_manualMmsi.clear();
  m_manualCallSign.clear();
  LoadProfile(m_profileKey, false);
  EnsureIdentityValues();
  ApplyIdentityFallbacks();
  RebuildDashboard();
}

bool boatinfo_pi::ParseSignalK(const wxString& message) {
  BoatInfoSignalKMessage parsed;
  if (!ParseBoatInfoSignalKMessage(message, parsed)) return false;

  bool handled = false;
  if (parsed.hasSelf && !parsed.self.IsEmpty()) {
    ActivateVesselProfile(parsed.self);
    handled = true;
  }

  if (parsed.hasContext &&
      !IsBoatInfoOwnVesselContext(parsed.context, m_signalKSelf)) {
    ApplySignalKSelfMmsiFallback();
    return handled;
  }

  handled = ApplySignalKIdentity(parsed.root) || handled;

  for (size_t i = 0; i < parsed.values.size(); ++i) {
    const BoatInfoSignalKValue& item = parsed.values[i];
    if (item.path.IsEmpty() && item.value.IsObject()) {
      handled = ApplySignalKIdentity(item.value) || handled;
    } else {
      handled = ObserveSignalKPath(item.path, item.value) || handled;
    }
  }

  ApplySignalKSelfMmsiFallback();
  return handled;
}

bool boatinfo_pi::ObserveSignalKPath(const wxString& path,
                                     const wxJSONValue& jsonValue) {
  const wxString key = wxT("signalk:") + path;
  double number = 0.0;
  if (JsonNumber(jsonValue, number)) {
    if (path.Lower() == wxT("mmsi"))
      ObserveText(key, wxT("Signal K"), path,
                  wxString::Format(wxT("%.0f"), number));
    else
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
                  wxDefaultPosition, wxSize(1040, 680),
                  wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

  wxStaticText* intro = new wxStaticText(
      &dialog, wxID_ANY,
      wxT("BoatInfo derives Signal K categories and suggested names directly "
          "from the data path. Selected values remain grouped by their source "
          "category and the live strip simplifies only when available space "
          "requires it."));
  intro->Wrap(dialog.FromDIP(990));
  root->Add(intro, 0, wxEXPAND | wxALL, dialog.FromDIP(12));

  wxStaticText* vesselTitle =
      new wxStaticText(&dialog, wxID_ANY, wxT("VESSEL IDENTITY"));
  wxFont sectionFont = vesselTitle->GetFont();
  sectionFont.SetWeight(wxFONTWEIGHT_BOLD);
  vesselTitle->SetFont(sectionFont);
  root->Add(vesselTitle, 0, wxLEFT | wxRIGHT | wxTOP, dialog.FromDIP(12));

  wxStaticText* profileInfo = new wxStaticText(
      &dialog, wxID_ANY,
      m_signalKSelf.IsEmpty()
          ? wxT("Profile: default (waiting for Signal K vessel identity)")
          : wxT("Profile: ") + m_signalKSelf);
  root->Add(profileInfo, 0, wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(12));

  wxFlexGridSizer* identityGrid =
      new wxFlexGridSizer(2, dialog.FromDIP(6), dialog.FromDIP(10));
  identityGrid->AddGrowableCol(1, 1);
  identityGrid->Add(new wxStaticText(&dialog, wxID_ANY, wxT("Manual name")), 0,
                    wxALIGN_CENTER_VERTICAL);
  wxTextCtrl* manualName = new wxTextCtrl(&dialog, wxID_ANY, m_manualVesselName);
  identityGrid->Add(manualName, 1, wxEXPAND);
  identityGrid->Add(new wxStaticText(&dialog, wxID_ANY, wxT("Manual MMSI")), 0,
                    wxALIGN_CENTER_VERTICAL);
  wxTextCtrl* manualMmsi = new wxTextCtrl(&dialog, wxID_ANY, m_manualMmsi);
  identityGrid->Add(manualMmsi, 1, wxEXPAND);
  identityGrid->Add(new wxStaticText(&dialog, wxID_ANY, wxT("Manual call sign")),
                    0, wxALIGN_CENTER_VERTICAL);
  wxTextCtrl* manualCallSign =
      new wxTextCtrl(&dialog, wxID_ANY, m_manualCallSign);
  identityGrid->Add(manualCallSign, 1, wxEXPAND);
  root->Add(identityGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            dialog.FromDIP(12));

  root->Add(new wxStaticLine(&dialog), 0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(12));

  wxScrolledWindow* scroll = new wxScrolledWindow(
      &dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
  scroll->SetScrollRate(0, dialog.FromDIP(10));
  wxFlexGridSizer* grid =
      new wxFlexGridSizer(6, dialog.FromDIP(5), dialog.FromDIP(10));
  grid->AddGrowableCol(3, 1);

  wxFont headerFont = dialog.GetFont();
  headerFont.SetWeight(wxFONTWEIGHT_BOLD);
  const wxString headers[] = {wxT("Show"), wxT("Priority"), wxT("Source"),
                              wxT("Name"), wxT("Presentation"), wxT("Path")};
  for (size_t i = 0; i < 6; ++i) {
    wxStaticText* header = new wxStaticText(scroll, wxID_ANY, headers[i]);
    header->SetFont(headerFont);
    grid->Add(header, 0, wxALIGN_CENTER_VERTICAL);
  }

  std::vector<BoatInfoValue*> ordered;
  for (std::map<wxString, BoatInfoValue>::iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    if (!IsIdentityPath(it->second.path)) ordered.push_back(&it->second);
  }
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
      for (int column = 1; column < 6; ++column) grid->AddSpacer(1);
    }

    PreferenceRow row;
    row.key = value.key;
    row.visible = new wxCheckBox(scroll, wxID_ANY, wxEmptyString);
    row.visible->SetValue(value.visible);
    grid->Add(row.visible, 0, wxALIGN_CENTER);

    wxArrayString priorities;
    priorities.Add(wxT("Primary"));
    priorities.Add(wxT("Secondary"));
    priorities.Add(wxT("Detail"));
    row.priority = new wxChoice(scroll, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, priorities);
    row.priority->SetSelection(SelectionFromPriority(value.priority));
    grid->Add(row.priority, 0, wxEXPAND);

    wxString sourceLabel = value.source;
    if (value.alternativeSource) sourceLabel += wxT(" (alt.)");
    grid->Add(new wxStaticText(scroll, wxID_ANY, sourceLabel), 0,
              wxALIGN_CENTER_VERTICAL);

    row.label = new wxTextCtrl(scroll, wxID_ANY,
                               value.label.IsEmpty() ? value.suggestedLabel
                                                     : value.label);
    grid->Add(row.label, 1, wxEXPAND);

    wxArrayString presentations;
    presentations.Add(wxT("Text"));
    row.presentation = new wxChoice(scroll, wxID_ANY, wxDefaultPosition,
                                    wxDefaultSize, presentations);
    row.presentation->SetSelection(
        SelectionFromPresentation(value.presentation));
    grid->Add(row.presentation, 0, wxEXPAND);

    wxStaticText* pathText = new wxStaticText(scroll, wxID_ANY, value.path);
    pathText->SetToolTip(value.path);
    grid->Add(pathText, 0, wxALIGN_CENTER_VERTICAL);
    rows.push_back(row);
  }

  scroll->SetSizer(grid);
  root->Add(scroll, 1, wxEXPAND | wxLEFT | wxRIGHT, dialog.FromDIP(12));

  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  wxButton* applyButton = new wxButton(&dialog, wxID_APPLY, wxT("Anwenden"));
  buttons->AddButton(applyButton);
  buttons->AddButton(new wxButton(&dialog, wxID_CANCEL));
  buttons->AddButton(new wxButton(&dialog, wxID_OK));
  buttons->Realize();
  root->Add(buttons, 0, wxEXPAND | wxALL, dialog.FromDIP(12));

  dialog.SetSizer(root);
  dialog.Layout();
  dialog.CentreOnParent();

  const auto applyChanges = [&]() {
    m_manualVesselName = manualName->GetValue();
    m_manualMmsi = manualMmsi->GetValue();
    m_manualCallSign = manualCallSign->GetValue();
    ApplyPreferenceRows(rows);

    const wxString keys[] = {kIdentityNameKey, kIdentityMmsiKey,
                             kIdentityCallSignKey};
    for (size_t i = 0; i < 3; ++i) {
      BoatInfoValue& value = m_values[keys[i]];
      if (value.manualFallback) {
        value.valid = false;
        value.manualFallback = false;
        value.textValue.clear();
      }
    }
    ApplyIdentityFallbacks();
    SaveConfiguration();
    RebuildDashboard();
  };

  applyButton->Bind(wxEVT_BUTTON,
                    [&](wxCommandEvent&) { applyChanges(); });

  if (dialog.ShowModal() == wxID_OK) applyChanges();
}

void boatinfo_pi::ApplyPreferenceRows(const std::vector<PreferenceRow>& rows) {
  for (size_t i = 0; i < rows.size(); ++i) {
    std::map<wxString, BoatInfoValue>::iterator found =
        m_values.find(rows[i].key);
    if (found == m_values.end()) continue;
    BoatInfoValue& value = found->second;
    value.visible = rows[i].visible->GetValue();
    value.priority = PriorityFromSelection(rows[i].priority->GetSelection());
    value.presentation =
        PresentationFromSelection(rows[i].presentation->GetSelection());
    const wxString enteredLabel = rows[i].label->GetValue();
    value.label = enteredLabel.IsEmpty() ? value.suggestedLabel : enteredLabel;
    value.labelCustomized = value.label != value.suggestedLabel;
    value.userConfigured = true;
  }
}

void boatinfo_pi::LoadConfiguration() {
  if (m_configLoaded) return;
  m_configLoaded = true;
  m_profileKey = wxT("default");
  LoadProfile(m_profileKey, true);
}

void boatinfo_pi::LoadProfile(const wxString& profileKey, bool allowLegacy) {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  const wxString profilePath = wxT("/Plugins/BoatInfo/Profiles/") + profileKey;
  config->SetPath(profilePath);
  config->Read(wxT("ManualName"), &m_manualVesselName, wxEmptyString);
  config->Read(wxT("ManualMMSI"), &m_manualMmsi, wxEmptyString);
  config->Read(wxT("ManualCallSign"), &m_manualCallSign, wxEmptyString);

  long count = 0;
  bool hasCount = config->Read(wxT("InstrumentCount"), &count);
  wxString basePath = profilePath;
  if (!hasCount && allowLegacy) {
    config->SetPath(wxT("/Plugins/BoatInfo"));
    hasCount = config->Read(wxT("InstrumentCount"), &count);
    basePath = wxT("/Plugins/BoatInfo");
  }
  if (!hasCount) return;

  for (long i = 0; i < count; ++i) {
    config->SetPath(basePath + wxString::Format(wxT("/Instrument%ld"), i));
    wxString key, source, path, label, savedSuggestion;
    long primitive = BoatInfoValue::PRIMITIVE_VALUE;
    long presentation = BoatInfoValue::PRESENTATION_TEXT;
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
    config->Read(wxT("Presentation"), &presentation,
                 static_cast<long>(BoatInfoValue::PRESENTATION_TEXT));

    BoatInfoValue& value = EnsureValue(key, source, path);
    long priority = static_cast<long>(value.priority);
    config->Read(wxT("Priority"), &priority, priority);
    value.userConfigured = true;
    value.visible = visible;
    if (primitive >= BoatInfoValue::PRIMITIVE_VALUE &&
        primitive <= BoatInfoValue::PRIMITIVE_NONE)
      value.primitive = static_cast<BoatInfoValue::Primitive>(primitive);
    if (presentation >= BoatInfoValue::PRESENTATION_TEXT &&
        presentation <= BoatInfoValue::PRESENTATION_DIGITAL_ANALOG)
      value.presentation =
          static_cast<BoatInfoValue::Presentation>(presentation);
    if (priority >= BoatInfoValue::PRIORITY_PRIMARY &&
        priority <= BoatInfoValue::PRIORITY_DETAIL)
      value.priority = static_cast<BoatInfoValue::Priority>(priority);
    if (!savedSuggestion.IsEmpty())
      value.labelCustomized = labelCustomized;
    else
      value.labelCustomized = !label.IsEmpty() && !IsLegacyAutoLabel(label);
    ApplySemanticDefaults(value);
    if (value.labelCustomized && !label.IsEmpty()) value.label = label;
    value.valid = false;
    value.stale = false;
    value.manualFallback = false;
  }
}

void boatinfo_pi::SaveProfile(wxFileConfig* config,
                              const wxString& profileKey) {
  if (!config) return;
  const wxString profilePath = wxT("/Plugins/BoatInfo/Profiles/") + profileKey;
  config->SetPath(profilePath);
  config->Write(wxT("SignalKSelf"), m_signalKSelf);
  config->Write(wxT("ManualName"), m_manualVesselName);
  config->Write(wxT("ManualMMSI"), m_manualMmsi);
  config->Write(wxT("ManualCallSign"), m_manualCallSign);

  long count = 0;
  for (std::map<wxString, BoatInfoValue>::const_iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    if (!IsIdentityPath(it->second.path)) ++count;
  }
  config->Write(wxT("InstrumentCount"), count);

  long index = 0;
  for (std::map<wxString, BoatInfoValue>::const_iterator it = m_values.begin();
       it != m_values.end(); ++it) {
    const BoatInfoValue& value = it->second;
    if (IsIdentityPath(value.path)) continue;
    config->SetPath(profilePath +
                    wxString::Format(wxT("/Instrument%ld"), index++));
    config->Write(wxT("Key"), value.key);
    config->Write(wxT("Source"), value.source);
    config->Write(wxT("Path"), value.path);
    config->Write(wxT("Label"), value.label);
    config->Write(wxT("SuggestedLabel"), value.suggestedLabel);
    config->Write(wxT("LabelCustomized"), value.labelCustomized);
    config->Write(wxT("Visible"), value.visible);
    config->Write(wxT("Primitive"), static_cast<long>(value.primitive));
    config->Write(wxT("Presentation"),
                  static_cast<long>(value.presentation));
    config->Write(wxT("Priority"), static_cast<long>(value.priority));
  }
}

void boatinfo_pi::SaveConfiguration() {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;
  SaveProfile(config, m_profileKey);
  config->SetPath(wxT("/Plugins/BoatInfo"));
  config->Write(wxT("LastProfile"), m_profileKey);
  config->Flush();
}

void boatinfo_pi::ClearControlPointers() { m_dashboard = nullptr; }

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

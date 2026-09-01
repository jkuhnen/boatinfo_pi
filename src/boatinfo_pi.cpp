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
#include <wx/panel.h>
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

wxString SafeProfileKey(const wxString& input) {
  if (input.IsEmpty()) return wxT("default");
  wxString result;
  for (size_t i = 0; i < input.length(); ++i) {
    const wxChar c = input[i];
    const bool safe = (c >= wxT('a') && c <= wxT('z')) ||
                      (c >= wxT('A') && c <= wxT('Z')) ||
                      (c >= wxT('0') && c <= wxT('9')) || c == wxT('_') ||
                      c == wxT('-');
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

BoatInfoValue::Priority PriorityFromSelection(int selection) {
  switch (selection) {
    case 0:
      return BoatInfoValue::PRIORITY_PRIMARY;
    case 2:
      return BoatInfoValue::PRIORITY_DETAIL;
    default:
      return BoatInfoValue::PRIORITY_SECONDARY;
  }
}

int SelectionFromPriority(BoatInfoValue::Priority priority) {
  switch (priority) {
    case BoatInfoValue::PRIORITY_PRIMARY:
      return 0;
    case BoatInfoValue::PRIORITY_DETAIL:
      return 2;
    default:
      return 1;
  }
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
  if (value.category == wxT("Navigation")) {
    if (path.Find(wxT("courseoverground")) != wxNOT_FOUND) return 0;
    if (path.Find(wxT("speedoverground")) != wxNOT_FOUND) return 1;
    if (path.Find(wxT("heading")) != wxNOT_FOUND) return 2;
    if (path.Find(wxT("latitude")) != wxNOT_FOUND) return 8;
    if (path.Find(wxT("longitude")) != wxNOT_FOUND) return 9;
  } else if (value.category == wxT("Electrical")) {
    if (path.Find(wxT("stateofcharge")) != wxNOT_FOUND) return 0;
    if (path.Find(wxT("service.voltage")) != wxNOT_FOUND ||
        path.Find(wxT("house.voltage")) != wxNOT_FOUND)
      return 1;
    if (path.Find(wxT("current")) != wxNOT_FOUND) return 2;
    if (path.Find(wxT("starter.voltage")) != wxNOT_FOUND) return 3;
    if (path.Find(wxT("power")) != wxNOT_FOUND) return 4;
  } else if (value.category == wxT("Tanks")) {
    if (path.Find(wxT("currentlevel")) != wxNOT_FOUND) return 0;
  } else if (value.category == wxT("Propulsion")) {
    if (path.Find(wxT("revolutions")) != wxNOT_FOUND) return 0;
    if (path.Find(wxT("temperature")) != wxNOT_FOUND) return 1;
  } else if (value.category == wxT("Environment")) {
    if (path.Find(wxT("depth")) != wxNOT_FOUND) return 0;
  }
  return 10;
}

bool ValueOrder(const BoatInfoValue* a, const BoatInfoValue* b) {
  const int aCategory = CategoryRank(a->category);
  const int bCategory = CategoryRank(b->category);
  if (aCategory != bCategory) return aCategory < bCategory;
  if (a->priority != b->priority) return a->priority < b->priority;
  const int aSemantic = SemanticRank(*a);
  const int bSemantic = SemanticRank(*b);
  if (aSemantic != bSemantic) return aSemantic < bSemantic;
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

wxString UnitWithQualifier(const BoatInfoValue& value) {
  if (value.qualifier.IsEmpty()) return value.unit;
  if (value.unit.IsEmpty()) return value.qualifier;
  return value.unit + wxT(" ") + value.qualifier;
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
}  // namespace

class BoatInfoDashboardPanel : public wxPanel {
public:
  enum ResponsiveMode { MODE_FULL = 0, MODE_COMPACT, MODE_MINIMAL };

  explicit BoatInfoDashboardPanel(wxWindow* parent)
      : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                wxBORDER_NONE) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &BoatInfoDashboardPanel::OnPaint, this);
    Bind(wxEVT_SIZE, &BoatInfoDashboardPanel::OnSize, this);
  }

  void SetState(const std::vector<BoatInfoValue>& values,
                const wxString& vesselName, const wxString& mmsi,
                const wxString& callSign, const wxString& sourceSummary) {
    m_values = values;
    m_vesselName = vesselName.IsEmpty() ? wxT("—") : vesselName;
    m_mmsi = mmsi.IsEmpty() ? wxT("—") : mmsi;
    m_callSign = callSign.IsEmpty() ? wxT("—") : callSign;
    m_sourceSummary = sourceSummary;
    Refresh(false);
  }

private:
  struct Token {
    enum Kind { VESSEL, CATEGORY, VALUE } kind = VALUE;
    wxString category;
    const BoatInfoValue* value = nullptr;
  };

  int RowHeight(ResponsiveMode mode) const {
    if (mode == MODE_FULL) return FromDIP(72);
    if (mode == MODE_COMPACT) return FromDIP(54);
    return FromDIP(38);
  }

  int VesselWidth(ResponsiveMode mode) const {
    if (mode == MODE_FULL) return FromDIP(205);
    if (mode == MODE_COMPACT) return FromDIP(165);
    return FromDIP(115);
  }

  int CategoryWidth(ResponsiveMode mode) const {
    if (mode == MODE_FULL) return FromDIP(88);
    if (mode == MODE_COMPACT) return FromDIP(76);
    return FromDIP(62);
  }

  int ValueWidth(const BoatInfoValue& value, ResponsiveMode mode) const {
    if (mode == MODE_MINIMAL) return FromDIP(108);
    if (mode == MODE_COMPACT) {
      if (value.primitive == BoatInfoValue::PRIMITIVE_LEVEL ||
          value.primitive == BoatInfoValue::PRIMITIVE_TAPE ||
          value.primitive == BoatInfoValue::PRIMITIVE_TREND)
        return FromDIP(148);
      return FromDIP(126);
    }
    if (value.primitive == BoatInfoValue::PRIMITIVE_TAPE) return FromDIP(220);
    if (value.primitive == BoatInfoValue::PRIMITIVE_LEVEL ||
        value.primitive == BoatInfoValue::PRIMITIVE_TREND)
      return FromDIP(185);
    if (value.path.Lower().Find(wxT("latitude")) != wxNOT_FOUND ||
        value.path.Lower().Find(wxT("longitude")) != wxNOT_FOUND)
      return FromDIP(185);
    return FromDIP(145);
  }

  int TokenWidth(const Token& token, ResponsiveMode mode) const {
    if (token.kind == Token::VESSEL) return VesselWidth(mode);
    if (token.kind == Token::CATEGORY) return CategoryWidth(mode);
    return token.value ? ValueWidth(*token.value, mode) : FromDIP(100);
  }

  std::vector<Token> BuildTokens(BoatInfoValue::Priority cutoff) const {
    std::vector<Token> tokens;
    Token vessel;
    vessel.kind = Token::VESSEL;
    tokens.push_back(vessel);

    wxString currentCategory;
    for (size_t i = 0; i < m_values.size(); ++i) {
      const BoatInfoValue& value = m_values[i];
      if (!value.visible || value.priority > cutoff) continue;
      if (value.category != currentCategory) {
        currentCategory = value.category;
        Token category;
        category.kind = Token::CATEGORY;
        category.category = currentCategory;
        tokens.push_back(category);
      }
      Token item;
      item.kind = Token::VALUE;
      item.category = currentCategory;
      item.value = &value;
      tokens.push_back(item);
    }
    return tokens;
  }

  int CountRows(const std::vector<Token>& tokens, ResponsiveMode mode,
                int availableWidth) const {
    if (tokens.empty()) return 0;
    const int gap = FromDIP(4);
    int rows = 1;
    int used = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
      int width = std::min(availableWidth, TokenWidth(tokens[i], mode));
      if (tokens[i].kind == Token::CATEGORY && i + 1 < tokens.size() &&
          tokens[i + 1].kind == Token::VALUE) {
        const int pair = width + gap +
                         std::min(availableWidth,
                                  TokenWidth(tokens[i + 1], mode));
        if (used > 0 && used + gap + pair > availableWidth) {
          ++rows;
          used = 0;
        }
      }
      if (used > 0 && used + gap + width > availableWidth) {
        ++rows;
        used = 0;
      }
      if (used > 0) used += gap;
      used += width;
    }
    return rows;
  }

  void ChooseLayout(ResponsiveMode& mode, BoatInfoValue::Priority& cutoff,
                    int& maxRows) const {
    const wxSize size = GetClientSize();
    const int pad = FromDIP(5);
    const int availableWidth = std::max(FromDIP(120), size.x - 2 * pad);

    struct Candidate {
      ResponsiveMode mode;
      BoatInfoValue::Priority cutoff;
    } candidates[] = {
        {MODE_FULL, BoatInfoValue::PRIORITY_DETAIL},
        {MODE_COMPACT, BoatInfoValue::PRIORITY_DETAIL},
        {MODE_MINIMAL, BoatInfoValue::PRIORITY_DETAIL},
        {MODE_MINIMAL, BoatInfoValue::PRIORITY_SECONDARY},
        {MODE_MINIMAL, BoatInfoValue::PRIORITY_PRIMARY}};

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
      const int rowHeight = RowHeight(candidates[i].mode);
      const int candidateRows = std::max(1, (size.y - 2 * pad) / rowHeight);
      const std::vector<Token> tokens = BuildTokens(candidates[i].cutoff);
      if (CountRows(tokens, candidates[i].mode, availableWidth) <=
          candidateRows) {
        mode = candidates[i].mode;
        cutoff = candidates[i].cutoff;
        maxRows = candidateRows;
        return;
      }
    }

    mode = MODE_MINIMAL;
    cutoff = BoatInfoValue::PRIORITY_PRIMARY;
    maxRows = std::max(1, (size.y - 2 * pad) / RowHeight(mode));
  }

  wxString RenderedValue(const BoatInfoValue& value) const {
    wxString rendered = value.hasNumericValue ? FormatNumber(value)
                                               : value.textValue;
    if (!value.valid || rendered.IsEmpty()) rendered = wxT("—");
    return rendered;
  }

  void DrawVessel(wxDC& dc, const wxRect& rect, ResponsiveMode mode,
                  const wxColour& text, const wxColour& secondary) {
    const int pad = FromDIP(5);
    wxFont sectionFont = GetFont();
    sectionFont.SetPointSize(std::max(7, sectionFont.GetPointSize() - 2));
    sectionFont.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(sectionFont);
    dc.SetTextForeground(secondary);
    dc.DrawText(wxT("VESSEL"), rect.x + pad, rect.y + FromDIP(2));
    dc.SetPen(wxPen(secondary));
    dc.DrawLine(rect.x + pad, rect.y + FromDIP(15), rect.GetRight() - pad,
                rect.y + FromDIP(15));

    wxFont nameFont = GetFont();
    nameFont.SetWeight(wxFONTWEIGHT_BOLD);
    nameFont.SetPointSize(mode == MODE_FULL
                              ? std::max(12, nameFont.GetPointSize() + 3)
                              : mode == MODE_COMPACT
                                    ? std::max(10, nameFont.GetPointSize() + 1)
                                    : std::max(9, nameFont.GetPointSize()));
    dc.SetFont(nameFont);
    dc.SetTextForeground(text);
    dc.DrawText(m_vesselName, rect.x + pad,
                rect.y + (mode == MODE_MINIMAL ? FromDIP(18) : FromDIP(20)));

    if (mode != MODE_MINIMAL) {
      wxString meta;
      if (m_mmsi != wxT("—")) meta += wxT("MMSI ") + m_mmsi;
      if (m_callSign != wxT("—")) {
        if (!meta.IsEmpty()) meta += wxT(" · ");
        meta += m_callSign;
      }
      wxFont metaFont = GetFont();
      metaFont.SetPointSize(std::max(7, metaFont.GetPointSize() - 2));
      dc.SetFont(metaFont);
      dc.SetTextForeground(text);
      if (!meta.IsEmpty())
        dc.DrawText(meta, rect.x + pad,
                    rect.y + (mode == MODE_FULL ? FromDIP(47) : FromDIP(39)));

      if (mode == MODE_FULL && !m_sourceSummary.IsEmpty()) {
        dc.SetTextForeground(secondary);
        wxCoord width = 0, height = 0;
        dc.GetTextExtent(m_sourceSummary, &width, &height);
        if (width < rect.width / 2)
          dc.DrawText(m_sourceSummary,
                      rect.GetRight() - pad - static_cast<int>(width),
                      rect.y + FromDIP(2));
      }
    }
  }

  void DrawCategory(wxDC& dc, const wxRect& rect, ResponsiveMode mode,
                    const wxColour& secondary) {
    const int pad = FromDIP(5);
    wxFont font = GetFont();
    font.SetPointSize(std::max(7, font.GetPointSize() - 2));
    font.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(font);
    dc.SetTextForeground(secondary);
    dc.DrawText(rect.width < FromDIP(68) ? rect.height > FromDIP(35)
                                               ? rect.width > FromDIP(50)
                                                     ? wxString(rect.GetWidth() > 0
                                                                    ? wxT("")
                                                                    : wxT(""))
                                                     : wxT("")
                                               : wxT("")
                                         : wxT(""),
                rect.x, rect.y);
    wxString title;
    // Category titles are deliberately short in minimal mode.
    if (mode == MODE_MINIMAL) {
      if (rect.width <= FromDIP(65)) {
        title = wxT("");
      }
    }
    dc.DrawText(title.IsEmpty() ? wxT("") : title, rect.x + pad,
                rect.y + FromDIP(2));
  }

  void DrawCategoryNamed(wxDC& dc, const wxRect& rect,
                         const wxString& category, ResponsiveMode mode,
                         const wxColour& text,
                         const wxColour& secondary) {
    const int pad = FromDIP(5);
    wxString title = category.Upper();
    if (mode == MODE_MINIMAL) {
      if (category == wxT("Navigation")) title = wxT("NAV");
      if (category == wxT("Electrical")) title = wxT("ELEC");
      if (category == wxT("Propulsion")) title = wxT("PROP");
      if (category == wxT("Environment")) title = wxT("ENV");
      if (category == wxT("Technical")) title = wxT("TECH");
    }
    wxFont font = GetFont();
    font.SetPointSize(std::max(7, font.GetPointSize() - 2));
    font.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(font);
    dc.SetTextForeground(text);
    dc.DrawText(title, rect.x + pad, rect.y + FromDIP(4));
    dc.SetPen(wxPen(secondary));
    const int lineY = rect.y + (mode == MODE_MINIMAL ? FromDIP(20)
                                                   : FromDIP(22));
    dc.DrawLine(rect.x + pad, lineY, rect.GetRight() - FromDIP(4), lineY);
    dc.DrawLine(rect.GetRight() - 1, rect.y + FromDIP(4), rect.GetRight() - 1,
                rect.GetBottom() - FromDIP(4));
  }

  void DrawValue(wxDC& dc, const wxRect& rect, const BoatInfoValue& value,
                 ResponsiveMode mode, const wxColour& text,
                 const wxColour& secondary, const wxColour& accent) {
    dc.SetClippingRegion(rect);
    const int pad = FromDIP(5);
    const wxString label =
        value.label.IsEmpty() ? value.suggestedLabel : value.label;
    const wxString rendered = RenderedValue(value);
    const wxString unit = UnitWithQualifier(value);

    wxFont labelFont = GetFont();
    labelFont.SetPointSize(std::max(7, labelFont.GetPointSize() - 2));
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(labelFont);
    dc.SetTextForeground(text);
    dc.DrawText(label, rect.x + pad, rect.y + FromDIP(2));

    wxFont valueFont = GetFont();
    if (mode == MODE_FULL)
      valueFont.SetPointSize(std::max(12, valueFont.GetPointSize() + 3));
    else if (mode == MODE_COMPACT)
      valueFont.SetPointSize(std::max(10, valueFont.GetPointSize() + 1));
    else
      valueFont.SetPointSize(std::max(9, valueFont.GetPointSize()));
    valueFont.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(valueFont);
    dc.SetTextForeground(value.stale ? secondary : text);
    const int valueY = rect.y + (mode == MODE_MINIMAL ? FromDIP(16)
                                                       : FromDIP(18));
    dc.DrawText(rendered, rect.x + pad, valueY);

    wxCoord valueWidth = 0, valueHeight = 0;
    dc.GetTextExtent(rendered, &valueWidth, &valueHeight);
    if (value.valid && !unit.IsEmpty() &&
        value.path.Lower().Find(wxT("latitude")) == wxNOT_FOUND &&
        value.path.Lower().Find(wxT("longitude")) == wxNOT_FOUND) {
      wxFont unitFont = GetFont();
      unitFont.SetPointSize(std::max(7, unitFont.GetPointSize() - 2));
      dc.SetFont(unitFont);
      dc.SetTextForeground(text);
      dc.DrawText(unit, rect.x + pad + valueWidth + FromDIP(3),
                  valueY + (mode == MODE_FULL ? FromDIP(7) : FromDIP(4)));
    }

    if (!value.valid || value.stale) {
      wxFont stateFont = GetFont();
      stateFont.SetPointSize(std::max(7, stateFont.GetPointSize() - 2));
      dc.SetFont(stateFont);
      dc.SetTextForeground(secondary);
      const wxString state = value.stale ? wxT("STALE") : wxT("NO DATA");
      wxCoord stateWidth = 0, stateHeight = 0;
      dc.GetTextExtent(state, &stateWidth, &stateHeight);
      dc.DrawText(state, rect.GetRight() - pad - static_cast<int>(stateWidth),
                  rect.y + FromDIP(2));
    }

    if (value.valid && mode != MODE_MINIMAL) {
      const int graphY = rect.GetBottom() - FromDIP(12);
      const int graphWidth = std::max(1, rect.width - 2 * pad);
      if (value.primitive == BoatInfoValue::PRIMITIVE_LEVEL &&
          value.hasNumericValue && value.maximum > value.minimum) {
        dc.SetPen(wxPen(secondary));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(rect.x + pad, graphY, graphWidth, FromDIP(5));
        double fraction =
            (value.displayValue - value.minimum) / (value.maximum - value.minimum);
        fraction = std::max(0.0, std::min(1.0, fraction));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(value.stale ? secondary : accent));
        dc.DrawRectangle(rect.x + pad, graphY,
                         static_cast<int>(graphWidth * fraction), FromDIP(5));
      } else if (value.primitive == BoatInfoValue::PRIMITIVE_TAPE &&
                 value.hasNumericValue) {
        const int center = rect.x + rect.width / 2;
        dc.SetPen(wxPen(secondary));
        dc.DrawLine(rect.x + pad, graphY + FromDIP(2), rect.GetRight() - pad,
                    graphY + FromDIP(2));
        dc.SetPen(wxPen(value.stale ? secondary : text,
                        std::max(1, FromDIP(2))));
        dc.DrawLine(center, graphY - FromDIP(3), center,
                    graphY + FromDIP(7));
        if (mode == MODE_FULL) {
          wxFont tapeFont = GetFont();
          tapeFont.SetPointSize(std::max(7, tapeFont.GetPointSize() - 2));
          dc.SetFont(tapeFont);
          dc.SetTextForeground(text);
          for (int offset = -2; offset <= 2; ++offset) {
            double tick = value.displayValue + offset * 10.0;
            while (tick < 0.0) tick += 360.0;
            while (tick >= 360.0) tick -= 360.0;
            const int x = center + offset * FromDIP(38);
            if (x < rect.x + pad || x > rect.GetRight() - pad) continue;
            dc.DrawLine(x, graphY, x, graphY + FromDIP(5));
            dc.DrawText(wxString::Format(wxT("%.0f"), tick), x - FromDIP(7),
                        graphY + FromDIP(5));
          }
        }
      } else if (value.primitive == BoatInfoValue::PRIMITIVE_TREND &&
                 value.trend.size() > 1) {
        double minValue = *std::min_element(value.trend.begin(), value.trend.end());
        double maxValue = *std::max_element(value.trend.begin(), value.trend.end());
        if (maxValue <= minValue) maxValue = minValue + 1.0;
        const int top = graphY - FromDIP(8);
        const int bottom = rect.GetBottom() - FromDIP(3);
        dc.SetPen(wxPen(value.stale ? secondary : accent));
        wxPoint previous;
        for (size_t i = 0; i < value.trend.size(); ++i) {
          const double fx = static_cast<double>(i) /
                            static_cast<double>(value.trend.size() - 1);
          const double fy = (value.trend[i] - minValue) / (maxValue - minValue);
          const wxPoint point(rect.x + pad + static_cast<int>(graphWidth * fx),
                              bottom - static_cast<int>((bottom - top) * fy));
          if (i > 0) dc.DrawLine(previous.x, previous.y, point.x, point.y);
          previous = point;
        }
      }
    }

    dc.SetPen(wxPen(secondary));
    dc.DrawLine(rect.GetRight() - 1, rect.y + FromDIP(5), rect.GetRight() - 1,
                rect.GetBottom() - FromDIP(5));
    dc.DestroyClippingRegion();
  }

  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    wxColour background, text, secondary, accent;
    ResolveHostColours(background, text, secondary, accent);
    dc.SetBackground(wxBrush(background));
    dc.Clear();

    const wxSize size = GetClientSize();
    if (size.x <= 0 || size.y <= 0) return;

    ResponsiveMode mode = MODE_FULL;
    BoatInfoValue::Priority cutoff = BoatInfoValue::PRIORITY_DETAIL;
    int maxRows = 1;
    ChooseLayout(mode, cutoff, maxRows);

    const std::vector<Token> tokens = BuildTokens(cutoff);
    const int pad = FromDIP(5);
    const int gap = FromDIP(4);
    const int availableWidth = std::max(FromDIP(120), size.x - 2 * pad);
    const int rowHeight = RowHeight(mode);
    int x = pad;
    int y = pad;
    int row = 0;
    int hidden = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
      int width = std::min(availableWidth, TokenWidth(tokens[i], mode));
      if (tokens[i].kind == Token::CATEGORY && i + 1 < tokens.size() &&
          tokens[i + 1].kind == Token::VALUE) {
        const int pair = width + gap +
                         std::min(availableWidth,
                                  TokenWidth(tokens[i + 1], mode));
        if (x > pad && x - pad + gap + pair > availableWidth) {
          ++row;
          x = pad;
          y += rowHeight;
        }
      }
      if (x > pad && x - pad + gap + width > availableWidth) {
        ++row;
        x = pad;
        y += rowHeight;
      }
      if (row >= maxRows) {
        if (tokens[i].kind == Token::VALUE) ++hidden;
        continue;
      }
      if (x > pad) x += gap;
      const wxRect rect(x, y, width, rowHeight - FromDIP(2));
      if (tokens[i].kind == Token::VESSEL)
        DrawVessel(dc, rect, mode, text, secondary);
      else if (tokens[i].kind == Token::CATEGORY)
        DrawCategoryNamed(dc, rect, tokens[i].category, mode, text, secondary);
      else if (tokens[i].value)
        DrawValue(dc, rect, *tokens[i].value, mode, text, secondary, accent);
      x += width;
    }

    for (size_t i = 0; i < m_values.size(); ++i) {
      if (m_values[i].visible && m_values[i].priority > cutoff) ++hidden;
    }
    if (hidden > 0) {
      wxString note = wxString::Format(wxT("+%d hidden"), hidden);
      wxFont font = GetFont();
      font.SetPointSize(std::max(7, font.GetPointSize() - 2));
      dc.SetFont(font);
      dc.SetTextForeground(secondary);
      wxCoord width = 0, height = 0;
      dc.GetTextExtent(note, &width, &height);
      dc.DrawText(note, size.x - pad - static_cast<int>(width),
                  size.y - pad - static_cast<int>(height));
    }
  }

  void OnSize(wxSizeEvent& event) {
    Refresh(false);
    event.Skip();
  }

  std::vector<BoatInfoValue> m_values;
  wxString m_vesselName;
  wxString m_mmsi;
  wxString m_callSign;
  wxString m_sourceSummary;
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
      .MinSize(-1, m_panel->FromDIP(82))
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
  value.suggestedLabel = SuggestedLabelFromPath(path);
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

void boatinfo_pi::ApplySemanticDefaults(BoatInfoValue& value) {
  const wxString path = value.path.Lower();
  const bool configured = value.userConfigured;
  if (value.suggestedLabel.IsEmpty())
    value.suggestedLabel = SuggestedLabelFromPath(value.path);

  value.category = wxT("Technical");
  value.semanticId = path;
  value.alternativeSource = false;
  value.freshnessSeconds = 30;
  if (!configured) value.priority = BoatInfoValue::PRIORITY_DETAIL;
  wxString suggestion = value.suggestedLabel;

  if (value.source == wxT("OpenCPN")) {
    value.category = wxT("Navigation");
    value.freshnessSeconds = 10;
    if (path == wxT("navigation.speedoverground")) {
      suggestion = wxT("SOG");
      value.semanticId = wxT("navigation.sog");
      value.unit = wxT("kn");
      if (!configured) {
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_PRIMARY;
      }
    } else if (path == wxT("navigation.courseovergroundtrue")) {
      suggestion = wxT("COG");
      value.semanticId = wxT("navigation.cog.true");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_PRIMARY;
      }
    } else if (path == wxT("navigation.headingtrue")) {
      suggestion = wxT("Heading");
      value.semanticId = wxT("navigation.heading.true");
      value.unit = wxT("°");
      value.qualifier = wxT("T");
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_SECONDARY;
      }
    } else if (path == wxT("navigation.headingmagnetic")) {
      suggestion = wxT("Heading magnetic");
      value.semanticId = wxT("navigation.heading.magnetic");
      value.unit = wxT("°");
      value.qualifier = wxT("M");
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_TAPE;
        value.visible = false;
        value.priority = BoatInfoValue::PRIORITY_DETAIL;
      }
    } else if (path == wxT("navigation.position.latitude")) {
      suggestion = wxT("Latitude");
      value.semanticId = wxT("navigation.latitude");
      value.unit.clear();
      if (!configured) {
        value.visible = false;
        value.priority = BoatInfoValue::PRIORITY_DETAIL;
      }
    } else if (path == wxT("navigation.position.longitude")) {
      suggestion = wxT("Longitude");
      value.semanticId = wxT("navigation.longitude");
      value.unit.clear();
      if (!configured) {
        value.visible = false;
        value.priority = BoatInfoValue::PRIORITY_DETAIL;
      }
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
    value.priority = BoatInfoValue::PRIORITY_PRIMARY;
  } else if (path == wxT("mmsi")) {
    value.category = wxT("Vessel");
    suggestion = wxT("MMSI");
    value.freshnessSeconds = 300;
    value.visible = true;
    value.primitive = BoatInfoValue::PRIMITIVE_NONE;
    value.priority = BoatInfoValue::PRIORITY_PRIMARY;
  } else if (path == wxT("communication.callsignvhf")) {
    value.category = wxT("Vessel");
    suggestion = wxT("Call sign");
    value.freshnessSeconds = 300;
    value.visible = true;
    value.primitive = BoatInfoValue::PRIMITIVE_NONE;
    value.priority = BoatInfoValue::PRIORITY_PRIMARY;
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
        value.priority = BoatInfoValue::PRIORITY_PRIMARY;
      }
    } else if (path.Find(wxT("timeremaining")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" time remaining");
      value.unit = wxT("h");
      if (!configured) value.priority = BoatInfoValue::PRIORITY_DETAIL;
    } else if (path.Find(wxT("dischargesincefull")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" discharge since full");
      value.unit = wxT("Ah");
      if (!configured) value.priority = BoatInfoValue::PRIORITY_DETAIL;
    } else if (path.Find(wxT("lifetimedischarge")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" lifetime discharge");
      value.unit = wxT("Ah");
      if (!configured) value.priority = BoatInfoValue::PRIORITY_DETAIL;
    } else if (path.Find(wxT("lifetimerecharge")) != wxNOT_FOUND) {
      suggestion = battery + wxT(" lifetime recharge");
      value.unit = wxT("Ah");
      if (!configured) value.priority = BoatInfoValue::PRIORITY_DETAIL;
    } else if (path.EndsWith(wxT(".voltage"))) {
      suggestion = battery + wxT(" voltage");
      value.unit = wxT("V");
      if (!configured) {
        value.visible = path.Find(wxT("service")) != wxNOT_FOUND ||
                        path.Find(wxT("starter")) != wxNOT_FOUND;
        value.priority = path.Find(wxT("service")) != wxNOT_FOUND
                             ? BoatInfoValue::PRIORITY_SECONDARY
                             : BoatInfoValue::PRIORITY_DETAIL;
      }
    } else if (path.EndsWith(wxT(".current"))) {
      suggestion = battery + wxT(" current");
      value.unit = wxT("A");
      if (!configured) {
        value.visible = path.Find(wxT("service")) != wxNOT_FOUND;
        value.priority = BoatInfoValue::PRIORITY_SECONDARY;
      }
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
      if (!configured) {
        value.primitive = BoatInfoValue::PRIMITIVE_LEVEL;
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_PRIMARY;
      }
    }
  } else if (path.StartsWith(wxT("propulsion."))) {
    value.category = wxT("Propulsion");
    const wxString engine = PathSegmentAfter(path, wxT("propulsion."));
    const wxString engineName = engine.IsEmpty() ? wxT("Engine")
                                                  : HumanizeToken(engine);
    if (path.Find(wxT("revolutions")) != wxNOT_FOUND) {
      suggestion = engineName + wxT(" RPM");
      value.unit = wxT("rpm");
      if (!configured) {
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_SECONDARY;
      }
    } else if (path.Find(wxT("temperature")) != wxNOT_FOUND) {
      suggestion = engineName + wxT(" temperature");
      value.unit = wxT("°C");
    }
  } else if (path.StartsWith(wxT("environment."))) {
    value.category = wxT("Environment");
    if (path.Find(wxT("depth")) != wxNOT_FOUND) {
      value.unit = wxT("m");
      suggestion = wxT("Depth");
      if (!configured) {
        value.visible = true;
        value.priority = BoatInfoValue::PRIORITY_PRIMARY;
      }
    }
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
    if (!configured) {
      value.visible = false;
      value.priority = BoatInfoValue::PRIORITY_DETAIL;
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
    if (!configured) value.priority = BoatInfoValue::PRIORITY_DETAIL;
  }

  value.suggestedLabel = suggestion;
  if (!value.labelCustomized) value.label = suggestion;
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
    const bool shouldBeStale =
        value.valid && value.lastUpdateSeconds > 0 &&
        now - value.lastUpdateSeconds > value.freshnessSeconds;
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
  const wxString normalized = NormalizeSignalKSelf(signalKSelf);
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
  wxJSONValue root;
  wxJSONReader reader;
  if (reader.Parse(message, &root) != 0 || !root.IsObject()) return false;

  bool handled = false;
  if (root.HasMember(wxT("self")) && root[wxT("self")].IsString()) {
    const wxString self = NormalizeSignalKSelf(root[wxT("self")].AsString());
    if (!self.IsEmpty()) {
      ActivateVesselProfile(self);
      handled = true;
    }
  }

  if (root.HasMember(wxT("context")) && root[wxT("context")].IsString()) {
    const wxString context = NormalizeSignalKSelf(root[wxT("context")].AsString());
    if (!m_signalKSelf.IsEmpty() && context != m_signalKSelf) return handled;
  }
  if (!root.HasMember(wxT("updates")) || !root[wxT("updates")].IsArray())
    return handled;

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
      wxT("BoatInfo is a responsive own-vessel status strip. It is docked below "
          "the OpenCPN canvas, wraps selected values onto additional rows and "
          "automatically simplifies Full → Compact → Minimal presentation when "
          "space becomes tight. Detail values disappear first, then Secondary; "
          "Primary values are protected as long as possible. No scrolling is "
          "required in the live navigation view."));
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

  wxFlexGridSizer* identityGrid = new wxFlexGridSizer(2, dialog.FromDIP(6),
                                                      dialog.FromDIP(10));
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
  wxFlexGridSizer* grid = new wxFlexGridSizer(6, dialog.FromDIP(5),
                                             dialog.FromDIP(10));
  grid->AddGrowableCol(3, 1);

  wxFont headerFont = dialog.GetFont();
  headerFont.SetWeight(wxFONTWEIGHT_BOLD);
  const wxString headers[] = {wxT("Show"), wxT("Priority"), wxT("Source"),
                              wxT("Name"), wxT("Display"), wxT("Path")};
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
    row.primitive->SetSelection(SelectionFromPrimitive(value.primitive));
    grid->Add(row.primitive, 0, wxEXPAND);

    wxStaticText* pathText = new wxStaticText(scroll, wxID_ANY, value.path);
    pathText->SetToolTip(value.path);
    grid->Add(pathText, 0, wxALIGN_CENTER_VERTICAL);
    rows.push_back(row);
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
  }
}

void boatinfo_pi::ApplyPreferenceRows(const std::vector<PreferenceRow>& rows) {
  for (size_t i = 0; i < rows.size(); ++i) {
    std::map<wxString, BoatInfoValue>::iterator found =
        m_values.find(rows[i].key);
    if (found == m_values.end()) continue;
    BoatInfoValue& value = found->second;
    value.visible = rows[i].visible->GetValue();
    value.priority = PriorityFromSelection(rows[i].priority->GetSelection());
    value.primitive = PrimitiveFromSelection(rows[i].primitive->GetSelection());
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
    long priority = static_cast<long>(value.priority);
    config->Read(wxT("Priority"), &priority, priority);
    value.userConfigured = true;
    value.visible = visible;
    if (primitive >= BoatInfoValue::PRIMITIVE_VALUE &&
        primitive <= BoatInfoValue::PRIMITIVE_NONE)
      value.primitive = static_cast<BoatInfoValue::Primitive>(primitive);
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

#include "boatinfo_dashboard.h"

#include <algorithm>
#include <cmath>

#include <wx/dcbuffer.h>
#include <wx/settings.h>

namespace {
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
}  // namespace

BoatInfoDashboardPanel::BoatInfoDashboardPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxBORDER_NONE) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &BoatInfoDashboardPanel::OnPaint, this);
  Bind(wxEVT_SIZE, &BoatInfoDashboardPanel::OnSize, this);
}

void BoatInfoDashboardPanel::SetState(
    const std::vector<BoatInfoValue>& values, const wxString& vesselName,
    const wxString& mmsi, const wxString& callSign,
    const wxString& sourceSummary) {
  m_values = values;
  m_vesselName = vesselName.IsEmpty() ? wxT("—") : vesselName;
  m_mmsi = mmsi.IsEmpty() ? wxT("—") : mmsi;
  m_callSign = callSign.IsEmpty() ? wxT("—") : callSign;
  m_sourceSummary = sourceSummary;
  Refresh(false);
}

int BoatInfoDashboardPanel::HeaderHeight(ResponsiveMode mode) const {
  return FromDIP(mode == MODE_MINIMAL ? 18 : 21);
}

int BoatInfoDashboardPanel::ContentRowHeight(ResponsiveMode mode) const {
  if (mode == MODE_FULL) return FromDIP(54);
  if (mode == MODE_COMPACT) return FromDIP(40);
  return FromDIP(29);
}

int BoatInfoDashboardPanel::VesselWidth(ResponsiveMode mode) const {
  if (mode == MODE_FULL) return FromDIP(210);
  if (mode == MODE_COMPACT) return FromDIP(175);
  return FromDIP(125);
}

int BoatInfoDashboardPanel::ValueWidth(const BoatInfoValue& value,
                                       ResponsiveMode mode) const {
  if (mode == MODE_MINIMAL) return FromDIP(105);
  if (mode == MODE_COMPACT) {
    if (value.primitive == BoatInfoValue::PRIMITIVE_LEVEL ||
        value.primitive == BoatInfoValue::PRIMITIVE_TAPE ||
        value.primitive == BoatInfoValue::PRIMITIVE_TREND)
      return FromDIP(145);
    return FromDIP(122);
  }
  if (value.primitive == BoatInfoValue::PRIMITIVE_TAPE) return FromDIP(215);
  if (value.primitive == BoatInfoValue::PRIMITIVE_LEVEL ||
      value.primitive == BoatInfoValue::PRIMITIVE_TREND)
    return FromDIP(180);
  if (value.path.Lower().Find(wxT("latitude")) != wxNOT_FOUND ||
      value.path.Lower().Find(wxT("longitude")) != wxNOT_FOUND)
    return FromDIP(180);
  return FromDIP(142);
}

std::vector<BoatInfoDashboardPanel::Group>
BoatInfoDashboardPanel::BuildGroups(BoatInfoValue::Priority cutoff) const {
  std::vector<Group> groups;
  Group vessel;
  vessel.vessel = true;
  groups.push_back(vessel);

  wxString current;
  for (size_t i = 0; i < m_values.size(); ++i) {
    const BoatInfoValue& value = m_values[i];
    if (!value.visible || value.priority > cutoff) continue;
    if (groups.empty() || value.category != current) {
      Group group;
      group.category = value.category;
      groups.push_back(group);
      current = value.category;
    }
    groups.back().values.push_back(&value);
  }
  return groups;
}

int BoatInfoDashboardPanel::GroupPreferredWidth(const Group& group,
                                                ResponsiveMode mode,
                                                int availableWidth) const {
  if (group.vessel) return std::min(availableWidth, VesselWidth(mode));
  const int gap = FromDIP(3);
  int sum = 0;
  int widest = FromDIP(105);
  for (size_t i = 0; i < group.values.size(); ++i) {
    const int width = ValueWidth(*group.values[i], mode);
    sum += width + (i ? gap : 0);
    widest = std::max(widest, width);
  }
  int cap = FromDIP(mode == MODE_FULL ? 560 : mode == MODE_COMPACT ? 430 : 325);
  return std::min(availableWidth, std::max(widest, std::min(sum, cap)));
}

int BoatInfoDashboardPanel::GroupHeight(const Group& group,
                                        ResponsiveMode mode, int width) const {
  if (group.vessel) return HeaderHeight(mode) + ContentRowHeight(mode);
  const int gap = FromDIP(3);
  int rows = 1;
  int used = 0;
  for (size_t i = 0; i < group.values.size(); ++i) {
    const int itemWidth = std::min(width, ValueWidth(*group.values[i], mode));
    if (used > 0 && used + gap + itemWidth > width) {
      ++rows;
      used = 0;
    }
    if (used > 0) used += gap;
    used += itemWidth;
  }
  return HeaderHeight(mode) + rows * ContentRowHeight(mode);
}

int BoatInfoDashboardPanel::LayoutHeight(const std::vector<Group>& groups,
                                         ResponsiveMode mode,
                                         int availableWidth) const {
  const int gap = FromDIP(5);
  int totalHeight = 0;
  int rowWidth = 0;
  int rowHeight = 0;
  for (size_t i = 0; i < groups.size(); ++i) {
    const int width = GroupPreferredWidth(groups[i], mode, availableWidth);
    const int height = GroupHeight(groups[i], mode, width);
    if (rowWidth > 0 && rowWidth + gap + width > availableWidth) {
      totalHeight += rowHeight + gap;
      rowWidth = 0;
      rowHeight = 0;
    }
    if (rowWidth > 0) rowWidth += gap;
    rowWidth += width;
    rowHeight = std::max(rowHeight, height);
  }
  if (rowWidth > 0) totalHeight += rowHeight;
  return totalHeight;
}

void BoatInfoDashboardPanel::ChooseLayout(
    ResponsiveMode& mode, BoatInfoValue::Priority& cutoff) const {
  const wxSize size = GetClientSize();
  const int pad = FromDIP(5);
  const int availableWidth = std::max(FromDIP(120), size.x - 2 * pad);
  const int availableHeight = std::max(FromDIP(30), size.y - 2 * pad);

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
    const std::vector<Group> groups = BuildGroups(candidates[i].cutoff);
    if (LayoutHeight(groups, candidates[i].mode, availableWidth) <=
        availableHeight) {
      mode = candidates[i].mode;
      cutoff = candidates[i].cutoff;
      return;
    }
  }
  mode = MODE_MINIMAL;
  cutoff = BoatInfoValue::PRIORITY_PRIMARY;
}

wxString BoatInfoDashboardPanel::RenderedValue(
    const BoatInfoValue& value) const {
  wxString rendered =
      value.hasNumericValue ? FormatNumber(value) : value.textValue;
  if (!value.valid || rendered.IsEmpty()) rendered = wxT("—");
  return rendered;
}

wxString BoatInfoDashboardPanel::DisplayLabel(
    const BoatInfoValue& value, ResponsiveMode mode) const {
  wxString label = value.label.IsEmpty() ? value.suggestedLabel : value.label;
  if (mode != MODE_MINIMAL) return label;
  const wxString path = value.path.Lower();
  if (path.Find(wxT("speedoverground")) != wxNOT_FOUND) return wxT("SOG");
  if (path.Find(wxT("courseoverground")) != wxNOT_FOUND) return wxT("COG");
  if (path.Find(wxT("heading")) != wxNOT_FOUND) return wxT("HDG");
  if (path.Find(wxT("stateofcharge")) != wxNOT_FOUND) return wxT("SOC");
  if (path.Find(wxT("voltage")) != wxNOT_FOUND) return wxT("Voltage");
  if (path.Find(wxT("current")) != wxNOT_FOUND) return wxT("Current");
  if (path.Find(wxT("revolutions")) != wxNOT_FOUND) return wxT("RPM");
  if (path.Find(wxT("depth")) != wxNOT_FOUND) return wxT("Depth");
  if (path.Find(wxT("wind.angleapparent")) != wxNOT_FOUND)
    return wxT("App. angle");
  if (path.Find(wxT("wind.speedapparent")) != wxNOT_FOUND)
    return wxT("App. speed");
  return label;
}

wxString BoatInfoDashboardPanel::CategoryTitle(
    const wxString& category, ResponsiveMode mode) const {
  if (mode != MODE_MINIMAL) return category.Upper();
  if (category == wxT("Navigation")) return wxT("NAV");
  if (category == wxT("Electrical")) return wxT("ELEC");
  if (category == wxT("Propulsion")) return wxT("PROP");
  if (category == wxT("Environment")) return wxT("ENV");
  if (category == wxT("Technical")) return wxT("TECH");
  return category.Upper();
}

void BoatInfoDashboardPanel::DrawVessel(wxDC& dc, const wxRect& rect,
                                        ResponsiveMode mode,
                                        const wxColour& text,
                                        const wxColour& secondary) {
  const int pad = FromDIP(5);
  wxFont header = GetFont();
  header.SetPointSize(std::max(7, header.GetPointSize() - 2));
  header.SetWeight(wxFONTWEIGHT_BOLD);
  dc.SetFont(header);
  dc.SetTextForeground(secondary);
  dc.DrawText(wxT("VESSEL"), rect.x + pad, rect.y + FromDIP(2));
  dc.SetPen(wxPen(secondary));
  dc.DrawLine(rect.x + pad, rect.y + HeaderHeight(mode) - FromDIP(4),
              rect.GetRight() - pad,
              rect.y + HeaderHeight(mode) - FromDIP(4));

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
              rect.y + HeaderHeight(mode) + FromDIP(1));

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
                  rect.y + HeaderHeight(mode) +
                      (mode == MODE_FULL ? FromDIP(28) : FromDIP(22)));
  }
}

void BoatInfoDashboardPanel::DrawValue(
    wxDC& dc, const wxRect& rect, const BoatInfoValue& value,
    ResponsiveMode mode, const wxColour& text, const wxColour& secondary,
    const wxColour& accent) {
  dc.SetClippingRegion(rect);
  const int pad = FromDIP(4);
  const wxString label = DisplayLabel(value, mode);
  const wxString rendered = RenderedValue(value);
  const wxString unit = UnitWithQualifier(value);

  wxFont labelFont = GetFont();
  labelFont.SetPointSize(std::max(7, labelFont.GetPointSize() - 2));
  labelFont.SetWeight(wxFONTWEIGHT_BOLD);
  dc.SetFont(labelFont);
  dc.SetTextForeground(text);
  dc.DrawText(label, rect.x + pad, rect.y + FromDIP(1));

  wxFont valueFont = GetFont();
  valueFont.SetWeight(wxFONTWEIGHT_BOLD);
  if (mode == MODE_FULL)
    valueFont.SetPointSize(std::max(12, valueFont.GetPointSize() + 3));
  else if (mode == MODE_COMPACT)
    valueFont.SetPointSize(std::max(10, valueFont.GetPointSize() + 1));
  else
    valueFont.SetPointSize(std::max(9, valueFont.GetPointSize()));
  dc.SetFont(valueFont);
  dc.SetTextForeground(value.stale ? secondary : text);
  const int valueY = rect.y + (mode == MODE_MINIMAL ? FromDIP(14)
                                                       : FromDIP(17));
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
                rect.y + FromDIP(1));
  }

  if (value.valid && mode != MODE_MINIMAL) {
    const int graphY = rect.GetBottom() - FromDIP(10);
    const int graphWidth = std::max(1, rect.width - 2 * pad);
    if (value.primitive == BoatInfoValue::PRIMITIVE_LEVEL &&
        value.hasNumericValue && value.maximum > value.minimum) {
      dc.SetPen(wxPen(secondary));
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.DrawRectangle(rect.x + pad, graphY, graphWidth, FromDIP(4));
      double fraction =
          (value.displayValue - value.minimum) / (value.maximum - value.minimum);
      fraction = std::max(0.0, std::min(1.0, fraction));
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(value.stale ? secondary : accent));
      dc.DrawRectangle(rect.x + pad, graphY,
                       static_cast<int>(graphWidth * fraction), FromDIP(4));
    } else if (value.primitive == BoatInfoValue::PRIMITIVE_TAPE &&
               value.hasNumericValue) {
      const int center = rect.x + rect.width / 2;
      dc.SetPen(wxPen(secondary));
      dc.DrawLine(rect.x + pad, graphY + FromDIP(1), rect.GetRight() - pad,
                  graphY + FromDIP(1));
      dc.SetPen(wxPen(value.stale ? secondary : text,
                      std::max(1, FromDIP(2))));
      dc.DrawLine(center, graphY - FromDIP(4), center, graphY + FromDIP(5));
      if (mode == MODE_FULL) {
        wxFont tapeFont = GetFont();
        tapeFont.SetPointSize(std::max(7, tapeFont.GetPointSize() - 2));
        dc.SetFont(tapeFont);
        dc.SetTextForeground(text);
        for (int offset = -2; offset <= 2; ++offset) {
          double tick = value.displayValue + offset * 10.0;
          while (tick < 0.0) tick += 360.0;
          while (tick >= 360.0) tick -= 360.0;
          const int x = center + offset * FromDIP(36);
          if (x < rect.x + pad || x > rect.GetRight() - pad) continue;
          dc.DrawLine(x, graphY - FromDIP(1), x, graphY + FromDIP(4));
          dc.DrawText(wxString::Format(wxT("%.0f"), tick), x - FromDIP(7),
                      graphY + FromDIP(4));
        }
      }
    } else if (value.primitive == BoatInfoValue::PRIMITIVE_TREND &&
               value.trend.size() > 1) {
      double minValue =
          *std::min_element(value.trend.begin(), value.trend.end());
      double maxValue =
          *std::max_element(value.trend.begin(), value.trend.end());
      if (maxValue <= minValue) maxValue = minValue + 1.0;
      const int top = graphY - FromDIP(7);
      const int bottom = rect.GetBottom() - FromDIP(2);
      dc.SetPen(wxPen(value.stale ? secondary : accent));
      wxPoint previous;
      for (size_t i = 0; i < value.trend.size(); ++i) {
        const double fx = static_cast<double>(i) /
                          static_cast<double>(value.trend.size() - 1);
        const double fy =
            (value.trend[i] - minValue) / (maxValue - minValue);
        const wxPoint point(rect.x + pad + static_cast<int>(graphWidth * fx),
                            bottom - static_cast<int>((bottom - top) * fy));
        if (i > 0) dc.DrawLine(previous.x, previous.y, point.x, point.y);
        previous = point;
      }
    }
  }
  dc.DestroyClippingRegion();
}

void BoatInfoDashboardPanel::DrawGroup(
    wxDC& dc, const wxRect& rect, const Group& group, ResponsiveMode mode,
    const wxColour& text, const wxColour& secondary, const wxColour& accent) {
  const int pad = FromDIP(4);
  const int gap = FromDIP(3);
  const int headerHeight = HeaderHeight(mode);

  wxFont headerFont = GetFont();
  headerFont.SetPointSize(std::max(7, headerFont.GetPointSize() - 2));
  headerFont.SetWeight(wxFONTWEIGHT_BOLD);
  dc.SetFont(headerFont);
  dc.SetTextForeground(text);
  dc.DrawText(CategoryTitle(group.category, mode), rect.x + pad,
              rect.y + FromDIP(2));
  dc.SetPen(wxPen(secondary));
  dc.DrawLine(rect.x + pad, rect.y + headerHeight - FromDIP(4),
              rect.GetRight() - pad, rect.y + headerHeight - FromDIP(4));

  int x = rect.x;
  int y = rect.y + headerHeight;
  for (size_t i = 0; i < group.values.size(); ++i) {
    const int width = std::min(rect.width, ValueWidth(*group.values[i], mode));
    if (x > rect.x && x - rect.x + gap + width > rect.width) {
      x = rect.x;
      y += ContentRowHeight(mode);
    }
    if (x > rect.x) x += gap;
    wxRect valueRect(x, y, width, ContentRowHeight(mode));
    DrawValue(dc, valueRect, *group.values[i], mode, text, secondary, accent);
    x += width;
  }

  dc.SetPen(wxPen(secondary));
  dc.DrawLine(rect.GetRight() - 1, rect.y + FromDIP(3), rect.GetRight() - 1,
              rect.GetBottom() - FromDIP(3));
}

void BoatInfoDashboardPanel::OnPaint(wxPaintEvent&) {
  wxAutoBufferedPaintDC dc(this);
  wxColour background, text, secondary, accent;
  ResolveHostColours(background, text, secondary, accent);
  dc.SetBackground(wxBrush(background));
  dc.Clear();

  const wxSize size = GetClientSize();
  if (size.x <= 0 || size.y <= 0) return;

  ResponsiveMode mode = MODE_FULL;
  BoatInfoValue::Priority cutoff = BoatInfoValue::PRIORITY_DETAIL;
  ChooseLayout(mode, cutoff);
  const std::vector<Group> groups = BuildGroups(cutoff);

  const int pad = FromDIP(5);
  const int gap = FromDIP(5);
  const int availableWidth = std::max(FromDIP(120), size.x - 2 * pad);
  int x = pad;
  int y = pad;
  int shelfHeight = 0;
  int hidden = 0;

  for (size_t i = 0; i < groups.size(); ++i) {
    const int width = GroupPreferredWidth(groups[i], mode, availableWidth);
    const int height = GroupHeight(groups[i], mode, width);
    if (x > pad && x - pad + gap + width > availableWidth) {
      x = pad;
      y += shelfHeight + gap;
      shelfHeight = 0;
    }
    if (y + height > size.y - pad) {
      if (!groups[i].vessel) hidden += static_cast<int>(groups[i].values.size());
      continue;
    }
    if (x > pad) x += gap;
    const wxRect rect(x, y, width, height);
    if (groups[i].vessel)
      DrawVessel(dc, rect, mode, text, secondary);
    else
      DrawGroup(dc, rect, groups[i], mode, text, secondary, accent);
    x += width;
    shelfHeight = std::max(shelfHeight, height);
  }

  for (size_t i = 0; i < m_values.size(); ++i) {
    if (m_values[i].visible && m_values[i].priority > cutoff) ++hidden;
  }
  if (hidden > 0) {
    const wxString note = wxString::Format(wxT("+%d hidden"), hidden);
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

void BoatInfoDashboardPanel::OnSize(wxSizeEvent& event) {
  Refresh(false);
  event.Skip();
}

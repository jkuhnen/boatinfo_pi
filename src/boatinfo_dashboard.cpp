#include "boatinfo_dashboard.h"

#include <algorithm>

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
  if (mode == MODE_FULL) return FromDIP(20);
  if (mode == MODE_COMPACT) return FromDIP(18);
  return FromDIP(17);
}

int BoatInfoDashboardPanel::ContentRowHeight(ResponsiveMode mode) const {
  if (mode == MODE_FULL) return FromDIP(36);
  if (mode == MODE_COMPACT) return FromDIP(31);
  return FromDIP(27);
}

int BoatInfoDashboardPanel::VesselWidth(ResponsiveMode mode) const {
  if (mode == MODE_FULL) return FromDIP(205);
  if (mode == MODE_COMPACT) return FromDIP(170);
  return FromDIP(125);
}

int BoatInfoDashboardPanel::ValueWidth(const BoatInfoValue&,
                                       ResponsiveMode mode) const {
  // Fixed tile widths are intentional. Values never stretch individually;
  // this keeps columns stable and left-aligned across all categories.
  if (mode == MODE_FULL) return FromDIP(160);
  if (mode == MODE_COMPACT) return FromDIP(132);
  return FromDIP(108);
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
  const int tile = ValueWidth(BoatInfoValue(), mode);
  const int gap = FromDIP(3);
  const int count = std::max(1, static_cast<int>(group.values.size()));
  const int desired = count * tile + (count - 1) * gap;
  return std::min(availableWidth, desired);
}

int BoatInfoDashboardPanel::GroupHeight(const Group& group,
                                        ResponsiveMode mode, int width) const {
  if (group.vessel) return HeaderHeight(mode) + ContentRowHeight(mode);
  const int gap = FromDIP(3);
  const int tile = ValueWidth(BoatInfoValue(), mode);
  const int columns = std::max(1, (width + gap) / (tile + gap));
  const int rows = std::max(
      1, (static_cast<int>(group.values.size()) + columns - 1) / columns);
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
    const BoatInfoValue& value, ResponsiveMode) const {
  return value.label.IsEmpty() ? value.suggestedLabel : value.label;
}

wxString BoatInfoDashboardPanel::CategoryTitle(
    const wxString& category, ResponsiveMode) const {
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
  dc.DrawText(wxT("VESSEL"), rect.x + pad, rect.y + FromDIP(1));
  dc.SetPen(wxPen(secondary));
  dc.DrawLine(rect.x, rect.y + HeaderHeight(mode) - FromDIP(3),
              rect.GetRight(), rect.y + HeaderHeight(mode) - FromDIP(3));

  wxFont nameFont = GetFont();
  nameFont.SetWeight(wxFONTWEIGHT_BOLD);
  nameFont.SetPointSize(mode == MODE_FULL
                            ? std::max(11, nameFont.GetPointSize() + 2)
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
    if (!meta.IsEmpty()) {
      wxFont metaFont = GetFont();
      metaFont.SetPointSize(std::max(7, metaFont.GetPointSize() - 2));
      dc.SetFont(metaFont);
      dc.SetTextForeground(text);
      dc.DrawText(meta, rect.x + pad,
                  rect.y + HeaderHeight(mode) + FromDIP(20));
    }
  }
}

void BoatInfoDashboardPanel::DrawValue(
    wxDC& dc, const wxRect& rect, const BoatInfoValue& value,
    ResponsiveMode mode, const wxColour& text, const wxColour&,
    const wxColour&) {
  dc.SetClippingRegion(rect);
  const int pad = FromDIP(4);
  const wxString label = DisplayLabel(value, mode);
  const bool displayValid = value.valid && !value.stale;

  wxFont labelFont = GetFont();
  labelFont.SetPointSize(std::max(7, labelFont.GetPointSize() - 2));
  labelFont.SetWeight(wxFONTWEIGHT_BOLD);
  dc.SetFont(labelFont);
  dc.SetTextForeground(displayValid ? text : *wxWHITE);
  dc.DrawText(label, rect.x + pad, rect.y + FromDIP(1));

  // Preserve the tile and category geometry, but never present a stale,
  // unavailable or otherwise invalid measurement as a current value.
  if (!displayValid) {
    dc.DestroyClippingRegion();
    return;
  }

  const wxString rendered = RenderedValue(value);
  const wxString unit = UnitWithQualifier(value);

  wxFont valueFont = GetFont();
  valueFont.SetWeight(wxFONTWEIGHT_BOLD);
  if (mode == MODE_FULL)
    valueFont.SetPointSize(std::max(10, valueFont.GetPointSize() + 1));
  else
    valueFont.SetPointSize(std::max(9, valueFont.GetPointSize()));
  dc.SetFont(valueFont);
  dc.SetTextForeground(text);
  const int valueY = rect.y + FromDIP(15);
  dc.DrawText(rendered, rect.x + pad, valueY);

  wxCoord valueWidth = 0, valueHeight = 0;
  dc.GetTextExtent(rendered, &valueWidth, &valueHeight);
  if (!unit.IsEmpty() &&
      value.path.Lower().Find(wxT("latitude")) == wxNOT_FOUND &&
      value.path.Lower().Find(wxT("longitude")) == wxNOT_FOUND) {
    wxFont unitFont = GetFont();
    unitFont.SetPointSize(std::max(7, unitFont.GetPointSize() - 2));
    dc.SetFont(unitFont);
    dc.SetTextForeground(text);
    dc.DrawText(unit, rect.x + pad + valueWidth + FromDIP(3),
                valueY + FromDIP(2));
  }

  // Text-only baseline: no Level, Tape, Trend or analog geometry is rendered.
  dc.DestroyClippingRegion();
}

void BoatInfoDashboardPanel::DrawGroup(
    wxDC& dc, const wxRect& rect, const Group& group, ResponsiveMode mode,
    const wxColour& text, const wxColour& secondary, const wxColour& accent) {
  const int pad = FromDIP(4);
  const int gap = FromDIP(3);
  const int headerHeight = HeaderHeight(mode);
  const int tile = ValueWidth(BoatInfoValue(), mode);

  wxFont headerFont = GetFont();
  headerFont.SetPointSize(std::max(7, headerFont.GetPointSize() - 2));
  headerFont.SetWeight(wxFONTWEIGHT_BOLD);
  dc.SetFont(headerFont);
  dc.SetTextForeground(text);
  dc.DrawText(CategoryTitle(group.category, mode), rect.x + pad,
              rect.y + FromDIP(1));
  dc.SetPen(wxPen(secondary));
  dc.DrawLine(rect.x, rect.y + headerHeight - FromDIP(3), rect.GetRight(),
              rect.y + headerHeight - FromDIP(3));

  int x = rect.x;
  int y = rect.y + headerHeight;
  for (size_t i = 0; i < group.values.size(); ++i) {
    if (x > rect.x && x - rect.x + gap + tile > rect.width) {
      x = rect.x;
      y += ContentRowHeight(mode);
    }
    if (x > rect.x) x += gap;
    const int width = std::min(tile, rect.GetRight() - x + 1);
    wxRect valueRect(x, y, width, ContentRowHeight(mode));
    DrawValue(dc, valueRect, *group.values[i], mode, text, secondary, accent);
    x += tile;
  }

  dc.SetPen(wxPen(secondary));
  dc.DrawLine(rect.GetRight() - 1, rect.y, rect.GetRight() - 1,
              rect.GetBottom());
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
  const int availableHeight = std::max(FromDIP(30), size.y - 2 * pad);

  struct ShelfItem {
    size_t groupIndex;
    int width;
    int height;
  };
  struct Shelf {
    std::vector<ShelfItem> items;
    int height;
    Shelf() : height(0) {}
  };

  std::vector<Shelf> shelves;
  Shelf current;
  int usedWidth = 0;
  for (size_t i = 0; i < groups.size(); ++i) {
    const int preferred = GroupPreferredWidth(groups[i], mode, availableWidth);
    const int height = GroupHeight(groups[i], mode, preferred);
    if (!current.items.empty() && usedWidth + gap + preferred > availableWidth) {
      shelves.push_back(current);
      current = Shelf();
      usedWidth = 0;
    }
    if (!current.items.empty()) usedWidth += gap;
    ShelfItem item;
    item.groupIndex = i;
    item.width = preferred;
    item.height = height;
    current.items.push_back(item);
    usedWidth += preferred;
    current.height = std::max(current.height, height);
  }
  if (!current.items.empty()) shelves.push_back(current);

  // Category blocks fill each complete shelf, but value tiles inside them keep
  // fixed widths and remain left aligned. Only the block boundary stretches.
  for (size_t shelfIndex = 0; shelfIndex < shelves.size(); ++shelfIndex) {
    Shelf& shelf = shelves[shelfIndex];
    int base = gap * static_cast<int>(shelf.items.size() > 0
                                         ? shelf.items.size() - 1
                                         : 0);
    for (size_t i = 0; i < shelf.items.size(); ++i) base += shelf.items[i].width;
    int extra = std::max(0, availableWidth - base);
    if (!shelf.items.empty()) {
      // Stretch only the last category boundary to the right edge. This keeps
      // all earlier category column starts stable and all value tiles fixed.
      shelf.items.back().width += extra;
      shelf.items.back().height = GroupHeight(
          groups[shelf.items.back().groupIndex], mode, shelf.items.back().width);
      shelf.height = 0;
      for (size_t i = 0; i < shelf.items.size(); ++i)
        shelf.height = std::max(shelf.height, shelf.items[i].height);
    }
  }

  int layoutHeight = 0;
  for (size_t i = 0; i < shelves.size(); ++i) {
    if (i > 0) layoutHeight += gap;
    layoutHeight += shelves[i].height;
  }
  int y = pad + std::max(0, (availableHeight - layoutHeight) / 2);

  for (size_t shelfIndex = 0; shelfIndex < shelves.size(); ++shelfIndex) {
    Shelf& shelf = shelves[shelfIndex];
    int x = pad;
    for (size_t i = 0; i < shelf.items.size(); ++i) {
      const ShelfItem& item = shelf.items[i];
      const wxRect rect(x, y, item.width, item.height);
      const Group& group = groups[item.groupIndex];
      if (group.vessel)
        DrawVessel(dc, rect, mode, text, secondary);
      else
        DrawGroup(dc, rect, group, mode, text, secondary, accent);
      x += item.width + gap;
    }
    y += shelf.height + gap;
  }

  int hidden = 0;
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

#ifndef BOATINFO_DASHBOARD_H
#define BOATINFO_DASHBOARD_H

#include "boatinfo_pi.h"

#include <vector>

#include <wx/panel.h>

class wxDC;
class wxPaintEvent;
class wxRect;
class wxSizeEvent;

class BoatInfoDashboardPanel : public wxPanel {
public:
  explicit BoatInfoDashboardPanel(wxWindow* parent);

  void SetState(const std::vector<BoatInfoValue>& values,
                const wxString& vesselName, const wxString& mmsi,
                const wxString& callSign, const wxString& sourceSummary);

private:
  enum ResponsiveMode { MODE_FULL = 0, MODE_COMPACT, MODE_MINIMAL };

  struct Group {
    bool vessel = false;
    wxString category;
    std::vector<const BoatInfoValue*> values;
  };

  int HeaderHeight(ResponsiveMode mode) const;
  int ContentRowHeight(ResponsiveMode mode) const;
  int VesselWidth(ResponsiveMode mode) const;
  int ValueWidth(const BoatInfoValue& value, ResponsiveMode mode) const;
  int GroupPreferredWidth(const Group& group, ResponsiveMode mode,
                          int availableWidth) const;
  int GroupHeight(const Group& group, ResponsiveMode mode, int width) const;
  int LayoutHeight(const std::vector<Group>& groups, ResponsiveMode mode,
                   int availableWidth) const;

  std::vector<Group> BuildGroups(BoatInfoValue::Priority cutoff) const;
  void ChooseLayout(ResponsiveMode& mode, BoatInfoValue::Priority& cutoff) const;

  wxString RenderedValue(const BoatInfoValue& value) const;
  wxString DisplayLabel(const BoatInfoValue& value, ResponsiveMode mode) const;
  wxString CategoryTitle(const wxString& category, ResponsiveMode mode) const;

  void DrawVessel(wxDC& dc, const wxRect& rect, ResponsiveMode mode,
                  const wxColour& text, const wxColour& secondary);
  void DrawGroup(wxDC& dc, const wxRect& rect, const Group& group,
                 ResponsiveMode mode, const wxColour& text,
                 const wxColour& secondary, const wxColour& accent);
  void DrawValue(wxDC& dc, const wxRect& rect, const BoatInfoValue& value,
                 ResponsiveMode mode, const wxColour& text,
                 const wxColour& secondary, const wxColour& accent);

  void OnPaint(wxPaintEvent& event);
  void OnSize(wxSizeEvent& event);

  std::vector<BoatInfoValue> m_values;
  wxString m_vesselName;
  wxString m_mmsi;
  wxString m_callSign;
  wxString m_sourceSummary;
};

#endif

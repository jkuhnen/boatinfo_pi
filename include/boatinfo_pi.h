#ifndef BOATINFO_PI_H
#define BOATINFO_PI_H

#include "ocpn_plugin.h"

#include <map>
#include <vector>

#include <wx/aui/aui.h>
#include <wx/bitmap.h>
#include <wx/string.h>

class wxCheckBox;
class wxChoice;
class wxFileConfig;
class wxJSONValue;
class wxTextCtrl;
class wxTimer;
class wxWindow;

struct BoatInfoValue {
  enum Presentation {
    PRESENTATION_TEXT = 0,
    PRESENTATION_DIGITAL,
    PRESENTATION_DIGITAL_ANALOG
  };

  // Legacy PR #15 modes remain readable in stored profiles. Stable v1 renders
  // text only; these values no longer control the live dashboard.
  enum Primitive {
    PRIMITIVE_VALUE = 0,
    PRIMITIVE_LEVEL,
    PRIMITIVE_TAPE,
    PRIMITIVE_TREND,
    PRIMITIVE_NONE
  };

  enum Priority {
    PRIORITY_PRIMARY = 0,
    PRIORITY_SECONDARY,
    PRIORITY_DETAIL
  };

  enum Validity {
    VALIDITY_VALID = 0,
    VALIDITY_STALE,
    VALIDITY_NO_DATA,
    VALIDITY_INVALID,
    VALIDITY_OUT_OF_RANGE,
    VALIDITY_UNKNOWN
  };

  wxString key;
  wxString source;
  wxString path;
  wxString category;
  wxString semanticId;
  wxString suggestedLabel;
  wxString label;
  wxString unit;
  wxString qualifier;

  double value = 0.0;
  double displayValue = 0.0;
  bool hasNumericValue = false;
  wxString textValue;

  // valid/stale are retained for the already-tested acquisition path. The
  // presentation layer maps these into explicit DevKit validity states.
  bool valid = false;
  bool stale = false;
  Validity validity = VALIDITY_NO_DATA;
  bool manualFallback = false;
  long long lastUpdateSeconds = 0;
  int freshnessSeconds = 30;

  bool visible = false;
  bool userConfigured = false;
  bool labelCustomized = false;
  bool alternativeSource = false;
  Presentation presentation = PRESENTATION_TEXT;
  Primitive primitive = PRIMITIVE_VALUE;  // legacy config compatibility only
  Priority priority = PRIORITY_SECONDARY;

  // Normalized supporting data retained for later digital presentations.
  bool bounded = false;
  double minimum = 0.0;
  double maximum = 1.0;
  std::vector<double> trend;

  Validity EffectiveValidity() const {
    if (stale) return VALIDITY_STALE;
    if (valid) return VALIDITY_VALID;
    return validity == VALIDITY_VALID ? VALIDITY_NO_DATA : validity;
  }

  bool IsCurrent() const { return EffectiveValidity() == VALIDITY_VALID; }
};

class BoatInfoDashboardPanel;

class boatinfo_pi : public opencpn_plugin_118 {
public:
  explicit boatinfo_pi(void* ppimgr);
  ~boatinfo_pi() override;

  int Init() override;
  bool DeInit() override;

  void SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) override;
  void SetNMEASentence(wxString& sentence) override;
  void SetPluginMessage(wxString& message_id, wxString& message_body) override;
  void SetColorScheme(PI_ColorScheme cs) override;
  void ShowPreferencesDialog(wxWindow* parent) override;

  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  wxBitmap* GetPlugInBitmap() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;

private:
  struct PreferenceRow {
    wxString key;
    wxCheckBox* visible = nullptr;
    wxChoice* priority = nullptr;
    wxTextCtrl* label = nullptr;
    wxChoice* primitive = nullptr;  // legacy config compatibility in v1
  };

  void ApplyHostStyle();
  void ClearControlPointers();
  void BuildMainPanel();
  void RebuildDashboard();
  void LoadConfiguration();
  void LoadProfile(const wxString& profileKey, bool allowLegacy);
  void SaveConfiguration();
  void SaveProfile(wxFileConfig* config, const wxString& profileKey);
  void ApplyPreferenceRows(const std::vector<PreferenceRow>& rows);
  void RefreshFreshness();
  void UpdateSourceSummary();
  void EnsureIdentityValues();
  void ApplyIdentityFallbacks();
  void ActivateVesselProfile(const wxString& signalKSelf);

  bool ParseSignalK(const wxString& message);
  bool ObserveSignalKPath(const wxString& path, const wxJSONValue& value);
  void ObserveNumeric(const wxString& key, const wxString& source,
                      const wxString& path, double rawValue);
  void ObserveText(const wxString& key, const wxString& source,
                   const wxString& path, const wxString& value);
  void ObserveNoData(const wxString& key, const wxString& source,
                     const wxString& path);
  BoatInfoValue& EnsureValue(const wxString& key, const wxString& source,
                             const wxString& path);
  void ApplySemanticDefaults(BoatInfoValue& value);

  wxBitmap m_pluginBitmap;
  PI_ColorScheme m_colorScheme = PI_GLOBAL_COLOR_SCHEME_DAY;

  wxAuiManager* m_auiManager = nullptr;
  wxWindow* m_panel = nullptr;
  BoatInfoDashboardPanel* m_dashboard = nullptr;
  wxTimer* m_staleTimer = nullptr;

  std::map<wxString, BoatInfoValue> m_values;
  wxString m_signalKSelf;
  wxString m_profileKey = wxT("default");
  wxString m_manualVesselName;
  wxString m_manualMmsi;
  wxString m_manualCallSign;
  wxString m_sourceSummary;
  bool m_configLoaded = false;
  bool m_seenOpenCPN = false;
  bool m_seenSignalK = false;
  bool m_seenNmeaXdr = false;
};

#endif

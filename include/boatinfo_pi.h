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
class wxFlexGridSizer;
class wxJSONValue;
class wxStaticText;
class wxTextCtrl;
class wxTimer;
class wxWindow;

struct BoatInfoValue {
  enum Primitive {
    PRIMITIVE_VALUE = 0,
    PRIMITIVE_LEVEL,
    PRIMITIVE_TAPE,
    PRIMITIVE_TREND,
    PRIMITIVE_NONE
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

  bool valid = false;
  bool stale = false;
  long long lastUpdateSeconds = 0;
  int freshnessSeconds = 30;

  bool visible = false;
  bool userConfigured = false;
  bool labelCustomized = false;
  bool alternativeSource = false;
  Primitive primitive = PRIMITIVE_VALUE;

  bool bounded = false;
  double minimum = 0.0;
  double maximum = 1.0;
  std::vector<double> trend;
};

class BoatInfoInstrumentPanel;

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
    wxTextCtrl* label = nullptr;
    wxChoice* primitive = nullptr;
  };

  void ApplyHostStyle();
  void ClearControlPointers();
  void BuildMainPanel();
  void RebuildInstrumentGrid();
  void LoadConfiguration();
  void SaveConfiguration();
  void ApplyPreferenceRows(const std::vector<PreferenceRow>& rows);
  void RefreshFreshness();
  void UpdateSourceSummary();

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
  void UpdateInstrument(const wxString& key);

  wxBitmap m_pluginBitmap;
  PI_ColorScheme m_colorScheme = PI_GLOBAL_COLOR_SCHEME_DAY;

  wxAuiManager* m_auiManager = nullptr;
  wxWindow* m_panel = nullptr;
  wxFlexGridSizer* m_instrumentGrid = nullptr;
  wxStaticText* m_emptyHint = nullptr;
  wxStaticText* m_dataSourceValue = nullptr;
  wxTimer* m_staleTimer = nullptr;

  std::map<wxString, BoatInfoValue> m_values;
  std::map<wxString, BoatInfoInstrumentPanel*> m_instruments;
  wxString m_signalKSelf;
  bool m_configLoaded = false;
  bool m_seenOpenCPN = false;
  bool m_seenSignalK = false;
  bool m_seenNmeaXdr = false;
};

#endif

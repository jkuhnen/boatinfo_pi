#ifndef BOATINFO_PI_H
#define BOATINFO_PI_H

#include "ocpn_plugin.h"

#include <wx/aui/aui.h>
#include <wx/bitmap.h>

class wxJSONValue;
class wxStaticText;
class wxWindow;

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

  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  wxBitmap* GetPlugInBitmap() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;

private:
  void ApplyHostStyle();
  void ClearControlPointers();
  bool ParseSignalK(const wxString& message);
  bool UpdateSignalKPath(const wxString& path, const wxJSONValue& value);

  wxBitmap m_dayPluginBitmap;
  wxBitmap m_lowLightPluginBitmap;
  PI_ColorScheme m_colorScheme = PI_GLOBAL_COLOR_SCHEME_DAY;

  wxAuiManager* m_auiManager = nullptr;
  wxWindow* m_panel = nullptr;

  wxStaticText* m_nameValue = nullptr;
  wxStaticText* m_mmsiValue = nullptr;
  wxStaticText* m_callSignValue = nullptr;

  wxStaticText* m_latitudeValue = nullptr;
  wxStaticText* m_longitudeValue = nullptr;
  wxStaticText* m_cogValue = nullptr;
  wxStaticText* m_sogValue = nullptr;
  wxStaticText* m_dateValue = nullptr;
  wxStaticText* m_timeValue = nullptr;

  wxStaticText* m_socValue = nullptr;
  wxStaticText* m_remainingValue = nullptr;
  wxStaticText* m_currentValue = nullptr;
  wxStaticText* m_voltageValue = nullptr;
  wxStaticText* m_powerValue = nullptr;
  wxStaticText* m_starterVoltageValue = nullptr;
  wxStaticText* m_dataSourceValue = nullptr;

  wxString m_signalKSelf;
};

#endif

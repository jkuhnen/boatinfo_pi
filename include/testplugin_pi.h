#ifndef BENCHYNAV_PI_H
#define BENCHYNAV_PI_H

#include "ocpn_plugin.h"

#include <wx/aui/aui.h>

class wxJSONValue;
class wxStaticText;

class testplugin_pi : public opencpn_plugin_118 {
public:
  testplugin_pi(void* ppimgr);
  ~testplugin_pi();

  int Init();
  bool DeInit();

  void SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) override;
  void SetNMEASentence(wxString& sentence) override;
  void SetPluginMessage(wxString& message_id, wxString& message_body) override;

  int GetPlugInVersionMajor();
  int GetPlugInVersionMinor();

  int GetAPIVersionMajor();
  int GetAPIVersionMinor();

  wxBitmap* GetPlugInBitmap();

  wxString GetCommonName();
  wxString GetShortDescription();
  wxString GetLongDescription();

private:
  void ParseSignalK(const wxString& message);
  void UpdateSignalKPath(const wxString& path, const wxJSONValue& value);

  wxAuiManager* m_auiManager = nullptr;
  wxWindow* m_helloPanel = nullptr;

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
  wxStaticText* m_signalKStatusValue = nullptr;

  wxString m_signalKSelf;
};

#endif

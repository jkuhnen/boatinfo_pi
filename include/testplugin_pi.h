#ifndef BENCHYNAV_PI_H
#define BENCHYNAV_PI_H

#include "ocpn_plugin.h"

#include <wx/aui/aui.h>

class testplugin_pi : public opencpn_plugin_118 {
public:
  testplugin_pi(void* ppimgr);
  ~testplugin_pi();

  int Init();
  bool DeInit();

  int GetPlugInVersionMajor();
  int GetPlugInVersionMinor();

  int GetAPIVersionMajor();
  int GetAPIVersionMinor();

  wxBitmap* GetPlugInBitmap();

  wxString GetCommonName();
  wxString GetShortDescription();
  wxString GetLongDescription();

private:
  wxAuiManager* m_auiManager = nullptr;
  wxWindow* m_helloPanel = nullptr;
};

#endif

#include "testplugin_pi.h"
#include "version.h"

#include <wx/sizer.h>
#include <wx/stattext.h>

#ifdef _WIN32
#define BENCHYNAV_EXPORT __declspec(dllexport)
#else
#define BENCHYNAV_EXPORT
#endif



extern "C" BENCHYNAV_EXPORT opencpn_plugin* create_pi(void* ppimgr) {
  return new testplugin_pi(ppimgr);
}

extern "C" BENCHYNAV_EXPORT void destroy_pi(opencpn_plugin* p) { delete p; }

testplugin_pi::testplugin_pi(void* ppimgr) : opencpn_plugin_118(ppimgr) {}

testplugin_pi::~testplugin_pi() {}

int testplugin_pi::Init() {
  m_auiManager = GetFrameAuiManager();

  wxWindow* auiParent = m_auiManager->GetManagedWindow();

  m_helloPanel = new wxWindow(auiParent, wxID_ANY, wxDefaultPosition,
                              wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);

  wxColour panelBackground;
  GetGlobalColor(wxT("DILG1"), &panelBackground);
  m_helloPanel->SetBackgroundColour(panelBackground);

  wxColour panelText;
  GetGlobalColor(wxT("DILG3"), &panelText);

  wxStaticText* helloText =
      new wxStaticText(m_helloPanel, wxID_ANY, wxT("BenchyNav"));

  helloText->SetForegroundColour(panelText);

  wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(helloText, 0, wxALL, 20);

  m_helloPanel->SetSizer(sizer);

  wxAuiPaneInfo pane;
  pane.Name(wxT("BenchyNavPanel"))
      .Caption(wxT("BenchyNav"))
      .Right()
      .BestSize(300, -1)
      .MinSize(200, -1)
      .Floatable(false)
      .RightDockable(true)
      .LeftDockable(false)
      .TopDockable(false)
      .BottomDockable(false)
      .CloseButton(true)
      .Show(true);

  m_auiManager->AddPane(m_helloPanel, pane);
  m_auiManager->Update();

  return USES_AUI_MANAGER;
}

bool testplugin_pi::DeInit() {
  if (m_helloPanel && m_auiManager) {
    m_auiManager->DetachPane(m_helloPanel);
    m_helloPanel->Destroy();
    m_helloPanel = nullptr;

    m_auiManager->Update();
  }

  m_auiManager = nullptr;

  return true;
}

int testplugin_pi::GetAPIVersionMajor() { return OCPN_API_VERSION_MAJOR; }

int testplugin_pi::GetAPIVersionMinor() { return OCPN_API_VERSION_MINOR; }

int testplugin_pi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }

int testplugin_pi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }

wxBitmap* testplugin_pi::GetPlugInBitmap() {
  static wxBitmap bitmap(32, 32);
  return &bitmap;
}

wxString testplugin_pi::GetCommonName() { return wxT("BenchyNav"); }

wxString testplugin_pi::GetShortDescription() {
  return wxT("Benchy vessel information and navigation data");
}

wxString testplugin_pi::GetLongDescription() {
  return wxT(
      "Displays Benchy vessel information and navigation data "
      "in a docked OpenCPN panel.");
}

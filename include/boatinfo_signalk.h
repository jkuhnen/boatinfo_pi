#ifndef BOATINFO_SIGNALK_H
#define BOATINFO_SIGNALK_H

#include <vector>

#include <wx/jsonval.h>
#include <wx/string.h>

struct BoatInfoSignalKValue {
  wxString path;
  wxJSONValue value;
};

struct BoatInfoSignalKMessage {
  bool hasSelf = false;
  wxString self;
  bool hasContext = false;
  wxString context;
  wxJSONValue root;
  std::vector<BoatInfoSignalKValue> values;
};

struct BoatInfoSignalKIdentity {
  bool hasName = false;
  wxString name;
  bool hasMmsi = false;
  wxString mmsi;
  bool hasCallSign = false;
  wxString callSign;
};

wxString NormalizeBoatInfoSignalKSelf(const wxString& self);
bool IsBoatInfoOwnVesselContext(const wxString& context,
                                const wxString& normalizedSelf);
bool ParseBoatInfoSignalKMessage(const wxString& json,
                                 BoatInfoSignalKMessage& message);
bool ExtractBoatInfoSignalKIdentity(wxJSONValue model,
                                    const wxString& normalizedSelf,
                                    BoatInfoSignalKIdentity& identity);
bool DeriveBoatInfoMmsiFromSelf(const wxString& signalKSelf,
                                wxString& mmsi);

#endif

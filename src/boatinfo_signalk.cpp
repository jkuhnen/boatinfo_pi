#include "boatinfo_signalk.h"

#include <cmath>

#include <wx/jsonreader.h>

namespace {
bool MmsiScalarToString(wxJSONValue value, wxString& result) {
  if (value.IsString()) {
    result = value.AsString();
    return !result.IsEmpty();
  }

  double number = 0.0;
  switch (value.GetType()) {
    case wxJSONTYPE_DOUBLE: number = value.AsDouble(); break;
    case wxJSONTYPE_INT: number = static_cast<double>(value.AsInt()); break;
    case wxJSONTYPE_UINT: number = static_cast<double>(value.AsUInt()); break;
    case wxJSONTYPE_LONG: number = static_cast<double>(value.AsLong()); break;
    case wxJSONTYPE_ULONG: number = static_cast<double>(value.AsULong()); break;
    case wxJSONTYPE_SHORT: number = static_cast<double>(value.AsShort()); break;
    case wxJSONTYPE_USHORT:
      number = static_cast<double>(value.AsUShort());
      break;
    default: return false;
  }
  if (!std::isfinite(number)) return false;
  result = wxString::Format(wxT("%.0f"), number);
  return true;
}

bool ExtractDirectIdentity(wxJSONValue vessel,
                           BoatInfoSignalKIdentity& identity) {
  if (!vessel.IsObject()) return false;

  bool handled = false;
  if (vessel.HasMember(wxT("name")) && vessel[wxT("name")].IsString()) {
    identity.hasName = true;
    identity.name = vessel[wxT("name")].AsString();
    handled = true;
  }

  if (vessel.HasMember(wxT("mmsi"))) {
    wxString mmsi;
    if (MmsiScalarToString(vessel[wxT("mmsi")], mmsi)) {
      identity.hasMmsi = true;
      identity.mmsi = mmsi;
      handled = true;
    }
  }

  if (vessel.HasMember(wxT("communication")) &&
      vessel[wxT("communication")].IsObject()) {
    wxJSONValue& communication = vessel[wxT("communication")];
    if (communication.HasMember(wxT("callsignVhf")) &&
        communication[wxT("callsignVhf")].IsString()) {
      identity.hasCallSign = true;
      identity.callSign = communication[wxT("callsignVhf")].AsString();
      handled = true;
    }
  }
  return handled;
}

bool IsExactlyNineDigits(const wxString& value) {
  if (value.length() != 9) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] < wxT('0') || value[i] > wxT('9')) return false;
  }
  return true;
}
}  // namespace

wxString NormalizeBoatInfoSignalKSelf(const wxString& self) {
  if (self.IsEmpty() || self.StartsWith(wxT("vessels."))) return self;
  return wxT("vessels.") + self;
}

bool IsBoatInfoOwnVesselContext(const wxString& context,
                                const wxString& normalizedSelf) {
  if (context.IsEmpty() || normalizedSelf.IsEmpty()) return true;
  const wxString normalizedContext = NormalizeBoatInfoSignalKSelf(context);
  return normalizedContext.Lower() == wxT("vessels.self") ||
         normalizedContext == normalizedSelf;
}

bool ParseBoatInfoSignalKMessage(const wxString& json,
                                 BoatInfoSignalKMessage& message) {
  wxJSONValue root;
  wxJSONReader reader;
  if (reader.Parse(json, &root) != 0 || !root.IsObject()) return false;

  message = BoatInfoSignalKMessage();
  message.root = root;
  if (root.HasMember(wxT("self")) && root[wxT("self")].IsString()) {
    message.hasSelf = true;
    message.self = NormalizeBoatInfoSignalKSelf(root[wxT("self")].AsString());
  }
  if (root.HasMember(wxT("context")) && root[wxT("context")].IsString()) {
    message.hasContext = true;
    message.context =
        NormalizeBoatInfoSignalKSelf(root[wxT("context")].AsString());
  }

  if (!root.HasMember(wxT("updates")) || !root[wxT("updates")].IsArray())
    return true;

  wxJSONValue& updates = root[wxT("updates")];
  for (int i = 0; i < updates.Size(); ++i) {
    wxJSONValue& update = updates[i];
    if (!update.IsObject() || !update.HasMember(wxT("values")) ||
        !update[wxT("values")].IsArray())
      continue;
    wxJSONValue& values = update[wxT("values")];
    for (int j = 0; j < values.Size(); ++j) {
      wxJSONValue& item = values[j];
      if (!item.IsObject() || !item.HasMember(wxT("path")) ||
          !item[wxT("path")].IsString() || !item.HasMember(wxT("value")))
        continue;
      BoatInfoSignalKValue parsedValue;
      parsedValue.path = item[wxT("path")].AsString();
      parsedValue.value = item[wxT("value")];
      message.values.push_back(parsedValue);
    }
  }
  return true;
}

bool ExtractBoatInfoSignalKIdentity(wxJSONValue model,
                                    const wxString& normalizedSelf,
                                    BoatInfoSignalKIdentity& identity) {
  BoatInfoSignalKIdentity extracted;
  if (model.IsObject() && model.HasMember(wxT("vessels")) &&
      model[wxT("vessels")].IsObject() &&
      normalizedSelf.StartsWith(wxT("vessels."))) {
    const wxString vesselKey = normalizedSelf.Mid(8);
    wxJSONValue& vessels = model[wxT("vessels")];
    if (!vesselKey.IsEmpty() && vessels.HasMember(vesselKey) &&
        ExtractDirectIdentity(vessels[vesselKey], extracted)) {
      identity = extracted;
      return true;
    }
  }

  if (!ExtractDirectIdentity(model, extracted)) return false;
  identity = extracted;
  return true;
}

bool DeriveBoatInfoMmsiFromSelf(const wxString& signalKSelf,
                                wxString& mmsi) {
  wxString candidate = signalKSelf;
  if (candidate.StartsWith(wxT("vessels."))) candidate = candidate.Mid(8);

  if (IsExactlyNineDigits(candidate)) {
    mmsi = candidate;
    return true;
  }

  const wxString marker = wxT(":mmsi:");
  const int markerPosition = candidate.Lower().Find(marker);
  if (markerPosition == wxNOT_FOUND) return false;
  candidate = candidate.Mid(markerPosition + marker.length());
  if (!IsExactlyNineDigits(candidate)) return false;
  mmsi = candidate;
  return true;
}

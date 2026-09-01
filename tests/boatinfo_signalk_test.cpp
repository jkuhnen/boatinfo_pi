#include "boatinfo_signalk.h"

#include <iostream>

namespace {
int failures = 0;

void Check(bool condition, const char* description) {
  if (condition) return;
  std::cerr << "FAILED: " << description << std::endl;
  ++failures;
}

BoatInfoSignalKMessage Parse(const wxString& json) {
  BoatInfoSignalKMessage message;
  Check(ParseBoatInfoSignalKMessage(json, message), "message parses");
  return message;
}
}  // namespace

int main() {
  const wxString own = wxT("vessels.urn:mrn:imo:mmsi:211234567");

  BoatInfoSignalKMessage leaf = Parse(
      wxT("{\"self\":\"urn:mrn:imo:mmsi:211234567\","
          "\"context\":\"vessels.urn:mrn:imo:mmsi:211234567\","
          "\"updates\":[{\"values\":["
          "{\"path\":\"name\",\"value\":\"Aurora\"},"
          "{\"path\":\"mmsi\",\"value\":211234567},"
          "{\"path\":\"communication.callsignVhf\","
          "\"value\":\"DA1234\"}]}]}"));
  Check(leaf.hasSelf && leaf.self == own, "standard leaf delta self");
  Check(leaf.values.size() == 3, "standard leaf delta values");

  BoatInfoSignalKMessage pathless = Parse(
      wxT("{\"self\":\"urn:mrn:imo:mmsi:211234567\","
          "\"context\":\"vessels.urn:mrn:imo:mmsi:211234567\","
          "\"updates\":[{\"values\":[{\"path\":\"\",\"value\":{"
          "\"name\":\"Aurora\",\"mmsi\":\"211234567\","
          "\"communication\":{\"callsignVhf\":\"DA1234\"}}}]}]}"));
  Check(pathless.values.size() == 1 && pathless.values[0].path.IsEmpty(),
        "pathless delta retained");
  BoatInfoSignalKIdentity identity;
  Check(ExtractBoatInfoSignalKIdentity(pathless.values[0].value, own,
                                       identity),
        "pathless identity extracted");
  Check(identity.hasName && identity.name == wxT("Aurora"), "nested name");
  Check(identity.hasMmsi && identity.mmsi == wxT("211234567"),
        "string MMSI");
  Check(identity.hasCallSign && identity.callSign == wxT("DA1234"),
        "nested callsign");

  BoatInfoSignalKMessage fullModel = Parse(
      wxT("{\"self\":\"urn:mrn:imo:mmsi:211234567\",\"vessels\":{"
          "\"urn:mrn:imo:mmsi:211234567\":{\"name\":\"Aurora\","
          "\"mmsi\":211234567,"
          "\"communication\":{\"callsignVhf\":\"DA1234\"}},"
          "\"urn:mrn:imo:mmsi:244999999\":{\"name\":\"Foreign\"}}}"));
  identity = BoatInfoSignalKIdentity();
  Check(ExtractBoatInfoSignalKIdentity(fullModel.root, fullModel.self, identity),
        "root vessel model extracted");
  Check(identity.name == wxT("Aurora") && identity.mmsi == wxT("211234567"),
        "root model selects own vessel");

  BoatInfoSignalKMessage directModel = Parse(
      wxT("{\"name\":\"Aurora\",\"mmsi\":211234567,"
          "\"communication\":{\"callsignVhf\":\"DA1234\"}}"));
  identity = BoatInfoSignalKIdentity();
  Check(ExtractBoatInfoSignalKIdentity(directModel.root, own, identity) &&
            identity.name == wxT("Aurora") &&
            identity.callSign == wxT("DA1234"),
        "direct vessel model extracted");

  Check(!IsBoatInfoOwnVesselContext(
            wxT("vessels.urn:mrn:imo:mmsi:244999999"), own),
        "foreign vessel context ignored");
  Check(IsBoatInfoOwnVesselContext(wxT("vessels.self"), own),
        "vessels.self context selects own vessel");

  wxString derived;
  Check(DeriveBoatInfoMmsiFromSelf(own, derived) &&
            derived == wxT("211234567"),
        "nine-digit MMSI derived from self");
  Check(!DeriveBoatInfoMmsiFromSelf(
            wxT("vessels.urn:mrn:signalk:uuid:12345678-1234-4123-8123-"
                "123456789012"),
            derived),
        "UUID self does not produce MMSI");

  return failures == 0 ? 0 : 1;
}

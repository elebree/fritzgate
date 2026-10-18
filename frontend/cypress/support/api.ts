import type { StatusData } from "../../src/interfaces";
import fixture from "../fixtures/status.json";

// From localhost the app calls http://fritzgate/api/..., so stubbed responses need CORS headers
export const corsHeaders = {
  "access-control-allow-origin": "*",
  "access-control-allow-headers": "*",
  "access-control-allow-methods": "GET, POST, OPTIONS",
};

/** The fixture with a connected FRITZ!Box, plus any top-level changes */
export function statusData(changes: Partial<StatusData> = {}): StatusData {
  return structuredClone({
    fritz: { host: "fritz.box", user: "demo", status: "CONNECTED" },
    ...(fixture as Omit<StatusData, "fritz">),
    ...changes,
  });
}

/** A temperature as the app shows it, in the browser's locale (19.7 or 19,7) */
export const celsius = (value: number) =>
  value.toLocaleString(undefined, { minimumFractionDigits: 1, maximumFractionDigits: 1 });

/** An extra sensor that no thermostat in the fixture uses */
export const unusedSensor = {
  ...fixture.sensors[0],
  name: "Sensor EEEE",
  id: "GVH5075_EEEE",
  serial: "EEEE",
};

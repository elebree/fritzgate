import type { HistoryData, StatusData } from "../../src/interfaces";
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

/**
 * A history of the devices of a status up to now, shaped like GET /api/history. FritzGate started halfway, so the
 * arrays have the second half of the points. The values are steady, 0.5 °C, 2 % or 3 °C below the status, and the
 * newest point has the values of the status.
 */
export function historyData(hours = 24, step = 300, status: StatusData = statusData()): HistoryData {
  const count = Math.ceil((hours * 60 * 60) / step);
  const length = count - Math.ceil(count / 2);
  const values = (before: number, newest: number) =>
    Array.from({ length }, (_, i) => (i === length - 1 ? newest : before));
  return {
    last: Math.floor(Date.now() / 1000 / step) * step,
    step,
    sensors: status.sensors.map((sensor) => ({
      id: sensor.id,
      temperature: values(sensor.temperature - 0.5, sensor.temperature),
      humidity: values(sensor.humidity - 2, sensor.humidity),
    })),
    thermostats: status.thermostats.map((thermostat) => ({
      id: thermostat.id,
      temperature: values(thermostat.temperature - 0.5, thermostat.temperature),
      targetTemperature: values(thermostat.targetTemperature - 3, thermostat.targetTemperature),
      offset: values(thermostat.offset, thermostat.offset),
    })),
  };
}

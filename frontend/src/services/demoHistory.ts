import type { DeviceStatus, HistoryData, SensorStatus, StatusData } from "../interfaces";

// The demo gateway has been running for 9 days, so the 30-day view starts with a gap...
const UPTIME_DAYS = 9;
// ...and was without power for an hour two and a half days ago
const OUTAGE_DAYS_AGO = 2.5;
const OUTAGE_HOURS = 1;

const DAY = 24 * 60 * 60;

/** Repeatable noise between -1 and 1 for a series and a 5-minute slot */
function noise(series: number, slot: number) {
  const x = Math.sin(series * 12.9898 + slot * 78.233) * 43758.5453;
  return (x - Math.floor(x)) * 2 - 1;
}

const roundTo = (value: number, step: number) => Math.round(value / step) * step;
const tenths = (value: number) => Math.round(value * 10) / 10;

/** Between -1 and 1 over a day: warmest in the late afternoon, coolest in the early morning */
function dailyWave(time: number) {
  const date = new Date(time * 1000);
  const hour = date.getHours() + date.getMinutes() / 60;
  return Math.sin(((hour - 11) / 24) * 2 * Math.PI);
}

/** The demo thermostats heat from 6:00 to 22:00 */
function heating(time: number) {
  const hour = new Date(time * 1000).getHours();
  return hour >= 6 && hour < 22;
}

/** The temperature of a sensor, without noise as FritzGate averages it */
function sensorAverage(sensor: SensorStatus, time: number) {
  const warmth = heating(time) ? 0.3 : -0.5;
  return sensor.temperature + 0.8 * dailyWave(time) + warmth;
}

function sensorTemperature(sensor: SensorStatus, index: number, time: number) {
  return sensorAverage(sensor, time) + 0.15 * noise(index, Math.floor(time / 300));
}

function thermostatTemperature(thermostat: DeviceStatus, time: number) {
  // the thermostat sits at the radiator: it follows the heating more than the room does
  const warmth = heating(time) ? 0.6 : -1;
  return roundTo(thermostat.temperature + 0.6 * dailyWave(time) + warmth, 0.5);
}

/** Made-up readings of the demo devices, shaped like GET /api/history */
export function demoHistory(status: StatusData, now: number, hours: number, step: number): HistoryData {
  const last = Math.floor(now / step);
  const count = Math.ceil((hours * 60 * 60) / step);
  const from = (last - count + 1) * step;
  const started = now - UPTIME_DAYS * DAY;
  const outageStart = now - OUTAGE_DAYS_AGO * DAY;
  const outageEnd = outageStart + OUTAGE_HOURS * 60 * 60;

  // One value per step, taken in the middle of the step (or now, for the current one), from the first recorded one
  function series(value: (time: number) => number): (number | null)[] {
    const values = Array.from({ length: count }, (_, i) => {
      const time = Math.min(from + i * step + step / 2, now);
      const recorded = time >= started && (time < outageStart || time >= outageEnd);
      return recorded ? value(time) : null;
    });
    const first = values.findIndex((value) => value != null);
    return first < 0 ? [] : values.slice(first);
  }

  const sensors = status.sensors.map((sensor, index) => ({
    id: sensor.id,
    temperature: series((time) => tenths(sensorTemperature(sensor, index, time))),
    humidity: series((time) =>
      tenths(sensor.humidity - 3 * dailyWave(time) + 0.8 * noise(index + 50, Math.floor(time / 300)))
    ),
  }));

  const thermostats = status.thermostats.map((thermostat) => {
    const reference = status.sensors.find((sensor) => sensor.id === thermostat.referenceSensor);
    return {
      id: thermostat.id,
      temperature: series((time) => thermostatTemperature(thermostat, time)),
      targetTemperature: series((time) =>
        heating(time) ? thermostat.targetTemperature : thermostat.targetTemperature - 3
      ),
      // FritzGate sets the difference between the reference sensor and the thermostat, in 0.5 °C, at most 5 °C
      offset: series((time) =>
        reference
          ? Math.max(
              -5,
              Math.min(
                5,
                roundTo(sensorAverage(reference, time) - thermostatTemperature(thermostat, time), 0.5)
              )
            )
          : thermostat.offset
      ),
    };
  });

  return { last: last * step, step, sensors, thermostats };
}

import type { HistoryData, SensorHistory, ThermostatHistory } from "../interfaces";

/*
 * FritzGate loses its history when it restarts. The browser keeps the last answer of each range of /api/history too,
 * and joins it with each new one, so that the charts keep the readings from before the restart.
 */

type Values = (number | null)[];

const storageKey = (hours: number, step: number) => `fritzgate.history.${hours}.${step}`;

/**
 * The values of one series in the steps of the range that ends at last, in the format of /api/history: ending with the
 * step at last, without the steps before the first value. A step takes the fresh value, or the kept one where FritzGate
 * has none (any more). Both arrays end at their last, which FritzGate aligns to a multiple of the step.
 */
function joinValues(
  kept: Values | undefined,
  keptLast: number,
  fresh: Values | undefined,
  last: number,
  step: number,
  count: number
): Values {
  const first = last - (count - 1) * step;
  const joined: Values = Array<number | null>(count).fill(null);
  const put = (values: Values | undefined, end: number) =>
    values?.forEach((value, i) => {
      const time = end - (values.length - 1 - i) * step;
      if (value != null && time >= first && time <= last) {
        joined[(time - first) / step] = value;
      }
    });
  put(kept, keptLast);
  // the fresh values win
  put(fresh, last);
  const firstValue = joined.findIndex((value) => value != null);
  return firstValue < 0 ? [] : joined.slice(firstValue);
}

// The devices of both answers, the fresh ones first; a device that only the kept answer has stays while it has values
function joinDevices<T extends { id: string }>(
  kept: T[],
  fresh: T[],
  join: (kept: T | undefined, fresh: T | undefined, id: string) => T,
  hasValues: (device: T) => boolean
): T[] {
  const ids = [...new Set([...fresh.map((device) => device.id), ...kept.map((device) => device.id)])];
  return ids
    .map((id) => {
      const freshDevice = fresh.find((device) => device.id === id);
      const joined = join(kept.find((device) => device.id === id), freshDevice, id);
      return { joined, keep: !!freshDevice || hasValues(joined) };
    })
    .filter((device) => device.keep)
    .map((device) => device.joined);
}

/**
 * Join a kept answer of /api/history with a fresh one of the same range of hours. Without a kept answer of the same
 * step, or with one newer than the fresh one (FritzGate's clock went back), the fresh one stays as it is.
 */
export function joinHistory(kept: HistoryData | undefined, fresh: HistoryData, hours: number): HistoryData {
  if (!kept || kept.step !== fresh.step || kept.last > fresh.last) {
    return fresh;
  }
  const { last, step } = fresh;
  const count = Math.ceil((hours * 60 * 60) / step);
  const values = (keptValues: Values | undefined, freshValues: Values | undefined) =>
    joinValues(keptValues, kept.last, freshValues, last, step, count);

  return {
    last,
    step,
    sensors: joinDevices<SensorHistory>(
      kept.sensors,
      fresh.sensors,
      (k, f, id) => ({ id, temperature: values(k?.temperature, f?.temperature), humidity: values(k?.humidity, f?.humidity) }),
      (device) => device.temperature.length > 0 || device.humidity.length > 0
    ),
    thermostats: joinDevices<ThermostatHistory>(
      kept.thermostats,
      fresh.thermostats,
      (k, f, id) => ({
        id,
        temperature: values(k?.temperature, f?.temperature),
        targetTemperature: values(k?.targetTemperature, f?.targetTemperature),
        offset: values(k?.offset, f?.offset),
      }),
      (device) => device.temperature.length > 0 || device.targetTemperature.length > 0 || device.offset.length > 0
    ),
  };
}

function isHistory(value: unknown): value is HistoryData {
  const data = value as HistoryData | null;
  return (
    !!data &&
    typeof data.last === "number" &&
    typeof data.step === "number" &&
    Array.isArray(data.sensors) &&
    Array.isArray(data.thermostats)
  );
}

/**
 * Join a fresh answer of /api/history with the one the browser kept for the same range, keep the result, and return it.
 * Without storage (blocked, or full), the fresh answer is joined with nothing.
 */
export function keepHistory(hours: number, fresh: HistoryData): HistoryData {
  const key = storageKey(hours, fresh.step);
  let kept: HistoryData | undefined = undefined;
  try {
    const parsed = JSON.parse(localStorage.getItem(key) ?? "null");
    kept = isHistory(parsed) ? parsed : undefined;
  } catch {
    // nothing kept, or not readable
  }
  const joined = joinHistory(kept, fresh, hours);
  try {
    localStorage.setItem(key, JSON.stringify(joined));
  } catch {
    // shown anyway, only not kept
  }
  return joined;
}

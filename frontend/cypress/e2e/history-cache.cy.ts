import type { HistoryData } from "../../src/interfaces";
import { joinHistory } from "../../src/services/historyCache";

// One hour in 5-minute steps: 12 points. FritzGate aligns last to a multiple of the step.
const HOURS = 1;
const STEP = 300;
const LAST = 1_000 * 3_600;

const history = (last: number, temperatures: (number | null)[], step = STEP): HistoryData => ({
  last,
  step,
  sensors: [],
  thermostats: [{ id: "16", temperature: temperatures, targetTemperature: [], offset: [] }],
});

const temperatures = (data: HistoryData) => data.thermostats.find((thermostat) => thermostat.id === "16")?.temperature;

describe("history kept in the browser", () => {
  it("fills the steps that FritzGate lost in a restart with the kept values", () => {
    const kept = history(LAST - STEP, [20, 20.5, 21]);
    // after the restart, FritzGate has only the current step
    const fresh = history(LAST, [22]);

    const joined = joinHistory(kept, fresh, HOURS);

    expect(joined.last).to.eq(LAST);
    expect(temperatures(joined)).to.deep.eq([20, 20.5, 21, 22]);
  });

  it("takes the fresh value of a step, and the kept one where FritzGate has none", () => {
    const kept = history(LAST, [20, 21]);
    const fresh = history(LAST, [19.5, null]);

    expect(temperatures(joinHistory(kept, fresh, HOURS))).to.deep.eq([19.5, 21]);
  });

  it("leaves out the steps older than the range", () => {
    // 12 steps before the fresh answer: older than its first step
    const kept = history(LAST - 12 * STEP, [18, 18.5]);
    const fresh = history(LAST, [22]);

    expect(temperatures(joinHistory(kept, fresh, HOURS))).to.deep.eq([22]);
  });

  it("keeps a device that FritzGate doesn't report any more while it has values", () => {
    const kept: HistoryData = {
      last: LAST - STEP,
      step: STEP,
      sensors: [{ id: "GVH5075_EEEE", temperature: [20], humidity: [40] }],
      thermostats: [],
    };
    const fresh: HistoryData = { last: LAST, step: STEP, sensors: [], thermostats: [] };

    expect(joinHistory(kept, fresh, HOURS).sensors).to.deep.eq([
      { id: "GVH5075_EEEE", temperature: [20, null], humidity: [40, null] },
    ]);
  });

  it("ignores a kept answer of another step, or one newer than the fresh answer", () => {
    const fresh = history(LAST, [22]);

    expect(joinHistory(history(LAST, [20], 1800), fresh, HOURS)).to.eq(fresh);
    expect(joinHistory(history(LAST + STEP, [20]), fresh, HOURS)).to.eq(fresh);
    expect(joinHistory(undefined, fresh, HOURS)).to.eq(fresh);
  });
});

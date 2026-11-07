/** A line of a chart */
export interface ChartSeries {
  key: string;
  label: string;
  /** CSS color of the line */
  color: string;
  /** One value per point, null for a gap */
  values: (number | null)[];
  /** Holds each value until the next point, for settings that change in steps */
  stepped?: boolean;
  /** Dashed, for a setting rather than a measurement */
  dashed?: boolean;
}

/** Steps between value ticks, smallest first */
const VALUE_STEPS = [0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100];

/**
 * Round values for a value axis from min to max, with at most about maxTicks steps between them, and the axis range
 * they cover
 */
export function valueTicks(min: number, max: number, maxTicks = 4) {
  const span = Math.max(max - min, 1e-9);
  const step = VALUE_STEPS.find((value) => span / value <= maxTicks) ?? VALUE_STEPS[VALUE_STEPS.length - 1];
  const low = Math.floor(min / step + 1e-9) * step;
  let high = Math.ceil(max / step - 1e-9) * step;
  if (high <= low) {
    high = low + step;
  }
  const ticks: number[] = [];
  for (let value = low; value <= high + step / 2; value += step) {
    ticks.push(Math.round(value * 10) / 10);
  }
  return { ticks, low, high };
}

/** Intervals between time ticks, shortest first */
const TIME_INTERVALS = [
  { hours: 1 },
  { hours: 3 },
  { hours: 6 },
  { hours: 12 },
  { days: 1 },
  { days: 2 },
  { days: 7 },
] as const;

const HOUR_MS = 60 * 60 * 1000;

/**
 * Hours and minutes on a 24-hour clock in every locale, also in English ones. hourCycle "h23" rather than hour12: false,
 * which some browsers show as "24:00" at midnight.
 */
const TIME_FORMAT: Intl.DateTimeFormatOptions = { hour: "2-digit", minute: "2-digit", hourCycle: "h23" };

/**
 * Ticks for a time axis from from to to (milliseconds), at most maxTicks: on whole hours, local midnights or Mondays,
 * labeled with the time, the weekday or the date
 */
export function timeTicks(from: number, to: number, maxTicks: number) {
  const interval =
    TIME_INTERVALS.find(
      (option) => (to - from) / (("hours" in option ? option.hours : option.days * 24) * HOUR_MS) <= maxTicks
    ) ?? TIME_INTERVALS[TIME_INTERVALS.length - 1];

  const date = new Date(from);
  if ("hours" in interval) {
    date.setMinutes(0, 0, 0);
    date.setHours(Math.floor(date.getHours() / interval.hours) * interval.hours);
  } else {
    date.setHours(0, 0, 0, 0);
    // weeks start on Monday
    while (interval.days === 7 && date.getDay() !== 1) {
      date.setDate(date.getDate() + 1);
    }
  }

  const ticks: { time: number; label: string }[] = [];
  for (; date.getTime() <= to; ) {
    if (date.getTime() >= from) {
      ticks.push({
        time: date.getTime(),
        label:
          "hours" in interval
            ? date.toLocaleTimeString(undefined, TIME_FORMAT)
            : interval.days < 7
              ? date.toLocaleDateString(undefined, { weekday: "short" })
              : date.toLocaleDateString(undefined, { day: "numeric", month: "short" }),
      });
    }
    if ("hours" in interval) {
      date.setHours(date.getHours() + interval.hours);
    } else {
      date.setDate(date.getDate() + interval.days);
    }
  }
  return ticks;
}

/** The time of a point, with the weekday, and with the date if the points span weeks */
export function formatTime(time: number, withDate: boolean) {
  return new Date(time).toLocaleString(
    undefined,
    withDate ? { day: "numeric", month: "short", ...TIME_FORMAT } : { weekday: "short", ...TIME_FORMAT }
  );
}

/** SVG path of a line through the values, broken at gaps. A value between two gaps becomes a dot. */
export function linePath(
  values: (number | null)[],
  x: (index: number) => number,
  y: (value: number) => number,
  stepped = false
) {
  let path = "";
  let run = 0;
  values.forEach((value, index) => {
    if (value == null) {
      // a zero-length line with round caps draws a dot
      path += run === 1 ? "h0" : "";
      run = 0;
      return;
    }
    const px = x(index).toFixed(1);
    const py = y(value).toFixed(1);
    path += run === 0 ? `M${px},${py}` : stepped ? `H${px}V${py}` : `L${px},${py}`;
    run++;
  });
  return path + (run === 1 ? "h0" : "");
}

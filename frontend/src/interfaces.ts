export interface SensorStatus {
    humidity: number;
    id: string;
    name: string;
    model: string;
    data: string;
    serial: string;
    rssi: number;
    temperature: number;
    timestamp: number;
    seriesTimestamp: number;
    battery: number;
}

export interface StatusData {
    sensors: SensorStatus[];
    thermostats: DeviceStatus[];
    fritz: {
        status: string;
        host: string;
        user: string;
    }
}

export interface DeviceStatus extends SensorStatus {
    offset: number;
    targetTemperature: number;
    referenceSensor: string;
}

export interface Binding {
    id: string,
    referenceSensor: string,
}
/**
 * Readings of the last hours, from GET /api/history: one point per step, null where FritzGate has none. Each array ends
 * with the current step and leaves out the steps before its first value, so the arrays can differ in length.
 */
export interface HistoryData {
    /** UTC seconds at the start of the current step, the one of the last point of every array */
    last: number;
    /** Seconds per point; each point is the mean of its step */
    step: number;
    sensors: SensorHistory[];
    thermostats: ThermostatHistory[];
}

export interface SensorHistory {
    id: string;
    temperature: (number | null)[];
    humidity: (number | null)[];
}

export interface ThermostatHistory {
    id: string;
    /** Measured by the thermostat, without its offset */
    temperature: (number | null)[];
    /** 0 while heating is off */
    targetTemperature: (number | null)[];
    offset: (number | null)[];
}

export interface ApiService {
    fetchStatus(): Promise<StatusData | undefined>;
    /** Readings of the last hours (at most 720), one point per step seconds (a multiple of 300) */
    fetchHistory(hours: number, step: number): Promise<HistoryData>;
    setBinding(device: string | undefined, reference: string | undefined): Promise<Response>;
    setConfig(host: string, user: string, pass: string): Promise<Response>;
    readonly demo?: boolean; // Set by the demo, which simulates the FRITZ!Box and accepts any credentials
}

export class Status {
    public static unconfigured = "NOT_CONFIGURED";
    public static connected = "CONNECTED";
    public static configured = "CONFIGURED";
    public static connecting = "CONNECTING";
    public static failure = "FAILURE";
}


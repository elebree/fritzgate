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
    /** Release tag of the firmware, such as "v1.2.0", or "dev" */
    version?: string;
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

/** State of the board and its last log lines, from GET /api/diagnostics */
export interface Diagnostics {
    /** Release tag of the firmware, such as "v1.2.0", or "dev" */
    version: string;
    /** Seconds since the start */
    uptime: number;
    /** Why the board started last: power_on, software, crash, task_watchdog, interrupt_watchdog, watchdog, brownout, ... */
    resetReason: string;
    /** Bytes of free memory, now and the lowest since the start, and of free PSRAM */
    freeHeap: number;
    minFreeHeap: number;
    freePsram: number;
    /** Wi-Fi signal in dBm, 0 while not connected */
    rssi: number;
    /** FRITZ!OS version, such as "8.26"; empty if unknown */
    fritzOsVersion: string;
    /** API to the FRITZ!Box chosen after the login, such as "Smart Home REST API"; null while not logged in */
    fritzApi: string | null;
    log: DiagnosticsLog;
}

/**
 * Log lines of the board, oldest first, such as "[  12345][W] message". The board numbers its lines from its start and
 * keeps the last few hundred.
 */
export interface DiagnosticsLog {
    /** Number of the first of the lines */
    first: number;
    /** Number of the next line: ask with after=next for the lines logged since */
    next: number;
    lines: string[];
}

export interface ApiService {
    fetchStatus(): Promise<StatusData | undefined>;
    /** Readings of the last hours (at most 720), one point per step seconds (a multiple of 300) */
    fetchHistory(hours: number, step: number): Promise<HistoryData>;
    /**
     * The state of the board, with the kept log lines numbered after or later. An after beyond the board's next line
     * comes from before a restart: then all kept lines.
     */
    fetchDiagnostics(after: number): Promise<Diagnostics>;
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


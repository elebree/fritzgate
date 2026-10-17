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
export interface ApiService {
    fetchStatus(): Promise<StatusData | undefined>;
    setBinding(device: string | undefined, reference: string | undefined): Promise<Response>;
    setConfig(host: string, user: string, pass: string): Promise<Response>;
}

export class Status {
    public static unconfigured = "NOT_CONFIGURED";
    public static connected = "CONNECTED";
    public static configured = "CONFIGURED";
    public static connecting = "CONNECTING";
    public static failure = "FAILURE";
}


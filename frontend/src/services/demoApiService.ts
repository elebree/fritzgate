import { Status, type ApiService, type Diagnostics, type HistoryData, type SensorStatus, type StatusData } from "../interfaces";
import { demoHistory } from "./demoHistory";

export class DemoApiService implements ApiService {
    readonly demo = true;
    private statusPromise: Promise<StatusData> = undefined!;
    private sensors: SensorStatus[] = [];
    private url!: string;

    constructor() {
        const baseUrl = window.location.origin + window.location.pathname
        this.url = baseUrl + "status.json";
    }

    private async fetchStatusInt(): Promise<StatusData> {
        const response = await fetch(this.url);
        return await response.json();
    }

    async ensureStatusLoaded(): Promise<StatusData> {
        if (!this.statusPromise) {
            this.statusPromise = this.fetchStatusInt();
            var data = await this.statusPromise;
            data.fritz = { host: 'fritz.box', user: 'demo', status: Status.unconfigured };
            for (const sensor of data.sensors) {
                setTimeout(() => this.sensors.push(sensor), Math.random() * 8000);
            }

            return data;
        } else {
            return await this.statusPromise;
        }
    }

    async fetchStatus(): Promise<StatusData | undefined> {
        var status = await this.ensureStatusLoaded();
        const res = {
            fritz: status.fritz,
            sensors: this.sensors.map(v => ({
                ...v,
                temperature: v.temperature + Math.random() / 5,
                humidity: v.humidity + Math.random(),
                rssi: v.rssi + Math.random() * 5
            })),
            thermostats: status.fritz.status === Status.connected
                ? status.thermostats//.map(v => ({ ...v }))
                : [],
        };
        return res;
    }

    async fetchHistory(hours: number, step: number): Promise<HistoryData> {
        const status = await this.ensureStatusLoaded();
        return demoHistory(status, Date.now() / 1000, hours, step);
    }

    async fetchDiagnostics(after: number): Promise<Diagnostics> {
        const status = await this.ensureStatusLoaded();
        // the lines a board logs after its start, in the format of logLine() in the firmware
        const line = (seconds: number, level: string, message: string) =>
            `[${String(Math.round(seconds * 1000)).padStart(6)}][${level}] ${message}`;
        const lines = [
            line(0.4, "S", "FritzGate demo starting"),
            line(0.9, "I", "WiFi connecting: demo"),
            line(3.2, "S", "WiFi ready: http://192.0.2.10"),
            line(31, "S", "FRITZ authenticated"),
            line(31.4, "S", "FRITZ!OS 8.26, API: Smart Home REST API"),
            ...status.sensors.map((sensor, i) =>
                line(40 + i, "D", `Theengs device: ${JSON.stringify({
                    id: sensor.id,
                    name: sensor.name,
                    tempc: sensor.temperature,
                    hum: sensor.humidity,
                })}`)
            ),
            line(61, "H", "HTTP request: GET /api/status HTTP/1.1"),
        ];
        // like the board: the lines numbered after or later, all of them for an after beyond the last
        const first = after > lines.length ? 0 : after;
        return {
            version: "demo",
            uptime: 3600 + Math.floor(performance.now() / 1000),
            resetReason: "power_on",
            freeHeap: 92 * 1024,
            minFreeHeap: 71 * 1024,
            freePsram: 2600 * 1024,
            rssi: -58,
            fritzOsVersion: "8.26",
            fritzApi: "Smart Home REST API",
            log: { first, next: lines.length, lines: lines.slice(first) },
        };
    }

    async setBinding(device: string, reference: string): Promise<Response> {
        var status = await this.ensureStatusLoaded();
        const thermostat = status.thermostats.find(v => v.id === device);
        if (thermostat) {
            thermostat.referenceSensor = reference;
        }
        return { ok: true } as any;
    }

    async setConfig(host: string, user: string, pass: string): Promise<Response> {
        var status = await this.ensureStatusLoaded();
        status.fritz.status = Status.connecting;
        setTimeout(() => {
            status.fritz.status = Status.connected;
        }, 1000);
        return { ok: true } as any;
    }
}
import { Status, type ApiService, type SensorStatus, type StatusData } from "../interfaces";

export class DemoApiService implements ApiService {
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
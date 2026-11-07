import type { ApiService, Diagnostics, HistoryData, StatusData } from "../interfaces";
import { keepHistory } from "./historyCache";

export class FritzGateApiService implements ApiService {
    private getEndpoint(endpoint: string) {
        const apiUrl = (["localhost", "127.0.0.1"].includes(window.location.hostname) ? "http://fritzgate" : "") + "/api/";
        return apiUrl + endpoint;
    }

    async fetchStatus(): Promise<StatusData | undefined> {
        var result: StatusData | undefined = undefined;
        const controller = new AbortController();
        const timeoutId = setTimeout(() => {
            controller.abort();
            //  busy = true;
        }, 2000);

        try {
            const sensorsJson = await fetch(this.getEndpoint("status"), {
                signal: controller.signal,
            });
            result = await sensorsJson.json();
        } finally {
            clearTimeout(timeoutId);
        }
        return result;
    }

    async fetchHistory(hours: number, step: number): Promise<HistoryData> {
        const response = await fetch(this.getEndpoint(`history?hours=${hours}&step=${step}`), {
            signal: AbortSignal.timeout(10000),
        });
        if (!response.ok) {
            throw new Error(`history: HTTP ${response.status}`);
        }
        // joined with the answers the browser kept, as FritzGate loses its history when it restarts
        return keepHistory(hours, await response.json());
    }

    async fetchDiagnostics(after: number): Promise<Diagnostics> {
        // the first answer has all kept lines, up to about 70 KB; later ones only the new lines
        const response = await fetch(this.getEndpoint(`diagnostics?after=${after}`), {
            signal: AbortSignal.timeout(15000),
        });
        if (!response.ok) {
            throw new Error(`diagnostics: HTTP ${response.status}`);
        }
        return await response.json();
    }

    async setBinding(device: string, reference: string): Promise<Response> {
        const response = await fetch(this.getEndpoint("thermostat"), {
            method: "POST",
            body: JSON.stringify({
                id: device,
                referenceSensor: reference,
            }),
            headers: {
                "Content-Type": "application/json",
            },
        });
        return response;
    }

    async setConfig(host: string, user: string, pass: string): Promise<Response> {
        return await fetch(this.getEndpoint("fritzauth"), {
            method: "POST",
            body: JSON.stringify({ host, user, pass }),
        });
    }
}
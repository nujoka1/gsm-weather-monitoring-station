import { describe, expect, it } from "vitest";
import { isOnline, OFFLINE_AFTER_MS, readingAlerts } from "./reading";
import type { Reading } from "./types";

const reading: Reading = { id: 1, station_id: "station", temperature_c: 28, humidity_percent: 60, pressure_hpa: 1008, bmp_temperature_c: 28, altitude_m: 500, signal_rssi: 20, recorded_at: "2026-08-22T10:00:00Z", received_at: "2026-08-22T10:00:00Z" };

describe("station health", () => {
  it("changes to offline at the twelve-minute boundary", () => {
    const received = Date.parse(reading.received_at);
    expect(isOnline(reading, received + OFFLINE_AFTER_MS - 1)).toBe(true);
    expect(isOnline(reading, received + OFFLINE_AFTER_MS)).toBe(false);
  });
  it("reports every breached threshold", () => {
    expect(readingAlerts({ ...reading, temperature_c: 46, humidity_percent: 96, pressure_hpa: 849 }, false)).toEqual(["Extreme temperature", "Extreme humidity", "Unusual pressure", "Station is offline"]);
  });
});


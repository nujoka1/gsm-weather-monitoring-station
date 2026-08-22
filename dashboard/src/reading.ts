import type { Reading } from "./types";

export const OFFLINE_AFTER_MS = 12 * 60 * 1000;

export function isOnline(reading: Reading | undefined, now = Date.now()): boolean {
  if (!reading) return false;
  const received = Date.parse(reading.received_at);
  return Number.isFinite(received) && now - received < OFFLINE_AFTER_MS;
}

export function readingAlerts(reading: Reading | undefined, online: boolean): string[] {
  if (!reading) return [];
  return [
    reading.temperature_c > 45 && "Extreme temperature",
    reading.humidity_percent > 95 && "Extreme humidity",
    (reading.pressure_hpa < 850 || reading.pressure_hpa > 1050) && "Unusual pressure",
    !online && "Station is offline",
  ].filter((value): value is string => Boolean(value));
}


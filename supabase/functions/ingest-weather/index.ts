import { createClient } from "npm:@supabase/supabase-js@2.57.4";

const JSON_HEADERS = { "content-type": "application/json; charset=utf-8" };
const MAX_BODY_BYTES = 2048;
const STATION_PATTERN = /^[A-Z0-9_]{3,32}$/;

type Payload = {
  station_id: string;
  temperature_c: number;
  humidity_percent: number;
  pressure_hpa: number;
  bmp_temperature_c?: number;
  altitude_m?: number;
  signal_rssi?: number;
};

function reply(status: number, code: string, message: string, extra = {}) {
  return new Response(JSON.stringify({ error: status >= 400 ? { code, message } : undefined, ...extra }), {
    status,
    headers: JSON_HEADERS,
  });
}

function constantTimeEqual(left: string, right: string): boolean {
  const encoder = new TextEncoder();
  const a = encoder.encode(left);
  const b = encoder.encode(right);
  const length = Math.max(a.length, b.length);
  let mismatch = a.length ^ b.length;
  for (let index = 0; index < length; index++) mismatch |= (a[index] ?? 0) ^ (b[index] ?? 0);
  return mismatch === 0;
}

function finite(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value);
}

export function validatePayload(value: unknown): { data?: Payload; status?: number; message?: string } {
  if (!value || typeof value !== "object" || Array.isArray(value)) return { status: 400, message: "Body must be a JSON object." };
  const body = value as Record<string, unknown>;
  const required = ["temperature_c", "humidity_percent", "pressure_hpa"];
  if (typeof body.station_id !== "string" || !STATION_PATTERN.test(body.station_id) || required.some(key => !finite(body[key]))) {
    return { status: 400, message: "Required fields have invalid types." };
  }
  const ranges: [string, number, number][] = [
    ["temperature_c", -40, 85], ["humidity_percent", 0, 100], ["pressure_hpa", 300, 1100],
  ];
  for (const [key, min, max] of ranges) {
    const number = body[key] as number;
    if (number < min || number > max) return { status: 422, message: `${key} is outside its allowed range.` };
  }
  for (const key of ["bmp_temperature_c", "altitude_m", "signal_rssi"]) {
    if (body[key] !== undefined && !finite(body[key])) return { status: 400, message: `${key} must be a finite number.` };
  }
  if (finite(body.bmp_temperature_c) && (body.bmp_temperature_c < -40 || body.bmp_temperature_c > 85)) return { status: 422, message: "bmp_temperature_c is outside its allowed range." };
  if (finite(body.altitude_m) && (body.altitude_m < -500 || body.altitude_m > 10000)) return { status: 422, message: "altitude_m is outside its allowed range." };
  if (finite(body.signal_rssi) && (!Number.isInteger(body.signal_rssi) || !((body.signal_rssi >= 0 && body.signal_rssi <= 31) || body.signal_rssi === 99))) return { status: 422, message: "signal_rssi is outside its allowed range." };
  return { data: body as Payload };
}

export async function handler(request: Request): Promise<Response> {
  if (request.method === "OPTIONS") return new Response(null, { status: 204, headers: { allow: "POST, OPTIONS" } });
  if (request.method !== "POST") return new Response(JSON.stringify({ error: { code: "method_not_allowed", message: "Use POST." } }), { status: 405, headers: { ...JSON_HEADERS, allow: "POST, OPTIONS" } });
  const expectedKey = Deno.env.get("WEATHER_STATION_API_KEY");
  if (!expectedKey) return reply(503, "service_unavailable", "Ingestion is not configured.");
  const providedKey = request.headers.get("x-station-key") ?? "";
  if (!constantTimeEqual(providedKey, expectedKey)) return reply(401, "unauthorized", "Station authentication failed.");
  if (!(request.headers.get("content-type") ?? "").toLowerCase().startsWith("application/json")) return reply(415, "unsupported_media_type", "Content-Type must be application/json.");
  const contentLength = Number(request.headers.get("content-length") ?? 0);
  if (contentLength > MAX_BODY_BYTES) return reply(413, "payload_too_large", "Request body is too large.");
  const text = await request.text();
  if (new TextEncoder().encode(text).length > MAX_BODY_BYTES) return reply(413, "payload_too_large", "Request body is too large.");
  let parsed: unknown;
  try { parsed = JSON.parse(text); } catch { return reply(400, "invalid_json", "Body is not valid JSON."); }
  const validation = validatePayload(parsed);
  if (!validation.data) return reply(validation.status ?? 400, validation.status === 422 ? "invalid_measurement" : "invalid_request", validation.message ?? "Invalid request.");

  const keys = JSON.parse(Deno.env.get("SUPABASE_SECRET_KEYS") ?? "{}") as Record<string, string>;
  const secretKey = keys.default ?? Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
  const url = Deno.env.get("SUPABASE_URL");
  if (!url || !secretKey) return reply(503, "service_unavailable", "Database access is not configured.");
  const supabase = createClient(url, secretKey, { auth: { persistSession: false, autoRefreshToken: false } });
  const payload = validation.data;
  const { data: station, error: stationError } = await supabase.from("weather_stations").select("id,is_active").eq("station_code", payload.station_id).maybeSingle();
  if (stationError) return reply(500, "database_error", "Unable to process the reading.");
  if (!station || !station.is_active) return reply(404, "station_not_found", "Station is unknown or inactive.");
  const { data: inserted, error } = await supabase.from("weather_readings").insert({
    station_id: station.id,
    temperature_c: payload.temperature_c,
    humidity_percent: payload.humidity_percent,
    pressure_hpa: payload.pressure_hpa,
    bmp_temperature_c: payload.bmp_temperature_c ?? null,
    altitude_m: payload.altitude_m ?? null,
    signal_rssi: payload.signal_rssi ?? null,
  }).select("id,received_at").single();
  if (error) return reply(500, "database_error", "Unable to store the reading.");
  console.info("weather_reading_ingested", { station_id: payload.station_id, reading_id: inserted.id });
  return new Response(JSON.stringify({ data: inserted }), { status: 201, headers: JSON_HEADERS });
}

if (import.meta.main) Deno.serve(handler);


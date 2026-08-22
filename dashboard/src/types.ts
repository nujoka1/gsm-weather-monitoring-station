export type Station = { id: string; station_code: string; name: string; location_name: string | null };
export type Reading = { id: number; station_id: string; temperature_c: number; humidity_percent: number; pressure_hpa: number; bmp_temperature_c: number | null; altitude_m: number | null; signal_rssi: number | null; recorded_at: string; received_at: string };


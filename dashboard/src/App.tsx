import { useCallback, useEffect, useMemo, useState } from "react";
import { AlertTriangle, CloudSun, Download, Droplets, Gauge, MapPin, RefreshCw, Signal, Thermometer, Wind } from "lucide-react";
import { CartesianGrid, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import { configured, supabase } from "./supabase";
import type { Reading, Station } from "./types";
import { isOnline, readingAlerts } from "./reading";

const PAGE_SIZE = 20;
type Range = "24h" | "7d" | "30d";

function App() {
  const [stations, setStations] = useState<Station[]>([]), [stationId, setStationId] = useState("");
  const [readings, setReadings] = useState<Reading[]>([]), [range, setRange] = useState<Range>("24h"), [page, setPage] = useState(0);
  const [loading, setLoading] = useState(configured), [error, setError] = useState("");
  const loadStations = useCallback(async () => {
    if (!supabase) return setLoading(false);
    const { data, error: queryError } = await supabase.from("weather_stations").select("id,station_code,name,location_name").order("name");
    if (queryError) { setError(queryError.message); setLoading(false); return; }
    const list = (data ?? []) as Station[]; setStations(list); setStationId(current => current || list[0]?.id || "");
  }, []);
  const loadReadings = useCallback(async () => {
    if (!supabase || !stationId) return;
    setError(""); setLoading(true);
    const since = new Date(Date.now() - ({ "24h": 1, "7d": 7, "30d": 30 }[range]) * 86400000).toISOString();
    const { data, error: queryError } = await supabase.from("weather_readings").select("*").eq("station_id", stationId).gte("recorded_at", since).order("recorded_at", { ascending: false }).limit(5000);
    if (queryError) setError(queryError.message); else setReadings((data ?? []) as Reading[]);
    setLoading(false);
  }, [range, stationId]);
  useEffect(() => { void loadStations(); }, [loadStations]);
  useEffect(() => { setPage(0); void loadReadings(); }, [loadReadings]);
  useEffect(() => {
    if (!supabase || !stationId) return;
    const client = supabase;
    const channel = client.channel(`station-${stationId}`).on("postgres_changes", { event: "INSERT", schema: "public", table: "weather_readings", filter: `station_id=eq.${stationId}` }, event => setReadings(items => [event.new as Reading, ...items])).subscribe();
    const timer = window.setInterval(() => void loadReadings(), 5 * 60 * 1000);
    return () => { window.clearInterval(timer); void client.removeChannel(channel); };
  }, [loadReadings, stationId]);
  const current = readings[0], online = isOnline(current);
  const alerts = readingAlerts(current, online);
  const chart = useMemo(() => readings.slice().reverse().map(item => ({ ...item, time: new Date(item.recorded_at).toLocaleString([], { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" }) })), [readings]);
  const shown = readings.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE);
  const exportCsv = () => {
    const rows = [["recorded_at","temperature_c","humidity_percent","pressure_hpa","altitude_m","signal_rssi"], ...readings.map(item => [item.recorded_at,item.temperature_c,item.humidity_percent,item.pressure_hpa,item.altitude_m ?? "",item.signal_rssi ?? ""] )];
    const blob = new Blob([rows.map(row => row.join(",")).join("\n")], { type: "text/csv" }); const link = document.createElement("a"); link.href = URL.createObjectURL(blob); link.download = "weather-readings.csv"; link.click(); URL.revokeObjectURL(link.href);
  };
  if (!configured) return <main className="center"><CloudSun/><h1>Weather dashboard</h1><p>Set the two Vite Supabase variables described in <code>.env.example</code>.</p></main>;
  return <div className="shell"><header><div className="brand"><img src="./icon-192.png" alt=""/><div><strong>AtmosWatch</strong><small>Abuja monitoring network</small></div></div><label>Station<select value={stationId} onChange={event => setStationId(event.target.value)}>{stations.map(item => <option value={item.id} key={item.id}>{item.name}</option>)}</select></label></header><main>
    <section className={`status ${online ? "online" : "offline"}`}><div><span/><strong>{online ? "Station online" : "Station offline"}</strong><p>{current ? `Last received ${new Date(current.received_at).toLocaleString()}` : "Waiting for the first reading"}</p></div><MapPin/>{stations.find(item => item.id === stationId)?.location_name ?? "Location unavailable"}</section>
    {error && <div className="error" role="alert"><AlertTriangle/>{error}<button onClick={() => void loadReadings()}><RefreshCw/>Retry</button></div>}
    {alerts.length > 0 && <section className="alerts"><AlertTriangle/><div><strong>Attention needed</strong><p>{alerts.join(" · ")}</p></div></section>}
    <section className="metrics"><Metric icon={<Thermometer/>} label="Temperature" value={current ? `${current.temperature_c.toFixed(1)} °C` : "—"}/><Metric icon={<Droplets/>} label="Humidity" value={current ? `${current.humidity_percent.toFixed(1)}%` : "—"}/><Metric icon={<Gauge/>} label="Pressure" value={current ? `${current.pressure_hpa.toFixed(1)} hPa` : "—"}/><Metric icon={<Wind/>} label="Est. altitude" value={current?.altitude_m != null ? `${current.altitude_m.toFixed(0)} m` : "—"}/><Metric icon={<Signal/>} label="MTN signal" value={current?.signal_rssi != null ? `${current.signal_rssi}/31` : "—"}/></section>
    <section className="panel"><div className="panel-head"><div><small>Environmental trends</small><h2>Weather over time</h2></div><div className="range">{(["24h","7d","30d"] as Range[]).map(item => <button className={range === item ? "active" : ""} onClick={() => setRange(item)} key={item}>{item}</button>)}</div></div>{loading ? <p className="empty">Loading readings…</p> : chart.length ? <div className="chart"><ResponsiveContainer width="100%" height="100%"><LineChart data={chart}><CartesianGrid strokeDasharray="3 3"/><XAxis dataKey="time" minTickGap={50}/><YAxis/><Tooltip/><Line type="monotone" dataKey="temperature_c" stroke="#d45d32" dot={false}/><Line type="monotone" dataKey="humidity_percent" stroke="#277da1" dot={false}/></LineChart></ResponsiveContainer></div> : <p className="empty">No readings in this range.</p>}</section>
    <section className="panel history"><div className="panel-head"><div><small>Station archive</small><h2>Reading history</h2></div><button onClick={exportCsv} disabled={!readings.length}><Download/>Export CSV</button></div><div className="table-wrap"><table><thead><tr><th>Time</th><th>Temperature</th><th>Humidity</th><th>Pressure</th><th>RSSI</th></tr></thead><tbody>{shown.map(item => <tr key={item.id}><td>{new Date(item.recorded_at).toLocaleString()}</td><td>{item.temperature_c.toFixed(1)} °C</td><td>{item.humidity_percent.toFixed(1)}%</td><td>{item.pressure_hpa.toFixed(1)} hPa</td><td>{item.signal_rssi ?? "—"}</td></tr>)}</tbody></table></div><div className="pager"><button disabled={page === 0} onClick={() => setPage(value => value - 1)}>Previous</button><span>Page {page + 1}</span><button disabled={(page + 1) * PAGE_SIZE >= readings.length} onClick={() => setPage(value => value + 1)}>Next</button></div></section>
    <section className="about"><h2>About this station</h2><p>AHT20 provides ambient temperature and humidity; BMP280 provides pressure and secondary temperature. Altitude is an estimate calculated from pressure against assumed sea-level pressure and is not survey-grade elevation.</p></section>
  </main><footer>WS_ABU_001 · Five-minute telemetry · Public read-only academic dashboard</footer></div>;
}
function Metric({ icon, label, value }: { icon: React.ReactNode; label: string; value: string }) { return <article><span>{icon}</span><small>{label}</small><strong>{value}</strong></article>; }
export default App;

// Clima (Windows) — capa de datos. Port 1:1 de c/clima_data.c + clima_ambiental_c.inc +
// los scrapers de Mineduc/airerm. Mismo JSON que produce la versión Linux (UI idéntica).
"use strict";
const fs = require("fs");
const path = require("path");

const TZ = "America/Santiago";
const UA = "ClimaKotone/2.0 (Windows; Electron)";
let DATA_DIR = path.join(__dirname, "..", "app", "data");
let CACHE_FILE = path.join(process.env.APPDATA || ".", "Clima", "last.json");
function configure(o) { if (o.dataDir) DATA_DIR = o.dataDir; if (o.cacheFile) CACHE_FILE = o.cacheFile; }

// ------------------------------------------------------------------ helpers
const lround = (x) => Math.sign(x) * Math.round(Math.abs(x));   // como C lround (mitad lejos de 0)
const num = (o, k) => (o && o[k] != null && typeof o[k] === "number" ? o[k] : 0);
const has = (o, k) => o != null && o[k] != null;
const str = (o, k) => (has(o, k) ? String(o[k]) : "");
const an = (o, k, i) => { const a = o && o[k]; return Array.isArray(a) && a[i] != null ? Number(a[i]) : 0; };
const as = (o, k, i) => { const a = o && o[k]; return Array.isArray(a) && a[i] != null ? String(a[i]) : ""; };
const al = (o, k) => (o && Array.isArray(o[k]) ? o[k].length : 0);
const fold = (s) => String(s).toLowerCase().normalize("NFD").replace(/\p{M}/gu, "");

async function http(url, ms = 20000, extra = {}) {
  const r = await fetch(url, { headers: { "User-Agent": UA }, signal: AbortSignal.timeout(ms), redirect: "follow", ...extra });
  if (!r.ok) throw new Error("HTTP " + r.status);
  return r.text();
}
async function httpJson(url, ms) { try { return JSON.parse(await http(url, ms)); } catch (e) { return null; } }

// Fecha/hora en Santiago (independiente de la zona de Windows)
function stgo(d = new Date()) {
  const p = {};
  for (const x of new Intl.DateTimeFormat("en-CA", { timeZone: TZ, hourCycle: "h23", year: "numeric", month: "2-digit",
    day: "2-digit", hour: "2-digit", minute: "2-digit", weekday: "short" }).formatToParts(d)) p[x.type] = x.value;
  const wdMap = { Mon: 0, Tue: 1, Wed: 2, Thu: 3, Fri: 4, Sat: 5, Sun: 6 };
  return { y: +p.year, m: +p.month, d: +p.day, h: +p.hour, wd: wdMap[p.weekday] };
}

// ------------------------------------------------------------------- caché
function cacheSave(json) { try { fs.mkdirSync(path.dirname(CACHE_FILE), { recursive: true }); fs.writeFileSync(CACHE_FILE, json); } catch (e) {} }
function cacheLoad() {
  try { const o = JSON.parse(fs.readFileSync(CACHE_FILE, "utf8")); o.cached = true; return JSON.stringify(o); } catch (e) { return null; }
}

// ----------------------------------------------------------------- comunas
let _comunas = null;
function comunas() {
  if (!_comunas) { try { _comunas = JSON.parse(fs.readFileSync(path.join(DATA_DIR, "comunas_cl.json"), "utf8")); } catch (e) { _comunas = []; } }
  return _comunas;
}
function searchJson(query, max) {
  const q = fold(query), out = [], seen = new Set();
  for (let pass = 0; pass < 2 && out.length < max; pass++)
    for (const o of comunas()) {
      if (out.length >= max) break;
      const nm = fold(str(o, "name"));
      if (!(pass === 0 ? nm.startsWith(q) : nm.includes(q)) || seen.has(o)) continue;
      seen.add(o);
      out.push({ name: str(o, "name"), region: str(o, "region"), lat: num(o, "lat"), lon: num(o, "lon") });
    }
  return out;
}
function nearestComuna(lat, lon) {
  let best = null, bd = 1e18;
  for (const o of comunas()) { const a = num(o, "lat") - lat, b = num(o, "lon") - lon, d = a * a + b * b; if (d < bd) { bd = d; best = o; } }
  return best && bd < 0.20 ? best : null;
}
const locFromComuna = (c, lat, lon) => ({ name: str(c, "name"), region: str(c, "region"), lat, lon, is_comuna: true, rm_restr: c.rm_restr === true });
function locFromCoords(lat, lon) {
  const c = nearestComuna(lat, lon);
  return c ? locFromComuna(c, lat, lon) : { name: "Mi ubicación", region: "", lat, lon, is_comuna: false, rm_restr: false };
}
async function geocode(name) {
  const r = searchJson(name, 1);
  if (r.length) { const c = nearestComuna(r[0].lat, r[0].lon); return { ...r[0], is_comuna: true, rm_restr: !!(c && c.rm_restr === true) }; }
  const g = await httpJson(`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(name)}&count=1&language=es&format=json`);
  if (g && g.results && g.results.length) { const x = g.results[0]; return { name: str(x, "name"), region: str(x, "admin1"), lat: num(x, "latitude"), lon: num(x, "longitude"), is_comuna: false, rm_restr: false }; }
  return null;
}
// Ubicación automática por IP (Windows no trae GeoClue); Santiago por defecto
async function currentLocation() {
  const o = await httpJson("http://ip-api.com/json/?fields=city,lat,lon,regionName", 8000);
  if (o && typeof o.lat === "number") {
    const c = nearestComuna(o.lat, o.lon);
    return { name: c ? str(c, "name") : str(o, "city"), region: c ? str(c, "region") : str(o, "regionName"),
             lat: o.lat, lon: o.lon, is_comuna: !!c, rm_restr: !!(c && c.rm_restr === true) };
  }
  return { name: "Santiago", region: "Región Metropolitana", lat: -33.4489, lon: -70.6693, is_comuna: true, rm_restr: true };
}

// --------------------------------------------------------------------- WMO
const WMO = {
  0: ["☀️", "Despejado", "clear"], 1: ["🌤️", "Mayormente despejado", "clear"], 2: ["⛅", "Parcialmente nublado", "cloud"], 3: ["☁️", "Nublado", "cloud"],
  45: ["🌫️", "Niebla", "fog"], 48: ["🌫️", "Niebla con escarcha", "fog"],
  51: ["🌦️", "Llovizna ligera", "rain"], 53: ["🌦️", "Llovizna", "rain"], 55: ["🌧️", "Llovizna intensa", "rain"],
  56: ["🌧️", "Llovizna helada", "rain"], 57: ["🌧️", "Llovizna helada", "rain"],
  61: ["🌧️", "Lluvia ligera", "rain"], 63: ["🌧️", "Lluvia", "rain"], 65: ["🌧️", "Lluvia fuerte", "rain"],
  66: ["🌧️", "Lluvia helada", "rain"], 67: ["🌧️", "Lluvia helada", "rain"],
  71: ["🌨️", "Nieve ligera", "snow"], 73: ["🌨️", "Nieve", "snow"], 75: ["❄️", "Nieve fuerte", "snow"], 77: ["🌨️", "Granizo fino", "snow"],
  80: ["🌦️", "Chubascos ligeros", "rain"], 81: ["🌧️", "Chubascos", "rain"], 82: ["⛈️", "Chubascos violentos", "storm"],
  85: ["🌨️", "Chubascos de nieve", "snow"], 86: ["❄️", "Chubascos de nieve", "snow"],
  95: ["⛈️", "Tormenta", "storm"], 96: ["⛈️", "Tormenta con granizo", "storm"], 99: ["⛈️", "Tormenta fuerte", "storm"],
};
const wmo = (c) => WMO[c] || ["☁️", "—", "cloud"];
const RAIN = new Set([51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82, 95, 96, 99]);
const isRain = (c) => RAIN.has(c);
const isStorm = (c) => c === 95 || c === 96 || c === 99;
const isSnow = (c) => [71, 73, 75, 77, 85, 86].includes(c);
const weekdayEs = (iso) => ["lunes", "martes", "miércoles", "jueves", "viernes", "sábado", "domingo"][(new Date(iso + "T00:00:00Z").getUTCDay() + 6) % 7];
const rose = (deg) => ["del norte", "del noreste", "del este", "del sureste", "del sur", "del suroeste", "del oeste", "del noroeste"][lround(((deg + 360) % 360) / 45) % 8];

// -------------------------------------------------------------- aviso lluvia
function buildRain(daily) {
  const n = al(daily, "time"), labels = []; let cnt = 0, first = "";
  for (let i = 0; i < n && i < 8; i++) {
    if (isRain(Math.trunc(an(daily, "weather_code", i))) && an(daily, "precipitation_probability_max", i) >= 50) {
      const lbl = i === 0 ? "hoy" : i === 1 ? "mañana" : "el " + weekdayEs(as(daily, "time", i));
      if (!cnt) first = lbl; labels.push(lbl); cnt++;
    }
  }
  if (!cnt) return null;
  return { summary: cnt <= 3 ? `Se espera lluvia ${labels.join(", ")}.` : `Semana lluviosa: lluvia ${first} y ${cnt - 1} días más de los próximos 7.`, count: cnt };
}

// ----------------------------------------------------------------- contexto
function hourlyNow(h) {
  const n = al(h, "time"); if (!n) return -1;
  const t = stgo(), p = (x) => String(x).padStart(2, "0");
  const want = `${t.y}-${p(t.m)}-${p(t.d)}T${p(t.h)}`;
  for (let i = 0; i < n; i++) if (as(h, "time", i).startsWith(want)) return i;
  return -1;
}
const hourLabel = (h, i) => { const s = as(h, "time", i); return s.length >= 16 ? s.slice(11, 16) : "más tarde"; };

function buildContext(w, cur, day) {
  const hourly = w.hourly || null, nowI = hourly ? hourlyNow(hourly) : -1;
  const code = Math.trunc(num(cur, "weather_code")), precip = num(cur, "precipitation"), wind = num(cur, "wind_speed_10m"),
    gust = num(cur, "wind_gusts_10m"), wdir = num(cur, "wind_direction_10m"), temp = num(cur, "temperature_2m"),
    feels = num(cur, "apparent_temperature"), cloud = num(cur, "cloud_cover"), isDay = num(cur, "is_day") !== 0,
    tmax = an(day, "temperature_2m_max", 0), tmin = an(day, "temperature_2m_min", 0), uv = an(day, "uv_index_max", 0), ndays = al(day, "time");
  let icon = "🌤️", txt = "", kind = "info";
  const set = (i, k, t) => { icon = i; kind = k; txt = t; };

  if (!txt && (precip > 0.05 || isRain(code) || isSnow(code)) && nowI >= 0) {
    const n = al(hourly, "time"); let stop = -1;
    for (let i = nowI + 1; i < n && i <= nowI + 12; i++) {
      const hc = Math.trunc(an(hourly, "weather_code", i)), hp = an(hourly, "precipitation_probability", i);
      if (!isRain(hc) && !isSnow(hc) && hp < 40) { stop = i; break; }
    }
    let que = isSnow(code) ? "Nieve" : isStorm(code) ? "Tormenta" : "Lluvia";
    if (code >= 51 && code <= 57) que = "Llovizna";
    icon = isSnow(code) ? "🌨️" : "🌧️"; kind = isSnow(code) ? "snow" : "rain";
    if (stop < 0) txt = `${que} ahora; sigue durante las próximas horas.`;
    else if (stop - nowI <= 1) txt = `${que} ahora; para dentro de poco.`;
    else txt = `${que} ahora; amaina hacia las ${hourLabel(hourly, stop)}.`;
  }
  if (!txt && nowI >= 0) {
    const n = al(hourly, "time"), dcode = Math.trunc(an(day, "weather_code", 0));
    for (let i = nowI + 1; i < n && i <= nowI + 6; i++) {
      const hc = Math.trunc(an(hourly, "weather_code", i)), hp = an(hourly, "precipitation_probability", i);
      const porCodigo = (isRain(hc) || isSnow(hc)) && hp >= 50, porProb = hp >= 70;
      if (!porCodigo && !porProb) continue;
      const ref = isRain(hc) || isSnow(hc) ? hc : dcode;
      const que = isSnow(ref) ? "nieve" : isStorm(ref) ? "tormenta" : ref >= 51 && ref <= 57 ? "llovizna" : "lluvia";
      set(isStorm(ref) ? "⛈️" : isSnow(ref) ? "🌨️" : "🌧️", isSnow(ref) ? "snow" : "rain",
        `Se espera ${que} hacia las ${hourLabel(hourly, i)} (${lround(hp)}%).`);
      break;
    }
  }
  if (!txt) {
    const dcode = Math.trunc(an(day, "weather_code", 0)), dprob = an(day, "precipitation_probability_max", 0);
    if ((isRain(dcode) || isSnow(dcode)) && dprob >= 60)
      set(isSnow(dcode) ? "🌨️" : "🌧️", isSnow(dcode) ? "snow" : "rain",
        `${isSnow(dcode) ? "Nieve" : dcode >= 51 && dcode <= 57 ? "Llovizna" : "Lluvia"} probable en algún momento de hoy (${lround(dprob)}%).`);
  }
  if (!txt && isStorm(Math.trunc(an(day, "weather_code", 0)))) set("⛈️", "storm", "Posibles tormentas eléctricas hoy.");
  if (!txt && gust >= 45) set("💨", "wind", `Ráfagas de hasta ${lround(gust)} km/h ${rose(wdir)}.`);
  if (!txt && wind >= 30) set("💨", "wind", `Viento de ${lround(wind)} km/h ${rose(wdir)}.`);
  if (!txt && (code === 45 || code === 48)) set("🌫️", "fog", "Niebla: visibilidad reducida, ojo al manejar.");
  if (!txt && tmax >= 32) set("🥵", "heat", `Calor: hoy llega a ${lround(tmax)}°. Hidrátate.`);
  if (!txt && tmin <= 1) set("🥶", "cold", `Helada: la mínima baja a ${lround(tmin)}°.`);
  if (!txt && Math.abs(feels - temp) >= 4)
    set(feels < temp ? "🥶" : "🥵", "feels", `Se siente como ${lround(feels)}°, ${feels < temp ? "más frío" : "más caluroso"} de lo que marca.`);
  if (!txt && isDay && uv >= 8) set("🧴", "uv", `Índice UV muy alto (${lround(uv)}): usa protector.`);
  if (!txt)
    for (let i = 1; i < ndays && i < 8; i++)
      if (isRain(Math.trunc(an(day, "weather_code", i))) && an(day, "precipitation_probability_max", i) >= 50) {
        set("💧", "rain", `Se espera lluvia ${i === 1 ? "mañana" : "el " + weekdayEs(as(day, "time", i))}.`); break;
      }
  if (!txt && ndays > 1) {
    const t2 = an(day, "temperature_2m_max", 1);
    if (Math.abs(t2 - tmax) >= 6)
      set(t2 > tmax ? "📈" : "📉", "trend", `Mañana ${t2 > tmax ? "sube" : "baja"}: máxima de ${lround(t2)}°, ${lround(Math.abs(t2 - tmax))}° ${t2 > tmax ? "más" : "menos"} que hoy.`);
  }
  if (!txt && nowI >= 0 && hourly) {
    const later = nowI + 4;
    if (later < al(hourly, "time")) {
      const hc = Math.trunc(an(hourly, "weather_code", later));
      if (cloud >= 70 && hc <= 1) set("🌤️", "clear", `Se despeja hacia las ${hourLabel(hourly, later)}.`);
      else if (cloud <= 30 && hc >= 3) set("☁️", "cloud", `Se nubla hacia las ${hourLabel(hourly, later)}.`);
    }
  }
  if (!txt) set(isDay ? "🌤️" : "🌙", "steady", `Sin cambios notorios: máxima de ${lround(tmax)}° y mínima de ${lround(tmin)}°.`);
  return { icon, text: txt, kind };
}

// ---------------------------------------------------------------- ambiental
async function airData(loc) {
  const o = await httpJson(`https://air-quality-api.open-meteo.com/v1/air-quality?latitude=${loc.lat.toFixed(4)}&longitude=${loc.lon.toFixed(4)}` +
    `&timezone=America/Santiago&current=pm2_5,pm10&hourly=pm2_5&forecast_days=1`);
  if (!o) return { air: null, key: "normal" };
  const cur = o.current || {}, hh = o.hourly || {}, m = al(hh, "pm2_5");
  const pm25 = num(cur, "pm2_5"), pm10 = num(cur, "pm10");
  let sum = 0, c = 0, sumAll = 0;
  for (let i = 0; i < m; i++) { const v = an(hh, "pm2_5", i); sumAll += v; if (v > 0 || i === 0) { sum += v; c++; } }
  const mean24 = c ? sum / c : pm25, meanAll = m ? sumAll / m : 0;
  let lvl = "Bueno", col = "#34C759", key = "normal";
  if (mean24 >= 170) { lvl = "Emergencia"; col = "#AF52DE"; key = "emergencia"; }
  else if (mean24 >= 110) { lvl = "Preemergencia"; col = "#FF3B30"; key = "preemergencia"; }
  else if (mean24 >= 80) { lvl = "Alerta"; col = "#FF9500"; key = "alerta"; }
  else if (mean24 >= 50) { lvl = "Regular"; col = "#FFD426"; key = "normal"; }
  const r1 = (x) => Math.round(x * 10) / 10;
  const epKey = meanAll >= 170 ? "emergencia" : meanAll >= 110 ? "preemergencia" : meanAll >= 80 ? "alerta" : "normal";
  return { air: { pm25: r1(pm25), pm10: r1(pm10), mean24: r1(mean24), level: lvl, color: col, key }, key: epKey };
}
function buildMoon() {
  const names = ["Luna nueva", "Luna creciente", "Cuarto creciente", "Gibosa creciente", "Luna llena", "Gibosa menguante", "Cuarto menguante", "Luna menguante"];
  const emo = ["🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘"];
  const days = (Date.now() - Date.UTC(2000, 0, 6)) / 86400000, syn = 29.53058867;
  let frac = (days % syn) / syn; if (frac < 0) frac += 1;
  const idx = Math.floor(frac * 8 + 0.5) % 8;
  return { name: names[idx], emoji: emo[idx], illumination: lround((1 - Math.cos(2 * Math.PI * frac)) / 2 * 100) };
}
const PORTS = [["Arica", -18.48, -70.32], ["Iquique", -20.21, -70.15], ["Antofagasta", -23.65, -70.40], ["Caldera", -27.07, -70.82],
  ["Coquimbo", -29.95, -71.34], ["Valparaíso", -33.03, -71.63], ["San Antonio", -33.59, -71.61], ["Constitución", -35.33, -72.41],
  ["Talcahuano", -36.72, -73.11], ["Lebu", -37.61, -73.65], ["Corral", -39.87, -73.43], ["Puerto Montt", -41.47, -72.94],
  ["Castro", -42.48, -73.76], ["Puerto Chacabuco", -45.46, -72.82], ["Punta Arenas", -53.16, -70.91]];
async function buildTides(loc) {
  let best = 0, bd = 1e18;
  PORTS.forEach((p, i) => { const a = p[1] - loc.lat, b = (p[2] - loc.lon) * Math.cos(loc.lat * Math.PI / 180), d = a * a + b * b; if (d < bd) { bd = d; best = i; } });
  const dist = Math.sqrt(bd) * 111, P = PORTS[best];
  const n = await httpJson(`https://marine-api.open-meteo.com/v1/marine?latitude=${P[1].toFixed(3)}&longitude=${P[2].toFixed(3)}&timezone=America/Santiago&hourly=sea_level_height_msl&forecast_days=2`);
  if (!n) return null;
  const hh = n.hourly || {}, m = al(hh, "sea_level_height_msl"); if (m < 6) return null;
  let hi = null, lo = null;
  for (let i = 1; i + 1 < m; i++) {
    const y0 = an(hh, "sea_level_height_msl", i - 1), y1 = an(hh, "sea_level_height_msl", i), y2 = an(hh, "sea_level_height_msl", i + 1);
    if (!hi && y1 >= y0 && y1 >= y2) hi = [as(hh, "time", i), Math.round(y1 * 100) / 100];
    else if (!lo && y1 <= y0 && y1 <= y2) lo = [as(hh, "time", i), Math.round(y1 * 100) / 100];
    if (hi && lo) break;
  }
  return { port: P[0], coastal: dist < 15, dist_km: lround(dist), next_high: hi, next_low: lo };
}

// Mineduc / airerm (scrapers portados)
function htmlText(html) {
  return html.replace(/<(script|style)[\s\S]*?<\/\1\s*>/gi, "").replace(/<[^>]*>/g, " ")
    .replace(/&nbsp;/g, " ").replace(/&amp;/g, "&").replace(/&#(\d+);/g, (_, d) => String.fromCharCode(+d));
}
const norm = (s) => fold(s).replace(/[^\x00-\x7f]/g, " ");
async function restriccionOficial() {
  try {
    const t = norm(htmlText(await http("https://airerm.mma.gob.cl/", 6000))).replace(/\s+/g, " ");
    const s = stgo(), MES = ["enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"];
    const hoy = `${s.d} de ${MES[s.m - 1]}`, p = t.indexOf("restriccion vehicular para hoy");
    if (p >= 0) {
      const win = t.substr(p, 1000);
      if (win.includes(hoy)) return { oficial: true, episodio: win.includes("emergencia ambiental") ? "Emergencia" : win.includes("preemergencia") ? "Preemergencia" : "Normal" };
    }
  } catch (e) {}
  return { oficial: false, episodio: "Normal" };
}
async function buildRestriccion(loc, key) {
  const s = stgo(), wd = s.wd, season = s.m >= 5 && s.m < 9, dig = ["8 y 9", "0 y 1", "2 y 3", "4 y 5", "6 y 7"];
  if (!loc.rm_restr) return { aplica: false, motivo: "La restricción por dígitos rige en la Provincia de Santiago, San Bernardo y Puente Alto." };
  if (!season) return { aplica: false, motivo: "Fuera de temporada (rige del 1 de mayo al 31 de agosto)." };
  if (wd >= 5) return { aplica: false, motivo: "Fin de semana: sin restricción (rige lunes a viernes)." };
  let lvl = { alerta: "Alerta", preemergencia: "Preemergencia", emergencia: "Emergencia" }[key] || "Normal";
  const r = await restriccionOficial(); let oficial = false;
  if (r.oficial && (r.episodio === "Preemergencia" || r.episodio === "Emergencia")) { oficial = true; lvl = r.episodio; }
  return { aplica: true, level: lvl, digitos: dig[wd], oficial };
}

const regionToken = (r) => ["arica", "tarapaca", "antofagasta", "atacama", "coquimbo", "valparaiso", "metropolitana", "o'higgins", "ohiggins",
  "libertador", "maule", "nuble", "biobio", "araucania", "los rios", "los lagos", "aysen", "magallanes"].find((t) => r.includes(t)) || "";
const comunaMarker = (s) => s.includes("comuna de") || s.includes("comunas de") || s.includes("provincia de");
function regionWide(win, rt) {
  if (!rt) return false;
  const i = win.indexOf(rt); if (i < 0) return false;
  const before = win.substring(Math.max(0, i - 75), i), after = win.substr(i, 95), cut = after.indexOf(" y ");
  return !(comunaMarker(before) || comunaMarker(cut < 0 ? after : after.substring(0, cut)));
}
function dateAnchors(text, dd, mes) {
  const res = [], needle = "de " + mes; let p = 0;
  while ((p = text.indexOf(needle, p)) >= 0) {
    const pre = text.substring(Math.max(0, p - 40), p), nums = [];
    for (let i = 0; i < pre.length;) {
      if (/\d/.test(pre[i])) { let v = 0, st = i; while (i < pre.length && /\d/.test(pre[i]) && i - st < 2) { v = v * 10 + +pre[i]; i++; } if (v >= 1 && v <= 31) nums.push(v); }
      else i++;
    }
    let covered = nums.includes(dd);
    if (!covered && pre.includes(" al ") && nums.length >= 2 && dd >= Math.min(...nums) && dd <= Math.max(...nums)) covered = true;
    if (covered) res.push(p);
    p += needle.length;
  }
  return res;
}
async function mineduc(loc) {
  const rt = regionToken(norm(loc.region || "")), cn = norm(loc.name || "");
  const MES = ["enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"];
  const s = stgo(), base = Date.UTC(s.y, s.m - 1, s.d), days = ["hoy", "manana", "pasado"].map((etiqueta, k) => {
    const d = new Date(base + k * 86400000); return { etiqueta, dd: d.getUTCDate(), mes: MES[d.getUTCMonth()], susp: false, fuente: "", titular: "" };
  });
  const arts = []; let oficial = false;
  try {
    const feed = await http("https://www.mineduc.cl/feed/", 8000); oficial = true;
    let pos = 0, fetched = 0;
    while (fetched < 3) {
      const a = feed.indexOf("<item", pos); if (a < 0) break;
      const b = feed.indexOf("</item>", a); if (b < 0) break;
      const item = feed.substring(a, b); pos = b + 7;
      const ni = norm(item); if (!(ni.includes("suspension") && ni.includes("clase"))) continue;
      const lm = /<link>([\s\S]*?)<\/link>/.exec(item), tm = /<title>([\s\S]*?)<\/title>/.exec(item);
      const link = lm ? lm[1].trim() : "", title = tm ? tm[1].replace(/<!\[CDATA\[|\]\]>/g, "").trim() : "";
      let text = ""; try { text = norm(htmlText(await http(link, 8000))); } catch (e) {}
      arts.push({ link, title, text }); fetched++;
    }
  } catch (e) {}
  for (const d of days) for (const ar of arts) {
    const tn = norm(ar.title);
    if (dateAnchors(tn, d.dd, d.mes).length) {
      if (cn.length >= 4 && tn.includes(cn)) d.susp = true; else if (regionWide(tn, rt)) d.susp = true;
    }
    if (!d.susp) for (const p of dateAnchors(ar.text, d.dd, d.mes)) {
      const win = ar.text.substr(Math.max(0, p - 140), 360);
      if (cn.length >= 4 && win.includes(cn)) d.susp = true; else if (regionWide(win, rt)) d.susp = true;
      if (d.susp) break;
    }
    if (d.susp) { d.fuente = ar.link; d.titular = ar.title; break; }
  }
  return { oficial, dias: days };
}
async function buildSuspension(loc, daily) {
  let peor = 0, cuando = null, motivo = "";
  const lbl = ["hoy", "mañana", "pasado mañana"];
  for (let i = 0; i < 3 && i < al(daily, "time"); i++) {
    const code = Math.trunc(an(daily, "weather_code", i)), gust = an(daily, "wind_gusts_10m_max", i), rain = an(daily, "precipitation_sum", i);
    const snow = (code >= 71 && code <= 77) || code === 85 || code === 86, storm = code >= 95;
    let r = 0;
    if (gust >= 90 || rain >= 60 || storm || (snow && rain >= 20)) r = 2; else if (gust >= 65 || rain >= 30 || snow) r = 1;
    if (r > peor) {
      peor = r; cuando = lbl[i];
      motivo = storm ? "tormenta eléctrica prevista" : snow ? `nieve (viento ${lround(gust)} km/h)` : `viento ${lround(gust)} km/h y ${lround(rain)} mm de lluvia`;
    }
  }
  const niveles = ["Sin aviso", "Posible suspensión por temporal", "Suspensión probable por temporal"];
  const m = await mineduc(loc), dia = m.dias.find((d) => d.susp);
  if (dia) {
    const ct = dia.etiqueta === "hoy" ? "hoy" : dia.etiqueta === "manana" ? "mañana" : "pasado mañana";
    return { nivel: `Clases suspendidas ${ct}`, riesgo: 2, cuando: ct, motivo: "informado por el Ministerio de Educación", oficial: true, fuente: dia.fuente || "https://www.mineduc.cl/" };
  }
  return { nivel: niveles[peor], riesgo: peor, cuando: peor ? cuando : null, motivo: peor ? motivo : "sin eventos severos previstos", oficial: m.oficial };
}
async function buildAmbiental(loc, daily) {
  const [a, tides, susp] = await Promise.all([airData(loc), buildTides(loc), buildSuspension(loc, daily)]);
  return { air: a.air, moon: buildMoon(), tides, restriccion: await buildRestriccion(loc, a.key), suspension: susp };
}

// ------------------------------------------------------------ análisis total
async function analyze(loc) {
  const w = await httpJson(`https://api.open-meteo.com/v1/forecast?latitude=${loc.lat.toFixed(4)}&longitude=${loc.lon.toFixed(4)}&timezone=America/Santiago` +
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure,pressure_msl,cloud_cover,dew_point_2m" +
    "&hourly=temperature_2m,weather_code,precipitation_probability,visibility,is_day" +
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,precipitation_probability_max,precipitation_sum,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant" +
    "&forecast_days=10&wind_speed_unit=kmh", 20000);
  if (!w || !w.current) {
    const c = cacheLoad(); if (c) return c;
    throw new Error("sin conexión y sin datos en caché");
  }
  const cur = w.current, day = w.daily, [icon, text, cls] = wmo(Math.trunc(num(cur, "weather_code")));
  const out = {
    location: { name: loc.name, region: loc.region, lat: loc.lat, lon: loc.lon, comuna: !!loc.is_comuna, rm_restr: !!loc.rm_restr },
    current: {
      temp: lround(num(cur, "temperature_2m")), feels: lround(num(cur, "apparent_temperature")), icon, text, class: cls, is_day: num(cur, "is_day") !== 0,
      humidity: Math.trunc(num(cur, "relative_humidity_2m")), wind: lround(num(cur, "wind_speed_10m")), gust: lround(num(cur, "wind_gusts_10m")),
      wind_dir: num(cur, "wind_direction_10m"), pressure: lround(has(cur, "pressure_msl") ? num(cur, "pressure_msl") : num(cur, "surface_pressure")),
      cloud: Math.trunc(num(cur, "cloud_cover")), precip: num(cur, "precipitation"), dew: has(cur, "dew_point_2m") ? lround(num(cur, "dew_point_2m")) : null,
    },
    today: { max: lround(an(day, "temperature_2m_max", 0)), min: lround(an(day, "temperature_2m_min", 0)), uv: an(day, "uv_index_max", 0),
      sunrise: as(day, "sunrise", 0), sunset: as(day, "sunset", 0) },
    rain: buildRain(day), context: buildContext(w, cur, day), ambiental: await buildAmbiental(loc, day),
    fetched: Math.floor(Date.now() / 1000), raw: w,
  };
  const s = JSON.stringify(out); cacheSave(s); return s;
}

// Estado del Metro de Santiago (API oficial de metro.cl) para el recuadro «Metro» — port 1:1 de
// c/clima_data.c:clima_metro_json (Kotone 2026-10-31). Devuelve {"ok":bool,"problemas":[{"linea":"L2","texto":"…"}]}.
async function metroJson() {
  const n = await httpJson("https://www.metro.cl/api/estadoRedDetalle.php");
  const ok = !!n && typeof n === "object" && !Array.isArray(n);
  const problemas = [];
  if (ok) {
    for (const k of Object.keys(n)) {
      const L = n[k];
      if (!L || typeof L !== "object" || Array.isArray(L)) continue;
      const etq = "L" + k.slice(1).toUpperCase();
      if (String(L.estado ?? "1") !== "1") problemas.push({ linea: etq, texto: L.mensaje_app ?? "Con problemas" });
      for (const e of Array.isArray(L.estaciones) ? L.estaciones : []) {
        if (!e || String(e.estado ?? "1") === "1") continue;
        problemas.push({ linea: etq, texto: `${e.nombre ?? ""}: ${e.descripcion_app ?? "Con problemas"}` });
      }
    }
  }
  return JSON.stringify({ ok, problemas });
}

module.exports = { configure, analyze, cacheLoad, searchJson, geocode, currentLocation, locFromCoords, fold, metroJson };

// Pruebas del núcleo de datos de Android (android/web/clima-android.js) sin navegador ni teléfono:
// se carga en un contexto aislado con un puente falso. Uso: node tests/clima-android.test.js
"use strict";
const fs = require("fs"), vm = require("vm"), path = require("path"), assert = require("assert");
const SRC = fs.readFileSync(path.join(__dirname, "../android/web/clima-android.js"), "utf8");

function iso(d) { return d.toISOString().slice(0, 10); }
function forecast() {
  const base = new Date(); const days = [], hours = [];
  for (let i = 0; i < 10; i++) { const d = new Date(base.getTime() + i * 864e5); days.push(iso(d)); }
  for (let h = 0; h < 48; h++) hours.push(days[0] + "T" + String(h % 24).padStart(2, "0") + ":00");
  const rep = (v, n) => Array(n).fill(v);
  return {
    current: { temperature_2m: 18.4, relative_humidity_2m: 55, apparent_temperature: 17, is_day: 1, precipitation: 0, weather_code: 1,
      wind_speed_10m: 9, wind_direction_10m: 200, wind_gusts_10m: 15, surface_pressure: 950, pressure_msl: 1015, cloud_cover: 10, dew_point_2m: 8 },
    hourly: { time: hours, temperature_2m: rep(18, 48), weather_code: rep(1, 48), precipitation_probability: rep(0, 48), visibility: rep(24000, 48), is_day: rep(1, 48) },
    daily: { time: days, weather_code: rep(1, 10), temperature_2m_max: rep(24, 10), temperature_2m_min: rep(9, 10),
      sunrise: days.map(d => d + "T06:40"), sunset: days.map(d => d + "T20:10"), uv_index_max: rep(7, 10),
      precipitation_probability_max: rep(0, 10), precipitation_sum: rep(0, 10), wind_speed_10m_max: rep(20, 10),
      wind_gusts_10m_max: rep(30, 10), wind_direction_10m_dominant: rep(180, 10) },
  };
}

// Crea un entorno nuevo. red(url) devuelve el cuerpo, null (falla) o "colgar" (no responde nunca).
function entorno({ red, cache, oficialColgado }) {
  const out = { datos: [], errores: [], guardado: null };
  const waiting = [];
  const A = {
    get(id, url) { const r = red(url); if (r === "colgar") return; setTimeout(() => ctx.window.__climaHttp(id, r != null, r == null ? "" : r), 0); },
    oficial(id) { if (oficialColgado) return; setTimeout(() => ctx.window.__climaHttp(id, true, "{}"), 0); },
    locate(id) { setTimeout(() => ctx.window.__climaLoc(id, null, null), 0); },
    asset() { return JSON.stringify([{ name: "Santiago", region: "Región Metropolitana", lat: -33.45, lon: -70.66, rm_restr: true }]); },
    cacheLoad() { return cache ? JSON.stringify(cache) : ""; },
    cacheSave(j) { out.guardado = j; },
    hayBici() { return false; }, hayMetro() { return false; }, metro(id) { setTimeout(() => ctx.window.__climaHttp(id, false, ""), 0); },
  };
  const window = { ClimaAndroid: A, __climaTopeMs: 150, addEventListener() {}, onClimaData: d => out.datos.push(d), onError: m => out.errores.push(m),
    onSearchResults() {} };
  const ctx = { window, document: { hidden: false }, console: { info() {}, log() {}, warn() {}, error() {} }, setTimeout, setInterval() {}, clearTimeout, Promise, JSON, Math, Date };
  vm.createContext(ctx); vm.runInContext(SRC, ctx);
  out.pedir = (accion, extra) => ctx.window.webkit.messageHandlers.bridge.postMessage(JSON.stringify(Object.assign({ action: accion }, extra)));
  return out;
}
const esperar = (ms) => new Promise(r => setTimeout(r, ms));
const redOk = url => /api\.open-meteo\.com\/v1\/forecast/.test(url) ? JSON.stringify(forecast()) : null;

(async () => {
  // 1. pronóstico normal: se entrega y se guarda; las fuentes secundarias caídas no lo impiden
  let e = entorno({ red: redOk });
  e.pedir("city", { name: "Santiago", region: "RM", lat: -33.45, lon: -70.66 });
  await esperar(400);
  assert.strictEqual(e.datos.length, 1, "debe entregar datos");
  assert.strictEqual(e.errores.length, 0);
  assert.ok(e.guardado, "debe guardar el caché");
  const d = e.datos[0];
  assert.strictEqual(d.current.temp, 18); assert.ok(d.context.text.length > 5);
  assert.strictEqual(d.ambiental.air, null, "aire caído = sin dato, no un valor inventado");

  // 2. fuentes oficiales colgadas: el pronóstico aparece igual tras el tope
  e = entorno({ red: url => redOk(url) || "colgar", oficialColgado: true });
  e.pedir("city", { name: "Santiago", region: "RM", lat: -33.45, lon: -70.66 });
  await esperar(500);
  assert.strictEqual(e.datos.length, 1, "no debe quedar cargando por fuentes secundarias");
  assert.strictEqual(e.datos[0].ambiental.suspension.oficial, false, "sin dato oficial no se afirma nada oficial");

  // 3. sin red y caché de OTRO lugar: error claro, no datos de la ciudad equivocada
  const previo = { location: { name: "Santiago", lat: -33.45, lon: -70.66 }, fetched: 1 };
  e = entorno({ red: () => null, cache: previo });
  e.pedir("city", { name: "Punta Arenas", region: "Magallanes", lat: -53.16, lon: -70.91 });
  await esperar(300);
  assert.strictEqual(e.datos.length, 0, "no debe mostrar el clima de otra ciudad");
  assert.strictEqual(e.errores.length, 1);
  assert.ok(/Punta Arenas/.test(e.errores[0]));

  // 4. sin red y caché del MISMO lugar: se muestra marcado como guardado
  e = entorno({ red: () => null, cache: previo });
  e.pedir("city", { name: "Santiago", region: "RM", lat: -33.46, lon: -70.65 });
  await esperar(300);
  assert.strictEqual(e.datos.length, 1); assert.strictEqual(e.datos[0].cached, true);

  // 5. sin red y sin caché
  e = entorno({ red: () => null });
  e.pedir("city", { name: "Santiago", region: "RM", lat: -33.45, lon: -70.66 });
  await esperar(300);
  assert.strictEqual(e.errores.length, 1);

  // 6. la interfaz compartida no bloquea el zoom
  const html = fs.readFileSync(path.join(__dirname, "../ui/index.html"), "utf8");
  assert.ok(!/user-scalable\s*=\s*no|maximum-scale/i.test(html), "el viewport no debe impedir el zoom");
  assert.ok(/id="q"[^>]*aria-label=/.test(html), "el buscador necesita nombre accesible");
  console.log("clima-android: 6 grupos de pruebas OK");
})().catch(err => { console.error(err); process.exit(1); });

// Clima para Windows x64 — host Electron. Equivale a c/clima.c (GTK4+WebKit): carga la
// misma interfaz (ui/), inyecta los datos, gestiona ubicación automática y refresco.
"use strict";
const { app, BrowserWindow, ipcMain, Menu, shell } = require("electron");
const path = require("path");
const data = require("./data");

app.disableHardwareAcceleration();                 // sin aceleración 3D (decisión de diseño, igual que en Linux)
if (!app.requestSingleInstanceLock()) app.quit();

const ROOT = path.join(__dirname, "..", "app");
const REFRESH_MS = 15 * 60 * 1000;
let win = null, loc = null, busy = false, manual = false, located = false, geoPending = false;

function inject(fn, arg) {
  if (!win || win.isDestroyed()) return;
  win.webContents.executeJavaScript(`window.${fn}(${arg})`).catch(() => {});
}
async function startFetch(l) {
  if (busy || !l) return;
  busy = true;
  try {
    const json = await data.analyze(l);
    loc = l;
    inject("onClimaData", json);
    if (!located && !manual) { located = true; setImmediate(doLocate); }   // tras pintar: ubicación automática
  } catch (e) {
    inject("onError", JSON.stringify(String(e.message || e)));
  } finally { busy = false; }
}
async function doLocate() {
  if (geoPending) return;
  if (busy) { setTimeout(doLocate, 2000); return; }
  geoPending = true;
  try { await startFetchAfterLocate(); } finally { geoPending = false; }
}
async function startFetchAfterLocate() { const l = await data.currentLocation(); await startFetch(l); }

ipcMain.on("bridge", async (_e, msg) => {
  let o; try { o = JSON.parse(msg); } catch (e) { return; }
  if (o.action === "search") inject("onSearchResults", JSON.stringify(data.searchJson(o.q || "", 8)));
  else if (o.action === "city") {
    const near = await data.geocode(o.name);
    manual = true;
    startFetch({ name: o.name, region: o.region || "", lat: o.lat, lon: o.lon, is_comuna: true, rm_restr: !!(near && near.rm_restr) });
  } else if (o.action === "locate") { manual = false; doLocate(); }
  else if (o.action === "refresh") { if (loc) startFetch({ ...loc }); else doLocate(); }
  // recuadro «Metro»: estado oficial de metro.cl (igual que c/clima.c: metro_hilo → window.onMetro)
  else if (o.action === "metro") { data.metroJson().then((j) => inject("onMetro", j)).catch(() => {}); }
  // abrir la app Metro Chile (Store: protocolo propio); si no está instalada, el sitio oficial
  else if (o.action === "abrirMetro") { shell.openExternal("ms-windows-store://pdp/?productid=9N6Z8F2SJ3V5").catch(() => shell.openExternal("https://www.metro.cl")); }
});

function onLoad() {
  if (process.env.CLIMA_CAPTURA) return;           // modo captura: sin caché ni ubicación automática (evita carreras)
  const cached = data.cacheLoad();                 // pinta al tiro el último dato
  if (cached) {
    inject("onClimaData", cached);
    try {
      const l = JSON.parse(cached).location;
      if (l && !loc) { loc = { name: l.name, region: l.region || "", lat: l.lat, lon: l.lon, is_comuna: !!l.comuna, rm_restr: !!l.rm_restr }; startFetch({ ...loc }); }
    } catch (e) {}
  }
  if (!loc && !busy) doLocate();                   // primer arranque sin caché
}

function createWindow() {
  win = new BrowserWindow({
    width: 430, height: 880, minWidth: 360, minHeight: 560, title: "Clima", backgroundColor: "#1c2a40",
    icon: path.join(ROOT, "clima.png"), autoHideMenuBar: true,
    webPreferences: { preload: path.join(__dirname, "preload.js"), contextIsolation: false, sandbox: true, nodeIntegration: false, spellcheck: false },
  });
  Menu.setApplicationMenu(null);
  win.webContents.setWindowOpenHandler(({ url }) => { if (/^https?:/.test(url)) shell.openExternal(url); return { action: "deny" }; });
  win.webContents.on("will-navigate", (e, url) => { if (!url.startsWith("file:")) { e.preventDefault(); if (/^https?:/.test(url)) shell.openExternal(url); } });
  win.webContents.on("did-finish-load", onLoad);
  win.webContents.on("before-input-event", (_e, i) => {
    if (i.type !== "keyDown") return;
    if (i.key === "F11") win.setFullScreen(!win.isFullScreen());
    else if (i.key === "Escape" && win.isFullScreen()) win.setFullScreen(false);
    else if (i.key === "F5") { if (loc) startFetch({ ...loc }); }
  });
  win.loadFile(path.join(ROOT, "ui", "index.html"));
}

// ── Modo captura (solo con CLIMA_CAPTURA=<carpeta>): capturas REALES de Windows para la ficha de la tienda.
//    Ventana oculta del tamaño pedido (CLIMA_CAPTURA_TAM, por defecto 1920x1080), datos reales de cada comuna,
//    espera a que el cielo animado esté en marcha y recorre las secciones con scroll. No afecta el uso normal.
async function modoCaptura(dir) {
  const fs = require("fs");
  const [W, H] = (process.env.CLIMA_CAPTURA_TAM || "1920x1080").split("x").map(Number);
  const comunas = (process.env.CLIMA_CAPTURA_COMUNAS || "Santiago,San Pedro de Atacama,Punta Arenas,Valparaíso").split(",");
  fs.mkdirSync(dir, { recursive: true });
  win.setContentSize(W, H);
  await win.webContents.insertCSS("::-webkit-scrollbar{display:none!important}");   // ficha limpia, sin barra
  const esperar = (ms) => new Promise((r) => setTimeout(r, ms));
  const guardar = async (nombre) => { const img = await win.webContents.capturePage(); fs.writeFileSync(path.join(dir, nombre), img.toPNG()); };
  await esperar(1500);
  for (let i = 0; i < comunas.length; i++) {
    const n = comunas[i].trim();
    const near = await data.geocode(n);
    if (!near) continue;
    manual = true;
    while (busy) await esperar(300);
    await startFetch({ name: n, region: near.region || "", lat: near.lat, lon: near.lon, is_comuna: true, rm_restr: !!near.rm_restr });
    // comprobar que la pantalla muestra ESTA comuna y que el Metro ya respondió (o no aplica) antes de capturar
    for (let t = 0; t < 40; t++) {
      const ok = await win.webContents.executeJavaScript(
        `document.body.innerText.includes(${JSON.stringify(n)}) && !document.body.innerText.includes("Revisando el estado de la red")`);
      if (ok) break;
      await esperar(500);
    }
    await esperar(4000);                                              // cielo animado en marcha
    const slug = n.normalize("NFD").replace(/[^\w]+/g, "-").toLowerCase();
    await win.webContents.executeJavaScript("window.scrollTo(0,0)");
    await esperar(800);
    await guardar(`${String(i + 1).padStart(2, "0")}-${slug}-inicio.png`);
    if (i === 0) {                                                    // secciones completas en la primera comuna
      const alto = await win.webContents.executeJavaScript("document.documentElement.scrollHeight");
      for (let y = H * 0.85, k = 1; y < alto - 10 && k <= 5; y += H * 0.85, k++) {
        await win.webContents.executeJavaScript(`window.scrollTo({top:${Math.round(y)},behavior:"instant"})`);
        await esperar(1200);
        await guardar(`${String(i + 1).padStart(2, "0")}-${slug}-seccion${k}.png`);
      }
    }
  }
  app.quit();
}

app.whenReady().then(() => {
  data.configure({ dataDir: path.join(ROOT, "data"), cacheFile: path.join(app.getPath("userData"), "last.json") });
  createWindow();
  if (process.env.CLIMA_CAPTURA) { win.webContents.once("did-finish-load", () => modoCaptura(process.env.CLIMA_CAPTURA)); return; }
  setInterval(() => { if (!manual) doLocate(); else if (loc) startFetch({ ...loc }); }, REFRESH_MS);
});
app.on("second-instance", () => { if (win) { if (win.isMinimized()) win.restore(); win.focus(); } });
app.on("window-all-closed", () => app.quit());

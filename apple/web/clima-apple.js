// Clima para macOS e iOS — implementa window.ClimaAndroid (el contrato que usa clima-android.js) sobre el puente
// nativo de WKWebView («nativo»): red, ubicación y apertura de enlaces en Swift; caché en localStorage; las fuentes
// OFICIALES (Mineduc y airerm) portadas 1:1 de Oficial.java (que a su vez es port de los scrapers C++ de escritorio).
// Debe cargarse ANTES de clima-android.js (que reemplaza window.webkit). Desarrollado por Javiera Navarrete.
"use strict";
(function () {
  const N = window.webkit.messageHandlers.nativo;            // se captura antes de que clima-android.js lo tape
  const nat = o => N.postMessage(JSON.stringify(o));
  let seq = 0; const esperas = {};
  window.__nativoHttp = (id, ok, body) => { const f = esperas[id]; delete esperas[id]; if (f) f(ok ? body : null); };
  const getNativo = url => new Promise(r => { const id = "o" + (++seq); esperas[id] = r; nat({ a: "get", id, url }); });

  // ------------------------------------------------------------------ Oficial (port de Oficial.java)
  const MESES = ["enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"];
  const REGIONES = ["arica", "tarapaca", "antofagasta", "atacama", "coquimbo", "valparaiso", "metropolitana", "o'higgins", "ohiggins",
    "libertador", "maule", "nuble", "biobio", "araucania", "los rios", "los lagos", "aysen", "magallanes"];
  const unescape = s => s.replace(/&(#x?[0-9a-fA-F]+|[a-zA-Z]+);/g, (m, e) => {
    if (/^#x/i.test(e)) return String.fromCodePoint(parseInt(e.slice(2), 16));
    if (e[0] === "#") return String.fromCodePoint(parseInt(e.slice(1), 10));
    const t = { amp: "&", lt: "<", gt: ">", quot: '"', nbsp: " ", laquo: "«", raquo: "»", ntilde: "ñ", Ntilde: "Ñ" }[e];
    if (t) return t;
    if (e.length === 6 && e.endsWith("acute")) return (e[0] + "́").normalize("NFC");
    return m;
  });
  const norm = s => s.normalize("NFD").toLowerCase().replace(/[^\x00-\x7f]/g, "");
  const sinBloques = h => h.replace(/<(script|style)\b[\s\S]*?(<\/\1\s*>|$)/gi, " ");
  const sinEtiquetas = h => { let o = "", d = false; for (const c of h) { if (c === "<") d = true; else if (c === ">") d = false; else if (!d) o += c; } return o; };
  const enSantiago = mas => {   // fecha de Santiago (sin depender de la zona del equipo)
    const p = new Intl.DateTimeFormat("en-CA", { timeZone: "America/Santiago", year: "numeric", month: "2-digit", day: "2-digit" }).format(new Date(Date.now() + mas * 86400000));
    const [y, m, d] = p.split("-").map(Number); return { y, m, d, iso: p };
  };
  const regionToken = r => REGIONES.find(t => r.includes(t)) || "";
  const marcaComuna = s => s.includes("comuna de") || s.includes("comunas de") || s.includes("provincia de");
  function regionWide(win, rt) {
    if (!rt) return false;
    const i = win.indexOf(rt); if (i < 0) return false;
    const antes = win.slice(Math.max(0, i - 75), i), despues = win.slice(i, i + 95), cut = despues.indexOf(" y ");
    return !(marcaComuna(antes) || marcaComuna(cut < 0 ? despues : despues.slice(0, cut)));
  }
  function anclas(texto, dd, mes) {
    const res = [], aguja = "de " + mes; let p = 0;
    while ((p = texto.indexOf(aguja, p)) >= 0) {
      const pre = texto.slice(Math.max(0, p - 40), p), nums = [];
      for (let i = 0; i < pre.length;) {
        if (/\d/.test(pre[i])) { let v = 0; const st = i; while (i < pre.length && /\d/.test(pre[i]) && i - st < 2) { v = v * 10 + (+pre[i]); i++; } if (v >= 1 && v <= 31) nums.push(v); }
        else i++;
      }
      let cub = nums.includes(dd);
      if (!cub && pre.includes(" al ") && nums.length >= 2) cub = dd >= Math.min(...nums) && dd <= Math.max(...nums);
      if (cub) res.push(p);
      p += aguja.length;
    }
    return res;
  }
  const etiqueta = (item, t) => {
    const a = item.indexOf("<" + t + ">"); if (a < 0) return "";
    const b = item.indexOf("</" + t + ">", a); let v = b < 0 ? "" : item.slice(a + t.length + 2, b).trim();
    if (v.startsWith("<![CDATA[") && v.endsWith("]]>")) v = v.slice(9, -3);
    return v;
  };
  async function suspension(region, comuna) {
    const rt = regionToken(norm(region || "")), cn = norm(comuna || ""), etq = ["hoy", "manana", "pasado"], arts = [];
    const feed = await getNativo("https://www.mineduc.cl/feed/");
    const oficial = !!feed;
    if (oficial) {
      let pos = 0;
      while (arts.length < 3) {
        const a = feed.indexOf("<item", pos); if (a < 0) break;
        const b = feed.indexOf("</item>", a); if (b < 0) break;
        const item = feed.slice(a, b); pos = b + 7;
        const ni = norm(item);
        if (!(ni.includes("suspension") && ni.includes("clase"))) continue;
        const link = unescape(etiqueta(item, "link")), titulo = unescape(etiqueta(item, "title"));
        const pag = link.startsWith("https://") ? await getNativo(link) : null;
        arts.push([link, titulo, pag ? norm(unescape(sinEtiquetas(sinBloques(pag)))) : ""]);
      }
    }
    const dias = [];
    for (let k = 0; k < 3; k++) {
      const f = enSantiago(k), dd = f.d, mes = MESES[f.m - 1];
      let susp = false, alcance = "", fuente = "", titular = "";
      for (const [link, tit, txt] of arts) {
        const tn = norm(tit);
        if (anclas(tn, dd, mes).length) {
          if (cn.length >= 4 && tn.includes(cn)) { susp = true; alcance = "comuna"; }
          else if (regionWide(tn, rt)) { susp = true; alcance = "region"; }
        }
        if (!susp) for (const p of anclas(txt, dd, mes)) {
          const s = Math.max(0, p - 140), win = txt.slice(s, s + 360);
          if (cn.length >= 4 && win.includes(cn)) { susp = true; alcance = "comuna"; }
          else if (regionWide(win, rt)) { susp = true; alcance = "region"; }
          if (susp) break;
        }
        if (susp) { fuente = link; titular = tit; break; }
      }
      dias.push({ etiqueta: etq[k], fecha: f.iso, suspendido: susp, alcance, fuente, titular });
    }
    return { oficial, dias };
  }
  async function restriccion() {
    const html = await getNativo("https://airerm.mma.gob.cl/"), f = enSantiago(0);
    let oficial = false, episodio = "Normal";
    if (html) {
      const t = norm(unescape(sinEtiquetas(sinBloques(html)))).replace(/\s+/g, " "), hoy = f.d + " de " + MESES[f.m - 1];
      const p = t.indexOf("restriccion vehicular para hoy");
      if (p >= 0) {
        const win = t.slice(p, p + 1000);
        if (win.includes(hoy)) { oficial = true; if (win.includes("emergencia ambiental")) episodio = "Emergencia"; else if (win.includes("preemergencia")) episodio = "Preemergencia"; }
      }
    }
    return { oficial, fecha: f.iso, episodio };
  }

  // ------------------------------------------------------------------ contrato ClimaAndroid
  window.ClimaAndroid = {
    get(id, url) { nat({ a: "get", id: String(id), url, destino: "clima" }); },
    locate(id) { nat({ a: "locate", id: String(id) }); },
    asset(ruta) { return (window.__CLIMA_ASSETS || {})[ruta] || ""; },          // inyectado por Swift al cargar
    cacheLoad() { try { return localStorage.getItem("clima.cache") || ""; } catch (e) { return ""; } },
    cacheSave(s) { try { localStorage.setItem("clima.cache", s); } catch (e) { } },
    oficial(id, region, comuna, restr) {
      Promise.all([suspension(region, comuna), restr ? restriccion() : Promise.resolve(null)])
        .then(([s, r]) => window.__climaHttp(id, true, JSON.stringify({ suspension: s, restriccion: r })))
        .catch(() => window.__climaHttp(id, true, "{}"));
    },
    hayMetro() { return false; }, hayBici() { return false; },
    abrirMetro() { }, abrirBici() { },
    metro(id) { window.__climaHttp(id, false, ""); },
  };
})();

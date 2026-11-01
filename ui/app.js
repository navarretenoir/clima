// Clima — renderizador. Los datos llegan del host en C (c/clima.c) vía window.onClimaData.
// Interacción: búsqueda de ciudades, detalle por día, pull-to-refresh elástico. Kotone 2026-07-16.
"use strict";

const WMO = {
 0:["☀️","🌙","Despejado","clear"],1:["🌤️","🌙","Mayormente despejado","clear"],
 2:["⛅","☁️","Parcialmente nublado","cloud"],3:["☁️","☁️","Nublado","cloud"],
 45:["🌫️","🌫️","Niebla","fog"],48:["🌫️","🌫️","Niebla con escarcha","fog"],
 51:["🌦️","🌧️","Llovizna ligera","rain"],53:["🌦️","🌧️","Llovizna","rain"],55:["🌧️","🌧️","Llovizna intensa","rain"],
 56:["🌧️","🌧️","Llovizna helada","rain"],57:["🌧️","🌧️","Llovizna helada","rain"],
 61:["🌧️","🌧️","Lluvia ligera","rain"],63:["🌧️","🌧️","Lluvia","rain"],65:["🌧️","🌧️","Lluvia fuerte","rain"],
 66:["🌧️","🌧️","Lluvia helada","rain"],67:["🌧️","🌧️","Lluvia helada","rain"],
 71:["🌨️","🌨️","Nieve ligera","snow"],73:["🌨️","🌨️","Nieve","snow"],75:["❄️","❄️","Nieve fuerte","snow"],77:["🌨️","🌨️","Granizo fino","snow"],
 80:["🌦️","🌧️","Chubascos ligeros","rain"],81:["🌧️","🌧️","Chubascos","rain"],82:["⛈️","⛈️","Chubascos violentos","storm"],
 85:["🌨️","🌨️","Chubascos de nieve","snow"],86:["❄️","❄️","Chubascos de nieve","snow"],
 95:["⛈️","⛈️","Tormenta","storm"],96:["⛈️","⛈️","Tormenta con granizo","storm"],99:["⛈️","⛈️","Tormenta fuerte","storm"],
};
// Íconos del clima estilo SF Symbols multicolor (degradados compartidos en index.html: gSun, gCloud…).
// Propios, en SVG: se ven igual en Linux y Windows y no dependen de fuentes de emoji.
const CLOUD="M8.6 26h14.6a5.6 5.6 0 0 0 1.1-11.1A8.2 8.2 0 0 0 8.6 13.2 6.4 6.4 0 0 0 8.6 26z";
const SV={
 sun:'<circle cx="16" cy="16" r="6.4" fill="url(#gSun)"/><g stroke="#FFC21A" stroke-width="2.1" stroke-linecap="round"><path d="M16 3.2v2.9M16 25.9v2.9M3.2 16h2.9M25.9 16h2.9M6.95 6.95l2.05 2.05M23 23l2.05 2.05M6.95 25.05L9 23M23 9l2.05-2.05"/></g>',
 moon:'<path d="M22.6 19.8A9.6 9.6 0 1 1 12.2 5.4a7.8 7.8 0 0 0 10.4 14.4z" fill="url(#gMoon)"/><circle cx="13" cy="17" r="1.3" fill="#CFC8B0" opacity=".7"/><circle cx="16.5" cy="22" r=".9" fill="#CFC8B0" opacity=".6"/>',
 cl:(g)=>`<path d="${CLOUD}" fill="url(#${g})"/>`,
 drops:(n)=>{let s='';const xs=n===2?[12.5,19.5]:[10.5,16,21.5];for(const x of xs)s+=`<path d="M${x} 25.6c0 0-2 2.7-2 3.9a2 2 0 0 0 4 0c0-1.2-2-3.9-2-3.9z" fill="#3FB8FF"/>`;return s;},
 flakes:'<g fill="#fff"><circle cx="11" cy="27.3" r="1.6"/><circle cx="16.5" cy="29.5" r="1.6"/><circle cx="22" cy="27.3" r="1.6"/></g>',
 bolt:'<path d="M17.8 20.5l-5.1 6.8h3.9l-1.5 4.9 6.3-7.6h-4.1l1.6-4.1z" fill="url(#gBolt)" stroke="#E89400" stroke-width=".4" stroke-linejoin="round"/>',
 fog:'<g stroke="#E6EBF2" stroke-width="2.1" stroke-linecap="round"><path d="M6 25.2h20M9 29h15"/></g>',
};
function wsvg(code,isDay){
  const body=isDay?SV.sun:SV.moon, cl=SV.cl;
  const up=(s,dy,sc)=>`<g transform="translate(${16*(1-sc)},${16*(1-sc)+dy}) scale(${sc})">${s}</g>`;
  let k;
  if(code===0) k=body;
  else if(code===1) k=`<g transform="translate(-3,-3) scale(.85)">${body}</g>`+up(cl("gCloud"),6,.62);
  else if(code===2) k=`<g transform="translate(-1,-4) scale(.8)">${body}</g>`+up(cl("gCloud"),3,.86);
  else if(code===3) k=up(cl("gCloudG"),-5,.78)+up(cl("gCloud"),2,.92);
  else if(code===45||code===48) k=up(cl("gCloudG"),-5,.9)+SV.fog;
  else if(code>=51&&code<=57) k=up(cl("gCloudG"),-5,.95)+SV.drops(2);
  else if(code===80) k=`<g transform="translate(-1,-6) scale(.7)">${body}</g>`+up(cl("gCloud"),-3,.88)+SV.drops(2);
  else if((code>=61&&code<=67)||code===81) k=up(cl("gCloudD"),-5,.95)+SV.drops(3);
  else if((code>=71&&code<=77)||code===85||code===86) k=up(cl("gCloudG"),-5,.95)+SV.flakes;
  else if(code===82||code>=95) k=up(cl("gCloudD"),-6,.95)+SV.bolt;
  else k=cl("gCloud");
  return `<svg viewBox="0 0 32 32" class="wi" aria-hidden="true">${k}</svg>`;
}
// Escala de color por temperatura ABSOLUTA (como las barras del Tiempo de Apple).
const TSCALE=[[-10,[106,91,255]],[0,[62,155,255]],[8,[79,209,232]],[14,[95,211,141]],[20,[245,211,63]],[26,[255,159,10]],[32,[255,69,58]]];
function tcolor(v){
  if(v<=TSCALE[0][0]) return `rgb(${TSCALE[0][1]})`;
  for(let i=1;i<TSCALE.length;i++){const[a,ca]=TSCALE[i-1],[b,cb]=TSCALE[i];
    if(v<=b){const f=(v-a)/(b-a);return `rgb(${ca.map((x,k)=>Math.round(x+(cb[k]-x)*f)).join(",")})`;}}
  return `rgb(${TSCALE[TSCALE.length-1][1]})`;
}
function tgrad(lo,hi){const m=(lo+hi)/2;return `linear-gradient(90deg,${tcolor(lo)},${tcolor(m)},${tcolor(hi)})`;}
// íconos de cabecera de tarjeta
const HIC={
 clock:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><circle cx="8" cy="8" r="6.2"/><path d="M8 4.6V8l2.4 1.5" stroke-linecap="round"/></svg>',
 cal:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="2" y="3" width="12" height="11" rx="2.2"/><path d="M2 6.5h12M5.2 1.6v2.6M10.8 1.6v2.6" stroke-linecap="round"/></svg>',
};
const hic=k=>`<span class="hic">${HIC[k]}</span>`;
function wmo(code,isDay){const m=WMO[code]||["☁️","☁️","—","cloud"];return[wsvg(code,isDay),m[2],m[3]];}
const R=x=>Math.round(x);
const WD=["dom","lun","mar","mié","jue","vie","sáb"];
const WDL=["domingo","lunes","martes","miércoles","jueves","viernes","sábado"];
function dayShort(iso,i){if(i===0)return"Hoy";return WD[new Date(iso+"T00:00").getDay()];}
function dayLong(iso,i){if(i===0)return"Hoy";return WDL[new Date(iso+"T00:00").getDay()];}
function windDir(d){return["N","NE","E","SE","S","SO","O","NO"][Math.round(d/45)%8];}
function hourLabel(dt,isNow){ // 12h con a.m./p.m. (sin "h" sobrante), una sola línea
  if(isNow) return "Ahora";
  let h=dt.getHours(); const ap=h<12?"a.m.":"p.m."; let h12=h%12; if(h12===0)h12=12;
  return h12+" "+ap;
}
// también comillas: esc() se usa dentro de atributos (data-name="…") del buscador
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function hm(iso){const x=new Date(iso);return x.getHours()+":"+String(x.getMinutes()).padStart(2,"0");}  // «6:43», compacto como iOS
// Hora del DATO, no del render: `fetched` (epoch s) lo estampa el host al consultar y
// queda guardado en el caché. Si el dato está viejo (offline) hay que decirlo, no disimularlo.
function stamp(fetched){
  if(!fetched) return "—";
  const d=new Date(fetched*1000), mins=Math.round((Date.now()-d.getTime())/60000);
  const t=d.toLocaleTimeString("es-CL",{hour:"2-digit",minute:"2-digit"});
  if(mins>=1440) return `${t} · hace ${Math.floor(mins/1440)} d`;
  if(mins>=60)   return `${t} · hace ${Math.floor(mins/60)} h`;
  return t;
}

// íconos monocromáticos estilo SF Symbols para los headers de los tiles (línea iOS/macOS)
const ICONS={
 uv:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><circle cx="8" cy="8" r="3"/><path d="M8 1v1.6M8 13.4V15M1 8h1.6M13.4 8H15M3 3l1.1 1.1M11.9 11.9L13 13M13 3l-1.1 1.1M4.1 11.9L3 13"/></svg>',
 sunset:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M1.5 13h13"/><path d="M5 13a3 3 0 0 1 6 0"/><path d="M8 1.5V4M8 7l-1.6-1.6M8 7l1.6-1.6" transform="translate(0 -.5)"/></svg>',
 wind:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M2 5.5h7a1.8 1.8 0 1 0-1.8-1.8"/><path d="M2 8.5h9.5a1.9 1.9 0 1 1-1.9 1.9"/><path d="M2 11.5h5.5"/></svg>',
 drop:'<svg viewBox="0 0 16 16" fill="currentColor"><path d="M8 1.5s4 4.6 4 7.4A4 4 0 0 1 4 8.9C4 6.1 8 1.5 8 1.5z"/></svg>',
 thermo:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><path d="M6.2 2.6a1.8 1.8 0 0 1 3.6 0v6.1a3 3 0 1 1-3.6 0z" stroke-linejoin="round"/><circle cx="8" cy="11.4" r="1.4" fill="currentColor" stroke="none"/></svg>',
 eye:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><path d="M1.5 8S4 3.5 8 3.5 14.5 8 14.5 8 12 12.5 8 12.5 1.5 8 1.5 8z"/><circle cx="8" cy="8" r="1.9"/></svg>',
 gauge:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M3 12a6 6 0 1 1 10 0"/><path d="M8 8.5l3-2.2"/><circle cx="8" cy="9" r="1" fill="currentColor" stroke="none"/></svg>',
 dew:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><path d="M8 2.5S11.5 6.4 11.5 9A3.5 3.5 0 0 1 4.5 9C4.5 6.4 8 2.5 8 2.5z"/><path d="M6.4 9a1.6 1.6 0 0 0 1.6 1.6" stroke-linecap="round"/></svg>',
 aqi:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M2 5.5h8.5a1.8 1.8 0 1 0-1.8-1.8"/><path d="M2 8.5h6"/><path d="M2 11.2h9a1.7 1.7 0 1 1-1.7 1.7"/></svg>',
 car:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2"><path d="M2 10.5v-2l1.4-3.2A1.5 1.5 0 0 1 4.8 4.3h6.4a1.5 1.5 0 0 1 1.4.9L14 8.5v2" stroke-linejoin="round"/><path d="M2 10.5h12"/><circle cx="4.8" cy="10.6" r="1.1"/><circle cx="11.2" cy="10.6" r="1.1"/></svg>',
 school:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"><path d="M8 3L1.8 6 8 9l6.2-3L8 3z"/><path d="M4.5 7.4V10c0 .9 1.6 1.7 3.5 1.7s3.5-.8 3.5-1.7V7.4"/><path d="M14.2 6v3" stroke-linecap="round"/></svg>',
 moon:'<svg viewBox="0 0 16 16" fill="currentColor"><path d="M11 10.6A5 5 0 0 1 6.2 3a5 5 0 1 0 6.6 6.6 5 5 0 0 1-1.8 1z"/></svg>',
 tide:'<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M1.5 6.5c1.3 0 1.3 1.2 2.6 1.2S7 6.5 8.3 6.5s1.3 1.2 2.6 1.2S12.2 6.5 14.5 6.5"/><path d="M1.5 10.2c1.3 0 1.3 1.2 2.6 1.2s1.4-1.2 2.7-1.2 1.3 1.2 2.6 1.2 1.4-1.2 3.6-1.2"/></svg>',
};
function ic(n){return `<span class="tic">${ICONS[n]||""}</span>`;}

let FX=null, DATA=null;
function post(action,extra){
  try{window.webkit.messageHandlers.bridge.postMessage(JSON.stringify(Object.assign({action},extra||{})));}catch(e){}
}
function sceneFor(data){
  // Escena = condición WMO exacta + día/noche (catálogo completo en fx.js). Amanecer/atardecer sólo con cielo poco nublado.
  const c=data.current, isDay=c.is_day;
  const code=(data.raw&&data.raw.current&&data.raw.current.weather_code!=null)?data.raw.current.weather_code:null;
  const cond=code!=null&&window.SkyFX?window.SkyFX.condicion(code):(c.class==="clear"?"clear":c.class==="cloud"?"partly":c.class||"overcast");
  try{
    const now=Date.now(), sr=new Date(data.today.sunrise).getTime(), ss=new Date(data.today.sunset).getTime(), win=50*60*1000;
    if(/^(clear|mostly|partly)$/.test(cond)&&Math.abs(now-sr)<win) return "dawn";
    if(/^(clear|mostly|partly)$/.test(cond)&&Math.abs(now-ss)<win) return "dusk";
  }catch(e){}
  return cond+(isDay?"-day":"-night");
}
// grupo de cielo (para el degradado CSS): despejado, nubes, niebla, lluvia, nieve, hielo, tormenta
function grupoCielo(scene){
  if(scene==="dawn"||scene==="dusk") return scene;
  const b=scene.replace(/-(day|night)$/,""), n=/-night$/.test(scene)?"night":"day";
  const g=/^(clear|mostly)$/.test(b)?"clear":/^(partly)$/.test(b)?"partly":/^overcast$/.test(b)?"overcast":/^(fog|rime)$/.test(b)?"fog"
        :/^freezing/.test(b)?"ice":/^storm/.test(b)?"storm":/snow/.test(b)?"snow":"rain";
  return "sky-"+g+"-"+n;
}

function nowIdx(h){
  const now=Date.now();
  for(let i=0;i<h.time.length;i++){if(new Date(h.time[i]).getTime()>=now-3600e3)return i;}
  return 0;
}

// ===== render principal =====
function render(data){
  DATA=data;
  const c=data.current, d=data.raw.daily, h=data.raw.hourly, loc=data.location;
  const scene=sceneFor(data);
  document.body.className=scene+" "+grupoCielo(scene);
  if(!FX) FX=new window.SkyFX(document.getElementById("fx"));
  FX.set(scene, c.wind, c.is_day);

  const start=nowIdx(h);
  // amaneceres/atardeceres dentro de la ventana de 24 h (iOS los intercala en el por-hora)
  const t0=new Date(h.time[start]).getTime(), t1=t0+24*3600e3, evs=[];
  for(let k=0;k<d.time.length;k++){
    [[d.sunrise&&d.sunrise[k],"Amanecer",1],[d.sunset&&d.sunset[k],"Atardecer",0]].forEach(([iso,lbl,up])=>{
      if(!iso)return; const ts=new Date(iso).getTime(); if(ts>Date.now()&&ts<t1) evs.push({ts,lbl,up,iso});});
  }
  let hourly="";
  for(let i=start;i<start+24 && i<h.time.length;i++){
    const dt=new Date(h.time[i]);
    const lbl=hourLabel(dt, i===start);
    const [icn]=wmo(h.weather_code[i], h.is_day?h.is_day[i]:1);
    const pp=h.precipitation_probability?h.precipitation_probability[i]:0;
    hourly+=`<div class="hour${i===start?" now":""}"><div class="h">${esc(lbl)}</div><div class="i">${icn}`+
            `<div class="p">${pp>=20?pp+"%":""}</div></div><div class="t">${R(h.temperature_2m[i])}°</div></div>`;
    const nxt=dt.getTime()+3600e3;
    for(const e of evs) if(e.ts>=dt.getTime()&&e.ts<nxt)
      hourly+=`<div class="hour sun"><div class="h">${hm(e.iso)}</div><div class="i">${sunEvSvg(e.up)}<div class="p"></div></div><div class="t">${e.lbl}</div></div>`;
  }

  let gMin=Infinity,gMax=-Infinity;
  for(let i=0;i<d.time.length;i++){gMin=Math.min(gMin,d.temperature_2m_min[i]);gMax=Math.max(gMax,d.temperature_2m_max[i]);}
  let daily="";
  for(let i=0;i<d.time.length;i++){
    const [di]=wmo(d.weather_code[i],1);
    const lo=d.temperature_2m_min[i],hi=d.temperature_2m_max[i];
    const span=Math.max(gMax-gMin,1), left=((lo-gMin)/span)*100,wdt=((hi-lo)/span)*100;
    const pp=d.precipitation_probability_max[i]||0;
    const dot=i===0?`<div class="now" style="left:${Math.min(100,Math.max(0,((c.temp-gMin)/(gMax-gMin))*100))}%"></div>`:"";
    daily+=`<div class="day" data-day="${i}"><div class="name">${dayShort(d.time[i],i)}</div>`+
      `<div class="i">${di}${pp>=20?`<div class="p">${pp}%</div>`:""}</div>`+
      `<div class="lo">${R(lo)}°</div>`+
      `<div class="bar"><div class="fill" style="left:${left}%;width:${Math.max(wdt,6)}%;background:${tgrad(lo,hi)}"></div>${dot}</div>`+
      `<div class="hi">${R(hi)}°</div><div class="chev">›</div></div>`;
  }

  // Comentario contextual (estilo Apple): lo que más importa AHORA, no sólo lluvia.
  // Si un dato viejo del caché aún no lo trae, se cae al aviso de lluvia de antes.
  let ctx=data.context&&data.context.text
    ? {icon:data.context.icon||"💧", text:data.context.text, kind:data.context.kind||"info"}
    : (data.rain&&data.rain.summary ? {icon:"💧", text:data.rain.summary, kind:"rain"} : null);
  // Garantía: SIEMPRE hay un comentario del clima del día (nunca se omite el badge).
  if(!ctx){ ctx={icon:"", text:`${c.text||"Tiempo"} ahora; máxima de ${R(data.today.max)}° y mínima de ${R(data.today.min)}° hoy.`, kind:"steady"}; }
  const ctxLine=`<div class="ctx">${esc(ctx.text)}</div>`;
  const feels=c.feels, vis=(h.visibility&&h.visibility.length)?(h.visibility[start]/1000).toFixed(1):"—";
  const uv=data.today.uv, uvTxt=uv<3?"Bajo":uv<6?"Moderado":uv<8?"Alto":uv<11?"Muy alto":"Extremo";

  document.getElementById("content").innerHTML=`
    <div class="layout">
      <div class="hero-col">
        <div class="hero">
          <div class="city">${esc(loc.name)}</div>
          ${loc.region?`<div class="region">${esc(loc.region)}</div>`:""}
          <div class="temp">${c.temp}<sup>°</sup></div>
          <div class="cond">${esc(c.text)}</div>
          <div class="hl">Máx.: ${data.today.max}°<span class="sep"></span>Mín.: ${data.today.min}°</div>
        </div>
      </div>
      <div class="cards-col">
        <div class="card hourly-card">
          ${ctxLine}
          <div class="hourly">${hourly}</div>
        </div>
        <div class="card daily-card">
          <div class="card-h">${hic('cal')} Pronóstico para 10 días</div>
          ${daily}
        </div>
        <div class="grid">
          ${ambientalTiles(data.ambiental,"top")}
          <div class="tile"><div class="th">${ic('thermo')} Sensación</div><div class="tv">${feels}°</div><div class="ts">${feels>c.temp?"Más cálido por humedad/sol":feels<c.temp?"Más frío por viento/humedad":"Similar a la real"}</div></div>
          <div class="tile"><div class="th">${ic('drop')} Precipitación</div><div class="tv">${Number((d.precipitation_sum&&d.precipitation_sum[0])||0).toFixed(1)}<span class="u"> mm</span></div><div class="ts">Caída hoy${Number(c.precip||0)>0?` · ${Number(c.precip).toFixed(1)} mm última hora`:""}</div></div>
          <div class="tile"><div class="th">${ic('wind')} Viento</div><div class="viz">${compassSvg(c.wind_dir,c.wind)}<div><div class="tl">${c.wind} <span style="font-size:13px;font-weight:500">km/h</span></div><div class="ts" style="padding-top:2px">Viento ${windDir(c.wind_dir)}${c.gust?`<br>Ráfagas ${c.gust} km/h`:""}</div></div></div></div>
          <div class="tile"><div class="th">${ic('drop')} Humedad</div><div class="tv">${c.humidity}%</div><div class="ts">Nubosidad ${c.cloud}%</div></div>
          <div class="tile"><div class="th">${ic('dew')} Punto de rocío</div><div class="tv">${c.dew!=null?c.dew+"°":"—"}</div><div class="ts">${c.dew!=null&&c.dew>=16?"Bochornoso":c.dew!=null&&c.dew>=10?"Cómodo":"Aire seco"}</div></div>
          <div class="tile"><div class="th">${ic('uv')} Índice UV</div><div class="tv">${R(uv)}</div><div class="tl">${uvTxt}</div><div class="meter uv"><div class="dot" style="left:${Math.min(100,uv/11*100)}%"></div></div><div class="ts">${uv>=6?"Usa protección solar entre 11:00 y 17:00.":"Bajo el resto del día."}</div></div>
          <div class="tile"><div class="th">${ic('eye')} Visibilidad</div><div class="tv">${vis}<span class="u"> km</span></div><div class="ts">${vis>10?"Perfectamente clara":"Reducida"}</div></div>
          <div class="tile"><div class="th">${ic('gauge')} Presión</div>${gaugeSvg(c.pressure)}<div class="ts" style="text-align:center">Nivel del mar</div></div>
          <div class="tile"><div class="th">${ic('sunset')} Puesta de sol</div><div class="tv">${hm(data.today.sunset)}</div>${sunArcSvg(data.today.sunrise,data.today.sunset)}<div class="ts">Amanecer: ${hm(data.today.sunrise)}</div></div>
          ${ambientalTiles(data.ambiental,"bottom")}
        </div>
      </div>
    </div>
    <div class="updated">${data.cached?'<span class="cached">⚠ Sin conexión · datos guardados</span> · ':''}Actualizado ${stamp(data.fetched)} · Datos de Open-Meteo · GEC-MMA · MTT</div>
    <div class="credito">Desarrollado por Javiera Navarrete</div>
  `;
  document.querySelectorAll(".day").forEach(el=>{
    el.addEventListener("click",()=>openDay(parseInt(el.dataset.day,10)));
  });
}

// ===== visualizaciones de tiles (SVG) =====
function sunEvSvg(up){
  return `<svg viewBox="0 0 32 32" class="wi" aria-hidden="true"><path d="M5 23h22" stroke="#fff" stroke-width="2" stroke-linecap="round" opacity=".85"/>`+
    `<path d="M10 23a6 6 0 0 1 12 0z" fill="url(#gSun)"/>`+
    `<path d="M16 ${up?"13.5V7.5M13.2 10.3L16 7.5l2.8 2.8":"7.5v6M13.2 10.7L16 13.5l2.8-2.8"}" stroke="#fff" stroke-width="1.9" fill="none" stroke-linecap="round" stroke-linejoin="round"/></svg>`;
}
function compassSvg(dir,spd){
  let ticks="";
  for(let a=0;a<360;a+=10){const r1=a%90===0?32:34.5,rad=(a-90)*Math.PI/180;
    ticks+=`<line x1="${42+r1*Math.cos(rad)}" y1="${42+r1*Math.sin(rad)}" x2="${42+38*Math.cos(rad)}" y2="${42+38*Math.sin(rad)}" stroke="rgba(255,255,255,${a%90===0?.8:.32})" stroke-width="${a%90===0?1.6:1}"/>`;}
  const lab=[["N",42,13],["E",72,46],["S",42,77],["O",12,46]].map(([l,x,y])=>`<text x="${x}" y="${y}" fill="rgba(255,255,255,.7)" font-size="8.5" font-weight="700" text-anchor="middle">${l}</text>`).join("");
  // la flecha apunta hacia donde VA el viento (dirección de origen + 180°), como en iOS
  return `<svg class="compass" viewBox="0 0 84 84" aria-hidden="true">${ticks}${lab}`+
    `<g transform="rotate(${(dir||0)+180} 42 42)"><path d="M42 12l4.2 8h-8.4z" fill="#fff"/><line x1="42" y1="18" x2="42" y2="66" stroke="#fff" stroke-width="2.2" stroke-linecap="round"/><circle cx="42" cy="66" r="3" fill="none" stroke="#fff" stroke-width="2"/></g>`+
    `<circle cx="42" cy="42" r="13" fill="rgba(25,35,55,.55)"/><text x="42" y="46" fill="#fff" font-size="12" font-weight="600" text-anchor="middle">${spd}</text></svg>`;
}
function gaugeSvg(p){
  const lo=970,hi=1050,f=Math.max(0,Math.min(1,(p-lo)/(hi-lo))), a0=-220,a1=40, ang=(a0+(a1-a0)*f)*Math.PI/180;
  let ticks="";
  for(let i=0;i<=40;i++){const a=(a0+(a1-a0)*i/40)*Math.PI/180, on=i/40<=f;
    ticks+=`<line x1="${46+28*Math.cos(a)}" y1="${40+28*Math.sin(a)}" x2="${46+34*Math.cos(a)}" y2="${40+34*Math.sin(a)}" stroke="rgba(255,255,255,${on?.85:.25})" stroke-width="1.4" stroke-linecap="round"/>`;}
  return `<svg class="gauge" viewBox="0 0 92 70" aria-hidden="true">${ticks}`+
    `<line x1="${46+24*Math.cos(ang)}" y1="${40+24*Math.sin(ang)}" x2="${46+37*Math.cos(ang)}" y2="${40+37*Math.sin(ang)}" stroke="#fff" stroke-width="3" stroke-linecap="round"/>`+
    `<text x="46" y="44" fill="#fff" font-size="17" font-weight="600" text-anchor="middle">${p}</text>`+
    `<text x="46" y="57" fill="rgba(255,255,255,.65)" font-size="9" font-weight="600" text-anchor="middle">hPa</text></svg>`;
}
function sunArcSvg(sr,ss){
  const a=new Date(sr).getTime(), b=new Date(ss).getTime(), now=Date.now();
  const f=Math.max(0,Math.min(1,(now-a)/(b-a)));
  // curva tipo seno: horizonte en y=30, cenit en y=6
  let path="";for(let i=0;i<=40;i++){const x=4+i/40*152,y=30-Math.sin(i/40*Math.PI)*24;path+=(i?"L":"M")+x.toFixed(1)+" "+y.toFixed(1);}
  const sx=4+f*152, sy=30-Math.sin(f*Math.PI)*24, up=now>a&&now<b;
  return `<svg class="sunarc" viewBox="0 0 160 40" preserveAspectRatio="none" aria-hidden="true">`+
    `<path d="${path}" fill="none" stroke="rgba(255,255,255,.35)" stroke-width="1.6"/>`+
    `<line x1="0" y1="30" x2="160" y2="30" stroke="rgba(255,255,255,.45)" stroke-width="1"/>`+
    (up?`<circle cx="${sx}" cy="${sy}" r="4.6" fill="#fff" style="filter:drop-shadow(0 0 4px rgba(255,255,255,.9))"/>`:"")+`</svg>`;
}
function moonSvg(m){
  const ill=Math.max(0,Math.min(100,m.illumination||0))/100, wax=!/menguante/i.test(m.name||"");
  // Vista desde el hemisferio SUR (Chile): creciente iluminada a la IZQUIERDA, menguante a la DERECHA.
  // Borde iluminado = semicírculo; terminador = media elipse de semieje |2k−1|·r, que se curva hacia el
  // lado oscuro si k > ½ (gibosa) o hacia el iluminado si k < ½ (creciente/menguante delgada).
  const r=24, rx=Math.abs(1-2*ill)*r, cx=27, cy=27, right=!wax, big=ill>.5;
  const s1=right?1:0, s2=right?(big?1:0):(big?0:1);
  const d=`M${cx} ${cy-r} A${r} ${r} 0 0 ${s1} ${cx} ${cy+r} A${rx} ${r} 0 0 ${s2} ${cx} ${cy-r}z`;
  return `<svg class="moonsvg" viewBox="0 0 54 54" aria-hidden="true"><circle cx="${cx}" cy="${cy}" r="${r}" fill="rgba(255,255,255,.14)"/>`+
    `<path d="${d}" fill="url(#gMoon)"/></svg>`;
}

// ===== tiles ambientales (Chile) — group 'top' (práctico) o 'bottom' (astro/costa) =====
function ambientalTiles(amb, group){
  if(!amb) return "";
  let out="";
  const air=amb.air, restr=amb.restriccion, moon=amb.moon, tide=amb.tides;
  if(group==="bottom"){
    if(moon){
      out+=`<div class="tile"><div class="th">${ic('moon')} Fase lunar</div>`+
        moonSvg(moon)+
        `<div class="ts">${esc(moon.name)} · ${moon.illumination}% iluminada</div></div>`;
    }
    if(tide){
      const nh=tide.next_high, nl=tide.next_low;
      const lbl=tide.coastal?tide.port:`${tide.port} · costa más cercana`;
      out+=`<div class="tile"><div class="th">${ic('tide')} Mareas</div>`+
        `<div class="tv" style="font-size:17px;line-height:1.35">`+
        (nh?`▲ ${hm(nh[0])} <span class="u">${nh[1]} m</span><br>`:"")+
        (nl?`▼ ${hm(nl[0])} <span class="u">${nl[1]} m</span>`:"")+`</div>`+
        `<div class="ts">${esc(lbl)}</div></div>`;
    }
    return out;
  }
  if(air){
    out+=`<div class="tile"><div class="th">${ic('aqi')} Alerta ambiental</div>`+
      `<div class="tv" style="font-size:26px">${esc(air.level)}</div>`+
      `<div class="meter aqi"><div class="dot" style="left:${Math.min(100,(air.mean24||0)/200*100)}%"></div></div>`+
      `<div class="ts">MP2,5 ${air.mean24} µg/m³ · prom. 24h</div></div>`;
  }
  if(restr){
    if(restr.aplica){
      out+=`<div class="tile"><div class="th">${ic('car')} Restricción</div>`+
        `<div class="tv">${esc(restr.digitos)}</div>`+
        `<div class="ts">Sello verde · episodio ${esc(restr.level)}. Sin sello verde: restringidos.</div></div>`;
    }else{
      out+=`<div class="tile"><div class="th">${ic('car')} Restricción</div>`+
        `<div class="tv" style="font-size:20px">No aplica</div>`+
        `<div class="ts">${esc(restr.motivo)}</div></div>`;
    }
  }
  const susp=amb.suspension;
  if(susp){
    const oficial=susp.oficial&&/suspendidas/i.test(susp.nivel||"");
    const when=susp.cuando?` (${esc(susp.cuando)})`:"";
    const nivelTxt=(susp.nivel==="Sin aviso")?"Sin aviso de suspensión":susp.nivel;
    const foot=oficial
      ? `Confirmado por Mineduc.`
      : `${susp.riesgo?esc(susp.motivo)+when+". ":""}Estimación por pronóstico, sin confirmación de Mineduc.`;
    out+=`<div class="tile"><div class="th">${ic('school')} Clases</div>`+
      `<div class="tv" style="font-size:${oficial?20:susp.riesgo?22:20}px">${esc(nivelTxt)}</div>`+
      `<div class="ts">${foot}</div></div>`;
  }
  return out;
}

// ===== detalle por día =====
function openDay(i){
  if(!DATA) return;
  const d=DATA.raw.daily, h=DATA.raw.hourly;
  const date=d.time[i];
  const [di,dtxt]=wmo(d.weather_code[i],1);
  // horas de ese día
  let cells="";
  for(let k=0;k<h.time.length;k++){
    if(h.time[k].slice(0,10)!==date) continue;
    const dt=new Date(h.time[k]);
    const [icn]=wmo(h.weather_code[k], h.is_day?h.is_day[k]:1);
    const pp=h.precipitation_probability?h.precipitation_probability[k]:0;
    cells+=`<div class="hour"><div class="h">${esc(hourLabel(dt,false))}</div>`+
      `<div class="i">${icn}<div class="p">${pp>=20?pp+"%":""}</div></div><div class="t">${R(h.temperature_2m[k])}°</div></div>`;
  }
  const sr=hm(d.sunrise[i]), ss=hm(d.sunset[i]);
  const ov=document.getElementById("overlay");
  ov.innerHTML=`
    <div class="sheet">
      <div class="sheet-h">
        <button id="dback" class="back">‹ Volver</button>
        <div class="sheet-title">${i===0?"Hoy":dayLong(date,i)}</div>
        <div style="width:70px"></div>
      </div>
      <div class="sheet-body">
        <div class="day-hero">
          <div class="di">${di}</div>
          <div>
            <div class="dtxt">${esc(dtxt)}</div>
            <div class="dhl">Máx. <b>${R(d.temperature_2m_max[i])}°</b> · Mín. <b>${R(d.temperature_2m_min[i])}°</b></div>
          </div>
        </div>
        <div class="card"><div class="card-h">${hic('clock')} Por hora</div><div class="hourly">${cells}</div></div>
        <div class="grid">
          <div class="tile"><div class="th">${ic('drop')} Prob. lluvia</div><div class="tv">${d.precipitation_probability_max[i]||0}%</div><div class="ts">${(d.precipitation_sum[i]||0).toFixed(1)} mm previstos</div></div>
          <div class="tile"><div class="th">${ic('uv')} UV máx.</div><div class="tv">${R(d.uv_index_max[i])}</div><div class="ts">${(u=>u<3?"Bajo":u<6?"Moderado":u<8?"Alto":u<11?"Muy alto":"Extremo")(d.uv_index_max[i])}</div></div>
          <div class="tile"><div class="th">${ic('wind')} Viento máx.</div><div class="tv">${R(d.wind_speed_10m_max[i])}<span class="u"> km/h</span></div><div class="ts">${windDir(d.wind_direction_10m_dominant[i])}${d.wind_gusts_10m_max?` · ráfagas ${R(d.wind_gusts_10m_max[i])}`:""}</div></div>
          <div class="tile"><div class="th">${ic('sunset')} Sol</div><div class="tv" style="font-size:22px">${sr}</div><div class="ts">Atardece ${ss}</div></div>
        </div>
      </div>
    </div>`;
  ov.classList.add("show");
  document.getElementById("dback").onclick=closeDay;
}
function closeDay(){const ov=document.getElementById("overlay");ov.classList.remove("show");setTimeout(()=>ov.innerHTML="",300);}

// ===== callbacks del host =====
window.onClimaData=function(data){ setLoading(false); render(data); };
window.onError=function(msg){
  setLoading(false);
  document.getElementById("content").innerHTML=
    `<div class="err">No se pudo cargar el clima.<br><span style="opacity:.6">${esc(msg||"")}</span><br><br>¿Hay conexión a internet?</div>`;
};
window.onSearchResults=function(list){ renderSuggest(list); };

function setLoading(on){
  let l=document.getElementById("loadbar");
  if(on){ if(!l){l=document.createElement("div");l.id="loadbar";document.body.appendChild(l);} l.className="on"; }
  else if(l){ l.className=""; }
}

// ===== buscador =====
let suggestBox=null, searchT=null;
function renderSuggest(list){
  const box=document.getElementById("suggest");
  if(!list||!list.length){box.classList.remove("show");box.innerHTML="";return;}
  box.innerHTML=list.map(c=>`<div class="sug" data-lat="${c.lat}" data-lon="${c.lon}" data-name="${esc(c.name)}" data-region="${esc(c.region||"")}">`+
    `<span class="sn">${esc(c.name)}</span><span class="sr">${esc(c.region||"")}</span></div>`).join("");
  box.classList.add("show");
  box.querySelectorAll(".sug").forEach(el=>{
    el.onclick=()=>{
      document.getElementById("q").value=el.dataset.name;
      box.classList.remove("show");
      setLoading(true);
      post("city",{name:el.dataset.name,region:el.dataset.region,lat:parseFloat(el.dataset.lat),lon:parseFloat(el.dataset.lon)});
    };
  });
}

// ===== pull-to-refresh elástico =====
function setupPull(){
  let pull=0, pulling=false, ind=document.getElementById("pull");
  const THRESH=90;
  function scroller(){return document.scrollingElement||document.documentElement;}
  function begin(){pulling=true;}
  function move(dy){
    if(scroller().scrollTop>0){pull=0;ind.style.height="0px";ind.classList.remove("ready");return;}
    pull=Math.min(dy*0.45, 140);
    if(pull>0){ind.style.height=pull+"px";ind.classList.toggle("ready",pull>=THRESH);
      document.getElementById("app").style.transform=`translateY(${Math.min(pull*0.3,40)}px)`;}
  }
  function end(){
    document.getElementById("app").style.transform="";
    if(pull>=THRESH){ind.classList.add("spin");setLoading(true);post("refresh");
      setTimeout(()=>{ind.classList.remove("spin","ready");ind.style.height="0px";},900);}
    else{ind.style.height="0px";ind.classList.remove("ready");}
    pull=0;pulling=false;
  }
  // touch
  let startY=0;
  window.addEventListener("touchstart",e=>{startY=e.touches[0].clientY;begin();},{passive:true});
  window.addEventListener("touchmove",e=>{if(pulling)move(e.touches[0].clientY-startY);},{passive:true});
  window.addEventListener("touchend",end,{passive:true});
  // wheel (trackpad/mouse) — sobre-scroll hacia arriba en el tope
  let acc=0,wt=null;
  window.addEventListener("wheel",e=>{
    if(scroller().scrollTop<=0 && e.deltaY<0){
      acc+=-e.deltaY; move(acc);
      clearTimeout(wt); wt=setTimeout(()=>{end();acc=0;},160);
    } else { acc=0; }
  },{passive:true});
}

// ===== init =====
window.addEventListener("DOMContentLoaded",()=>{
  const q=document.getElementById("q");
  q.addEventListener("input",()=>{
    clearTimeout(searchT);
    const v=q.value.trim();
    if(v.length<2){document.getElementById("suggest").classList.remove("show");return;}
    searchT=setTimeout(()=>post("search",{q:v}),130);
  });
  q.addEventListener("keydown",e=>{ if(e.key==="Escape"){document.getElementById("suggest").classList.remove("show");q.blur();} });
  document.getElementById("loc").onclick=()=>{setLoading(true);document.getElementById("suggest").classList.remove("show");post("locate");};
  document.addEventListener("click",e=>{ if(!e.target.closest(".searchbar"))document.getElementById("suggest").classList.remove("show"); });
  setupPull();
  setLoading(true);
});

// ===== recuadros «Bici» y «Metro» — COMPARTIDOS por Linux y Android (Kotone 2026-10-31) =====
// Van PRIMERO en la grilla de detalles (pedido de Javiera): Bici, luego Metro, luego todos los demás.
// «Bici»: mismas reglas que Alertas.java (lluvia ahora/≤3 h, tormenta, viento, calor, helada, niebla, UV, aire).
// «Metro»: estado oficial que entrega el host (Linux: metro.cl desde C; Android: la app Metro o metro.cl).
let METRO=null, metroPedido=0;
window.onMetro=j=>{METRO=j;tilesExtra();};
const COLOR_LINEA={L1:"#E2231A",L2:"#FFB81C",L3:"#8B5A2B",L4:"#0055A5",L4A:"#3EA8DE",L5:"#009A44",L6:"#8E3C97"};
const SVG_BICI='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><circle cx="6" cy="16" r="4"/><circle cx="18" cy="16" r="4"/><path d="M6 16l3.5-6 3 6 3.5-6M9.5 10h6M15 7h2"/></svg>';
const SVG_METRO='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="3" width="12" height="13" rx="3"/><path d="M6 10h12M8 20l2-4M16 20l-2-4"/></svg>';
function alertasBici(D){
  const c=D.current||{}, raw=D.raw||{}, rc=raw.current||{}, h=raw.hourly||{}, code=rc.weather_code|0, al=[];
  const lluvia=x=>(x>=51&&x<=67)||(x>=80&&x<=82)||x>=95, nieve=x=>(x>=71&&x<=77)||x===85||x===86;
  const ahora=new Date(), clave=`${ahora.getFullYear()}-${String(ahora.getMonth()+1).padStart(2,"0")}-${String(ahora.getDate()).padStart(2,"0")}T${String(ahora.getHours()).padStart(2,"0")}`;
  const now=(h.time||[]).findIndex(t=>t.startsWith(clave));
  if(code>=95) al.push(["danger","Tormenta eléctrica ahora"]);
  else if((c.precip||0)>0.1||lluvia(code)||nieve(code)) al.push(["warn",(nieve(code)?"Nieve":"Lluvia")+" ahora: calzada resbaladiza"]);
  else if(now>=0) for(let i=now+1;i<=now+3&&i<h.time.length;i++){
    const cc=h.weather_code[i], pp=(h.precipitation_probability||[])[i]||0;
    if(((lluvia(cc)||nieve(cc))&&pp>=50)||pp>=70){al.push([cc>=95?"danger":"warn",`${cc>=95?"Tormenta":"Lluvia"} hacia las ${h.time[i].slice(11,16)} (${pp}%)`]);break;}
  }
  const g=c.gust||0, w=c.wind||0, t=c.temp, f=c.feels;
  if(g>=65) al.push(["danger",`Ráfagas de ${g} km/h`]); else if(g>=45||w>=30) al.push(["warn",`Viento fuerte: ráfagas de ${g} km/h`]);
  if(t>=35) al.push(["danger",`Calor extremo: ${t}°`]); else if(t>=31) al.push(["warn",`Calor: ${t}°`]);
  if(t<=1) al.push(["danger",`Helada: ${t}°`]); else if(f<=5) al.push(["info",`Frío: sensación de ${f}°`]);
  if(code===45||code===48) al.push(["warn","Niebla: luces encendidas"]);
  const air=D.ambiental&&D.ambiental.air;
  if(air&&air.key==="preemergencia"||air&&air.key==="emergencia") al.push(["danger",`Aire en ${air.level.toLowerCase()}`]);
  else if(air&&air.key==="alerta") al.push(["warn","Aire en alerta: pedalea suave"]);
  if(c.is_day&&(D.today&&D.today.uv)>=8) al.push(["info",`UV muy alto (${Math.round(D.today.uv)})`]);
  return ["danger","warn","info"].flatMap(n=>al.filter(a=>a[0]===n));
}
function tilesExtra(){
  if(!DATA) return;
  const g=document.querySelector("#content .grid"); if(!g) return;
  ["bici-tile","metro-tile"].forEach(id=>{const o=document.getElementById(id); if(o) o.remove();});
  // Metro
  const mt=document.createElement("div"); mt.className="tile"; mt.id="metro-tile";
  const p=METRO&&METRO.ok?(METRO.problemas||[]):null;
  if(p===null) mt.innerHTML=`<div class="th"><span class="tic">${SVG_METRO}</span> Metro</div><div class="tv" style="font-size:22px;color:var(--txt-dim)">Sin datos</div><div class="ts">Revisando el estado de la red…</div>`;
  else if(!p.length) mt.innerHTML=`<div class="th"><span class="tic">${SVG_METRO}</span> Metro</div><div class="tv tv-est"><i class="punto ok"></i>Normal</div><div class="ts">Toda la red opera con normalidad</div>`;
  else{
    const lineas=[...new Set(p.map(x=>x.linea))], est=p.map(x=>x.texto.split(":")[0]).filter((v,i,a)=>v&&a.indexOf(v)===i);
    mt.innerHTML=`<div class="th"><span class="tic">${SVG_METRO}</span> Metro</div><div class="tv tv-est"><i class="punto mal"></i>Problemas</div>`
      +`<div class="lin-chips">${lineas.map(l=>`<span style="background:${COLOR_LINEA[l]||"#8E8E93"};color:${l==="L2"?"#1c1c1e":"#fff"}">${esc(l.slice(1))}</span>`).join("")}</div>`
      +`<div class="ts ts-clamp">${esc(est.length?est.join(" · "):p[0].texto)}</div>`;
  }
  mt.style.cursor="pointer"; mt.onclick=()=>post("abrirMetro");
  // Bici
  const al=alertasBici(DATA), graves=al.filter(a=>a[0]!=="info"), peor=graves.length?graves[0][0]:"ok";
  const bt=document.createElement("div"); bt.className="tile"; bt.id="bici-tile";
  bt.innerHTML=`<div class="th"><span class="tic">${SVG_BICI}</span> Bici</div><div class="tv tv-est"><i class="punto ${peor==="danger"?"mal":peor==="warn"?"warn":"ok"}"></i>${peor==="danger"?"Mejor no":peor==="warn"?"Con cuidado":"Buen momento"}</div>`
    +`<div class="ts ts-clamp">${esc(graves.length?graves[0][1]:al.length?al[0][1]:"Sin alertas para pedalear")}</div>`;
  bt.style.cursor="pointer"; bt.onclick=()=>post("abrirBici");
  g.prepend(mt); g.prepend(bt);
  if(Date.now()-metroPedido>5*60000){metroPedido=Date.now(); post("metro");}
}
{const _od=window.onClimaData; window.onClimaData=function(d){_od(d); tilesExtra();};}

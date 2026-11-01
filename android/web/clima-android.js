// Clima para Android — núcleo de datos (port de c/clima_data.c + c/clima_ambiental_c.inc).
// Produce EXACTAMENTE el mismo JSON que el host de escritorio y se lo entrega a la misma interfaz
// (window.onClimaData). Red, ubicación y caché los pone el host Java (window.ClimaAndroid).
// Suspensión de clases (MINEDUC) y episodio de restricción (airerm) salen de las MISMAS fuentes oficiales
// que escritorio: el port Java de los scrapers C++ (Oficial.java), vía ClimaAndroid.oficial. Kotone 2026-10-31.
"use strict";
(function(){
const A=window.ClimaAndroid;
const REFRESH_MS=15*60*1000;
const SANTIAGO={name:"Santiago",region:"Región Metropolitana",lat:-33.4489,lon:-70.6693,comuna:true,rm_restr:true};

// ---------------------------------------------------------------- puente asíncrono
let seq=0; const waiting={};
window.__climaHttp=(id,ok,body)=>{const f=waiting[id];delete waiting[id];if(f)f(ok?body:null);};
window.__climaLoc=(id,lat,lon)=>{const f=waiting[id];delete waiting[id];if(f)f(lat==null?null:{lat,lon});};
const httpText=url=>new Promise(r=>{const id=++seq;waiting[id]=r;A.get(id,url);});
const httpJson=async url=>{const t=await httpText(url);if(!t)return null;try{return JSON.parse(t);}catch(e){return null;}};
const phoneLoc=()=>new Promise(r=>{const id=++seq;waiting[id]=r;A.locate(id);});
// fuentes oficiales: {suspension:{oficial,dias[]}, restriccion:{oficial,episodio}|null}
const oficial=(loc,restr)=>new Promise(r=>{const id=++seq;waiting[id]=t=>{try{r(JSON.parse(t)||{});}catch(e){r({});}};
  A.oficial(id,loc.region||"",loc.name||"",!!restr);});

// ---------------------------------------------------------------- comunas
let COMUNAS=null;
function comunas(){if(!COMUNAS){try{COMUNAS=JSON.parse(A.asset("data/comunas_cl.json"));}catch(e){COMUNAS=[];}}return COMUNAS;}
const fold=s=>String(s||"").toLowerCase().normalize("NFD").replace(/[\u0300-\u036f]/g,"");
function search(q,max){
  const f=fold(q), out=[];
  for(let pass=0;pass<2&&out.length<max;pass++)
    for(const o of comunas()){ if(out.length>=max)break;
      const nm=fold(o.name), hit=pass===0?nm.startsWith(f):(!nm.startsWith(f)&&nm.includes(f));
      if(hit) out.push({name:o.name,region:o.region,lat:o.lat,lon:o.lon}); }
  return out;
}
function nearest(lat,lon){
  let best=null,bd=1e18;
  for(const o of comunas()){const a=o.lat-lat,b=o.lon-lon,d=a*a+b*b;if(d<bd){bd=d;best=o;}}
  return best&&bd<0.20?best:null;
}
function locFromCoords(lat,lon,fbName,fbRegion){
  const c=nearest(lat,lon);
  return {name:c?c.name:(fbName||"Mi ubicación"),region:c?c.region:(fbRegion||""),lat,lon,comuna:!!c,rm_restr:!!(c&&c.rm_restr)};
}

// ---------------------------------------------------------------- WMO y utilidades
const WMO={0:["☀️","Despejado","clear"],1:["🌤️","Mayormente despejado","clear"],2:["⛅","Parcialmente nublado","cloud"],
 3:["☁️","Nublado","cloud"],45:["🌫️","Niebla","fog"],48:["🌫️","Niebla con escarcha","fog"],51:["🌦️","Llovizna ligera","rain"],
 53:["🌦️","Llovizna","rain"],55:["🌧️","Llovizna intensa","rain"],56:["🌧️","Llovizna helada","rain"],57:["🌧️","Llovizna helada","rain"],
 61:["🌧️","Lluvia ligera","rain"],63:["🌧️","Lluvia","rain"],65:["🌧️","Lluvia fuerte","rain"],66:["🌧️","Lluvia helada","rain"],
 67:["🌧️","Lluvia helada","rain"],71:["🌨️","Nieve ligera","snow"],73:["🌨️","Nieve","snow"],75:["❄️","Nieve fuerte","snow"],
 77:["🌨️","Granizo fino","snow"],80:["🌦️","Chubascos ligeros","rain"],81:["🌧️","Chubascos","rain"],82:["⛈️","Chubascos violentos","storm"],
 85:["🌨️","Chubascos de nieve","snow"],86:["❄️","Chubascos de nieve","snow"],95:["⛈️","Tormenta","storm"],
 96:["⛈️","Tormenta con granizo","storm"],99:["⛈️","Tormenta fuerte","storm"]};
const RAIN=new Set([51,53,55,56,57,61,63,65,66,67,80,81,82,95,96,99]);
const isRain=c=>RAIN.has(c), isStorm=c=>c===95||c===96||c===99, isSnow=c=>[71,73,75,77,85,86].includes(c);
const num=(a,i)=>{const v=a&&a[i];return typeof v==="number"?v:0;};
const DIAS=["domingo","lunes","martes","miércoles","jueves","viernes","sábado"];
const weekday=iso=>DIAS[new Date(iso+"T12:00").getDay()];
const ROSE=["del norte","del noreste","del este","del sureste","del sur","del suroeste","del oeste","del noroeste"];
const rose=d=>ROSE[Math.round((((d%360)+360)%360)/45)%8];
const pad=n=>String(n).padStart(2,"0");
function hourlyNow(h){
  const t=new Date(), want=`${t.getFullYear()}-${pad(t.getMonth()+1)}-${pad(t.getDate())}T${pad(t.getHours())}`;
  return h&&h.time?h.time.findIndex(s=>s.slice(0,13)===want):-1;
}
const hl=(h,i)=>{const s=h.time[i];return s&&s.length>=16?s.slice(11,16):"más tarde";};

// ---------------------------------------------------------------- lluvia y contexto
function buildRain(day){
  const lbls=[];
  for(let i=0;i<day.time.length&&i<8;i++){
    if(isRain(num(day.weather_code,i))&&num(day.precipitation_probability_max,i)>=50)
      lbls.push(i===0?"hoy":i===1?"mañana":"el "+weekday(day.time[i]));
  }
  if(!lbls.length) return null;
  const summary=lbls.length<=3?`Se espera lluvia ${lbls.join(", ")}.`
    :`Semana lluviosa: lluvia ${lbls[0]} y ${lbls.length-1} días más de los próximos 7.`;
  return {summary,count:lbls.length};
}
// Escalera de prioridades idéntica a build_context() de escritorio.
function buildContext(w,cur,day){
  const h=w.hourly, now=hourlyNow(h), code=cur.weather_code|0, R=Math.round;
  const precip=cur.precipitation||0, wind=cur.wind_speed_10m||0, gust=cur.wind_gusts_10m||0, wdir=cur.wind_direction_10m||0;
  const temp=cur.temperature_2m||0, feels=cur.apparent_temperature||0, cloud=cur.cloud_cover||0, isDay=!!cur.is_day;
  const tmax=num(day.temperature_2m_max,0), tmin=num(day.temperature_2m_min,0), uv=num(day.uv_index_max,0), nd=day.time.length;
  let icon="🌤️", txt="", kind="info";
  const set=(i,k,t)=>{icon=i;kind=k;txt=t;};
  if(!txt&&(precip>0.05||isRain(code)||isSnow(code))&&now>=0){
    let stop=-1;
    for(let i=now+1;i<h.time.length&&i<=now+12;i++){const hc=num(h.weather_code,i),hp=num(h.precipitation_probability,i);
      if(!isRain(hc)&&!isSnow(hc)&&hp<40){stop=i;break;}}
    let que=isSnow(code)?"Nieve":isStorm(code)?"Tormenta":"Lluvia"; if(code>=51&&code<=57)que="Llovizna";
    const k=isSnow(code)?"snow":"rain", ic=isSnow(code)?"🌨️":"🌧️";
    if(stop<0) set(ic,k,`${que} ahora; sigue durante las próximas horas.`);
    else if(stop-now<=1) set(ic,k,`${que} ahora; para dentro de poco.`);
    else set(ic,k,`${que} ahora; amaina hacia las ${hl(h,stop)}.`);
  }
  if(!txt&&now>=0){
    const dcode=num(day.weather_code,0);
    for(let i=now+1;i<h.time.length&&i<=now+6;i++){
      const hc=num(h.weather_code,i),hp=num(h.precipitation_probability,i);
      if(!(((isRain(hc)||isSnow(hc))&&hp>=50)||hp>=70)) continue;
      const ref=(isRain(hc)||isSnow(hc))?hc:dcode;
      const que=isSnow(ref)?"nieve":isStorm(ref)?"tormenta":(ref>=51&&ref<=57)?"llovizna":"lluvia";
      set(isStorm(ref)?"⛈️":isSnow(ref)?"🌨️":"🌧️",isSnow(ref)?"snow":"rain",`Se espera ${que} hacia las ${hl(h,i)} (${R(hp)}%).`);
      break;
    }
  }
  if(!txt){const dc=num(day.weather_code,0),dp=num(day.precipitation_probability_max,0);
    if((isRain(dc)||isSnow(dc))&&dp>=60) set(isSnow(dc)?"🌨️":"🌧️",isSnow(dc)?"snow":"rain",
      `${isSnow(dc)?"Nieve":(dc>=51&&dc<=57)?"Llovizna":"Lluvia"} probable en algún momento de hoy (${R(dp)}%).`);}
  if(!txt&&isStorm(num(day.weather_code,0))) set("⛈️","storm","Posibles tormentas eléctricas hoy.");
  if(!txt&&gust>=45) set("💨","wind",`Ráfagas de hasta ${R(gust)} km/h ${rose(wdir)}.`);
  if(!txt&&wind>=30) set("💨","wind",`Viento de ${R(wind)} km/h ${rose(wdir)}.`);
  if(!txt&&(code===45||code===48)) set("🌫️","fog","Niebla: visibilidad reducida, ojo al manejar.");
  if(!txt&&tmax>=32) set("🥵","heat",`Calor: hoy llega a ${R(tmax)}°. Hidrátate.`);
  if(!txt&&tmin<=1) set("🥶","cold",`Helada: la mínima baja a ${R(tmin)}°.`);
  if(!txt&&Math.abs(feels-temp)>=4) set(feels<temp?"🥶":"🥵","feels",`Se siente como ${R(feels)}°, ${feels<temp?"más frío":"más caluroso"} de lo que marca.`);
  if(!txt&&isDay&&uv>=8) set("🧴","uv",`Índice UV muy alto (${R(uv)}): usa protector.`);
  if(!txt) for(let i=1;i<nd&&i<8;i++){
    if(isRain(num(day.weather_code,i))&&num(day.precipitation_probability_max,i)>=50){
      set("💧","rain",`Se espera lluvia ${i===1?"mañana":"el "+weekday(day.time[i])}.`);break;}}
  if(!txt&&nd>1){const t2=num(day.temperature_2m_max,1);
    if(Math.abs(t2-tmax)>=6) set(t2>tmax?"📈":"📉","trend",`Mañana ${t2>tmax?"sube":"baja"}: máxima de ${R(t2)}°, ${R(Math.abs(t2-tmax))}° ${t2>tmax?"más":"menos"} que hoy.`);}
  if(!txt&&now>=0&&now+4<h.time.length){const hc=num(h.weather_code,now+4);
    if(cloud>=70&&hc<=1) set("🌤️","clear",`Se despeja hacia las ${hl(h,now+4)}.`);
    else if(cloud<=30&&hc>=3) set("☁️","cloud",`Se nubla hacia las ${hl(h,now+4)}.`);}
  if(!txt) set(isDay?"🌤️":"🌙","steady",`Sin cambios notorios: máxima de ${R(tmax)}° y mínima de ${R(tmin)}°.`);
  return {icon,text:txt,kind};
}

// ---------------------------------------------------------------- ambiental (Chile)
async function buildAir(loc){
  const o=await httpJson(`https://air-quality-api.open-meteo.com/v1/air-quality?latitude=${loc.lat.toFixed(4)}&longitude=${loc.lon.toFixed(4)}&timezone=America/Santiago&current=pm2_5,pm10&hourly=pm2_5&forecast_days=1`);
  if(!o||typeof o!=="object") return [null,"normal"];
  const cur=o.current||{}, hh=(o.hourly&&o.hourly.pm2_5)||[], pm25=cur.pm2_5||0, pm10=cur.pm10||0;
  let sum=0,c=0; hh.forEach((v,i)=>{v=v||0;if(v>0||i===0){sum+=v;c++;}});
  const m=c?sum/c:pm25;
  let level="Bueno",color="#34C759",key="normal";
  if(m>=170){level="Emergencia";color="#AF52DE";key="emergencia";}
  else if(m>=110){level="Preemergencia";color="#FF3B30";key="preemergencia";}
  else if(m>=80){level="Alerta";color="#FF9500";key="alerta";}
  else if(m>=50){level="Regular";color="#FFD426";}
  const r1=x=>Math.round(x*10)/10;
  return [{pm25:r1(pm25),pm10:r1(pm10),mean24:r1(m),level,color,key},key];
}
function buildMoon(){
  const names=["Luna nueva","Luna creciente","Cuarto creciente","Gibosa creciente","Luna llena","Gibosa menguante","Cuarto menguante","Luna menguante"];
  const emo=["🌑","🌒","🌓","🌔","🌕","🌖","🌗","🌘"], syn=29.53058867;
  const days=(Date.now()-Date.UTC(2000,0,6))/86400000;
  let f=(days%syn)/syn; if(f<0)f+=1;
  const i=Math.floor(f*8+0.5)%8;
  return {name:names[i],emoji:emo[i],illumination:Math.round((1-Math.cos(2*Math.PI*f))/2*100)};
}
const PORTS=[["Arica",-18.48,-70.32],["Iquique",-20.21,-70.15],["Antofagasta",-23.65,-70.40],["Caldera",-27.07,-70.82],
 ["Coquimbo",-29.95,-71.34],["Valparaíso",-33.03,-71.63],["San Antonio",-33.59,-71.61],["Constitución",-35.33,-72.41],
 ["Talcahuano",-36.72,-73.11],["Lebu",-37.61,-73.65],["Corral",-39.87,-73.43],["Puerto Montt",-41.47,-72.94],
 ["Castro",-42.48,-73.76],["Puerto Chacabuco",-45.46,-72.82],["Punta Arenas",-53.16,-70.91]];
async function buildTides(loc){
  let best=0,bd=1e18;
  PORTS.forEach((p,i)=>{const a=p[1]-loc.lat,b=(p[2]-loc.lon)*Math.cos(loc.lat*Math.PI/180),d=a*a+b*b;if(d<bd){bd=d;best=i;}});
  const P=PORTS[best], dist=Math.sqrt(bd)*111;
  const o=await httpJson(`https://marine-api.open-meteo.com/v1/marine?latitude=${P[1].toFixed(3)}&longitude=${P[2].toFixed(3)}&timezone=America/Santiago&hourly=sea_level_height_msl&forecast_days=2`);
  const hh=o&&o.hourly, y=hh&&hh.sea_level_height_msl;
  if(!y||y.length<6) return null;
  let hi=null,lo=null;
  for(let i=1;i+1<y.length;i++){
    const a=y[i-1]||0,b=y[i]||0,c=y[i+1]||0;
    if(!hi&&b>=a&&b>=c) hi=[hh.time[i],Math.round(b*100)/100];
    else if(!lo&&b<=a&&b<=c) lo=[hh.time[i],Math.round(b*100)/100];
    if(hi&&lo)break;
  }
  return {port:P[0],coastal:dist<15,dist_km:Math.round(dist),next_high:hi,next_low:lo};
}
// ¿hay que consultar airerm? (mismas condiciones que la rama «aplica» de escritorio)
function restrRige(loc){const t=new Date(),wd=(t.getDay()+6)%7,m=t.getMonth()+1;return !!loc.rm_restr&&m>=5&&m<=8&&wd<5;}
function buildRestriccion(loc,key,of){
  const t=new Date(), wd=(t.getDay()+6)%7, m=t.getMonth()+1, season=m>=5&&m<=8;
  const dig=["8 y 9","0 y 1","2 y 3","4 y 5","6 y 7"];
  if(!loc.rm_restr) return {aplica:false,motivo:"La restricción por dígitos rige en la Provincia de Santiago, San Bernardo y Puente Alto."};
  if(!season) return {aplica:false,motivo:"Fuera de temporada (rige del 1 de mayo al 31 de agosto)."};
  if(wd>=5) return {aplica:false,motivo:"Fin de semana: sin restricción (rige lunes a viernes)."};
  const level=key==="alerta"?"Alerta":key==="preemergencia"?"Preemergencia":key==="emergencia"?"Emergencia":"Normal";
  // episodio OFICIAL de airerm (tabla datada de HOY): sólo SUBE el nivel; los dígitos siguen del calendario
  const epi=of&&of.oficial&&(of.episodio==="Preemergencia"||of.episodio==="Emergencia")?of.episodio:null;
  return {aplica:true,level:epi||level,digitos:dig[wd],oficial:!!epi};
}
function buildSuspension(day,of){
  let peor=0,cuando="",motivo="";
  const lbl=["hoy","mañana","pasado mañana"];
  for(let i=0;i<3&&i<day.time.length;i++){
    const code=num(day.weather_code,i),gust=num(day.wind_gusts_10m_max,i),rain=num(day.precipitation_sum,i);
    const snow=(code>=71&&code<=77)||code===85||code===86, storm=code>=95;
    let r=0;
    if(gust>=90||rain>=60||storm||(snow&&rain>=20)) r=2; else if(gust>=65||rain>=30||snow) r=1;
    if(r>peor){peor=r;cuando=lbl[i];
      motivo=storm?"tormenta eléctrica prevista":snow?`nieve (viento ${Math.round(gust)} km/h)`:`viento ${Math.round(gust)} km/h y ${Math.round(rain)} mm de lluvia`;}
  }
  const niveles=["Sin aviso","Posible suspensión por temporal","Suspensión probable por temporal"];
  // dato OFICIAL de Mineduc: sólo afirma «suspendidas» si la fuente lo dice para esa fecha y alcance
  const dias=of&&Array.isArray(of.dias)?of.dias:[], s=dias.find(d=>d&&d.suspendido);
  if(s){const ct=s.etiqueta==="hoy"?"hoy":s.etiqueta==="manana"?"mañana":"pasado mañana";
    return {nivel:`Clases suspendidas ${ct}`,riesgo:2,cuando:ct,motivo:"informado por el Ministerio de Educación",
      oficial:true,fuente:s.fuente||"https://www.mineduc.cl/"};}
  return {nivel:niveles[peor],riesgo:peor,cuando:peor?cuando:null,motivo:peor?motivo:"sin eventos severos previstos",oficial:!!(of&&of.oficial)};
}

// ---------------------------------------------------------------- análisis completo
async function analyze(loc){
  const w=await httpJson(`https://api.open-meteo.com/v1/forecast?latitude=${loc.lat.toFixed(4)}&longitude=${loc.lon.toFixed(4)}&timezone=America/Santiago`+
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,"+
    "wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure,pressure_msl,cloud_cover,dew_point_2m"+
    "&hourly=temperature_2m,weather_code,precipitation_probability,visibility,is_day"+
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,"+
    "precipitation_probability_max,precipitation_sum,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant"+
    "&forecast_days=10&wind_speed_unit=kmh");
  if(!w||!w.current||!w.daily) return null;
  const cur=w.current, day=w.daily, code=cur.weather_code|0, m=WMO[code]||["☁️","—","cloud"], R=Math.round;
  // aire, mareas y fuentes oficiales en paralelo (en escritorio eran secuenciales)
  const [[air,key],tides,of]=await Promise.all([buildAir(loc),buildTides(loc),oficial(loc,restrRige(loc))]);
  return {
    location:{name:loc.name,region:loc.region||"",lat:loc.lat,lon:loc.lon,comuna:!!loc.comuna,rm_restr:!!loc.rm_restr},
    current:{temp:R(cur.temperature_2m||0),feels:R(cur.apparent_temperature||0),icon:m[0],text:m[1],class:m[2],is_day:!!cur.is_day,
      humidity:cur.relative_humidity_2m|0,wind:R(cur.wind_speed_10m||0),gust:R(cur.wind_gusts_10m||0),wind_dir:cur.wind_direction_10m||0,
      pressure:R(cur.pressure_msl!=null?cur.pressure_msl:(cur.surface_pressure||0)),cloud:cur.cloud_cover|0,precip:cur.precipitation||0,
      dew:cur.dew_point_2m!=null?R(cur.dew_point_2m):null},
    today:{max:R(num(day.temperature_2m_max,0)),min:R(num(day.temperature_2m_min,0)),uv:num(day.uv_index_max,0),
      sunrise:day.sunrise[0]||"",sunset:day.sunset[0]||""},
    rain:buildRain(day),
    context:buildContext(w,cur,day),
    ambiental:{air,moon:buildMoon(),tides,restriccion:buildRestriccion(loc,key,of.restriccion),suspension:buildSuspension(day,of.suspension)},
    fetched:Math.floor(Date.now()/1000),
    raw:w,
  };
}

// ---------------------------------------------------------------- recuadros Bici/Metro: viven en ui/app.js (compartidos con
// Linux); aquí sólo se entrega el estado del Metro: la app Metro (proveedor) o, si no está, metro.cl directo.
const metroProv=()=>new Promise(r=>{const id=++seq;waiting[id]=t=>{try{r(JSON.parse(t));}catch(e){r(null);}};A.metro?A.metro(id):r(null);});
async function estadoMetro(){
  let m=await metroProv();
  if(!m||!m.ok){
    const e=await httpJson("https://www.metro.cl/api/estadoRedDetalle.php");
    if(e&&e.l1){ const p=[]; Object.keys(e).forEach(k=>{const L=e[k]; if(!L||!L.estaciones)return; const l="L"+k.slice(1).toUpperCase();
      if(L.estado!=="1") p.push({linea:l,texto:L.mensaje_app||"Con problemas"});
      L.estaciones.forEach(s=>{if(s.estado!=="1") p.push({linea:l,texto:`${s.nombre}: ${s.descripcion_app||"Con problemas"}`});});});
      m={ok:true,problemas:p}; }
  }
  window.onMetro&&window.onMetro(m||{ok:false});
}

// ---------------------------------------------------------------- ciclo de la app
let LOC=null, busy=false, pending=null, manual=false, lastFetch=0;
function cachedData(){const t=A.cacheLoad();if(!t)return null;try{const d=JSON.parse(t);d.cached=true;return d;}catch(e){return null;}}
async function fetchFor(loc){
  if(busy){pending=loc;return;}
  busy=true;
  try{
    const d=await analyze(loc);
    if(d){LOC=loc;lastFetch=Date.now();A.cacheSave(JSON.stringify(d));window.onClimaData(d);
      const am=d.ambiental;console.info(`datos ${d.location.name}: ${d.current.temp}° · clases «${am.suspension.nivel}» (oficial ${am.suspension.oficial}) · restricción ${am.restriccion.aplica?am.restriccion.level:"no aplica"} · ${d.context.text}`);}
    else{const c=cachedData(); if(c) window.onClimaData(c); else window.onError("sin conexión y sin datos en caché");}
  }catch(e){window.onError(String(e&&e.message||e));}
  busy=false;
  if(pending){const p=pending;pending=null;fetchFor(p);}
}
async function locateAndFetch(){
  const p=await phoneLoc();
  if(p){fetchFor(locFromCoords(p.lat,p.lon));return;}
  // sin permiso o sin señal: ubicación aproximada por IP (HTTPS) y, si no, Santiago
  const ip=await httpJson("https://ipwho.is/");
  if(ip&&typeof ip.latitude==="number") fetchFor(locFromCoords(ip.latitude,ip.longitude,ip.city,ip.region));
  else fetchFor(LOC||SANTIAGO);
}
// Mismo contrato que el puente WebKit de escritorio (search / city / locate / refresh).
window.webkit={messageHandlers:{bridge:{postMessage(s){
  let m={};try{m=JSON.parse(s);}catch(e){return;}
  if(m.action==="search") window.onSearchResults(search(m.q||"",8));
  else if(m.action==="city"&&m.name){
    const c=nearest(m.lat,m.lon); manual=true;
    fetchFor({name:m.name,region:m.region||"",lat:+m.lat||0,lon:+m.lon||0,comuna:true,rm_restr:!!(c&&c.rm_restr)});
  }
  else if(m.action==="locate"){manual=false;locateAndFetch();}
  else if(m.action==="metro") estadoMetro();
  else if(m.action==="abrirBici"){ if(A.hayBici()) A.abrirBici(); }
  else if(m.action==="abrirMetro"){ if(A.hayMetro()) A.abrirMetro(); }
  else if(m.action==="refresh"){ if(LOC) fetchFor(LOC); else locateAndFetch(); }
}}}};
window.climaResume=()=>{ if(Date.now()-lastFetch>REFRESH_MS) (manual&&LOC)?fetchFor(LOC):locateAndFetch(); };
window.addEventListener("load",()=>{
  const c=cachedData();
  if(c){ window.onClimaData(c); const l=c.location; if(l&&l.name){LOC=l;} }
  // abre al tiro con el caché; luego, dato fresco: ubicación del teléfono (o la del caché si no hay)
  locateAndFetch();
  setInterval(()=>{ if(!document.hidden) (manual&&LOC)?fetchFor(LOC):locateAndFetch(); },REFRESH_MS);
});
})();

// SkyFX — animaciones del clima (canvas 2D, con profundidad por capas). Kotone 2026-07-16; catálogo completo 2026-10-31.
// Escena = «<condición>-<day|night>» (o «dawn»/«dusk»). Cada condición WMO de Open-Meteo tiene su receta:
// despejado, mayormente despejado, parcial, cubierto, niebla, niebla con escarcha, llovizna (normal/intensa/engelante),
// lluvia (débil/moderada/intensa/engelante), chubascos (débiles/moderados/violentos), nieve (débil/moderada/intensa),
// granos de nieve, chubascos de nieve, tormenta y tormenta con granizo — todas en versión día y noche.
// Optimizado para hardware modesto (tope 30 fps, sprites prerenderizados, pausa al ocultarse).
"use strict";

function rnd(a,b){return a+Math.random()*(b-a);}

// Receta por condición. clouds = n.º de nubes; dark = nubes de tormenta; sun/moon = astro visible (0..1 = opacidad);
// stars = n.º de estrellas de noche; drops/flakes/hail = partículas; dl/ds = largo/velocidad de gota (×);
// ice = precipitación engelante; frost = escarcha; gusty = ráfagas fuertes; mist = neblina a ras de suelo; bolt = rayos.
const RECETAS = {
  "clear":             {sun:1, moon:1, stars:110, clouds:0, shoot:true},
  "mostly":            {sun:1, moon:1, stars:80, clouds:2},
  "partly":            {sun:.85, moon:.85, stars:45, clouds:4},
  "overcast":          {clouds:7, grey:true, stars:0},
  "fog":               {clouds:2, grey:true, fog:14},
  "rime":              {clouds:2, grey:true, fog:16, frost:true},
  "drizzle":           {clouds:6, grey:true, drops:70, dl:.45, ds:.55, mist:true},
  "drizzle-heavy":     {clouds:6, grey:true, drops:130, dl:.5, ds:.6, mist:true},
  "freezing-drizzle":  {clouds:6, grey:true, drops:80, dl:.45, ds:.55, ice:true, frost:true},
  "rain-light":        {clouds:6, grey:true, drops:90},
  "rain":              {clouds:6, grey:true, drops:150, mist:true},
  "rain-heavy":        {clouds:7, dark:true, drops:270, dl:1.15, ds:1.2, mist:true},
  "freezing-rain":     {clouds:7, grey:true, drops:160, ice:true, frost:true, mist:true},
  "showers-light":     {sun:.6, moon:.5, stars:20, clouds:4, drops:80, gusty:true},
  "showers":           {clouds:5, grey:true, drops:170, gusty:true, mist:true},
  "showers-violent":   {clouds:7, dark:true, drops:300, dl:1.2, ds:1.3, gusty:true, mist:true},
  "snow-light":        {clouds:5, grey:true, flakes:60},
  "snow":              {clouds:6, grey:true, flakes:140},
  "snow-heavy":        {clouds:7, grey:true, flakes:260, mist:true},
  "snow-grains":       {clouds:6, grey:true, flakes:170, grains:true},
  "snow-showers":      {clouds:5, grey:true, flakes:160, gusty:true},
  "snow-showers-heavy":{clouds:7, grey:true, flakes:280, gusty:true, mist:true},
  "storm":             {clouds:7, dark:true, drops:230, mist:true, bolt:true},
  "storm-hail":        {clouds:7, dark:true, drops:180, hail:80, mist:true, bolt:true},
  "storm-hail-heavy":  {clouds:7, dark:true, drops:220, hail:160, mist:true, bolt:true},
  // compatibilidad con los nombres de escena anteriores
  "cloud":             {clouds:5, stars:30},
};

class SkyFX{
  constructor(canvas){
    this.c=canvas; this.ctx=canvas.getContext("2d");
    this.kind="clear-day"; this.wind=0; this.t=0; this.running=false;
    this.frameMin=1000/30; this.lastDraw=0;
    this.dpr=1; this.sp={};
    this.drops=[]; this.splashes=[]; this.flakes=[]; this.hail=[]; this.bounces=[]; this.stars=[]; this.clouds=[]; this.fog=[]; this.frost=[]; this.shoot=null;
    this.flash=0; this.nextFlash=rnd(2.5,6); this.bolt=null;
    window.addEventListener("resize",()=>this.resize());
    document.addEventListener("visibilitychange",()=>{
      if(document.hidden) this.running=false;
      else if(!this.running){this.running=true;this.last=performance.now();this.loop();}
    });
    this.resize();
  }
  spr(key,w,h,fn){let c=this.sp[key];if(!c){c=document.createElement('canvas');c.width=Math.ceil(w);c.height=Math.ceil(h);fn(c.getContext('2d'),c.width,c.height);this.sp[key]=c;}return c;}
  resize(){
    this.w=window.innerWidth; this.h=window.innerHeight;
    this.c.width=this.w*this.dpr; this.c.height=this.h*this.dpr;
    this.ctx.setTransform(this.dpr,0,0,this.dpr,0,0);
    this.sp={}; this.build();
  }
  // «rain-night» → {base:"rain", night:true}; «dawn»/«dusk» son escenas propias de luz rasante.
  parse(kind){
    if(kind==="dawn"||kind==="dusk") return {base:kind, night:false, r:{sun:1, clouds:3, warm:true}};
    const m=/^(.*)-(day|night)$/.exec(kind||"");
    const base=m?m[1]:(kind||"clear"), night=m?m[2]==="night":false;
    const viejos={rain:"rain", snow:"snow", fog:"fog", storm:"storm"};      // escenas antiguas sin sufijo
    return {base, night: m?night:(base in viejos ? !this.isDay : false), r: RECETAS[base]||RECETAS.cloud};
  }
  set(kind,wind,isDay){
    const changed = kind!==this.kind;
    this.kind=kind||"clear-day"; this.wind=Math.max(-1.4,Math.min((wind||0)/38,1.4));
    this.isDay = isDay===undefined ? (this.kind.indexOf("night")<0) : !!isDay;
    this.sc=this.parse(this.kind);
    if(this.sc.night) this.isDay=false;
    const r=this.sc.r;
    // tono de las nubes: cálido con luz rasante, gris/plomo con lluvia o tormenta, azulado de noche
    if(r.warm) this.tone="255,214,170";
    else if(r.dark) this.tone=this.sc.night?"92,100,124":"128,136,156";
    else if(r.grey) this.tone=this.sc.night?"128,140,166":"206,214,226";
    else this.tone=this.sc.night?"150,164,196":"255,255,255";
    if(changed) this.build();
    if(!this.running){this.running=true;this.last=performance.now();this.loop();}
  }
  build(){
    const w=this.w,h=this.h; if(!this.sc) this.sc=this.parse(this.kind);
    const r=this.sc.r, night=this.sc.night;
    this.drops=[];this.splashes=[];this.flakes=[];this.hail=[];this.bounces=[];this.stars=[];this.clouds=[];this.fog=[];this.frost=[];this.shoot=null;
    const area=Math.max(0.6,Math.min(1.6,(w*h)/(1280*800)));          // densidad proporcional a la pantalla
    if(r.drops){ const n=Math.round(r.drops*area), dl=r.dl||1, ds=r.ds||1;
      for(let i=0;i<n;i++){const z=rnd(0.35,1); this.drops.push({x:rnd(-40,w),y:rnd(0,h),z,l:(8+z*16)*dl,s:(6+z*10)*ds});} }
    if(r.flakes){ const n=Math.round(r.flakes*area);
      for(let i=0;i<n;i++){const z=rnd(0.3,1); this.flakes.push({x:rnd(0,w),y:rnd(0,h),z,r:r.grains?(0.6+z*1.2):(1+z*3),s:(r.grains?1.2:0.4)+z*(r.grains?2.2:1.4),ph:rnd(0,6.28)});} }
    if(r.hail){ const n=Math.round(r.hail*area);
      for(let i=0;i<n;i++){const z=rnd(0.4,1); this.hail.push({x:rnd(0,w),y:rnd(-h,h),z,r:1.4+z*2.6,s:9+z*9});} }
    if(night && r.stars){ for(let i=0;i<r.stars;i++) this.stars.push({x:rnd(0,w),y:rnd(0,h*0.75),r:rnd(.4,1.7),ph:rnd(0,6.28),sp:rnd(.5,2.2)}); }
    for(let i=0;i<(r.clouds||0);i++){const z=rnd(0.4,1);
      this.clouds.push({x:rnd(-200,w),y:rnd(h*0.03,h*(r.dark?0.36:0.42)),z,s:(80+z*150)*(r.dark?1.25:1),v:(.04+z*.14)*(r.gusty?1.8:1),o:(.06+z*.16)*(r.dark?1.5:r.grey?1.25:1)});}
    if(r.fog){ for(let i=0;i<r.fog;i++) this.fog.push({x:rnd(-200,w),y:rnd(h*.25,h),r:rnd(120,320),v:rnd(.05,.22),o:rnd(.04,.10),ph:rnd(0,6.28)}); }
    if(r.mist){ for(let i=0;i<4;i++) this.fog.push({x:rnd(-200,w),y:h-rnd(0,70),r:rnd(160,300),v:rnd(.04,.14),o:rnd(.03,.07),ph:rnd(0,6.28)}); }
    if(r.frost){ for(let i=0;i<46;i++) this.frost.push({x:rnd(0,w),y:rnd(h*0.55,h),r:rnd(.6,1.8),ph:rnd(0,6.28),sp:rnd(.4,1.6)}); }
  }
  loop(){
    if(!this.running) return;
    const now=performance.now();
    if(now-this.lastDraw < this.frameMin){ requestAnimationFrame(()=>this.loop()); return; }
    this.lastDraw=now;
    const dt=Math.min((now-(this.last||now))/16.7,3); this.last=now;
    this.t+=dt;
    const ctx=this.ctx,w=this.w,h=this.h, sc=this.sc||this.parse(this.kind), r=sc.r, night=sc.night, base=sc.base;
    ctx.clearRect(0,0,w,h);

    // ---- luz rasante (amanecer/atardecer) ----
    if(base==="dawn"||base==="dusk"){
      const gy=h*0.62, g=ctx.createLinearGradient(0,gy-h*0.3,0,h);
      g.addColorStop(0,"rgba(255,180,120,0)");
      g.addColorStop(0.6,base==="dawn"?"rgba(255,170,110,0.16)":"rgba(255,140,90,0.18)");
      g.addColorStop(1,base==="dawn"?"rgba(255,200,150,0.30)":"rgba(255,120,80,0.32)");
      ctx.fillStyle=g; ctx.fillRect(0,gy-h*0.3,w,h);
    }
    // ---- astros ----
    if(base==="dawn") this.sun(w*0.22,h*0.52,true,1);
    else if(base==="dusk") this.sun(w*0.80,h*0.54,true,1);
    else if(!night && r.sun) this.sun(w*0.80,h*0.16,false,r.sun);
    if(night){
      this.stars.forEach(s=>this.star(s,dt));
      if(r.moon) this.moon(w*0.80,h*0.17,r.moon);
      if(r.shoot) this.shootingStar(dt);
    }
    // ---- nubes ----
    for(const c of this.clouds){
      c.x+=c.v*(1+this.wind*3)*dt; if(c.x-c.s>w) c.x=-c.s*1.5;
      this.cloud(c.x,c.y,c.s,c.o,c.z);
    }
    // ---- niebla / neblina ----
    const fogTint = night ? "150,160,180" : (r.frost ? "238,246,255" : "235,240,246");
    for(const f of this.fog){
      f.ph+=0.005*dt; f.x+=(f.v*(1+Math.abs(this.wind)*2))*dt; if(f.x-f.r>w) f.x=-f.r;
      const oy=f.y+Math.sin(f.ph)*8, R=Math.round(f.r/40)*40;
      const sp=this.spr("fog"+R+fogTint,R*2,R*1.2,(g2,W,H)=>{
        const g=g2.createRadialGradient(W/2,H/2,R*0.1,W/2,H/2,R);
        g.addColorStop(0,`rgba(${fogTint},1)`); g.addColorStop(1,`rgba(${fogTint},0)`);
        g2.fillStyle=g; g2.beginPath(); g2.ellipse(W/2,H/2,R,R*0.6,0,0,6.283); g2.fill();});
      ctx.globalAlpha=f.o*1.6; ctx.drawImage(sp,f.x-R,oy-R*0.6); ctx.globalAlpha=1;
    }
    // ---- lluvia / llovizna (engelante = gotas pálidas y heladas) ----
    if(this.drops.length){
      const gust=Math.sin(this.t*0.008)*0.35 + Math.sin(this.t*0.021)*0.15 + (r.gusty?Math.sin(this.t*0.05)*0.55:0);
      const ang=(this.wind+gust)*7;
      const col = r.ice ? (night?"190,214,236":"226,240,252") : (night?"150,176,214":"176,208,244");
      ctx.lineCap="round";
      const B=[[],[],[],[]];
      for(const d of this.drops){
        B[Math.min(3,(d.z-0.35)/0.65*4|0)].push(d);
        d.y+=d.s*dt; d.x+=ang*d.z*0.5*dt;
        if(d.y>h){
          if(d.z>0.7 && this.splashes.length<24) this.splashes.push({x:d.x,y:h-rnd(2,10),r:1,a:0.5,ice:!!r.ice});
          d.y=-10; d.x=rnd(-40,w);
        }
      }
      for(let i=0;i<4;i++){const z=0.35+(i+0.5)*0.1625;
        ctx.strokeStyle=`rgba(${col},${0.12+z*0.34})`; ctx.lineWidth=(0.6+z*1.1)*(r.dl&&r.dl<0.6?0.75:1); ctx.beginPath();
        for(const d of B[i]){ctx.moveTo(d.x,d.y); ctx.lineTo(d.x+ang*d.z,d.y+d.l);}
        ctx.stroke();
      }
      for(let i=this.splashes.length-1;i>=0;i--){const s=this.splashes[i];
        if(s.ice){ ctx.fillStyle=`rgba(236,246,255,${s.a})`; ctx.fillRect(s.x-1,s.y-1,2,2); s.a-=0.03*dt; }   // perdigón de hielo
        else { ctx.strokeStyle=`rgba(190,216,246,${s.a})`; ctx.lineWidth=1; ctx.beginPath(); ctx.arc(s.x,s.y,s.r,Math.PI,2*Math.PI); ctx.stroke(); s.r+=0.8*dt; s.a-=0.04*dt; }
        if(s.a<=0) this.splashes.splice(i,1);
      }
    }
    // ---- nieve / granos de nieve ----
    const flakeCol = night ? "#dfe6f5" : "#fff";
    for(const f of this.flakes){
      f.ph+=0.02*dt; f.x+=(Math.sin(f.ph)*(r.grains?0.2:0.5)+this.wind*2*f.z*(r.gusty?2:1))*dt; f.y+=f.s*dt;
      if(f.y>h){f.y=-6;f.x=rnd(0,w);}
      ctx.globalAlpha=0.35+f.z*0.6; ctx.fillStyle=flakeCol;
      ctx.beginPath(); ctx.arc(f.x,f.y,f.r,0,6.283); ctx.fill();
    }
    ctx.globalAlpha=1;
    // ---- granizo: bolitas que caen rápido y rebotan ----
    if(this.hail.length){
      for(const g of this.hail){
        g.y+=g.s*dt; g.x+=this.wind*3*g.z*dt;
        if(g.y>h-2){ if(this.bounces.length<40) this.bounces.push({x:g.x,y:h-2,vy:-rnd(2,4.5)*g.z,vx:rnd(-1.5,1.5),r:g.r*0.8,a:0.9}); g.y=-rnd(10,h*0.5); g.x=rnd(0,w); }
        ctx.globalAlpha=0.55+g.z*0.4; ctx.fillStyle="#f4f8ff"; ctx.beginPath(); ctx.arc(g.x,g.y,g.r,0,6.283); ctx.fill();
      }
      for(let i=this.bounces.length-1;i>=0;i--){const b=this.bounces[i];
        b.vy+=0.35*dt; b.y+=b.vy*dt; b.x+=b.vx*dt; b.a-=0.025*dt;
        ctx.globalAlpha=Math.max(0,b.a); ctx.fillStyle="#f4f8ff"; ctx.beginPath(); ctx.arc(b.x,Math.min(b.y,h-1),b.r,0,6.283); ctx.fill();
        if(b.a<=0||b.y>h) this.bounces.splice(i,1);
      }
      ctx.globalAlpha=1;
    }
    // ---- escarcha: destellos cristalinos a ras de suelo ----
    for(const s of this.frost){ s.ph+=0.03*s.sp*dt; const a=Math.max(0,Math.sin(s.ph));
      ctx.globalAlpha=a*0.8; ctx.fillStyle="#f2f9ff"; ctx.beginPath(); ctx.arc(s.x,s.y,s.r,0,6.283); ctx.fill();
      if(a>0.85){ ctx.strokeStyle="rgba(242,249,255,0.6)"; ctx.lineWidth=0.6; ctx.beginPath(); ctx.moveTo(s.x-3,s.y); ctx.lineTo(s.x+3,s.y); ctx.moveTo(s.x,s.y-3); ctx.lineTo(s.x,s.y+3); ctx.stroke(); } }
    ctx.globalAlpha=1;
    // ---- tormenta: relámpago ramificado + doble destello ----
    if(r.bolt){
      this.nextFlash-=dt/60;
      if(this.nextFlash<=0){ this.flash=1; this.bolt=this.makeBolt(); this.nextFlash=rnd(3,7); }
      if(this.flash>0){
        ctx.fillStyle=`rgba(210,225,255,${this.flash*(night?0.55:0.45)})`; ctx.fillRect(0,0,w,h);
        if(this.bolt && this.flash>0.55) this.drawBolt(this.bolt);
        this.flash-=0.05*dt;
        if(this.flash>0.3 && this.flash<0.5) this.flash=0.75;
      }
    }
    // ---- viñeta atmosférica ----
    const dia=!night;
    ctx.drawImage(this.spr("vig"+dia,w,h,(g2,W,H)=>{
      const vg=g2.createRadialGradient(W/2,H*0.42,Math.min(W,H)*0.35,W/2,H*0.5,Math.max(W,H)*0.72);
      vg.addColorStop(0,"rgba(0,0,0,0)");
      vg.addColorStop(1, dia?"rgba(10,20,40,0.16)":"rgba(0,0,10,0.28)");
      g2.fillStyle=vg; g2.fillRect(0,0,W,H);}),0,0);
    requestAnimationFrame(()=>this.loop());
  }
  // ----- primitivas -----
  sun(x,y,warm,alpha){
    const ctx=this.ctx, rot=this.t*0.002; ctx.globalAlpha=alpha==null?1:alpha;
    const rays=this.spr("rays"+warm,260,260,(g2)=>{ g2.translate(130,130);
      for(let i=0;i<16;i++){const a=i*Math.PI/8, len=114;
        const g=g2.createLinearGradient(Math.cos(a)*54,Math.sin(a)*54,Math.cos(a)*len,Math.sin(a)*len);
        g.addColorStop(0,warm?"rgba(255,180,110,0.22)":"rgba(255,240,180,0.20)");
        g.addColorStop(1,"rgba(255,220,150,0)");
        g2.strokeStyle=g; g2.lineWidth=4;
        g2.beginPath(); g2.moveTo(Math.cos(a)*54,Math.sin(a)*54); g2.lineTo(Math.cos(a)*len,Math.sin(a)*len); g2.stroke();}});
    ctx.save(); ctx.translate(x,y); ctx.rotate(rot); ctx.drawImage(rays,-130,-130); ctx.restore();
    const body=this.spr("sun"+warm,180,180,(g2)=>{
      const halo=g2.createRadialGradient(90,90,10,90,90,86);
      halo.addColorStop(0, warm?"rgba(255,225,150,0.95)":"rgba(255,245,200,0.95)");
      halo.addColorStop(0.5, warm?"rgba(255,170,90,0.35)":"rgba(255,225,140,0.35)");
      halo.addColorStop(1,"rgba(255,200,120,0)");
      g2.fillStyle=halo; g2.beginPath(); g2.arc(90,90,86,0,6.283); g2.fill();
      g2.fillStyle=warm?"#FFD08A":"#FFF0B0"; g2.beginPath(); g2.arc(90,90,36,0,6.283); g2.fill();});
    ctx.drawImage(body,x-90,y-90); ctx.globalAlpha=1;
  }
  star(s,dt){const ctx=this.ctx; s.ph+=0.02*s.sp*dt; const a=0.35+0.65*Math.abs(Math.sin(s.ph));
    ctx.globalAlpha=a; ctx.fillStyle="#fff"; ctx.beginPath(); ctx.arc(s.x,s.y,s.r,0,6.283); ctx.fill(); ctx.globalAlpha=1;}
  shootingStar(dt){
    const ctx=this.ctx;
    if(!this.shoot && Math.random()<0.004*dt){ this.shoot={x:rnd(this.w*0.2,this.w*0.8),y:rnd(20,this.h*0.35),len:0,vx:rnd(6,10),vy:rnd(2,4),life:1}; }
    if(this.shoot){const s=this.shoot;
      const g=ctx.createLinearGradient(s.x,s.y,s.x-s.vx*8,s.y-s.vy*8);
      g.addColorStop(0,`rgba(255,255,255,${s.life})`); g.addColorStop(1,"rgba(255,255,255,0)");
      ctx.strokeStyle=g; ctx.lineWidth=2; ctx.beginPath(); ctx.moveTo(s.x,s.y); ctx.lineTo(s.x-s.vx*8,s.y-s.vy*8); ctx.stroke();
      s.x+=s.vx*dt; s.y+=s.vy*dt; s.life-=0.02*dt; if(s.life<=0||s.x>this.w) this.shoot=null;
    }
  }
  moon(x,y,alpha){
    const sp=this.spr("moon",120,120,(g2)=>{
      const g=g2.createRadialGradient(60,60,6,60,60,58);
      g.addColorStop(0,"rgba(232,240,255,0.95)"); g.addColorStop(0.55,"rgba(210,222,255,0.25)"); g.addColorStop(1,"rgba(200,215,255,0)");
      g2.fillStyle=g; g2.beginPath(); g2.arc(60,60,58,0,6.283); g2.fill();
      g2.fillStyle="#EAF0FF"; g2.beginPath(); g2.arc(60,60,28,0,6.283); g2.fill();
      g2.fillStyle="rgba(196,208,242,0.55)";
      [[ -9,-7,5],[8,5,3.5],[2,-11,3],[-3,9,4]].forEach(([dx,dy,r])=>{g2.beginPath();g2.arc(60+dx,60+dy,r,0,6.283);g2.fill();});});
    this.ctx.globalAlpha=alpha==null?1:alpha; this.ctx.drawImage(sp,x-60,y-60); this.ctx.globalAlpha=1;
  }
  cloud(x,y,s,o,z){const tone=this.tone||"255,255,255";
    const S=Math.round(s/20)*20, O=Math.min(0.5,Math.round(o*100)/100);
    const sp=this.spr(`c${S}|${O}|${tone}`,S*1.4,S*0.9,(g2,W,H)=>{
      g2.filter=`blur(${Math.max(2,S*0.045)}px)`;
      const puffs=[[0,0,.52],[.30,-.10,.44],[.58,.02,.5],[.26,.14,.6],[.72,.16,.4],
                   [.12,.12,.42],[.44,-.04,.38],[.86,.08,.34],[.50,.20,.5],[-.06,.16,.34]];
      for(const[dx,dy,r] of puffs){
        const cx=S*0.25+dx*S, cy=H*0.4+dy*S, rad=r*S*0.5;
        const g=g2.createRadialGradient(cx,cy-rad*0.35,rad*0.15,cx,cy,rad);
        g.addColorStop(0,`rgba(${tone},${O*1.5})`);
        g.addColorStop(0.6,`rgba(${tone},${O*0.7})`);
        g.addColorStop(1,`rgba(${tone},0)`);
        g2.fillStyle=g; g2.beginPath(); g2.ellipse(cx,cy,rad,rad*0.72,0,0,6.283); g2.fill();
      }});
    this.ctx.drawImage(sp,x-S*0.25,y-sp.height*0.4);
  }
  makeBolt(){
    const x=rnd(this.w*0.25,this.w*0.75); let y=0; const segs=[{x,y}];
    while(y<this.h*0.6){ y+=rnd(20,46); segs.push({x:segs[segs.length-1].x+rnd(-26,26),y});
      if(Math.random()<0.25){const b={x:segs[segs.length-1].x,y,br:[]}; let bx=b.x,by=y;
        for(let i=0;i<3;i++){bx+=rnd(-24,24);by+=rnd(14,30);b.br.push({x:bx,y:by});} segs.branch=segs.branch||[]; segs.branch.push(b);}
    }
    return segs;
  }
  drawBolt(segs){const ctx=this.ctx;
    ctx.strokeStyle="rgba(230,240,255,0.95)"; ctx.lineWidth=2.4; ctx.shadowColor="#cfe0ff"; ctx.shadowBlur=16;
    ctx.beginPath(); ctx.moveTo(segs[0].x,segs[0].y);
    for(let i=1;i<segs.length;i++) ctx.lineTo(segs[i].x,segs[i].y);
    ctx.stroke();
    if(segs.branch) for(const b of segs.branch){ctx.beginPath();ctx.moveTo(b.x,b.y);for(const p of b.br)ctx.lineTo(p.x,p.y);ctx.stroke();}
    ctx.shadowBlur=0;
  }
}
// código WMO de Open-Meteo → condición de escena (sin sufijo día/noche)
SkyFX.condicion = function(code){
  const M={0:"clear",1:"mostly",2:"partly",3:"overcast",45:"fog",48:"rime",51:"drizzle",53:"drizzle",55:"drizzle-heavy",
    56:"freezing-drizzle",57:"freezing-drizzle",61:"rain-light",63:"rain",65:"rain-heavy",66:"freezing-rain",67:"freezing-rain",
    71:"snow-light",73:"snow",75:"snow-heavy",77:"snow-grains",80:"showers-light",81:"showers",82:"showers-violent",
    85:"snow-showers",86:"snow-showers-heavy",95:"storm",96:"storm-hail",99:"storm-hail-heavy"};
  return M[code]||"overcast";
};
SkyFX.RECETAS = RECETAS;
window.SkyFX = SkyFX;

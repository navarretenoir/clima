#!/usr/bin/env bash
# clima-notify — notificaciones de CLIMA disparadas por timers de usuario (livianas, sin daemon).
#   clima-notify.sh check      → suspensión de clases (MINEDUC) + alerta de mal tiempo (mañana AM)
#   clima-notify.sh forecast   → pronóstico del día siguiente (22:00)
# Kotone 2026-07-20. No afecta a la app gráfica: es un one-shot que corre, notifica y termina.
#
# FIX 2026-07-21 (Kotone): el patrón `printf '%s' "$JSON" | python3 - <<'PY'` estaba ROTO: el
#   heredoc `<<'PY'` PISA el pipe, así que el programa Python leía `sys.stdin` VACÍO y nunca veía
#   el JSON → forecast con "Mín ?° / Máx ?°" y las alertas de mal tiempo/suspensión NUNCA dispararon.
#   Ahora el JSON (y la salida del scraper) se pasan por VARIABLE DE ENTORNO (os.environ), no por
#   stdin. Además se valida que el JSON traiga DATOS reales (location.name + raw.daily.time) antes de
#   notificar, y en forecast hay una guarda que evita el aviso hueco si por lo que sea faltan datos.
set -uo pipefail

CLIMA_BIN="${CLIMA_BIN:-$HOME/.local/bin/clima}"
SCRAPER="${CLIMA_SCRAPER:-$HOME/Proyectos/CLIMA/c/suspension-scraper}"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/clima"; mkdir -p "$STATE_DIR"
ICON=clima
mode="${1:-check}"

# desktop-entry = el .desktop de la app: así GNOME asocia el aviso a Clima y muestra SU ícono en la
# cabecera de la notificación (sin la pista salía un ícono genérico). Kotone 2026-10-31.
notify(){ notify-send -a "Clima" -i "$ICON" -h string:desktop-entry:io.github.navarretenoir.Clima -u "${3:-normal}" "$1" "$2"; }
# dedupe: no repetir la misma notificación el mismo día
seen(){ local key="$1" today; today=$(date +%F); [ "$(cat "$STATE_DIR/$key" 2>/dev/null)" = "$today" ]; }
mark(){ date +%F > "$STATE_DIR/$1"; }

# Valida que $1 sea JSON de clima CON DATOS reales (no solo que parsee): location.name + raw.daily.time.
valid_data(){ CLIMA_J="$1" python3 - <<'PY' 2>/dev/null
import os,sys,json
try: d=json.loads(os.environ.get("CLIMA_J",""))
except Exception: sys.exit(1)
loc=(d.get("location") or {}).get("name")
daily=(d.get("raw") or {}).get("daily") or {}
sys.exit(0 if (loc and daily.get("time")) else 1)
PY
}

# clima --json es intermitente en este host → reintenta; exige DATOS reales, no solo que parsee.
# Si tras 3 intentos no hay datos válidos, sale en SILENCIO (nunca notifica hueco).
JSON=""
for _ in 1 2 3; do
  J=$(timeout 25 "$CLIMA_BIN" --json 2>/dev/null)
  if valid_data "$J"; then JSON="$J"; break; fi
  sleep 3
done
[ -z "$JSON" ] && exit 0

REGION=$(CLIMA_J="$JSON" python3 -c "import os,json;print((json.loads(os.environ['CLIMA_J']).get('location') or {}).get('region','') or '')" 2>/dev/null)
COMUNA=$(CLIMA_J="$JSON" python3 -c "import os,json;print((json.loads(os.environ['CLIMA_J']).get('location') or {}).get('name','') or '')" 2>/dev/null)

if [ "$mode" = "forecast" ]; then
  # ---- Pronóstico de MAÑANA (daily[idx de mañana]) ----
  LINE=$(CLIMA_J="$JSON" python3 - <<'PY' 2>/dev/null
import os,sys,json,datetime
d=json.loads(os.environ["CLIMA_J"]); daily=(d.get("raw") or {}).get("daily") or {}
t=daily.get("time",[])
tom=(datetime.date.today()+datetime.timedelta(days=1)).isoformat()
i=t.index(tom) if tom in t else (1 if len(t)>1 else 0)
WC={0:'despejado',1:'mayormente despejado',2:'parcial nublado',3:'nublado',45:'niebla',48:'niebla',
51:'llovizna',53:'llovizna',55:'llovizna',61:'lluvia débil',63:'lluvia',65:'lluvia fuerte',
71:'nieve',73:'nieve',75:'nieve fuerte',80:'chubascos',81:'chubascos',82:'chubascos fuertes',
95:'tormenta',96:'tormenta con granizo',99:'tormenta fuerte'}
def g(k,dv=None):
    a=daily.get(k,[]); return a[i] if i<len(a) else dv
code=g('weather_code',0)
pp=g('precipitation_probability_max',0) or 0; psum=round(g('precipitation_sum',0) or 0,1)
# La lluvia SOLO se menciona si de verdad se espera (prob>=30% o >=0.2mm). Si no, campo vacío.
rain=f"Lluvia {pp}% ({psum} mm). " if (pp>=30 or psum>=0.2) else ""
print(f"{g('temperature_2m_min','?')}|{g('temperature_2m_max','?')}|{round(g('wind_speed_10m_max',0) or 0)}|{WC.get(code,'')}|{rain}")
PY
)
  IFS='|' read -r tmin tmax wind desc rain <<<"$LINE"
  # Guarda anti-aviso-hueco: si faltan comuna o temperaturas, NO notificar.
  if [ -z "$COMUNA" ] || [ -z "$tmin" ] || [ "$tmin" = "?" ] || [ "$tmax" = "?" ]; then exit 0; fi
  body="Mañana en ${COMUNA}: ${desc}. Mín ${tmin}° / Máx ${tmax}°. ${rain}Viento hasta ${wind} km/h."
  # Suspensión de clases de MAÑANA: SOLO se informa si hay una decretada; si no, nada.
  if [ -x "$SCRAPER" ] && [ -n "$REGION" ]; then
    SUS=$(timeout 25 "$SCRAPER" --region "$REGION" --comuna "$COMUNA" 2>/dev/null)
    # El scraper ya resuelve región/comuna con precisión (comuna-primero + guarda de alcance);
    # aquí solo consumimos su veredicto. Se muestra el titular + "(verifica si aplica)".
    SUSM=$(SUS_J="$SUS" python3 - <<'PY' 2>/dev/null
import os,sys,json
try: d=json.loads(os.environ.get("SUS_J",""))
except Exception: sys.exit()
dias=d.get('dias') if isinstance(d.get('dias'),list) else []
m=next((x for x in dias if x.get('etiqueta')=='manana'), {})
if m.get('suspendido'): print((m.get('titular') or 'Suspensión de clases decretada').strip())
PY
)
    [ -n "$SUSM" ] && body="${body}\n🏫 ${SUSM}. (Verifica si aplica a ${COMUNA}.)"
  fi
  notify "🌤️ Pronóstico de mañana" "$(printf '%b' "$body")" normal
  exit 0
fi

# ---- modo check: suspensión de clases + alerta de mal tiempo ----
# 1) Suspensión de clases (MINEDUC, fuente oficial; el scraper solo afirma si confirma región/fecha)
if [ -x "$SCRAPER" ] && [ -n "$REGION" ]; then
  SUS=$(timeout 25 "$SCRAPER" --region "$REGION" --comuna "$COMUNA" 2>/dev/null)
  # El scraper ya resuelve región/comuna con precisión; aquí solo consumimos su veredicto e
  # incluimos el TITULAR real + "(verifica si aplica)". BODY vacío => no hay suspensión hoy.
  BODY=$(SUS_J="$SUS" python3 - <<'PY' 2>/dev/null
import os,sys,json
try: d=json.loads(os.environ.get("SUS_J",""))
except Exception: sys.exit()
dias=d.get('dias') if isinstance(d.get('dias'),list) else [d]
hoy=next((x for x in dias if x.get('etiqueta')=='hoy'), dias[0] if dias else {})
if hoy.get('suspendido'):
    print((hoy.get('titular') or 'Hay suspensión de clases vigente').strip())
PY
)
  if [ -n "$BODY" ] && ! seen suspension; then
    notify "🏫 Suspensión de clases hoy" "${BODY}. (Verifica si aplica a ${COMUNA}.) Fuente: MINEDUC." critical
    mark suspension
  fi
fi

# 2) Alerta de mal tiempo (derivada de los datos que ya trae CLIMA: lluvia/viento/temp/tormenta)
ALERT=$(CLIMA_J="$JSON" python3 - <<'PY' 2>/dev/null
import os,sys,json,datetime
d=json.loads(os.environ["CLIMA_J"]); daily=(d.get("raw") or {}).get("daily") or {}; t=daily.get("time",[])
today=datetime.date.today().isoformat()
i=t.index(today) if today in t else 0
def g(k,dv=0):
    a=daily.get(k,[]); return (a[i] if i<len(a) else dv) or dv
code=g('weather_code',0); psum=g('precipitation_sum',0); pp=g('precipitation_probability_max',0)
wind=g('wind_speed_10m_max',0); tmax=g('temperature_2m_max',0); tmin=g('temperature_2m_min',0)
r=[]
if code in (95,96,99): r.append("tormenta eléctrica")
if psum>=15 or (pp>=80 and psum>=5): r.append(f"lluvia intensa ({round(psum,1)} mm, {pp}%)")
if wind>=50: r.append(f"viento fuerte (hasta {round(wind)} km/h)")
if tmax>=35: r.append(f"calor extremo ({round(tmax)}°)")
if tmin<=-2: r.append(f"frío extremo / helada ({round(tmin)}°)")
print(" · ".join(r))
PY
)
if [ -n "$ALERT" ] && ! seen badweather; then
  notify "⛈️ Alerta de mal tiempo" "Hoy en ${COMUNA}: ${ALERT}." critical
  mark badweather
fi
exit 0

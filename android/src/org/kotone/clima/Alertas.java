package org.kotone.clima;

import android.content.Context;
import android.content.Intent;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.InputStream;
import java.util.Calendar;
import java.util.Locale;

/**
 * Alertas meteorológicas PARA PEDALEAR, calculadas en el teléfono para una posición dada.
 * Las consumen: la app de la bici (ADO O260T, vía ClimaProveedor), la propia interfaz de Clima
 * (tarjeta «Para la bici») y los avisos en segundo plano (empuje a la bici). Fuente: Open-Meteo
 * (pronóstico + calidad del aire); umbrales prudentes de ciclismo urbano. Kotone 2026-10-31.
 *
 * JSON: {ok, lugar, region, temp, texto, codigo, es_dia, viento, rafagas, alertas:[{id,nivel,icono,titulo,detalle}], ts}
 *   nivel: "info" | "warn" | "danger"   (orden de gravedad; la primera alerta es la más grave)
 */
final class Alertas {
    private Alertas() { }

    static final String PKG_BICI = "com.bicipro.app";
    static final String ACCION = "org.kotone.clima.ALERTA";
    static final String PERMISO = "org.kotone.clima.permission.ALERTAS";

    private static JSONArray comunas;

    /** Comuna más cercana (assets/data/comunas_cl.json); null si está a más de ~50 km de todas. */
    static synchronized JSONObject comunaCercana(Context c, double lat, double lon) {
        try {
            if (comunas == null) try (InputStream in = c.getAssets().open("data/comunas_cl.json")) {
                comunas = new JSONArray(Oficial.read(in, 4 << 20));
            }
            JSONObject best = null; double bd = 1e18;
            for (int i = 0; i < comunas.length(); i++) {
                JSONObject o = comunas.getJSONObject(i);
                double a = o.optDouble("lat") - lat, b = o.optDouble("lon") - lon, d = a * a + b * b;
                if (d < bd) { bd = d; best = o; }
            }
            return bd < 0.20 ? best : null;
        } catch (Exception e) { return null; }
    }

    private static boolean lluvia(int c) { return (c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95; }
    private static boolean nieve(int c) { return (c >= 71 && c <= 77) || c == 85 || c == 86; }
    private static boolean tormenta(int c) { return c >= 95; }

    private static final String[][] TXT = {{"0", "Despejado"}, {"1", "Mayormente despejado"}, {"2", "Parcialmente nublado"},
            {"3", "Nublado"}, {"45", "Niebla"}, {"48", "Niebla"}, {"51", "Llovizna"}, {"53", "Llovizna"}, {"55", "Llovizna intensa"},
            {"61", "Lluvia ligera"}, {"63", "Lluvia"}, {"65", "Lluvia fuerte"}, {"71", "Nieve ligera"}, {"73", "Nieve"},
            {"75", "Nieve fuerte"}, {"80", "Chubascos"}, {"81", "Chubascos"}, {"82", "Chubascos violentos"},
            {"95", "Tormenta"}, {"96", "Tormenta con granizo"}, {"99", "Tormenta fuerte"}};
    static String texto(int code) {
        for (String[] t : TXT) if (Integer.parseInt(t[0]) == code) return t[1];
        return "—";
    }

    private static JSONObject alerta(String id, String nivel, String icono, String titulo, String detalle) throws Exception {
        return new JSONObject().put("id", id).put("nivel", nivel).put("icono", icono).put("titulo", titulo).put("detalle", detalle);
    }

    /** Calcula las alertas para (lat, lon). Bloqueante (red): llamar fuera del hilo principal. */
    static JSONObject calcular(Context c, double lat, double lon) {
        JSONObject r = new JSONObject();
        try {
            String w = Oficial.get(String.format(Locale.ROOT,
                    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&timezone=America/Santiago"
                    + "&current=temperature_2m,apparent_temperature,is_day,precipitation,weather_code,wind_speed_10m,wind_gusts_10m"
                    + "&hourly=weather_code,precipitation_probability,temperature_2m,wind_gusts_10m"
                    + "&daily=uv_index_max,temperature_2m_max,temperature_2m_min&forecast_days=2&wind_speed_unit=kmh", lat, lon), 8000, 15000);
            JSONObject o = w == null ? null : new JSONObject(w);
            JSONObject cur = o == null ? null : o.optJSONObject("current");
            if (cur == null) return r.put("ok", false);
            JSONObject h = o.getJSONObject("hourly"), d = o.getJSONObject("daily");
            int code = cur.optInt("weather_code"); boolean dia = cur.optInt("is_day") == 1;
            double temp = cur.optDouble("temperature_2m"), feels = cur.optDouble("apparent_temperature");
            double wind = cur.optDouble("wind_speed_10m"), gust = cur.optDouble("wind_gusts_10m"), prec = cur.optDouble("precipitation", 0);

            JSONObject com = comunaCercana(c, lat, lon);
            r.put("ok", true).put("lugar", com != null ? com.optString("name") : "Tu ubicación")
                    .put("region", com != null ? com.optString("region") : "")
                    .put("temp", Math.round(temp)).put("sensacion", Math.round(feels)).put("texto", texto(code)).put("codigo", code)
                    .put("es_dia", dia).put("viento", Math.round(wind)).put("rafagas", Math.round(gust))
                    .put("ts", System.currentTimeMillis() / 1000);

            JSONArray al = new JSONArray();
            // -- lluvia / tormenta: ahora o en las próximas 3 horas
            JSONArray ht = h.getJSONArray("time"), hc = h.getJSONArray("weather_code"), hp = h.getJSONArray("precipitation_probability");
            Calendar k = Calendar.getInstance(Oficial.SCL);
            String ahora = String.format(Locale.ROOT, "%04d-%02d-%02dT%02d", k.get(Calendar.YEAR), k.get(Calendar.MONTH) + 1,
                    k.get(Calendar.DAY_OF_MONTH), k.get(Calendar.HOUR_OF_DAY));
            int now = -1;
            for (int i = 0; i < ht.length(); i++) if (ht.optString(i).startsWith(ahora)) { now = i; break; }
            if (tormenta(code)) al.put(alerta("tormenta", "danger", "⛈️", "Tormenta eléctrica ahora", "No salgas a pedalear: busca refugio."));
            else if (prec > 0.1 || lluvia(code) || nieve(code))
                al.put(alerta("lluvia", "warn", nieve(code) ? "🌨️" : "🌧️", (nieve(code) ? "Nieve" : texto(code)) + " ahora",
                        "Calzada resbaladiza: frena antes y evita pintura y rejillas."));
            else if (now >= 0) for (int i = now + 1; i <= now + 3 && i < ht.length(); i++) {
                int cc = hc.optInt(i); int pp = hp.optInt(i);
                if ((lluvia(cc) || nieve(cc)) && pp >= 50 || pp >= 70) {
                    String hh = ht.optString(i).substring(11, 16);
                    al.put(alerta("lluvia_pronto", tormenta(cc) ? "danger" : "warn", tormenta(cc) ? "⛈️" : "🌧️",
                            (tormenta(cc) ? "Tormenta" : nieve(cc) ? "Nieve" : "Lluvia") + " hacia las " + hh + " (" + pp + "%)",
                            "Planifica la vuelta o lleva cortaviento."));
                    break;
                }
            }
            // -- viento
            if (gust >= 65) al.put(alerta("viento", "danger", "💨", "Ráfagas de " + Math.round(gust) + " km/h", "Viento peligroso para la bici: riesgo de caída."));
            else if (gust >= 45 || wind >= 30) al.put(alerta("viento", "warn", "💨", "Viento fuerte: ráfagas de " + Math.round(gust) + " km/h", "Ojo en cruces y avenidas abiertas."));
            // -- temperatura
            if (temp >= 35) al.put(alerta("calor", "danger", "🥵", "Calor extremo: " + Math.round(temp) + "°", "Evita esfuerzo intenso; hidrátate cada 15 min."));
            else if (temp >= 31) al.put(alerta("calor", "warn", "🥵", "Calor: " + Math.round(temp) + "°", "Lleva agua y modera el ritmo."));
            if (temp <= 1) al.put(alerta("helada", "danger", "🧊", "Helada: " + Math.round(temp) + "°", "Posible hielo en la calzada."));
            else if (feels <= 5) al.put(alerta("frio", "info", "🥶", "Frío: sensación de " + Math.round(feels) + "°", "Abrígate manos y orejas."));
            if (code == 45 || code == 48) al.put(alerta("niebla", "warn", "🌫️", "Niebla", "Visibilidad reducida: luces encendidas."));
            if (!dia) al.put(alerta("noche", "info", "🌙", "Es de noche", "Luces delantera y trasera encendidas."));
            double uv = d.optJSONArray("uv_index_max") == null ? 0 : d.getJSONArray("uv_index_max").optDouble(0, 0);
            if (dia && uv >= 8) al.put(alerta("uv", "info", "🧴", "Índice UV muy alto (" + Math.round(uv) + ")", "Protector solar antes de salir."));

            // -- calidad del aire (MP2,5; umbrales de episodio de la RM)
            String a = Oficial.get(String.format(Locale.ROOT,
                    "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f&timezone=America/Santiago&current=pm2_5",
                    lat, lon), 6000, 10000);
            if (a != null) {
                double pm = new JSONObject(a).optJSONObject("current") == null ? 0 : new JSONObject(a).getJSONObject("current").optDouble("pm2_5", 0);
                r.put("pm25", Math.round(pm * 10) / 10.0);
                if (pm >= 110) al.put(alerta("aire", "danger", "😷", "Aire en preemergencia (MP2,5 " + Math.round(pm) + ")", "Evita pedalear fuerte al aire libre."));
                else if (pm >= 80) al.put(alerta("aire", "warn", "😷", "Aire en alerta (MP2,5 " + Math.round(pm) + ")", "Pedalea suave y evita avenidas con tráfico."));
            }
            // ordenar por gravedad (danger → warn → info), estable
            JSONArray ord = new JSONArray();
            for (String nv : new String[]{"danger", "warn", "info"})
                for (int i = 0; i < al.length(); i++) if (nv.equals(al.getJSONObject(i).optString("nivel"))) ord.put(al.getJSONObject(i));
            r.put("alertas", ord);
        } catch (Exception e) {
            try { r.put("ok", false); } catch (Exception ignored) { }
        }
        return r;
    }

    /** Empuja a la app de la bici (si está instalada) un aviso de Clima. Sólo apps con la misma firma lo reciben. */
    static void empujarABici(Context c, String titulo, String texto, String nivel) {
        try {
            c.sendBroadcast(new Intent(ACCION).setPackage(PKG_BICI)
                    .putExtra("titulo", titulo).putExtra("texto", texto).putExtra("nivel", nivel), PERMISO);
        } catch (Exception ignored) { }
    }
}

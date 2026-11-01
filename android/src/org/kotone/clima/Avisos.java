package org.kotone.clima;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.PersistableBundle;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;

/**
 * Avisos de Clima en Android — port de c/clima-notify.sh + los timers de usuario de escritorio:
 *   «pronostico»  22:00 todos los días      → pronóstico de mañana (+ suspensión de mañana si Mineduc la decretó)
 *   «chequeo»     06:45 y 07:40 lun–vie     → suspensión de clases HOY (Mineduc) + alerta de mal tiempo
 * Alarma EXACTA en reposo si el sistema lo permite («Alarmas y recordatorios»; si no, inexacta, con ventana
 * de hasta 1 h que impone Android) → trabajo «expedited» con red (se salta Doze) → notificación.
 * Sin datos válidos no se notifica nada (nunca un aviso hueco); cada alerta, una vez por día. Kotone 2026-10-31.
 */
public class Avisos extends JobService {
    static final String PRONOSTICO = "pronostico", CHEQUEO = "chequeo";
    private static final String TAG = "ClimaAvisos";
    private static final String CH_DIARIO = "pronostico", CH_ALERTA = "alertas";

    // ---------------------------------------------------------- programación
    /** (Re)arma las dos alarmas. Idempotente: se llama al abrir la app, al reiniciar y tras cada aviso. */
    static void programar(Context c) {
        armar(c, PRONOSTICO);
        armar(c, CHEQUEO);
        Ahora.programar(c);          // Now Bar (sólo Samsung con Live Updates)
    }

    private static void armar(Context c, String tipo) {
        AlarmManager am = (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
        Intent i = new Intent(c, Disparo.class).setAction(tipo);
        PendingIntent pi = PendingIntent.getBroadcast(c, tipo.equals(PRONOSTICO) ? 1 : 2, i,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        long t = proxima(tipo, System.currentTimeMillis());
        if (Build.VERSION.SDK_INT < 23) am.setExact(AlarmManager.RTC_WAKEUP, t, pi);                 // Android 5: sin modo reposo
        else if (Build.VERSION.SDK_INT < 31 || am.canScheduleExactAlarms()) am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, t, pi);
        else am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, t, pi);
    }

    /** Próximo instante (hora de Santiago) en que toca el aviso. */
    static long proxima(String tipo, long ahora) {
        int[][] horas = tipo.equals(PRONOSTICO) ? new int[][]{{22, 0}} : new int[][]{{6, 45}, {7, 40}};
        for (int d = 0; d < 8; d++) {
            for (int[] hm : horas) {
                Calendar k = Calendar.getInstance(Oficial.SCL);
                k.setTimeInMillis(ahora);
                k.add(Calendar.DAY_OF_MONTH, d);
                k.set(Calendar.HOUR_OF_DAY, hm[0]); k.set(Calendar.MINUTE, hm[1]);
                k.set(Calendar.SECOND, 0); k.set(Calendar.MILLISECOND, 0);
                int wd = k.get(Calendar.DAY_OF_WEEK);
                if (tipo.equals(CHEQUEO) && (wd == Calendar.SATURDAY || wd == Calendar.SUNDAY)) continue;
                if (k.getTimeInMillis() > ahora + 30_000) return k.getTimeInMillis();
            }
        }
        return ahora + 24 * 3600_000L;
    }

    /** Alarma → trabajo con red (el receptor sólo encola; la red se hace en el JobService). */
    public static class Disparo extends BroadcastReceiver {
        @Override public void onReceive(Context c, Intent in) {
            String a = in.getAction();
            if (PRONOSTICO.equals(a) || CHEQUEO.equals(a)) encolar(c, a);
            else programar(c);   // BOOT_COMPLETED / permiso de alarmas exactas / MY_PACKAGE_REPLACED / cambio de hora: re-armar
        }
    }

    static void encolar(Context c, String tipo) {
        try { encolar0(c, tipo); } catch (RuntimeException e) { Log.w(TAG, "no se pudo encolar " + tipo, e); }
    }
    private static void encolar0(Context c, String tipo) {
        PersistableBundle x = new PersistableBundle();
        x.putString("tipo", tipo);
        JobInfo.Builder j = new JobInfo.Builder(tipo.equals(PRONOSTICO) ? 41 : 42, new ComponentName(c, Avisos.class))
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY).setExtras(x);
        JobScheduler js = (JobScheduler) c.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        // expedited: corre al tiro aunque el teléfono esté en reposo; si el cupo se agotó, trabajo normal
        if (Build.VERSION.SDK_INT >= 31 && js.schedule(j.setExpedited(true).build()) == JobScheduler.RESULT_SUCCESS) {
            Log.i(TAG, "encolado (expedited): " + tipo); return;
        }
        int r = js.schedule(j.setExpedited(false).setOverrideDeadline(20 * 60_000L).build());
        Log.i(TAG, "encolado (normal): " + tipo + " → " + r);
    }

    // ---------------------------------------------------------- trabajo
    @Override public boolean onStartJob(final JobParameters p) {
        final String tipo = p.getExtras().getString("tipo", CHEQUEO);
        new Thread(() -> {
            Log.i(TAG, "inicio: " + tipo);
            try { ejecutar(this, tipo, System.currentTimeMillis()); } catch (Exception e) { Log.w(TAG, "falló " + tipo, e); }
            armar(this, tipo);
            jobFinished(p, false);
        }, "clima-avisos").start();
        return true;
    }
    @Override public boolean onStopJob(JobParameters p) { return false; }

    /** Dónde estás: ubicación del teléfono si hay permiso «todo el tiempo» (posición de < 3 h, comuna más
     *  cercana); si no, la de la última consulta de la app (caché); si nunca abrió, Santiago. */
    static JSONObject ubicacion(Context c) {
        try {
            if (permiso(c, android.Manifest.permission.ACCESS_COARSE_LOCATION)
                    && (Build.VERSION.SDK_INT < 29 || permiso(c, android.Manifest.permission.ACCESS_BACKGROUND_LOCATION))) {
                android.location.LocationManager lm = (android.location.LocationManager) c.getSystemService(Context.LOCATION_SERVICE);
                android.location.Location best = null;
                for (String p : new String[]{"fused", "network", "passive", "gps"}) {
                    try { android.location.Location l = lm.getLastKnownLocation(p);
                        if (l != null && (best == null || l.getTime() > best.getTime())) best = l; } catch (Exception ignored) { }
                }
                if (best != null && System.currentTimeMillis() - best.getTime() < 3 * 3600_000L) {
                    JSONObject com = Alertas.comunaCercana(c, best.getLatitude(), best.getLongitude());
                    if (com != null) return new JSONObject().put("name", com.optString("name")).put("region", com.optString("region"))
                            .put("lat", best.getLatitude()).put("lon", best.getLongitude());
                }
            }
        } catch (Exception ignored) { }
        try (InputStream in = new FileInputStream(new File(c.getFilesDir(), "last.json"))) {
            JSONObject l = new JSONObject(Oficial.read(in, 8 << 20)).optJSONObject("location");
            if (l != null && l.has("lat") && l.has("lon") && !l.optString("name").isEmpty()) return l;
        } catch (Exception ignored) { }
        try {
            return new JSONObject().put("name", "Santiago").put("region", "Región Metropolitana")
                    .put("lat", -33.4489).put("lon", -70.6693);
        } catch (Exception e) { return new JSONObject(); }
    }

    private static final String[][] WC = {{"0", "despejado"}, {"1", "mayormente despejado"}, {"2", "parcial nublado"},
            {"3", "nublado"}, {"45", "niebla"}, {"48", "niebla"}, {"51", "llovizna"}, {"53", "llovizna"}, {"55", "llovizna"},
            {"61", "lluvia débil"}, {"63", "lluvia"}, {"65", "lluvia fuerte"}, {"71", "nieve"}, {"73", "nieve"},
            {"75", "nieve fuerte"}, {"80", "chubascos"}, {"81", "chubascos"}, {"82", "chubascos fuertes"},
            {"95", "tormenta"}, {"96", "tormenta con granizo"}, {"99", "tormenta fuerte"}};
    private static String desc(int code) {
        for (String[] w : WC) if (Integer.parseInt(w[0]) == code) return w[1];
        return "";
    }
    private static double g(JSONObject daily, String k, int i) {
        JSONArray a = daily.optJSONArray(k);
        return a == null || i >= a.length() ? 0 : a.optDouble(i, 0);
    }
    private static String isoDia(long t, int masDias) {
        Calendar k = Calendar.getInstance(Oficial.SCL);
        k.setTimeInMillis(t); k.add(Calendar.DAY_OF_MONTH, masDias);
        return String.format(Locale.ROOT, "%04d-%02d-%02d", k.get(Calendar.YEAR), k.get(Calendar.MONTH) + 1, k.get(Calendar.DAY_OF_MONTH));
    }
    private static String r1(double v) { return String.format(Locale.ROOT, "%.1f", v).replace(".0", ""); }

    static void ejecutar(Context c, String tipo, long ahora) throws Exception {
        JSONObject loc = ubicacion(c);
        String comuna = loc.optString("name"), region = loc.optString("region");
        // 3 intentos, como clima-notify.sh; sin daily.time válido → silencio
        JSONObject daily = null;
        for (int k = 0; k < 3 && daily == null; k++) {
            String t = Oficial.get(String.format(Locale.ROOT,
                    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&timezone=America/Santiago"
                    + "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
                    + "precipitation_sum,wind_speed_10m_max&forecast_days=3&wind_speed_unit=kmh",
                    loc.optDouble("lat"), loc.optDouble("lon")), 8000, 20000);
            try {
                JSONObject d = t == null ? null : new JSONObject(t).optJSONObject("daily");
                if (d != null && d.optJSONArray("time") != null && d.optJSONArray("time").length() > 0) daily = d;
            } catch (Exception ignored) { }
            if (daily == null && k < 2) Thread.sleep(3000);
        }
        if (daily == null || comuna.isEmpty()) { Log.i(TAG, "sin datos válidos: no se notifica"); return; }
        JSONArray t = daily.getJSONArray("time");
        List<String> dias = new ArrayList<>();
        for (int i = 0; i < t.length(); i++) dias.add(t.optString(i));

        if (tipo.equals(PRONOSTICO)) {
            int i = dias.indexOf(isoDia(ahora, 1));
            if (i < 0) i = dias.size() > 1 ? 1 : 0;
            int pp = (int) Math.round(g(daily, "precipitation_probability_max", i));
            double psum = Math.round(g(daily, "precipitation_sum", i) * 10) / 10.0;
            String lluvia = (pp >= 30 || psum >= 0.2) ? "Lluvia " + pp + "% (" + r1(psum) + " mm). " : "";
            StringBuilder body = new StringBuilder("Mañana en " + comuna + ": " + desc((int) g(daily, "weather_code", i))
                    + ". Mín " + r1(g(daily, "temperature_2m_min", i)) + "° / Máx " + r1(g(daily, "temperature_2m_max", i)) + "°. "
                    + lluvia + "Viento hasta " + Math.round(g(daily, "wind_speed_10m_max", i)) + " km/h.");
            JSONObject m = dia(Oficial.suspension(region, comuna), "manana");
            if (m != null && m.optBoolean("suspendido"))
                body.append("\n🏫 ").append(titular(m, "Suspensión de clases decretada")).append(". (Verifica si aplica a ").append(comuna).append(".)");
            notificar(c, 1, CH_DIARIO, "🌤️ Pronóstico de mañana", body.toString());
            return;
        }

        // ---- chequeo: 1) suspensión HOY (Mineduc)  2) mal tiempo HOY (derivado del pronóstico)
        SharedPreferences sp = c.getSharedPreferences("avisos", MODE_PRIVATE);
        String hoy = isoDia(ahora, 0);
        JSONObject h = dia(Oficial.suspension(region, comuna), "hoy");
        if (h != null && h.optBoolean("suspendido") && !hoy.equals(sp.getString("suspension", ""))) {
            notificar(c, 2, CH_ALERTA, "🏫 Suspensión de clases hoy",
                    titular(h, "Hay suspensión de clases vigente") + ". (Verifica si aplica a " + comuna + ".) Fuente: MINEDUC.");
            sp.edit().putString("suspension", hoy).apply();
        }
        int i = Math.max(0, dias.indexOf(hoy));
        int code = (int) g(daily, "weather_code", i);
        double psum = g(daily, "precipitation_sum", i), pp = g(daily, "precipitation_probability_max", i);
        double wind = g(daily, "wind_speed_10m_max", i), tmax = g(daily, "temperature_2m_max", i), tmin = g(daily, "temperature_2m_min", i);
        List<String> r = new ArrayList<>();
        if (code == 95 || code == 96 || code == 99) r.add("tormenta eléctrica");
        if (psum >= 15 || (pp >= 80 && psum >= 5)) r.add("lluvia intensa (" + r1(Math.round(psum * 10) / 10.0) + " mm, " + Math.round(pp) + "%)");
        if (wind >= 50) r.add("viento fuerte (hasta " + Math.round(wind) + " km/h)");
        if (tmax >= 35) r.add("calor extremo (" + Math.round(tmax) + "°)");
        if (tmin <= -2) r.add("frío extremo / helada (" + Math.round(tmin) + "°)");
        if (!r.isEmpty() && !hoy.equals(sp.getString("maltiempo", ""))) {
            notificar(c, 3, CH_ALERTA, "⛈️ Alerta de mal tiempo", "Hoy en " + comuna + ": " + String.join(" · ", r) + ".");
            Alertas.empujarABici(c, "Alerta de mal tiempo", "Hoy en " + comuna + ": " + String.join(" · ", r) + ".", "danger");
            sp.edit().putString("maltiempo", hoy).apply();
        }
    }

    private static JSONObject dia(JSONObject sus, String etq) {
        JSONArray d = sus.optJSONArray("dias");
        for (int i = 0; d != null && i < d.length(); i++) {
            JSONObject o = d.optJSONObject(i);
            if (o != null && etq.equals(o.optString("etiqueta"))) return o;
        }
        return null;
    }
    private static String titular(JSONObject d, String porDefecto) {
        String t = d.optString("titular").trim();
        return t.isEmpty() ? porDefecto : t;
    }

    // ---------------------------------------------------------- notificación
    /** Android 5: los permisos se conceden al instalar; desde Android 6 se consultan en tiempo de ejecución. */
    static boolean permiso(Context c, String p) {
        return Build.VERSION.SDK_INT < 23 || c.checkSelfPermission(p) == android.content.pm.PackageManager.PERMISSION_GRANTED;
    }

    static void canales(Context c) {
        if (Build.VERSION.SDK_INT < 26) return;             // los canales existen desde Android 8
        NotificationManager nm = (NotificationManager) c.getSystemService(Context.NOTIFICATION_SERVICE);
        NotificationChannel a = new NotificationChannel(CH_DIARIO, "Pronóstico de mañana", NotificationManager.IMPORTANCE_DEFAULT);
        a.setDescription("Todas las noches a las 22:00.");
        NotificationChannel b = new NotificationChannel(CH_ALERTA, "Alertas (clases y mal tiempo)", NotificationManager.IMPORTANCE_HIGH);
        b.setDescription("Suspensión de clases informada por Mineduc y alertas de mal tiempo, días de semana en la mañana.");
        nm.createNotificationChannel(a);
        nm.createNotificationChannel(b);
    }

    static void notificar(Context c, int id, String canal, String titulo, String texto) {
        canales(c);
        PendingIntent abrir = PendingIntent.getActivity(c, 0, new Intent(c, MainActivity.class)
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP), PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder nb = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(c, canal)
                : new Notification.Builder(c).setPriority(CH_ALERTA.equals(canal) ? Notification.PRIORITY_HIGH : Notification.PRIORITY_DEFAULT);
        Notification n = nb
                .setSmallIcon(R.drawable.ic_stat_clima)
                .setColor(0xFF3B82F6)
                .setContentTitle(titulo)
                .setContentText(texto)
                .setStyle(new Notification.BigTextStyle().bigText(texto))
                .setContentIntent(abrir)
                .setAutoCancel(true)
                .build();
        try { ((NotificationManager) c.getSystemService(Context.NOTIFICATION_SERVICE)).notify(id, n); Log.i(TAG, "notificado: " + titulo + " — " + texto); }
        catch (SecurityException e) { Log.w(TAG, "sin permiso de notificaciones"); }
    }
}

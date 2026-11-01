package org.kotone.clima;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Calendar;
import java.util.Locale;

/**
 * Now Bar de Samsung para Clima (Live Updates de Android 16), Kotone 2026-10-31.
 * Cada 30 min revisa la lluvia de las próximas 3 h donde estás:
 *   · si ya llueve → «Lloviendo · para hacia las 16:00»
 *   · si se viene  → «Lluvia hacia las 15:00» con cuenta regresiva («40 min») como chip
 * y la publica como notificación promovida (sale en la Now Bar); se retira sola a la hora indicada o cuando
 * el pronóstico ya no trae lluvia. En teléfonos que no son Samsung no se activa.
 */
public class Ahora extends JobService {
    static final String TAG = "ClimaAhora", CH = "now_bar";
    static final int JOB = 43, ID = 9;

    static boolean disponible(Context c) {
        if (Build.VERSION.SDK_INT < 36 || !"samsung".equalsIgnoreCase(Build.MANUFACTURER)) return false;
        try { return nm(c).canPostPromotedNotifications(); } catch (Throwable t) { return false; }
    }

    /** API 21+: el servicio por nombre (getSystemService(Clase) es de Android 6). */
    private static NotificationManager nm(Context c) { return (NotificationManager) c.getSystemService(Context.NOTIFICATION_SERVICE); }

    @android.annotation.TargetApi(36)            // sólo se llega aquí si disponible() (Android 16 + Samsung)
    static void programar(Context c) {
        if (!disponible(c)) return;
        JobScheduler js = (JobScheduler) c.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        if (js.getPendingJob(JOB) == null)
            js.schedule(new JobInfo.Builder(JOB, new ComponentName(c, Ahora.class)).setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                    .setPeriodic(30 * 60_000L).setPersisted(true).build());
    }

    @Override public boolean onStartJob(final JobParameters p) {
        new Thread(() -> { try { revisar(this); } catch (Exception e) { Log.w(TAG, "revisión: " + e); } jobFinished(p, false); }, "clima-ahora").start();
        return true;
    }
    @Override public boolean onStopJob(JobParameters p) { return true; }

    private static boolean lluvia(int c) { return (c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95; }

    @android.annotation.TargetApi(36)
    static void revisar(Context c) throws Exception {
        if (Build.VERSION.SDK_INT < 36) return;          // Now Bar: sólo Android 16 (antes de pedir nada al sistema)
        NotificationManager nm = nm(c);
        if (!disponible(c)) { nm.cancel(ID); return; }
        JSONObject loc = Avisos.ubicacion(c);
        String t = Oficial.get(String.format(Locale.ROOT,
                "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&timezone=America/Santiago"
                + "&current=precipitation,weather_code&hourly=weather_code,precipitation_probability,precipitation&forecast_days=1",
                loc.optDouble("lat"), loc.optDouble("lon")), 8000, 15000);
        if (t == null) return;                                   // sin red: se deja lo que hubiera
        JSONObject o = new JSONObject(t), cur = o.getJSONObject("current"), h = o.getJSONObject("hourly");
        JSONArray ht = h.getJSONArray("time"), hc = h.getJSONArray("weather_code"), hp = h.getJSONArray("precipitation_probability");
        Calendar k = Calendar.getInstance(Oficial.SCL);
        String ahora = String.format(Locale.ROOT, "%04d-%02d-%02dT%02d", k.get(Calendar.YEAR), k.get(Calendar.MONTH) + 1, k.get(Calendar.DAY_OF_MONTH), k.get(Calendar.HOUR_OF_DAY));
        int now = -1;
        for (int i = 0; i < ht.length(); i++) if (ht.optString(i).startsWith(ahora)) { now = i; break; }
        if (now < 0) return;
        boolean llueve = cur.optDouble("precipitation", 0) > 0.1 || lluvia(cur.optInt("weather_code"));
        String titulo = null, texto = null, chip = null; long hasta = 0;
        if (llueve) {
            int fin = -1;
            for (int i = now + 1; i <= now + 6 && i < ht.length(); i++) if (!lluvia(hc.optInt(i)) && hp.optInt(i) < 40) { fin = i; break; }
            titulo = "Lloviendo en " + loc.optString("name");
            texto = fin > 0 ? "Debería parar hacia las " + ht.optString(fin).substring(11, 16) : "Sigue durante las próximas horas";
            chip = "🌧 ahora";
            hasta = fin > 0 ? horaMs(ht.optString(fin)) : System.currentTimeMillis() + 3 * 3600_000L;
        } else {
            for (int i = now + 1; i <= now + 3 && i < ht.length(); i++) {
                if ((lluvia(hc.optInt(i)) && hp.optInt(i) >= 50) || hp.optInt(i) >= 70) {
                    long ms = horaMs(ht.optString(i)), min = Math.max(1, (ms - System.currentTimeMillis()) / 60_000);
                    titulo = "Lluvia hacia las " + ht.optString(i).substring(11, 16) + " (" + hp.optInt(i) + "%)";
                    texto = loc.optString("name") + " · lleva paraguas o cortaviento";
                    chip = "🌧 " + (min < 60 ? min + " min" : (min / 60) + " h");
                    hasta = ms + 3600_000L;
                    break;
                }
            }
        }
        if (titulo == null) { nm.cancel(ID); Log.i(TAG, "sin lluvia próxima"); return; }
        NotificationChannel ch = new NotificationChannel(CH, "Lluvia ahora (Now Bar)", NotificationManager.IMPORTANCE_DEFAULT);
        ch.setSound(null, null); ch.enableVibration(false);
        nm.createNotificationChannel(ch);
        PendingIntent abrir = PendingIntent.getActivity(c, 3, new Intent(c, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder nb = new Notification.Builder(c, CH).setSmallIcon(R.drawable.ic_stat_clima).setOngoing(true).setOnlyAlertOnce(true)
                .setContentTitle(titulo).setContentText(texto).setStyle(new Notification.BigTextStyle().bigText(texto))
                .setShortCriticalText(chip).setGroup("now-bar")  /* grupo propio: el autoagrupado la esconde de la Now Bar */.setContentIntent(abrir)
                .setTimeoutAfter(Math.max(60_000L, hasta - System.currentTimeMillis()));
        // setRequestPromotedOngoing es de Android 16 QPR (36.1): en 36.0 la llamada no existe
        if (Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1) nb.setRequestPromotedOngoing(true);
        Notification n = nb.build();
        try { nm.notify(ID, n); Log.i(TAG, "now bar: " + titulo + " · " + chip); } catch (SecurityException ignored) { }
    }

    private static long horaMs(String iso) {          // «2026-10-31T15:00» en hora de Santiago → epoch ms
        Calendar k = Calendar.getInstance(Oficial.SCL);
        k.set(Integer.parseInt(iso.substring(0, 4)), Integer.parseInt(iso.substring(5, 7)) - 1, Integer.parseInt(iso.substring(8, 10)),
                Integer.parseInt(iso.substring(11, 13)), Integer.parseInt(iso.substring(14, 16)), 0);
        k.set(Calendar.MILLISECOND, 0);
        return k.getTimeInMillis();
    }
}

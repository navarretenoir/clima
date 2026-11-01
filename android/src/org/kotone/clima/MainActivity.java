package org.kotone.clima;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.location.Location;
import android.location.LocationManager;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.window.OnBackInvokedDispatcher;

import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Clima para Android — host nativo de la MISMA interfaz web de la app de escritorio (ui/).
 * El núcleo de datos vive en assets/ui/clima-android.js (port del clima_data.c de escritorio);
 * aquí sólo se ofrece lo que el navegador no puede: red sin CORS, ubicación del teléfono,
 * caché en disco, las fuentes oficiales (Oficial.java), los avisos programados (Avisos.java) y los
 * márgenes del sistema (pantalla de borde a borde). Kotone 2026-10-31.
 */
public class MainActivity extends Activity {
    private static final int REQ_LOC = 7, REQ_NOTIF = 8, REQ_BG = 9;
    private WebView web;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final ExecutorService pool = Executors.newFixedThreadPool(4);
    private int pendingLoc = -1;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        Window w = getWindow();
        w.setStatusBarColor(Color.TRANSPARENT);
        w.setNavigationBarColor(Color.TRANSPARENT);
        if (Build.VERSION.SDK_INT >= 30) w.setDecorFitsSystemWindows(false);
        else w.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);

        web = new WebView(this);
        web.setBackgroundColor(Color.parseColor("#3B5272"));
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(false);          // sólo file:///android_asset (siempre permitido)
        s.setAllowContentAccess(false);
        s.setTextZoom(100);                   // el diseño ya está a escala; no lo agrande el sistema
        web.addJavascriptInterface(new Bridge(), "ClimaAndroid");
        // errores y avisos de la interfaz → logcat (etiqueta ClimaWeb), para diagnosticar sin DevTools
        web.setWebChromeClient(new android.webkit.WebChromeClient() {
            @Override public boolean onConsoleMessage(android.webkit.ConsoleMessage m) {
                android.util.Log.println(m.messageLevel() == android.webkit.ConsoleMessage.MessageLevel.ERROR
                        ? android.util.Log.ERROR : android.util.Log.INFO, "ClimaWeb",
                        m.message() + " (" + m.sourceId() + ":" + m.lineNumber() + ")");
                return true;
            }
        });
        // la interfaz es local: nada navega fuera de file:///android_asset
        web.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView v, WebResourceRequest r) {
                return !r.getUrl().toString().startsWith("file:///android_asset/");
            }
            // los márgenes del sistema llegan antes de que exista la página: se re-aplican al terminar de cargar
            @Override public void onPageFinished(WebView v, String url) { v.requestApplyInsets(); }
        });
        web.setOnApplyWindowInsetsListener((v, ins) -> {
            float d = getResources().getDisplayMetrics().density;
            int top, bottom;
            if (Build.VERSION.SDK_INT >= 30) {
                android.graphics.Insets i = ins.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                top = i.top; bottom = i.bottom;
            } else { top = ins.getSystemWindowInsetTop(); bottom = ins.getSystemWindowInsetBottom(); }
            js("document.documentElement.style.setProperty('--sat','" + Math.round(top / d) + "px');"
             + "document.documentElement.style.setProperty('--sab','" + Math.round(bottom / d) + "px');");
            return ins;
        });
        setContentView(web);
        web.loadUrl("file:///android_asset/ui/index.html");

        Avisos.canales(this);
        Avisos.programar(this);
        // prueba manual: adb shell am start -n org.kotone.clima/.MainActivity --es probar_aviso pronostico|chequeo
        String probar = getIntent().getStringExtra("probar_aviso");
        if (Avisos.PRONOSTICO.equals(probar) || Avisos.CHEQUEO.equals(probar)) Avisos.encolar(this, probar);
        else if ("ahora".equals(probar)) new Thread(() -> { try { Ahora.revisar(this); } catch (Exception ignored) { } }).start();
        else if ("empuje".equals(probar))   // prueba del enlace Clima → ADO O260T
            Alertas.empujarABici(this, "Prueba de enlace", "Clima puede avisarle a la bici.", "warn");

        if (Build.VERSION.SDK_INT >= 33) {
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_DEFAULT, this::onBack);
        }
    }

    /** Atrás: primero cierra la hoja del día (si está abierta); si no, sale. */
    private void onBack() {
        web.evaluateJavascript("(function(){var o=document.getElementById('overlay');"
                + "if(o&&o.classList.contains('show')){closeDay();return 1}return 0})()",
                r -> { if (!"1".equals(r)) finish(); });
    }

    @SuppressWarnings("deprecation")
    @Override
    public void onBackPressed() { onBack(); }

    @Override
    protected void onResume() {
        super.onResume();
        if (web != null) js("window.climaResume&&window.climaResume()");
    }

    @Override
    protected void onDestroy() {
        pool.shutdownNow();
        if (web != null) web.destroy();
        super.onDestroy();
    }

    private void js(String code) {
        ui.post(() -> { if (web != null) web.evaluateJavascript(code, null); });
    }

    // ------------------------------------------------------------- ubicación
    private boolean locGranted() {
        return Avisos.permiso(this, Manifest.permission.ACCESS_COARSE_LOCATION);
    }

    private void doLocate(int id) {
        if (!locGranted()) { js("window.__climaLoc(" + id + ",null,null)"); return; }
        LocationManager lm = (LocationManager) getSystemService(LOCATION_SERVICE);
        Location best = null;
        for (String p : new String[]{"fused", LocationManager.NETWORK_PROVIDER, LocationManager.PASSIVE_PROVIDER, LocationManager.GPS_PROVIDER}) {
            try {
                Location l = lm.getLastKnownLocation(p);
                if (l != null && (best == null || l.getTime() > best.getTime())) best = l;
            } catch (Exception ignored) { }
        }
        // Una posición de hace menos de 30 min basta para el clima; si no, se pide una fresca.
        if (best != null && System.currentTimeMillis() - best.getTime() < 30 * 60 * 1000) { sendLoc(id, best); return; }
        final Location fallback = best;
        if (Build.VERSION.SDK_INT >= 30 && lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            final boolean[] done = {false};
            CancellationSignal cs = new CancellationSignal();
            try {
                lm.getCurrentLocation(LocationManager.NETWORK_PROVIDER, cs, getMainExecutor(), l -> {
                    if (done[0]) return; done[0] = true; sendLoc(id, l != null ? l : fallback);
                });
                ui.postDelayed(() -> { if (!done[0]) { done[0] = true; cs.cancel(); sendLoc(id, fallback); } }, 8000);
                return;
            } catch (Exception ignored) { }
        }
        sendLoc(id, fallback);
    }

    private void sendLoc(int id, Location l) {
        if (l == null) js("window.__climaLoc(" + id + ",null,null)");
        else js("window.__climaLoc(" + id + "," + l.getLatitude() + "," + l.getLongitude() + ")");
    }

    @Override
    public void onRequestPermissionsResult(int req, String[] perms, int[] res) {
        if (req == REQ_LOC && pendingLoc >= 0) { int id = pendingLoc; pendingLoc = -1; doLocate(id); }
        // orden: ubicación → avisos → ubicación «todo el tiempo»; nunca dos diálogos encima
        if (req == REQ_LOC) { if (!askNotif()) askBackground(); }
        else if (req == REQ_NOTIF) askBackground();
    }

    /** Ubicación permanente (Android 10+): se explica primero y luego Android abre su selector
     *  «Permitir todo el tiempo». Se ofrece una vez; si dice que no, los avisos usan la última consulta. */
    private void askBackground() {
        if (Build.VERSION.SDK_INT < 29 || !locGranted()
                || checkSelfPermission(Manifest.permission.ACCESS_BACKGROUND_LOCATION) == PackageManager.PERMISSION_GRANTED) return;
        android.content.SharedPreferences sp = getSharedPreferences("avisos", MODE_PRIVATE);
        if (sp.getBoolean("bg_pedido", false)) return;
        sp.edit().putBoolean("bg_pedido", true).apply();
        new android.app.AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Ubicación todo el tiempo")
                .setMessage("Para que el pronóstico de las 22:00, las alertas de la mañana y las alertas para la bici "
                        + "sean de donde estás (y no de tu última consulta), elige «Permitir todo el tiempo» en la siguiente pantalla.")
                .setPositiveButton("Continuar", (d, w) -> requestPermissions(
                        new String[]{Manifest.permission.ACCESS_BACKGROUND_LOCATION}, REQ_BG))
                .setNegativeButton("Ahora no", null)
                .show();
    }

    /** Android 13+: permiso de notificaciones (una vez; si lo niega, la app sigue igual sin avisos). */
    private boolean askNotif() {
        if (Build.VERSION.SDK_INT >= 33
                && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
                && !getSharedPreferences("avisos", MODE_PRIVATE).getBoolean("pedido", false)) {
            getSharedPreferences("avisos", MODE_PRIVATE).edit().putBoolean("pedido", true).apply();
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_NOTIF);
            return true;
        }
        return false;
    }

    // ----------------------------------------------------------------- puente
    private class Bridge {
        /** GET asíncrono: responde con window.__climaHttp(id, ok, cuerpo). */
        @JavascriptInterface
        public void get(final int id, final String url) {
            pool.execute(() -> {
                String body = url.startsWith("https://") ? Oficial.get(url, 8000, 20000) : null;
                js("window.__climaHttp(" + id + "," + (body != null) + "," + JSONObject.quote(body == null ? "" : body) + ")");
            });
        }

        /** Fuentes oficiales (Mineduc siempre; airerm sólo si rige la restricción): window.__climaHttp(id, ok, json). */
        @JavascriptInterface
        public void oficial(final int id, final String region, final String comuna, final boolean restr) {
            pool.execute(() -> {
                JSONObject o = new JSONObject();
                try {
                    o.put("suspension", Oficial.suspension(region, comuna));
                    o.put("restriccion", restr ? Oficial.restriccion() : JSONObject.NULL);
                } catch (Exception ignored) { }
                js("window.__climaHttp(" + id + ",true," + JSONObject.quote(o.toString()) + ")");
            });
        }

        /** Alertas para pedalear en (lat, lon) — las mismas que recibe ADO O260T: window.__climaHttp(id, ok, json). */
        @JavascriptInterface
        public void bici(final int id, final double lat, final double lon) {
            pool.execute(() -> {
                String j = Alertas.calcular(MainActivity.this, lat, lon).toString();
                js("window.__climaHttp(" + id + ",true," + JSONObject.quote(j) + ")");
            });
        }

        /** Estado del Metro (proveedor de la app Metro, permiso de firma) → window.__climaHttp(id, ok, json). */
        @JavascriptInterface
        public void metro(final int id) {
            pool.execute(() -> {
                String j = null;
                try {
                    android.os.Bundle r = getContentResolver().call(android.net.Uri.parse("content://org.kotone.metro.estado"), "estado", null, null);
                    if (r != null) j = r.getString("json");
                } catch (Exception ignored) { }
                js("window.__climaHttp(" + id + "," + (j != null) + "," + JSONObject.quote(j == null ? "" : j) + ")");
            });
        }

        @JavascriptInterface public boolean hayMetro() { return getPackageManager().getLaunchIntentForPackage("org.kotone.metro") != null; }
        @JavascriptInterface public void abrirMetro() {
            android.content.Intent i = getPackageManager().getLaunchIntentForPackage("org.kotone.metro");
            if (i != null) ui.post(() -> startActivity(i));
        }

        /** ¿Está instalada la app de la bici? (la tarjeta ofrece abrirla) */
        @JavascriptInterface
        public boolean hayBici() {
            return getPackageManager().getLaunchIntentForPackage(Alertas.PKG_BICI) != null;
        }

        @JavascriptInterface
        public void abrirBici() {
            android.content.Intent i = getPackageManager().getLaunchIntentForPackage(Alertas.PKG_BICI);
            if (i != null) ui.post(() -> startActivity(i));
        }

        @JavascriptInterface
        public String asset(String path) {
            if (path.contains("..")) return "";
            try (InputStream in = getAssets().open(path)) { return Oficial.read(in, 4 << 20); }
            catch (Exception e) { return ""; }
        }

        @JavascriptInterface
        public void cacheSave(String json) {
            File f = new File(getFilesDir(), "last.json"), tmp = new File(getFilesDir(), "last.json.tmp");
            try (FileOutputStream o = new FileOutputStream(tmp)) {
                o.write(json.getBytes(StandardCharsets.UTF_8));
            } catch (Exception e) { return; }
            //noinspection ResultOfMethodCallIgnored
            tmp.renameTo(f);
        }

        @JavascriptInterface
        public String cacheLoad() {
            try (InputStream in = new java.io.FileInputStream(new File(getFilesDir(), "last.json"))) {
                return Oficial.read(in, 8 << 20);
            } catch (Exception e) { return ""; }
        }

        /** Ubicación del teléfono (pide el permiso la primera vez). */
        @JavascriptInterface
        public void locate(final int id) {
            ui.post(() -> {
                if (locGranted()) { doLocate(id); if (!askNotif()) askBackground(); }
                else if (Build.VERSION.SDK_INT >= 23) { pendingLoc = id; requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION}, REQ_LOC); }
                else doLocate(id);          // Android 5: permiso concedido al instalar
            });
        }
    }
}

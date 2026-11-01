package org.kotone.clima;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Fuentes OFICIALES de Chile para Android — port 1:1 de c/suspension_scraper.cpp (MINEDUC, feed RSS)
 * y c/restriccion_scraper.cpp (airerm.mma.gob.cl, tabla datada). Mismo JSON que los binarios de escritorio.
 *
 * SEGURIDAD (carve-out, anti-confabulación): sólo afirma «suspendido» / episodio si la fuente lo dice
 * para la FECHA y el ALCANCE pedidos; si no puede confirmar, devuelve false y la app usa la estimación.
 * Mejora sobre escritorio: el cotejo quita tildes y decodifica entidades HTML también en airerm
 * («Restricción» ≠ «restriccion» en el C++ original). Kotone 2026-10-31.
 */
final class Oficial {
    private Oficial() { }

    static final TimeZone SCL = TimeZone.getTimeZone("America/Santiago");
    private static final String FEED = "https://www.mineduc.cl/feed/";
    private static final String AIRERM = "https://airerm.mma.gob.cl/";
    private static final String[] MESES = {"enero", "febrero", "marzo", "abril", "mayo", "junio",
            "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"};

    // ------------------------------------------------------------------ red
    static String get(String url, int connectMs, int readMs) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(connectMs);
            c.setReadTimeout(readMs);
            c.setInstanceFollowRedirects(true);
            c.setRequestProperty("User-Agent", "ClimaKotone/1.1 (Android)");
            if (c.getResponseCode() >= 400) return null;
            return read(c.getInputStream(), 8 << 20);
        } catch (Exception e) {
            return null;
        } finally { if (c != null) c.disconnect(); }
    }

    static String read(InputStream in, int max) throws java.io.IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buf = new byte[16384];
        int n, total = 0;
        while ((n = in.read(buf)) > 0) {
            total += n;
            if (total > max) throw new java.io.IOException("respuesta demasiado grande");
            out.write(buf, 0, n);
        }
        return out.toString("UTF-8");
    }

    // ------------------------------------------------------------------ texto
    private static final Pattern ENT = Pattern.compile("&(#x?[0-9a-fA-F]+|[a-zA-Z]+);");

    /** Entidades HTML más comunes de WordPress (numéricas + las con nombre que usan tildes). */
    static String unescape(String s) {
        Matcher m = ENT.matcher(s);
        StringBuffer sb = new StringBuffer();
        while (m.find()) {
            String e = m.group(1), r = null;
            try {
                if (e.startsWith("#x") || e.startsWith("#X")) r = new String(Character.toChars(Integer.parseInt(e.substring(2), 16)));
                else if (e.startsWith("#")) r = new String(Character.toChars(Integer.parseInt(e.substring(1))));
            } catch (Exception ignored) { }
            if (r == null) switch (e) {
                case "amp": r = "&"; break;  case "lt": r = "<"; break;  case "gt": r = ">"; break;
                case "quot": r = "\""; break; case "nbsp": r = " "; break; case "laquo": r = "«"; break;
                case "raquo": r = "»"; break; case "ntilde": r = "ñ"; break; case "Ntilde": r = "Ñ"; break;
                default:
                    if (e.length() == 6 && e.endsWith("acute"))      // &oacute; → ó
                        r = Normalizer.normalize(e.charAt(0) + "\u0301", Normalizer.Form.NFC);
                    else r = m.group(0);
            }
            m.appendReplacement(sb, Matcher.quoteReplacement(r));
        }
        m.appendTail(sb);
        return sb.toString();
    }

    /** minúsculas sin tildes/ñ; el resto de lo no-ASCII se bota (igual que norm() del C++). */
    static String norm(String in) {
        String d = Normalizer.normalize(in, Normalizer.Form.NFD).toLowerCase(Locale.ROOT);
        StringBuilder o = new StringBuilder(d.length());
        for (int i = 0; i < d.length(); i++) { char c = d.charAt(i); if (c < 0x80) o.append(c); }
        return o.toString();
    }

    private static final Pattern BLOCKS = Pattern.compile("(?is)<(script|style)\\b.*?(</\\1\\s*>|$)");
    static String stripTags(String html) {
        StringBuilder o = new StringBuilder(html.length());
        boolean in = false;
        for (int i = 0; i < html.length(); i++) {
            char c = html.charAt(i);
            if (c == '<') in = true; else if (c == '>') in = false; else if (!in) o.append(c);
        }
        return o.toString();
    }

    private static Calendar hoy(int masDias) {
        Calendar c = Calendar.getInstance(SCL);
        c.add(Calendar.DAY_OF_MONTH, masDias);
        return c;
    }
    private static String iso(Calendar c) {
        return String.format(Locale.ROOT, "%04d-%02d-%02d", c.get(Calendar.YEAR), c.get(Calendar.MONTH) + 1, c.get(Calendar.DAY_OF_MONTH));
    }

    // ------------------------------------------------------------------ MINEDUC
    private static final String[] REGIONES = {"arica", "tarapaca", "antofagasta", "atacama", "coquimbo", "valparaiso",
            "metropolitana", "o'higgins", "ohiggins", "libertador", "maule", "nuble",
            "biobio", "araucania", "los rios", "los lagos", "aysen", "magallanes"};

    static String regionToken(String regNorm) {
        for (String t : REGIONES) if (regNorm.contains(t)) return t;
        return "";
    }
    private static boolean comunaMarker(String s) {
        return s.contains("comuna de") || s.contains("comunas de") || s.contains("provincia de");
    }
    /** ¿la región aparece como alcance AMPLIO (no ligada a una lista de comunas/provincia)? */
    static boolean regionWide(String win, String rt) {
        if (rt.isEmpty()) return false;
        int i = win.indexOf(rt);
        if (i < 0) return false;
        String before = win.substring(Math.max(0, i - 75), i);
        String after = win.substring(i, Math.min(win.length(), i + 95));
        int cut = after.indexOf(" y ");
        String head = cut < 0 ? after : after.substring(0, cut);
        return !(comunaMarker(before) || comunaMarker(head));
    }
    /** posiciones de «de <mes>» cuyo grupo de días (incl. «del A al B») cubre dd. */
    static List<Integer> dateAnchors(String text, int dd, String mes) {
        List<Integer> res = new ArrayList<>();
        String needle = "de " + mes;
        int p = 0;
        while ((p = text.indexOf(needle, p)) >= 0) {
            String pre = text.substring(Math.max(0, p - 40), p);
            List<Integer> nums = new ArrayList<>();
            for (int i = 0; i < pre.length(); ) {
                if (Character.isDigit(pre.charAt(i))) {
                    int v = 0, st = i;
                    while (i < pre.length() && Character.isDigit(pre.charAt(i)) && i - st < 2) { v = v * 10 + (pre.charAt(i) - '0'); i++; }
                    if (v >= 1 && v <= 31) nums.add(v);
                } else i++;
            }
            boolean cov = nums.contains(dd);
            if (!cov && pre.contains(" al ") && nums.size() >= 2) {
                int lo = nums.get(0), hi = nums.get(0);
                for (int v : nums) { lo = Math.min(lo, v); hi = Math.max(hi, v); }
                cov = dd >= lo && dd <= hi;
            }
            if (cov) res.add(p);
            p += needle.length();
        }
        return res;
    }
    private static String tag(String item, String t) {
        int a = item.indexOf("<" + t + ">");
        if (a < 0) return "";
        int b = item.indexOf("</" + t + ">", a);
        String v = b < 0 ? "" : item.substring(a + t.length() + 2, b).trim();
        if (v.startsWith("<![CDATA[") && v.endsWith("]]>")) v = v.substring(9, v.length() - 3);
        return v;
    }

    /** Veredicto de suspensión de clases para hoy, mañana y pasado (hora de Santiago). */
    static JSONObject suspension(String region, String comuna) {
        JSONObject r = new JSONObject();
        try {
            String rt = regionToken(norm(region == null ? "" : region));
            String cn = norm(comuna == null ? "" : comuna);
            String[] etq = {"hoy", "manana", "pasado"};

            List<String[]> arts = new ArrayList<>();          // {link, título, texto normalizado}
            String feed = get(FEED, 5000, 8000);
            boolean oficial = feed != null && !feed.isEmpty();
            if (oficial) {
                int pos = 0;
                while (arts.size() < 3) {
                    int a = feed.indexOf("<item", pos); if (a < 0) break;
                    int b = feed.indexOf("</item>", a); if (b < 0) break;
                    String item = feed.substring(a, b); pos = b + 7;
                    String ni = norm(item);
                    if (!(ni.contains("suspension") && ni.contains("clase"))) continue;
                    String link = unescape(tag(item, "link")), title = unescape(tag(item, "title"));
                    String page = link.startsWith("https://") ? get(link, 5000, 8000) : null;
                    String text = page == null ? "" : norm(unescape(stripTags(BLOCKS.matcher(page).replaceAll(" "))));
                    arts.add(new String[]{link, title, text});
                }
            }
            JSONArray dias = new JSONArray();
            for (int k = 0; k < 3; k++) {
                Calendar c = hoy(k);
                int dd = c.get(Calendar.DAY_OF_MONTH);
                String mes = MESES[c.get(Calendar.MONTH)];
                boolean susp = false; String alcance = "", fuente = "", titular = "";
                for (String[] ar : arts) {
                    String tn = norm(ar[1]);
                    if (!dateAnchors(tn, dd, mes).isEmpty()) {
                        if (cn.length() >= 4 && tn.contains(cn)) { susp = true; alcance = "comuna"; }
                        else if (regionWide(tn, rt)) { susp = true; alcance = "region"; }
                    }
                    if (!susp) for (int p : dateAnchors(ar[2], dd, mes)) {
                        int s = Math.max(0, p - 140);
                        String win = ar[2].substring(s, Math.min(ar[2].length(), s + 360));
                        if (cn.length() >= 4 && win.contains(cn)) { susp = true; alcance = "comuna"; }
                        else if (regionWide(win, rt)) { susp = true; alcance = "region"; }
                        if (susp) break;
                    }
                    if (susp) { fuente = ar[0]; titular = ar[1]; break; }
                }
                dias.put(new JSONObject().put("etiqueta", etq[k]).put("fecha", iso(c)).put("suspendido", susp)
                        .put("alcance", alcance).put("fuente", fuente).put("titular", titular));
            }
            r.put("oficial", oficial).put("dias", dias);
        } catch (Exception e) {
            try { r.put("oficial", false).put("dias", new JSONArray()); } catch (Exception ignored) { }
        }
        return r;
    }

    // ------------------------------------------------------------------ airerm
    /** Episodio OFICIAL declarado hoy en la RM (tabla datada de la portada de airerm). */
    static JSONObject restriccion() { return restriccionDe(get(AIRERM, 4000, 6000), hoy(0)); }

    static JSONObject restriccionDe(String html, Calendar c) {
        JSONObject r = new JSONObject();
        boolean oficial = false; String episodio = "Normal";
        if (html != null) {
            String t = norm(unescape(stripTags(BLOCKS.matcher(html).replaceAll(" ")))).replaceAll("\\s+", " ");
            String hoy = c.get(Calendar.DAY_OF_MONTH) + " de " + MESES[c.get(Calendar.MONTH)];
            int p = t.indexOf("restriccion vehicular para hoy");
            if (p >= 0) {
                String win = t.substring(p, Math.min(t.length(), p + 1000));   // sólo la tabla, no la leyenda
                if (win.contains(hoy)) {
                    oficial = true;
                    if (win.contains("emergencia ambiental")) episodio = "Emergencia";
                    else if (win.contains("preemergencia")) episodio = "Preemergencia";
                }
            }
        }
        try { r.put("oficial", oficial).put("fecha", iso(c)).put("episodio", episodio); } catch (Exception ignored) { }
        return r;
    }
}

package cl.kotone.clima;

import java.util.Hashtable;
import java.util.Random;
import java.util.Vector;
import javax.microedition.lcdui.*;

/**
 * Vista principal de Clima: cielo animado según la condición (día/noche, lluvia, nieve, tormenta, niebla, nubes,
 * estrellas), temperatura, detalle, 24 horas y 7 días. Táctil (arrastrar para desplazar) y teclado (↑↓ ←→, 5 = actualizar).
 */
public class Pantalla extends Canvas implements Runnable {
    final Clima app;
    Object json;
    boolean viejo, cargando;
    String error;
    volatile boolean viva = true;
    int scrollY, scrollH, maxY, maxH;
    int pressX, pressY, baseY, baseH;
    boolean arrastraHoras;
    long t0 = System.currentTimeMillis();
    final Random rnd = new Random();
    float[] px = new float[90], py = new float[90], pv = new float[90];
    Font fGrande, fMedia, fChica, fNegrita;
    int yHoras;

    Pantalla(Clima app) {
        this.app = app;
        setFullScreenMode(true);
        fChica = Font.getFont(Font.FACE_PROPORTIONAL, Font.STYLE_PLAIN, Font.SIZE_SMALL);
        fNegrita = Font.getFont(Font.FACE_PROPORTIONAL, Font.STYLE_BOLD, Font.SIZE_SMALL);
        fMedia = Font.getFont(Font.FACE_PROPORTIONAL, Font.STYLE_PLAIN, Font.SIZE_LARGE);
        fGrande = Fuentes.grande(fMedia);
        for (int i = 0; i < px.length; i++) { px[i] = rnd.nextFloat(); py[i] = rnd.nextFloat(); pv[i] = 0.5f + rnd.nextFloat(); }
        new Thread(this).start();
    }

    void parar() { viva = false; }
    void cargando(boolean c) { cargando = c; repaint(); }
    void error(String e) { error = e; repaint(); }
    void datos(Object j, boolean deCache) { json = j; viejo = deCache; if (j != null) error = null; repaint(); }

    public void run() {
        while (viva) {
            if (isShown()) repaint();
            try { Thread.sleep(66); } catch (InterruptedException e) { }
        }
    }

    // ---------------------------------------------------------------- condición
    int codigo() { return (int) Json.dbl(Json.obj(json, "current"), "weather_code", 0); }
    boolean esDia() { return json == null || Json.dbl(Json.obj(json, "current"), "is_day", 1) > 0; }

    static String texto(int c) {
        switch (c) {
            case 0: return "Despejado"; case 1: return "Mayormente despejado"; case 2: return "Parcialmente nublado";
            case 3: return "Nublado"; case 45: return "Niebla"; case 48: return "Niebla con escarcha";
            case 51: return "Llovizna débil"; case 53: return "Llovizna"; case 55: return "Llovizna intensa";
            case 56: case 57: return "Llovizna engelante"; case 61: return "Lluvia débil"; case 63: return "Lluvia";
            case 65: return "Lluvia intensa"; case 66: case 67: return "Lluvia engelante"; case 71: return "Nieve débil";
            case 73: return "Nieve"; case 75: return "Nieve intensa"; case 77: return "Granos de nieve";
            case 80: return "Chubascos débiles"; case 81: return "Chubascos"; case 82: return "Chubascos violentos";
            case 85: case 86: return "Chubascos de nieve"; case 95: return "Tormenta eléctrica";
            case 96: case 99: return "Tormenta con granizo"; default: return "—";
        }
    }
    /** 0 despejado, 1 nubes, 2 niebla, 3 lluvia, 4 nieve, 5 tormenta */
    static int grupo(int c) {
        if (c <= 1) return 0; if (c <= 3) return 1; if (c <= 48) return 2;
        if (c >= 71 && c <= 77 || c == 85 || c == 86) return 4; if (c >= 95) return 5; return 3;
    }
    /** Ícono vectorial de la condición centrado en (cx, cy), alto aprox. s. */
    void icono(Graphics g, int cx, int cy, int s, int c, boolean dia) {
        int gr = grupo(c), r = s / 3;
        if (gr == 0 || c == 1 || c == 2) {
            g.setColor(dia ? 0xFFD54A : 0xE6E9F2);
            int sx = cx - (gr == 0 ? 0 : r / 2), sy = cy - (gr == 0 ? 0 : r / 2);
            g.fillArc(sx - r, sy - r, 2 * r, 2 * r, 0, 360);
            if (!dia) { g.setColor(0x24304A); g.fillArc(sx - r + r / 2, sy - r - r / 3, 2 * r, 2 * r, 0, 360); }
            if (gr == 0) return;
        }
        int col = gr == 5 ? 0x8C93A6 : gr == 2 ? 0xC9CED4 : 0xEEF2F7;
        g.setColor(col);
        g.fillArc(cx - s / 2, cy - r / 2, s / 2 + 2, r + 2, 0, 360);
        g.fillArc(cx - r, cy - r, r * 2, r * 2 - 2, 0, 360);
        g.fillArc(cx, cy - r / 2, s / 2, r + 2, 0, 360);
        g.fillRect(cx - s / 3, cy, s * 2 / 3, r / 2 + 1);
        int by = cy + r / 2 + 3;
        if (gr == 3) { g.setColor(0x7FC4FF); for (int k = -1; k <= 1; k++) g.drawLine(cx + k * r, by, cx + k * r - 2, by + r); }
        else if (gr == 4) { g.setColor(0xFFFFFF); for (int k = -1; k <= 1; k++) g.fillRect(cx + k * r - 1, by + 1, 3, 3); }
        else if (gr == 5) { g.setColor(0xFFD54A); g.drawLine(cx, by, cx - 3, by + r / 2 + 2); g.drawLine(cx - 3, by + r / 2 + 2, cx + 2, by + r / 2 + 2); g.drawLine(cx + 2, by + r / 2 + 2, cx - 1, by + r + 3); }
        else if (gr == 2) { g.setColor(0xE0E4EA); g.drawLine(cx - s / 2, by, cx + s / 2, by); g.drawLine(cx - s / 3, by + 3, cx + s / 3, by + 3); }
    }

    static String glifo(int c, boolean dia) {
        switch (grupo(c)) {
            case 0: return dia ? "☀" : "☾"; case 1: return c == 2 ? (dia ? "⛅" : "☁") : "☁"; case 2: return "≋";
            case 3: return "☂"; case 4: return "❄"; default: return "⚡";
        }
    }

    // ---------------------------------------------------------------- dibujo
    protected void paint(Graphics g) {
        int w = getWidth(), h = getHeight();
        int c = codigo(), gr = grupo(c);
        boolean dia = esDia();
        cielo(g, w, h, gr, dia);
        particulas(g, w, h, gr, dia);
        g.setColor(0xFFFFFF);
        int y = 14 - scrollY;
        g.setFont(fMedia);
        g.drawString(app.comuna, w / 2, y, Graphics.TOP | Graphics.HCENTER); y += fMedia.getHeight();
        g.setFont(fChica);
        String sub = cargando ? "Actualizando…" : error != null ? error : viejo ? "Último dato guardado" : app.region;
        g.drawString(sub, w / 2, y, Graphics.TOP | Graphics.HCENTER); y += fChica.getHeight() + 4;
        if (json == null) {
            g.setFont(fMedia);
            g.drawString(cargando ? "Cargando pronóstico…" : "Sin datos", w / 2, h / 2, Graphics.BASELINE | Graphics.HCENTER);
            pie(g, w, h); return;
        }
        Hashtable cur = Json.obj(json, "current");
        g.setFont(fGrande);
        g.drawString(r(Json.dbl(cur, "temperature_2m", 0)) + "°", w / 2, y, Graphics.TOP | Graphics.HCENTER);
        y += fGrande.getHeight();
        g.setFont(fMedia);
        g.drawString(texto(c), w / 2, y, Graphics.TOP | Graphics.HCENTER); y += fMedia.getHeight();
        Hashtable d = Json.obj(json, "daily");
        Vector mx = Json.arr(d, "temperature_2m_max"), mn = Json.arr(d, "temperature_2m_min");
        g.setFont(fChica);
        g.drawString("Máx " + r(Json.dbl(mx, 0, 0)) + "°  Mín " + r(Json.dbl(mn, 0, 0)) + "°", w / 2, y, Graphics.TOP | Graphics.HCENTER);
        y += fChica.getHeight() + 10;

        // --- 24 horas (desplazable a lo ancho)
        yHoras = y;
        Hashtable ho = Json.obj(json, "hourly");
        Vector ht = Json.arr(ho, "time"), hT = Json.arr(ho, "temperature_2m"), hC = Json.arr(ho, "weather_code"), hP = Json.arr(ho, "precipitation_probability");
        int col = Math.max(52, w / 6), alto = fChica.getHeight() * 3 + 26;
        tarjeta(g, 8, y, w - 16, alto);
        g.setClip(8, y, w - 16, alto);
        int n = ht == null ? 0 : ht.size();
        maxH = Math.max(0, n * col - (w - 16));
        for (int i = 0; i < n; i++) {
            int x = 8 + i * col - scrollH + col / 2;
            if (x < -col || x > w + col) continue;
            String hora = (String) ht.elementAt(i);
            int cc = (int) Json.dbl(hC, i, 0);
            g.setColor(0xFFFFFF); g.setFont(fChica);
            g.drawString(i == 0 ? "Ahora" : hora.substring(11, 13), x, y + 6, Graphics.TOP | Graphics.HCENTER);
            icono(g, x, y + 8 + fChica.getHeight() + fChica.getHeight() / 2, fChica.getHeight(), cc, true);
            g.setFont(fNegrita);
            g.drawString(r(Json.dbl(hT, i, 0)) + "°", x, y + 10 + fChica.getHeight() * 2, Graphics.TOP | Graphics.HCENTER);
            int p = (int) Json.dbl(hP, i, 0);
            if (p >= 20) { g.setColor(0x9AD0FF); g.setFont(fChica); g.drawString(p + "%", x, y + alto - 4, Graphics.BOTTOM | Graphics.HCENTER); }
        }
        g.setClip(0, 0, w, h);
        y += alto + 10;

        // --- detalle
        int ad = fChica.getHeight() * 2 + 14;
        tarjeta(g, 8, y, w - 16, ad * 2);
        Vector sr = Json.arr(d, "sunrise"), ss = Json.arr(d, "sunset");
        dato(g, 8, y, (w - 16) / 2, ad, "Sensación", r(Json.dbl(cur, "apparent_temperature", 0)) + "°");
        dato(g, 8 + (w - 16) / 2, y, (w - 16) / 2, ad, "Humedad", r(Json.dbl(cur, "relative_humidity_2m", 0)) + "%");
        dato(g, 8, y + ad, (w - 16) / 2, ad, "Viento", r(Json.dbl(cur, "wind_speed_10m", 0)) + " km/h");
        dato(g, 8 + (w - 16) / 2, y + ad, (w - 16) / 2, ad, "Sol", hhmm(sr) + " – " + hhmm(ss));
        y += ad * 2 + 10;

        // --- 7 días
        Vector dt = Json.arr(d, "time"), dc = Json.arr(d, "weather_code"), dp = Json.arr(d, "precipitation_probability_max");
        int fila = fChica.getHeight() + 14, nd = dt == null ? 0 : dt.size();
        tarjeta(g, 8, y, w - 16, fila * nd + 8);
        double tmin = 99, tmax = -99;
        for (int i = 0; i < nd; i++) { tmin = Math.min(tmin, Json.dbl(mn, i, 0)); tmax = Math.max(tmax, Json.dbl(mx, i, 0)); }
        for (int i = 0; i < nd; i++) {
            int yy = y + 4 + i * fila + fila / 2;
            g.setColor(0xFFFFFF); g.setFont(fNegrita);
            g.drawString(i == 0 ? "Hoy" : dia((String) dt.elementAt(i)), 16, yy, Graphics.BASELINE | Graphics.LEFT);
            g.setFont(fChica);
            icono(g, 16 + w / 6 + 8, yy - fChica.getHeight() / 3, fChica.getHeight(), (int) Json.dbl(dc, i, 0), true);
            int pr = (int) Json.dbl(dp, i, 0);
            if (pr >= 20) { g.setColor(0x9AD0FF); g.drawString(pr + "%", 16 + w / 6 + 22, yy, Graphics.BASELINE | Graphics.LEFT); }
            // barra de rango como Apple Weather
            int bx = w / 2 - 10, bw = w / 2 - 70, lo = Json.dbl(mn, i, 0) > -99 ? (int) ((Json.dbl(mn, i, 0) - tmin) / Math.max(1, tmax - tmin) * bw) : 0;
            int hi = (int) ((Json.dbl(mx, i, 0) - tmin) / Math.max(1, tmax - tmin) * bw);
            g.setColor(0x5A6F8F); g.fillRoundRect(bx, yy - 6, bw, 5, 5, 5);
            g.setColor(0xFFC24B); g.fillRoundRect(bx + lo, yy - 6, Math.max(6, hi - lo), 5, 5, 5);
            g.setColor(0xC8D6EE); g.drawString(r(Json.dbl(mn, i, 0)) + "°", bx - 6, yy, Graphics.BASELINE | Graphics.RIGHT);
            g.setColor(0xFFFFFF); g.drawString(r(Json.dbl(mx, i, 0)) + "°", bx + bw + 6, yy, Graphics.BASELINE | Graphics.LEFT);
        }
        y += fila * nd + 18;
        maxY = Math.max(0, y + scrollY - h + 24);
        pie(g, w, h);
    }

    void pie(Graphics g, int w, int h) {
        g.setColor(0xDDE6F5); g.setFont(fChica);
        g.drawString("Opciones", 6, h - 4, Graphics.BOTTOM | Graphics.LEFT);
        g.drawString("Open-Meteo · J. Navarrete", w - 6, h - 4, Graphics.BOTTOM | Graphics.RIGHT);
    }

    void tarjeta(Graphics g, int x, int y, int w, int h) {
        // vidrio translúcido simulado: sólo bordes (MIDP no tiene alfa en primitivas)
        g.setColor(0xFFFFFF);
        g.drawRoundRect(x, y, w - 1, h - 1, 16, 16);
    }
    void dato(Graphics g, int x, int y, int w, int h, String et, String val) {
        g.setColor(0xC8D6EE); g.setFont(fChica); g.drawString(et, x + 10, y + 6, Graphics.TOP | Graphics.LEFT);
        g.setColor(0xFFFFFF); g.setFont(fNegrita); g.drawString(val, x + 10, y + 8 + fChica.getHeight(), Graphics.TOP | Graphics.LEFT);
    }

    static final int[][] CIELOS = {   // arriba, abajo · día y noche por grupo
        {0x2E7BD6, 0x8CC6F2, 0x0B1530, 0x2A3E6E}, {0x5D7FA8, 0xA9BED6, 0x161E2E, 0x39465E}, {0x8C97A3, 0xC9CED4, 0x2A2F37, 0x555C66},
        {0x3D5470, 0x7E93AA, 0x121A26, 0x2E3B4D}, {0x8FA9C8, 0xE3EBF5, 0x26334A, 0x56688A}, {0x2C3140, 0x5B6275, 0x0D0F16, 0x2B2F3D}};
    void cielo(Graphics g, int w, int h, int gr, boolean dia) {
        int a = CIELOS[gr][dia ? 0 : 2], b = CIELOS[gr][dia ? 1 : 3];
        int bandas = 24;
        for (int i = 0; i < bandas; i++) {
            g.setColor(mezcla(a, b, i * 255 / (bandas - 1)));
            g.fillRect(0, h * i / bandas, w, h / bandas + 1);
        }
        if (gr == 5 && (System.currentTimeMillis() / 100) % 47 == 0) { g.setColor(0xE8ECFF); g.fillRect(0, 0, w, h); }  // relámpago
    }
    void particulas(Graphics g, int w, int h, int gr, boolean dia) {
        float t = (System.currentTimeMillis() - t0) / 1000f;
        if (gr == 0) {
            if (dia) {
                int r0 = Math.min(w, h) / 6, cx = w - r0 - 12, cy = r0 + 30;
                for (int k = 3; k >= 1; k--) { g.setColor(mezcla(CIELOS[0][0], 0xFFE9A8, 90 / k)); int rr = r0 + k * 10 + (int) (4 * Math.sin(t * 2)); g.fillArc(cx - rr, cy - rr, rr * 2, rr * 2, 0, 360); }
                g.setColor(0xFFE27A); g.fillArc(cx - r0, cy - r0, r0 * 2, r0 * 2, 0, 360);
            } else estrellas(g, w, h, t);
        } else if (gr == 1 || gr == 2) {
            if (!dia) estrellas(g, w, h / 2, t);
            int base = gr == 2 ? 0xD8DCE2 : (dia ? 0xF2F5F9 : 0x5A6478);
            for (int i = 0; i < 4; i++) {
                int cw = w / 2 + i * 20, x = (int) (((px[i] * w + t * (8 + i * 4)) % (w + cw)) - cw), y = 40 + i * h / 9;
                nube(g, x, y, cw, mezcla(base, CIELOS[gr][dia ? 0 : 2], 60 + i * 30));
            }
            if (gr == 2) for (int i = 0; i < 6; i++) { g.setColor(mezcla(0xC9CED4, 0xFFFFFF, 40)); int y = h / 3 + i * h / 10 + (int) (6 * Math.sin(t + i)); g.drawLine(0, y, w, y); g.drawLine(0, y + 1, w, y + 1); }
        } else if (gr == 3 || gr == 5) {
            nube(g, -20, 20, w + 40, gr == 5 ? 0x3A3F4E : 0x6B7C91);
            g.setColor(0xB8CCE4);
            for (int i = 0; i < px.length; i++) {
                float y = (py[i] + t * pv[i] * 0.9f) % 1f, x = (px[i] + y * 0.08f) % 1f;
                int X = (int) (x * w), Y = (int) (y * h);
                g.drawLine(X, Y, X + 2, Y + 9);
            }
        } else if (gr == 4) {
            g.setColor(0xFFFFFF);
            for (int i = 0; i < px.length; i++) {
                float y = (py[i] + t * pv[i] * 0.12f) % 1f;
                int X = (int) ((px[i] + 0.03f * Math.sin(t + i)) * w), Y = (int) (y * h), s = 2 + (i % 3);
                g.fillArc(X, Y, s, s, 0, 360);
            }
        }
    }
    void estrellas(Graphics g, int w, int h, float t) {
        for (int i = 0; i < 50; i++) {
            int br = 140 + (int) (100 * Math.sin(t * pv[i] * 2 + i));
            g.setColor((br << 16) | (br << 8) | br);
            g.fillRect((int) (px[i] * w), (int) (py[i] * h * 0.7f), 2, 2);
        }
    }
    void nube(Graphics g, int x, int y, int w, int col) {
        g.setColor(col);
        int h = w / 4;
        g.fillArc(x, y + h / 3, w / 2, h, 0, 360);
        g.fillArc(x + w / 4, y, w / 2, h + h / 3, 0, 360);
        g.fillArc(x + w / 2, y + h / 4, w / 2, h, 0, 360);
        g.fillRect(x + w / 6, y + h / 2, w * 2 / 3, h * 2 / 3);
    }

    static int mezcla(int a, int b, int t) {   // t 0–255
        int r = ((a >> 16 & 255) * (255 - t) + (b >> 16 & 255) * t) / 255;
        int gg = ((a >> 8 & 255) * (255 - t) + (b >> 8 & 255) * t) / 255;
        int bb = ((a & 255) * (255 - t) + (b & 255) * t) / 255;
        return r << 16 | gg << 8 | bb;
    }
    static String r(double d) { return String.valueOf((int) Math.floor(d + 0.5)); }   // CLDC no tiene Math.round
    static String hhmm(Vector v) { if (v == null || v.size() == 0) return "—"; String s = (String) v.elementAt(0); return s.length() >= 16 ? s.substring(11, 16) : s; }
    static final String[] DIAS = {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"};
    static String dia(String iso) {   // yyyy-mm-dd → día de la semana (Zeller)
        int y = Integer.parseInt(iso.substring(0, 4)), m = Integer.parseInt(iso.substring(5, 7)), d = Integer.parseInt(iso.substring(8, 10));
        if (m < 3) { m += 12; y--; }
        int k = y % 100, j = y / 100, h = (d + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;   // 0 = sábado
        return DIAS[(h + 6) % 7];
    }
    static String sinTildes(String s) {
        StringBuffer b = new StringBuffer(s.length());
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            int k = "áéíóúüñ".indexOf(c);
            b.append(k >= 0 ? "aeiouun".charAt(k) : c);
        }
        return b.toString();
    }

    // ---------------------------------------------------------------- entrada
    protected void pointerPressed(int x, int y) {
        pressX = x; pressY = y; baseY = scrollY; baseH = scrollH;
        int yy = yHoras;
        arrastraHoras = y >= yy && y <= yy + fChica.getHeight() * 3 + 26;
        if (y > getHeight() - fChica.getHeight() - 8 && x < getWidth() / 3) app.disp.setCurrent(menu());
    }
    protected void pointerDragged(int x, int y) {
        if (arrastraHoras && Math.abs(x - pressX) > Math.abs(y - pressY)) scrollH = lim(baseH - (x - pressX), maxH);
        else scrollY = lim(baseY - (y - pressY), maxY);
        if (scrollY == 0 && y - pressY > 120 && !cargando) app.actualizar();   // tirar hacia abajo para actualizar
        repaint();
    }
    protected void keyPressed(int k) {
        int a = getGameAction(k);
        if (a == UP) scrollY = lim(scrollY - 40, maxY);
        else if (a == DOWN) scrollY = lim(scrollY + 40, maxY);
        else if (a == LEFT) scrollH = lim(scrollH - 60, maxH);
        else if (a == RIGHT) scrollH = lim(scrollH + 60, maxH);
        else if (k == KEY_NUM5 || a == FIRE) app.actualizar();
        else if (k == -6 || k == -7) app.disp.setCurrent(menu());      // teclas de función (Nokia)
        repaint();
    }
    static int lim(int v, int max) { return Math.max(0, Math.min(max, v)); }

    List menu;
    Displayable menu() {
        if (menu == null) {
            menu = new List("Clima", List.IMPLICIT, new String[]{"Actualizar", "Cambiar comuna", "Acerca de", "Salir"}, null);
            final Command volver = new Command("Volver", Command.BACK, 1);
            menu.addCommand(volver);
            menu.setCommandListener(new CommandListener() { public void commandAction(Command c, Displayable d) {
                if (c == volver) { app.disp.setCurrent(Pantalla.this); return; }
                int i = menu.getSelectedIndex();
                app.disp.setCurrent(Pantalla.this);
                app.commandAction(i == 0 ? app.cActualizar : i == 1 ? app.cComuna : i == 2 ? app.cAcerca : app.cSalir, Pantalla.this);
            }});
        }
        return menu;
    }
}

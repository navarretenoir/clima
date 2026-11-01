package cl.kotone.clima;

import java.io.*;
import java.util.Vector;
import javax.microedition.io.Connector;
import javax.microedition.io.HttpConnection;
import javax.microedition.lcdui.*;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;

/**
 * Clima para Symbian (Java ME · MIDP 2.0 / CLDC 1.1) — pensado para los últimos Symbian: Nokia Belle / Belle FP2
 * (808 PureView, 701, 700, 603, 600, E6, E7, N8, C7…) y también S60 5.ª ed. Pantalla táctil o teclado.
 * Datos: Open-Meteo por HTTP simple (Symbian no habla TLS 1.2; Open-Meteo lo admite sin cifrar y son datos públicos).
 * Desarrollado por Javiera Navarrete.
 */
public class Clima extends MIDlet implements CommandListener {
    static final String API = "http://api.open-meteo.com/v1/forecast?timezone=America%2FSantiago&forecast_days=7&forecast_hours=24"
            + "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
            + "&hourly=temperature_2m,weather_code,precipitation_probability"
            + "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_probability_max";

    Display disp;
    Pantalla pant;
    final Command cActualizar = new Command("Actualizar", Command.SCREEN, 1);
    final Command cComuna = new Command("Cambiar comuna", Command.SCREEN, 2);
    final Command cAcerca = new Command("Acerca de", Command.HELP, 3);
    final Command cSalir = new Command("Salir", Command.EXIT, 9);
    final Command cBuscar = new Command("Buscar", Command.OK, 1);
    final Command cVolver = new Command("Volver", Command.BACK, 2);
    final Command cElegir = new Command("Elegir", Command.ITEM, 1);

    String comuna = "Santiago", region = "Metropolitana";
    double lat = -33.4489, lon = -70.6693;
    Vector comunas;           // String[]{nombre, región, lat, lon}
    TextBox busca;
    List lista;
    Vector filtradas;

    protected void startApp() {
        if (disp != null) return;
        disp = Display.getDisplay(this);
        cargarPrefs();
        pant = new Pantalla(this);
        pant.addCommand(cActualizar); pant.addCommand(cComuna); pant.addCommand(cAcerca); pant.addCommand(cSalir);
        pant.setCommandListener(this);
        disp.setCurrent(pant);
        String guardado = leerRS("datos");
        if (guardado != null) try { pant.datos(Json.parse(guardado), true); } catch (Throwable t) { }
        actualizar();
    }
    protected void pauseApp() { }
    protected void destroyApp(boolean u) { if (pant != null) pant.parar(); }

    public void commandAction(Command c, Displayable d) {
        if (c == cSalir) { destroyApp(true); notifyDestroyed(); }
        else if (c == cActualizar) actualizar();
        else if (c == cAcerca) acerca();
        else if (c == cComuna) {
            busca = new TextBox("Buscar comuna", "", 40, TextField.ANY);
            busca.addCommand(cBuscar); busca.addCommand(cVolver); busca.setCommandListener(this);
            disp.setCurrent(busca);
        } else if (c == cBuscar && d == busca) mostrarLista(busca.getString());
        else if (c == cVolver) disp.setCurrent(pant);
        else if ((c == cElegir || c == List.SELECT_COMMAND) && d == lista) {
            int i = lista.getSelectedIndex();
            if (i >= 0) {
                String[] r = (String[]) filtradas.elementAt(i);
                comuna = r[0]; region = r[1]; lat = Double.parseDouble(r[2]); lon = Double.parseDouble(r[3]);
                guardarRS("prefs", comuna + "|" + region + "|" + r[2] + "|" + r[3]);
                guardarRS("datos", null);
                pant.datos(null, false);
                disp.setCurrent(pant);
                actualizar();
            }
        }
    }

    void acerca() {
        Alert a = new Alert("Clima", "Clima 1.0 para Symbian\n\nDesarrollado por Javiera Navarrete.\n\n"
                + "Pronóstico de Open-Meteo (CC BY 4.0) para las 346 comunas de Chile.\n"
                + "Sin publicidad, sin rastreo: sólo se consulta el pronóstico de la comuna elegida.", null, AlertType.INFO);
        a.setTimeout(Alert.FOREVER);
        disp.setCurrent(a, pant);
    }

    void mostrarLista(String q) {
        if (comunas == null) comunas = leerComunas();
        String nq = Pantalla.sinTildes(q.trim().toLowerCase());
        filtradas = new Vector();
        lista = new List("Comunas", List.IMPLICIT);
        for (int i = 0; i < comunas.size(); i++) {
            String[] r = (String[]) comunas.elementAt(i);
            if (nq.length() == 0 || Pantalla.sinTildes(r[0].toLowerCase()).indexOf(nq) >= 0) {
                filtradas.addElement(r);
                lista.append(r[0] + " · " + r[1], null);
            }
        }
        lista.addCommand(cElegir); lista.addCommand(cVolver);
        lista.setSelectCommand(cElegir);
        lista.setCommandListener(this);
        disp.setCurrent(lista);
    }

    Vector leerComunas() {
        Vector v = new Vector();
        try {
            InputStream in = getClass().getResourceAsStream("/comunas.txt");
            String t = new String(leerTodo(in), "UTF-8");
            int ini = 0;
            while (ini < t.length()) {
                int fin = t.indexOf('\n', ini); if (fin < 0) fin = t.length();
                String l = t.substring(ini, fin); ini = fin + 1;
                if (l.length() == 0) continue;
                String[] r = new String[4]; int p = 0, k = 0;
                for (int j = 0; j < 3; j++) { int q = l.indexOf('|', p); r[k++] = l.substring(p, q); p = q + 1; }
                r[3] = l.substring(p);
                v.addElement(r);
            }
        } catch (Exception e) { }
        return v;
    }

    void actualizar() {
        pant.cargando(true);
        new Thread() { public void run() {
            HttpConnection hc = null;
            try {
                hc = (HttpConnection) Connector.open(API + "&latitude=" + lat + "&longitude=" + lon);
                hc.setRequestProperty("User-Agent", "Clima-Symbian/1.0 (Javiera Navarrete)");
                if (hc.getResponseCode() != 200) throw new IOException("HTTP " + hc.getResponseCode());
                String txt = new String(leerTodo(hc.openInputStream()), "UTF-8");
                pant.datos(Json.parse(txt), false);
                guardarRS("datos", txt);
            } catch (Throwable t) {
                pant.error("Sin conexión: " + t.getMessage());
            } finally {
                try { if (hc != null) hc.close(); } catch (Exception e) { }
                pant.cargando(false);
            }
        }}.start();
    }

    static byte[] leerTodo(InputStream in) throws IOException {
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        byte[] buf = new byte[2048]; int n;
        while ((n = in.read(buf)) > 0) b.write(buf, 0, n);
        in.close();
        return b.toByteArray();
    }

    // ---------------------------------------------------------------- preferencias (RecordStore)
    void cargarPrefs() {
        String p = leerRS("prefs");
        if (p == null) return;
        try {
            int a = p.indexOf('|'), b = p.indexOf('|', a + 1), c = p.indexOf('|', b + 1);
            comuna = p.substring(0, a); region = p.substring(a + 1, b);
            lat = Double.parseDouble(p.substring(b + 1, c)); lon = Double.parseDouble(p.substring(c + 1));
        } catch (Exception e) { }
    }
    static String leerRS(String nombre) {
        try {
            RecordStore rs = RecordStore.openRecordStore("clima-" + nombre, true);
            String r = rs.getNumRecords() > 0 ? new String(rs.getRecord(1), "UTF-8") : null;
            rs.closeRecordStore();
            return r;
        } catch (Exception e) { return null; }
    }
    static void guardarRS(String nombre, String valor) {
        try {
            if (valor == null) { RecordStore.deleteRecordStore("clima-" + nombre); return; }
            RecordStore rs = RecordStore.openRecordStore("clima-" + nombre, true);
            byte[] b = valor.getBytes("UTF-8");
            if (rs.getNumRecords() > 0) rs.setRecord(1, b, 0, b.length); else rs.addRecord(b, 0, b.length);
            rs.closeRecordStore();
        } catch (Exception e) { }
    }
}

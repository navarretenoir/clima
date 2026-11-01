package cl.kotone.clima;

import java.util.Hashtable;
import java.util.Vector;

/** Lector JSON mínimo para CLDC 1.1 (objetos → Hashtable, listas → Vector, números → Double, null → JSON.NULL). */
public final class Json {
    public static final Object NULL = new Object();
    private final String s;
    private int i;

    private Json(String s) { this.s = s; }

    public static Object parse(String s) { Json j = new Json(s); j.ws(); return j.valor(); }

    private void ws() { while (i < s.length() && s.charAt(i) <= ' ') i++; }

    private Object valor() {
        ws();
        char c = s.charAt(i);
        if (c == '{') return objeto();
        if (c == '[') return lista();
        if (c == '"') return texto();
        if (s.startsWith("true", i)) { i += 4; return Boolean.TRUE; }
        if (s.startsWith("false", i)) { i += 5; return Boolean.FALSE; }
        if (s.startsWith("null", i)) { i += 4; return NULL; }
        int ini = i;
        while (i < s.length() && "+-0123456789.eE".indexOf(s.charAt(i)) >= 0) i++;
        return Double.valueOf(s.substring(ini, i));
    }

    private Hashtable objeto() {
        Hashtable h = new Hashtable();
        i++; ws();
        if (s.charAt(i) == '}') { i++; return h; }
        while (true) {
            ws(); String k = texto(); ws(); i++;            // ':'
            h.put(k, valor()); ws();
            if (s.charAt(i++) == '}') return h;             // si no, ','
        }
    }

    private Vector lista() {
        Vector v = new Vector();
        i++; ws();
        if (s.charAt(i) == ']') { i++; return v; }
        while (true) {
            v.addElement(valor()); ws();
            if (s.charAt(i++) == ']') return v;
        }
    }

    private String texto() {
        StringBuffer b = new StringBuffer();
        i++;
        while (true) {
            char c = s.charAt(i++);
            if (c == '"') return b.toString();
            if (c != '\\') { b.append(c); continue; }
            c = s.charAt(i++);
            switch (c) {
                case 'n': b.append('\n'); break;
                case 't': b.append('\t'); break;
                case 'r': break;
                case 'b': case 'f': break;
                case 'u': b.append((char) Integer.parseInt(s.substring(i, i + 4), 16)); i += 4; break;
                default: b.append(c);
            }
        }
    }

    // ---- accesos cómodos
    public static Hashtable obj(Object o, String k) { Object v = ((Hashtable) o).get(k); return v instanceof Hashtable ? (Hashtable) v : null; }
    public static Vector arr(Object o, String k) { Object v = ((Hashtable) o).get(k); return v instanceof Vector ? (Vector) v : null; }
    public static String str(Object o, String k) { Object v = o == null ? null : ((Hashtable) o).get(k); return v instanceof String ? (String) v : (v instanceof Double ? num(v) : ""); }
    public static double dbl(Object o, String k, double def) { Object v = o == null ? null : ((Hashtable) o).get(k); return v instanceof Double ? ((Double) v).doubleValue() : def; }
    public static double dbl(Vector v, int idx, double def) { if (v == null || idx >= v.size()) return def; Object o = v.elementAt(idx); return o instanceof Double ? ((Double) o).doubleValue() : def; }
    private static String num(Object v) { double d = ((Double) v).doubleValue(); return d == (long) d ? String.valueOf((long) d) : String.valueOf(d); }
}

package cl.kotone.clima;

import javax.microedition.lcdui.Font;

/** Cifra grande de temperatura: en Nokia (S60 5.ª / Belle) con DirectUtils a 64 px; si no existe, la fuente grande normal. */
final class Fuentes {
    static Font grande(Font respaldo) {
        try {
            Class.forName("com.nokia.mid.ui.DirectUtils");
            return Nokia.fuente(64);
        } catch (Throwable t) { return respaldo; }
    }
    /** Aparte para que el verificador no resuelva DirectUtils en teléfonos que no lo tienen. */
    static final class Nokia {
        static Font fuente(int px) {
            Font f = com.nokia.mid.ui.DirectUtils.getFont(Font.FACE_PROPORTIONAL, Font.STYLE_PLAIN, px);
            return f != null ? f : Font.getFont(Font.FACE_PROPORTIONAL, Font.STYLE_BOLD, Font.SIZE_LARGE);
        }
    }
}

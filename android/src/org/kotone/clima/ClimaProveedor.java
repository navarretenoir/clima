package org.kotone.clima;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;

/**
 * Puerta de Clima hacia la app de la bici: content://org.kotone.clima.alertas, método «alertas»
 * con extras {lat, lon} → Bundle{json} (ver Alertas.calcular). Protegida por un permiso de FIRMA:
 * sólo apps firmadas con la misma llave (ADO O260T) pueden llamarla. call() no aplica solo el
 * readPermission del manifiesto, así que se verifica aquí. Kotone 2026-10-31.
 */
public class ClimaProveedor extends ContentProvider {
    @Override public boolean onCreate() { return true; }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        Bundle out = new Bundle();
        if (getContext() == null || getContext().checkCallingPermission(Alertas.PERMISO) != PackageManager.PERMISSION_GRANTED) {
            out.putString("error", "sin permiso");
            return out;
        }
        if ("alertas".equals(method) && extras != null && extras.containsKey("lat") && extras.containsKey("lon")) {
            out.putString("json", Alertas.calcular(getContext(), extras.getDouble("lat"), extras.getDouble("lon")).toString());
        } else out.putString("error", "método o parámetros inválidos");
        return out;
    }

    @Override public Cursor query(Uri u, String[] p, String s, String[] a, String o) { return null; }
    @Override public String getType(Uri u) { return null; }
    @Override public Uri insert(Uri u, ContentValues v) { return null; }
    @Override public int delete(Uri u, String s, String[] a) { return 0; }
    @Override public int update(Uri u, ContentValues v, String s, String[] a) { return 0; }
}

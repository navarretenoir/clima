#!/usr/bin/env bash
# Clima para Symbian (Java ME). Requiere el kit de "$(git rev-parse --show-toplevel)/herramientas"/j2me (ecj + ProGuard + CLDC/MIDP).
exec "$(git rev-parse --show-toplevel)/herramientas"/j2me/construir-midlet.sh "$(dirname "$(readlink -f "$0")")" Clima cl.kotone.clima.Clima 1.0.0 javax.microedition.io.Connector.http

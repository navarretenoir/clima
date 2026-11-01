# Quita genéricos/anotaciones Java 5 de fuentes de Bouncy Castle para compilarlas con -source 1.3 (CLDC 1.1).
import re,sys
P=re.compile(r'<(?:[A-Z][\w.]*(?:\[\])?|\?(?: extends [\w.]+)?)(?:\s*,\s*(?:[A-Z][\w.]*(?:\[\])?|\?))*>')
for p in sys.argv[1:]:
    s=open(p,encoding="utf-8",errors="replace").read(); t=s
    for _ in range(3): t=P.sub("",t)
    t=re.sub(r'@Override\b|@SuppressWarnings\([^)]*\)','',t)
    if t!=s: open(p,"w",encoding="utf-8").write(t)

// Anfitrión WKWebView para macOS (Intel y Apple Silicon) e iOS/iPadOS — el mismo archivo en Clima, Metro y Tu Mundo Pet.
// Cada app lo configura con su Config.swift (ConfigApp.actual). Desarrollado por Javiera Navarrete.
import SwiftUI
import WebKit
import CoreLocation
#if os(macOS)
import AppKit
typealias VistaNativa = NSViewRepresentable
#else
import UIKit
typealias VistaNativa = UIViewRepresentable
#endif

enum Puente { case clima, metro, ninguno }

struct ConfigApp {
    let nombre: String
    let inicio: Inicio                  // interfaz local (carpeta «web» del paquete) o sitio remoto
    let puente: Puente
    let permitidos: [String]            // prefijos de URL que el puente puede pedir
    let fuera: [String]                 // dominios que se abren en el navegador / app del sistema
    let ventanasDentro: Bool            // pasarelas de pago: ventanas nuevas en la misma vista
    let activos: [String]               // archivos de «web/» que se inyectan como window.__CLIMA_ASSETS
    enum Inicio { case local(String), remoto(URL) }
}

/// Coordinador: mensajes de la página, navegación, ventanas nuevas y ubicación.
final class Coordinador: NSObject, WKScriptMessageHandler, WKNavigationDelegate, WKUIDelegate, CLLocationManagerDelegate {
    let cfg: ConfigApp
    weak var web: WKWebView?
    private let ubicacion = CLLocationManager()
    private var esperandoUbicacion: [String] = []

    init(cfg: ConfigApp) { self.cfg = cfg; super.init(); ubicacion.delegate = self; ubicacion.desiredAccuracy = kCLLocationAccuracyHundredMeters }

    // ------------------------------------------------------------------ JS → nativo
    func userContentController(_ ucc: WKUserContentController, didReceive m: WKScriptMessage) {
        guard let s = m.body as? String, let d = s.data(using: .utf8),
              let o = try? JSONSerialization.jsonObject(with: d) as? [String: Any] else { return }
        let accion = (o["a"] ?? o["action"]) as? String ?? ""
        let id = "\(o["id"] ?? "0")"
        switch accion {
        case "get":
            let url = o["url"] as? String ?? ""
            guard cfg.permitidos.contains(where: { url.hasPrefix($0) }), let u = URL(string: url) else { responder(id, ok: false, cuerpo: "", o: o); return }
            var req = URLRequest(url: u, timeoutInterval: 12)
            req.setValue("\(cfg.nombre)-Apple/1.0 (Javiera Navarrete)", forHTTPHeaderField: "User-Agent")
            URLSession.shared.dataTask(with: req) { datos, resp, _ in
                let codigo = (resp as? HTTPURLResponse)?.statusCode ?? 0
                let txt = datos.flatMap { String(data: $0, encoding: .utf8) ?? String(data: $0, encoding: .isoLatin1) } ?? ""
                DispatchQueue.main.async { self.responder(id, ok: codigo > 0 && codigo < 400 && datos != nil, cuerpo: txt, o: o) }
            }.resume()
        case "locate":
            esperandoUbicacion.append(id)
            switch ubicacion.authorizationStatus {
            case .notDetermined: ubicacion.requestWhenInUseAuthorization()
            case .denied, .restricted: entregarUbicacion(nil)
            default: ubicacion.requestLocation()
            }
        case "abrir":
            if let u = URL(string: o["url"] as? String ?? ""), u.scheme == "https" { abrirFuera(u) }
        default: break
        }
    }

    private func responder(_ id: String, ok: Bool, cuerpo: String, o: [String: Any]) {
        let lit = cadenaJS(cuerpo)
        let fn: String
        switch cfg.puente {
        case .metro: fn = "window.__metroHttp(\(Int(id) ?? 0),\(ok),\(lit))"
        case .clima: fn = id.hasPrefix("o") ? "window.__nativoHttp(\(cadenaJS(id)),\(ok),\(lit))" : "window.__climaHttp(\(Int(id) ?? 0),\(ok),\(lit))"
        case .ninguno: return
        }
        web?.evaluateJavaScript(fn)
    }
    private func entregarUbicacion(_ l: CLLocation?) {
        let fn = cfg.puente == .metro ? "__metroLoc" : "__climaLoc"
        for id in esperandoUbicacion {
            let args = l.map { "\($0.coordinate.latitude),\($0.coordinate.longitude)" } ?? "null,null"
            web?.evaluateJavaScript("window.\(fn)(\(Int(id) ?? 0),\(args))")
        }
        esperandoUbicacion.removeAll()
    }
    func locationManagerDidChangeAuthorization(_ m: CLLocationManager) {
        guard !esperandoUbicacion.isEmpty else { return }
        switch m.authorizationStatus {
        case .denied, .restricted: entregarUbicacion(nil)
        case .notDetermined: break
        default: m.requestLocation()
        }
    }
    func locationManager(_ m: CLLocationManager, didUpdateLocations l: [CLLocation]) { entregarUbicacion(l.last) }
    func locationManager(_ m: CLLocationManager, didFailWithError e: Error) { entregarUbicacion(nil) }

    // ------------------------------------------------------------------ navegación
    private func esFuera(_ u: URL) -> Bool {
        if let s = u.scheme, ["mailto", "tel", "whatsapp", "sms"].contains(s) { return true }
        let t = u.absoluteString
        return cfg.fuera.contains(where: { t.contains($0) })
    }
    private func abrirFuera(_ u: URL) {
        #if os(macOS)
        NSWorkspace.shared.open(u)
        #else
        UIApplication.shared.open(u)
        #endif
    }
    func webView(_ w: WKWebView, decidePolicyFor a: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        if let u = a.request.url, esFuera(u) { abrirFuera(u); decisionHandler(.cancel); return }
        decisionHandler(.allow)
    }
    func webView(_ w: WKWebView, createWebViewWith c: WKWebViewConfiguration, for a: WKNavigationAction, windowFeatures f: WKWindowFeatures) -> WKWebView? {
        if let u = a.request.url {
            if esFuera(u) || !cfg.ventanasDentro { abrirFuera(u) } else { w.load(a.request) }
        }
        return nil
    }
    func webView(_ w: WKWebView, didFail n: WKNavigation!, withError e: Error) { sinConexion(w, e) }
    func webView(_ w: WKWebView, didFailProvisionalNavigation n: WKNavigation!, withError e: Error) { sinConexion(w, e) }
    private func sinConexion(_ w: WKWebView, _ e: Error) {
        guard case .remoto(let url) = cfg.inicio, (e as NSError).code != NSURLErrorCancelled else { return }
        w.loadHTMLString("""
        <html><body style="font-family:-apple-system;text-align:center;padding-top:30vh;color:#046059">
        <h2>Sin conexión</h2><p>No se pudo abrir la tienda.</p><p><a href="\(url.absoluteString)">Reintentar</a></p></body></html>
        """, baseURL: url)
    }
}

func cadenaJS(_ s: String) -> String {
    let d = (try? JSONSerialization.data(withJSONObject: [s])) ?? Data("[\"\"]".utf8)
    let t = String(data: d, encoding: .utf8) ?? "[\"\"]"
    return String(t.dropFirst().dropLast())
}

/// La vista web, igual en macOS e iOS.
struct VistaWeb: VistaNativa {
    let cfg: ConfigApp
    func makeCoordinator() -> Coordinador { Coordinador(cfg: cfg) }

    private func crear(_ c: Coordinador) -> WKWebView {
        let conf = WKWebViewConfiguration()
        conf.websiteDataStore = .default()                     // carrito, sesión y caché persisten
        let ucc = WKUserContentController()
        switch cfg.puente {
        case .clima: ucc.add(c, name: "nativo")
        case .metro: ucc.add(c, name: "bridge")
        case .ninguno: break
        }
        if !cfg.activos.isEmpty, let web = Bundle.main.url(forResource: "web", withExtension: nil) {
            var dic: [String: String] = [:]
            for a in cfg.activos { dic[a] = (try? String(contentsOf: web.appendingPathComponent(a), encoding: .utf8)) ?? "" }
            if let d = try? JSONSerialization.data(withJSONObject: dic), let j = String(data: d, encoding: .utf8) {
                ucc.addUserScript(WKUserScript(source: "window.__CLIMA_ASSETS=\(j);", injectionTime: .atDocumentStart, forMainFrameOnly: true))
            }
        }
        conf.userContentController = ucc
        let w = WKWebView(frame: .zero, configuration: conf)
        w.navigationDelegate = c
        w.uiDelegate = c
        w.customUserAgent = nil
        #if os(iOS)
        w.scrollView.contentInsetAdjustmentBehavior = .never
        #endif
        c.web = w
        switch cfg.inicio {
        case .local(let archivo):
            if let web = Bundle.main.url(forResource: "web", withExtension: nil) {
                w.loadFileURL(web.appendingPathComponent(archivo), allowingReadAccessTo: web)
            }
        case .remoto(let url): w.load(URLRequest(url: url))
        }
        return w
    }
    #if os(macOS)
    func makeNSView(context: Context) -> WKWebView { crear(context.coordinator) }
    func updateNSView(_ v: WKWebView, context: Context) { }
    #else
    func makeUIView(context: Context) -> WKWebView { crear(context.coordinator) }
    func updateUIView(_ v: WKWebView, context: Context) { }
    #endif
}

@main
struct AppPrincipal: App {
    var body: some Scene {
        WindowGroup {
            VistaWeb(cfg: ConfigApp.actual)
                .ignoresSafeArea(edges: ConfigApp.actual.puente == .ninguno ? [] : .all)
                #if os(macOS)
                .frame(minWidth: 380, idealWidth: 460, minHeight: 600, idealHeight: 860)
                #endif
        }
        #if os(macOS)
        .commands {
            CommandGroup(replacing: .appInfo) {
                Button("Acerca de \(ConfigApp.actual.nombre)") {
                    NSApplication.shared.orderFrontStandardAboutPanel(options: [
                        .credits: NSAttributedString(string: "Desarrollado por Javiera Navarrete."),
                    ])
                }
            }
        }
        #endif
    }
}

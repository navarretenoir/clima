// Clima para macOS e iOS — configuración del anfitrión. Desarrollado por Javiera Navarrete.
import Foundation
extension ConfigApp {
    static let actual = ConfigApp(
        nombre: "Clima",
        inicio: .local("index.html"),
        puente: .clima,
        permitidos: ["https://api.open-meteo.com/", "https://air-quality-api.open-meteo.com/", "https://marine-api.open-meteo.com/",
                     "https://ipapi.co/", "https://www.metro.cl/", "https://www.mineduc.cl/", "https://airerm.mma.gob.cl/"],
        fuera: [],
        ventanasDentro: false,
        activos: ["data/comunas_cl.json"])
}

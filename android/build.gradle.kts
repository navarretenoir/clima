// Clima para Android (Kotone). Sin dependencias externas: sólo el SDK de Android (apto para F-Droid).
plugins { id("com.android.application") version "9.4.1" }

val uiCompartida = rootDir.resolve("../ui")
val assetsUi = layout.buildDirectory.dir("assets-ui").get().asFile

// Arma la interfaz igual que build.sh: ../ui + capas Android (web/) + datos, e inserta android.css y clima-android.js.
val prepararUi by tasks.registering(Sync::class) {
    into(assetsUi)
    from(uiCompartida) { include("*.js", "*.css"); into("ui") }
    from("web") { include("clima-android.js", "android.css"); into("ui") }
    from("web/fonts") { into("ui/fonts") }
    from(rootDir.resolve("../data/comunas_cl.json")) { into("data") }
    from(uiCompartida) {
        include("index.html"); into("ui")
        filter { l -> l.replace("<link rel=\"stylesheet\" href=\"style.css\">", "<link rel=\"stylesheet\" href=\"style.css\">\n<link rel=\"stylesheet\" href=\"android.css\">")
                       .replace("<script src=\"app.js\"></script>", "<script src=\"clima-android.js\"></script>\n  <script src=\"app.js\"></script>") }
    }
}

android {
    namespace = "org.kotone.clima"
    compileSdk { version = release(36) { minorApiLevel = 1 } }   // 36.1: Now Bar (setShortCriticalText…)
    defaultConfig {
        applicationId = "org.kotone.clima"
        minSdk = 21          // Android 5.0: la mayor compatibilidad; lo moderno va protegido por versión
        targetSdk = 36
        versionCode = 3
        versionName = "1.2"
    }
    sourceSets["main"].apply {
        manifest.srcFile("AndroidManifest.xml")
        java.srcDirs("src")
        res.srcDirs("res")
        assets.srcDirs(assetsUi)
    }
    androidResources { noCompress += "ttf" }
    buildTypes {
        release { isMinifyEnabled = false }
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    lint { abortOnError = false; checkOnly += setOf("NewApi", "InlinedApi") ; xmlReport = true }
}
tasks.named("preBuild") { dependsOn(prepararUi) }

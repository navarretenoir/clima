// Clima — proyecto Android Studio / F-Droid. Comparte src/, res/ y la interfaz ../ui con build.sh y la versión de escritorio.
pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories { google(); mavenCentral() }
}
rootProject.name = "Clima"

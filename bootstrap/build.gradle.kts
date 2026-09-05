plugins {
    id("com.android.application")
}

android {
    namespace = "org.guitargirlresuscitation.memorial"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.guitargirlresuscitation.memorial.bootstrap"
        minSdk = 23
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += "arm64-v8a" }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DGGFM_DOBBY_LIBRARY=${providers.gradleProperty("ggfmDobbyLibrary").get()}",
                    "-DGGFM_DOBBY_SHA256=${providers.gradleProperty("ggfmDobbySha256").get()}",
                    "-DGGFM_SERVER_LIBRARY=${providers.gradleProperty("ggfmServerLibrary").get()}",
                    "-DGGFM_SERVER_SHA256=${providers.gradleProperty("ggfmServerSha256").get()}",
                )
            }
        }
    }
    externalNativeBuild {
        cmake {
            path = file("../CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

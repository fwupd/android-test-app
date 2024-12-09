plugins {
  id("com.android.application")
  //id("com.android.library")
  id("kotlin-android")
  id("org.jetbrains.kotlin.plugin.compose") version "2.0.20"
}

val fwupdAidlPath: String by rootProject.extra
//val fwupdAidlPath by rootProject.extra

android {
  namespace = "org.freedesktop.fwupd"
  compileSdk = 34
  defaultConfig {
    applicationId = "org.freedesktop.fwupd"
    minSdk = 30
    targetSdk = 34
    versionCode = 1
    versionName = "0.0.0"
  }
  buildFeatures {
    aidl = true
    compose = true
  }
  sourceSets {
    val main by getting
    main.apply {
      aidl.setSrcDirs(listOf("src/main/aidl", fwupdAidlPath))
    }
  }

  composeOptions {
    kotlinCompilerExtensionVersion = "1.5.15"
  }
  //compileOptions {
  //  sourceCompatibility = JavaVersion.VERSION_17
  //  targetCompatibility = JavaVersion.VERSION_17
  //}

  kotlin {
    jvmToolchain(17)
  }
}

dependencies {
  implementation("androidx.core:core-ktx:1.13.1")
  implementation("androidx.activity:activity-compose:1.9.2")
  implementation("androidx.appcompat:appcompat:1.2.0")
  implementation("androidx.compose.material3:material3:1.3.0")
  implementation("androidx.compose.ui:ui:1.7.3")
  implementation("androidx.compose.runtime:runtime:1.7.3")
  implementation("androidx.compose.foundation:foundation:1.7.3")
  implementation("androidx.activity:activity:1.9.2")
  implementation("androidx.activity:activity-compose:1.9.2")
}
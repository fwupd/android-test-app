buildscript {
  val kotlinVersion = "2.0.20"
  repositories {
    maven {
      url = uri("https://maven.google.com")
    }
    google()
    mavenCentral()
  }
  dependencies {
    classpath("com.android.tools.build:gradle:8.7.0")
    classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:$kotlinVersion")
  }
}

allprojects {
  repositories {
    google()
    mavenCentral()
    maven("https://jitpack.io")
	}
}
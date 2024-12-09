buildscript {
  val kotlinVersion = "2.0.20"

  extra.apply{
    set("fwupdAidlPath", System.getenv("FWUPD_AIDL_PATH") ?: "fwupd/contrib/android/aidl")
    //set("targetSdkVersion", 27)
  }

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

//val fwupdAidlPath by project.extra { System.getenv("FWUPD_AIDL_PATH") ?: "fwupd/contrib/android/aidl" }
/*project.ext {
  fwupdAidlPath = System.getenv("FWUPD_AIDL_PATH") ?: "fwupd/contrib/android/aidl"
}*/



/*extra.apply {
  set("fwupdAidlPath", System.getenv("FWUPD_AIDL_PATH") ?: "fwupd/contrib/android/aidl")
}*/

allprojects {
  repositories {
    google()
    mavenCentral()
    maven("https://jitpack.io")
  }
}
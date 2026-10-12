plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.greatwhitek9lab.platypus"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.greatwhitek9lab.platypus"
        minSdk = 26   // broad Android-device coverage; USB host API itself works from API 12+
        targetSdk = 35 // bump alongside the Play/AOSP target level current at build time
        versionCode = 1
        versionName = "0.2.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.14"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.lifecycle:lifecycle-service:2.8.4")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.4")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.4")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")

    // Clean, crisp dashboard UI.
    val composeBom = platform("androidx.compose:compose-bom:2024.09.00")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.activity:activity-compose:1.9.1")
    debugImplementation("androidx.compose.ui:ui-tooling")

    // usb-serial-for-android: talks to CDC-ACM style USB devices purely
    // through Android's USB *host* API, with no kernel CDC driver needed.
    // That's what makes a single dongle/firmware work across different
    // phones/tablets (Galaxy S26 Ultra and otherwise) instead of depending
    // on each OEM's kernel USB config.
    // Check https://github.com/mik3y/usb-serial-for-android/releases for
    // the current tag and update this pin accordingly.
    implementation("com.github.mik3y:usb-serial-for-android:3.8.1")
}

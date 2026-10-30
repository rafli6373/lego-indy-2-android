# LEGO Indiana Jones 2: The Adventure Continues — Native Android Port Project

Proyek ini adalah arsitektur dasar (*scaffolding framework*) untuk menjalankan game **LEGO Indiana Jones 2: The Adventure Continues** (ID: `ULUS10487`) secara **native** di platform Android menggunakan **C++17, Android NDK, dan OpenGL ES 3.0**.

---

## 🏗️ Arsitektur Proyek

Proyek ini dibangun menggunakan arsitektur **Native Activity (NDK)** tanpa memerlukan aplikasi emulator eksternal:

```
D:\Game\LEGO Indiana Jones 2 (ANDROID)/
├── app/
│   ├── build.gradle.kts           # Konfigurasi NDK, CMake, dan ABI target (arm64-v8a, armeabi-v7a)
│   └── src/main/
│       ├── AndroidManifest.xml    # Entry point NativeActivity & konfigurasi fullscreen landscape
│       ├── assets/                # Folder tempat aset game (legoindy2psp.fib) disimpan
│       └── cpp/
│           ├── CMakeLists.txt     # Script build C++ & linkage library (GLESv3, EGL, Android, Log)
│           ├── native_main.cpp    # Lifecycle event loop Android, inisialisasi EGL, input polling
│           ├── psp_types.h        # Definisi hardware PSP (Register, Bitmask controller, Memori)
│           ├── psp_hle.h / .cpp   # Host High-Level Emulation layer (sceIo, sceCtrl, sceKernel RAM)
│           ├── psp_renderer_gl.*  # Renderer OpenGL ES 3.0 (Letterbox 16:9 & HUD controller)
│           └── touch_controls.*   # Multi-touch virtual on-screen gamepad (Analog stick & tombol)
├── build.gradle.kts               # Root gradle script
├── settings.gradle.kts            # Module settings
└── copy_assets.ps1                # Script otomatis menyalin data game ke folder assets
```

---

## 🎮 Fitur Komponen Native C++

1. **`native_main.cpp` (Android Lifecycle & EGL)**:
   * Mengatur *context* EGL OpenGL ES 3.0 secara langsung di hardware grafis ponsel.
   * Menangani *event loop* multi-touch Android secara *real-time* tanpa overhead Java/JNI.

2. **`touch_controls.cpp` (Virtual Gamepad On-Screen)**:
   * Virtual Analog Stick dengan deadzone dan normalisasi koordinat PSP (0 - 255).
   * Tombol aksi (*Cross, Circle, Triangle, Square*).
   * D-Pad (*Up, Down, Left, Right*), Trigger (*L, R*), dan Menu (*Start, Select*).

3. **`psp_renderer_gl.cpp` (OpenGL ES 3.0)**:
   * Mempertahankan *aspect ratio* asli PSP (16:9 / 480x272) dengan kalkulasi *letterbox* adaptif di layar smartphone modern (19:9 / 20:9).
   * Rendering 2D shader untuk HUD kontrol sentuh transparan.

4. **`psp_hle.cpp` (Host OS Abstraction Layer)**:
   * **`sceIoOpen / sceIoRead / sceIoLseek`**: Membaca file arsip game (`legoindy2psp.fib`) baik dari direktori internal APK (`AAssetManager`) maupun penyimpanan eksternal Android (`/sdcard/Android/data/...`).
   * **`sceCtrlReadBufferPositive`**: Memetakan sentuhan jari ke struktur data controller PSP (`SceCtrlData`).
   * **`PspMemory`**: Mengalokasikan blok memori 56 MB untuk simulasi *User RAM* PSP.

---

## 🚀 Cara Menyiapkan dan Build

### 1. Salin Aset Game
Jalankan skrip PowerShell berikut untuk menyalin arsip utama game dari folder game Anda ke direktori aset Android:
```powershell
cd "D:\Game\LEGO Indiana Jones 2 (ANDROID)"
.\copy_assets.ps1
```

### 2. Buka di Android Studio
1. Buka **Android Studio**.
2. Pilih **Open** lalu arahkan ke folder: `D:\Game\LEGO Indiana Jones 2 (ANDROID)`.
3. Tunggu hingga Gradle Sync selesai.
4. Hubungkan smartphone Android Anda (aktifkan *USB Debugging*), lalu tekan **Run (Shift + F10)**.

### 3. Build via Command Line (Gradle)
Jika Anda memiliki Android SDK dan NDK terpasang di sistem:
```bash
./gradlew assembleDebug
```
File APK akan dibuat di: `app/build/outputs/apk/debug/app-debug.apk`.

---

## 🔗 Menghubungkan Output Static Recompilation (PSPRecomp)
Jika Anda menggunakan framework **PSPRecomp** atau **psprecomp** untuk me-*lift* biner `EBOOT.BIN` menjadi kode C++:
1. Tempatkan file fungsi C++ hasil recompilasi ke dalam folder `app/src/main/cpp/recompiled/`.
2. Daftarkan file tersebut ke dalam `app/src/main/cpp/CMakeLists.txt`.
3. Panggil fungsi entry point game di dalam `native_main.cpp` setelah `PspHLE::instance().init()` berhasil dijalankan.

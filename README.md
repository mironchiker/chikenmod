# ChickenGun Chams Mod

## Структура

```
chickenmod/
├── main.cpp          — JNI_OnLoad, eglSwapBuffers hook
├── chams.h           — GL хуки + все режимы чамсов
├── menu.h            — ImGui меню
├── CMakeLists.txt    — сборочный скрипт
├── build.gradle      — Gradle конфиг
├── imgui/            — КЛОНИРУЙ СЮДА (см. ниже)
└── jniLibs/
    └── arm64-v8a/
        └── libdobby.a  — СКАЧАЙ СЮДА (см. ниже)
```

---

## Шаг 1 — Зависимости

### ImGui (reveny форк)
```bash
git clone https://github.com/reveny/Android-ImGui-Mod-Menu imgui
```

### Dobby (prebuilt .a)
Качаешь с https://github.com/jmpews/Dobby/releases
Кладёшь `libdobby.a` в `jniLibs/arm64-v8a/`

---

## Шаг 2 — Сборка

### Через Android Studio
1. File → Open → выбираешь папку chickenmod
2. Build → Make Project
3. .so будет здесь:
   `build/intermediates/cmake/debug/obj/arm64-v8a/libchickenmod.so`

### Через терминал
```bash
# NDK должен быть в PATH или ANDROID_NDK_HOME
./gradlew assembleDebug

# Найти .so:
find . -name "libchickenmod.so"
```

---

## Шаг 3 — Патчим APK (без рута)

```bash
# 1. Декомпилируем
java -jar apktool.jar d ChickenGun.apk -o out

# 2. Кидаем .so
cp libchickenmod.so out/lib/arm64-v8a/

# 3. В smali находим loadLibrary и добавляем:
#    grep -r "loadLibrary" out/smali/
#
#    Добавляем строки:
#    const-string v0, "chickenmod"
#    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

# 4. Собираем
java -jar apktool.jar b out -o mod.apk

# 5. Подписываем
java -jar uber-apk-signer.jar --apks mod.apk

# 6. Ставим
adb uninstall com.y8.chickengun
adb install mod-aligned-debugSigned.apk
```

---

## Чамс режимы

| # | Название | Описание |
|---|---------|----------|
| 0 | OFF | выключено |
| 1 | Default | сквозь стены, без цвета |
| 2 | Shading | цветная заливка сквозь стены |
| 3 | Wireframe | сетка поверх модели |
| 4 | Outline | чёрная подложка + цветной контур |
| 5 | Visibility Check Outline | контур: зелёный видим / красный за стеной |
| 6 | Visibility Check Shading | заливка: зелёная видим / красная за стеной |
| 7 | Visibility Check Shading + Outline | комбо 6+4 |

---

## Настройка shader uniform

Сначала включи **Dump** в меню — играй несколько секунд.
Потом смотри `/sdcard/Download/Shaders.txt`.
Ищи uniform типа `u_MVPMatrix`, `_mvp`, `MVP` — пробуй каждый через **Apply**.
Когда чамсы заработали — это правильный uniform.

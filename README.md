# Radiance / MageVK — port experimental para Apple Silicon

Este repositorio reúne el port de Radiance 0.1.6-alpha para Minecraft Java 1.21.1 con Fabric, MCVR y MageVK. La rama de trabajo es **`V0.1`**. Se ha renderizado un mundo en un MacBook Air M4 mediante Vulkan → MageVK → Metal.

**Estado experimental:** persisten transparencia incorrecta, ruido con poca luz, rendimiento bajo y cierres pendientes de investigar. FSR 1 EASU + RCAS funciona; FSR 3 Frame Generation está desactivado. Los 22 FPS comunicados en una escena plana casi vacía y 8 FPS a 720p son observaciones del usuario, no un benchmark comparable a render nativo.

## Empezar

Necesitas macOS Apple Silicon, Xcode completo y Homebrew. El entorno probado usa macOS 26.6.2 y Xcode 26.3; la revisión de MageVK está elegida para ese entorno. No se necesita actualizar a la rama principal de MageVK.

```bash
git clone --branch V0.1 --recurse-submodules https://github.com/pumitahou/magevk_radiance_port.git
cd magevk_radiance_port
./setup.sh
./compile.sh --no-install
```

`setup.sh` inicializa las revisiones exactas de los submódulos, instala las herramientas Homebrew ausentes y reutiliza un Vulkan SDK indicado con `VULKAN_SDK`. Si no lo encuentra, descarga el SDK LunarG 1.4.363.0, comprueba su SHA-256 y ejecuta su instalador local con aceptación de sus licencias. Xcode y Homebrew se instalan por separado. `./setup.sh --check` solo comprueba el entorno.

La compilación produce:

```text
dist/Radiance-M4-experimental/
  bin/java-magevk
  mods/Radiance-0.1.6-alpha-fabric-1.21.1-macos-arm64-experimental.jar
  mods/fabric-api-0.102.0+1.21.1.jar
  runtime/libMoltenVK.1.dylib
  runtime/libvulkan.1.dylib
  runtime/magevk_icd.json
  licenses/
  revisiones.json
  SHA256SUMS.txt
dist/Radiance-M4-experimental.zip
```

El JAR contiene `libcore.dylib`, Vanilla PT y los shaders de FSR 1. El paquete se genera desde fuentes y plantillas, sin necesitar un `dist` anterior. Java 21 se utiliza desde el equipo. El ZIP no contiene Minecraft, sesiones, mundos ni registros personales.

## Usarlo con Minecraft

1. Instala Fabric para Minecraft **1.21.1** en el Launcher oficial.
2. En las opciones avanzadas del perfil, selecciona como ejecutable Java la ruta absoluta de `dist/Radiance-M4-experimental/bin/java-magevk`.
3. Con Minecraft cerrado, ejecuta `python3 scripts/instalar-experimento.py`.
4. Inicia ese perfil desde el Launcher.

El instalador localiza el perfil que usa ese wrapper, respalda los JAR anteriores, copia Radiance y Fabric API y comprueba sus SHA-256. Si detecta otro Radiance o varios destinos, se detiene para evitar una instalación ambigua. No inicia ni cierra el juego. Los perfiles y las opciones del juego se conservan.

**Después de configurar el perfil, `./compile.sh` compila, empaqueta e instala automáticamente.** `--no-install` permite compilar en otra máquina o sin un perfil configurado. Para una carpeta de Launcher diferente, define `MINECRAFT_HOME`; Prism y otros launchers pueden usar el paquete copiando sus JAR y configurando el wrapper manualmente.

Tras el primer arranque, los accesos `09-fsr1-rapido-4x.command`, `10-fsr1-calidad-2x.command` y `11-fsr1-nativo.command` seleccionan el escalado con el juego cerrado. 4× por eje implica aproximadamente 1/16 de los píxeles del trazado: es agresivo y puede perder mucho detalle. Consulta `Display` en F3 para distinguir resolución real y tamaño lógico Retina.

## Organización

```text
Radiance/                    fork Java, rama V0.1
MCVR/                        fork del motor nativo, rama V0.1
  extern/nrd/                fork NRD, rama V0.1
  extern/...                 dependencias originales de MCVR
magevk/                      fork de la revisión compatible, rama V0.1
magevk-spirv-cross/           traductor requerido, commit fijado
third_party/ShaderMake/      submódulo, commit fijado
third_party/MathLib/         submódulo, commit fijado
third_party/sse2neon/        submódulo, commit fijado
scripts/                    entorno, compilación, paquete e instalación
templates/                  archivos que se generan dentro de dist
probes/                     fuentes de pruebas externas de GPU
docs/                       avance, arquitectura y contribuciones
requirements.json           herramientas probadas y revisiones auxiliares
```

Los submódulos guardan **commits concretos**. La configuración `branch = V0.1` identifica la rama de desarrollo; `setup.sh` no ejecuta `update --remote` ni avanza las dependencias automáticamente. ShaderMake, MathLib y sse2neon se suministran a CMake desde los submódulos para evitar descargas variables desde `main` o `master`. CPM descarga las dependencias de MageVK fijadas en sus archivos `ExternalRevisions`.

`build/`, `.cache/`, `.tools/`, `.local/`, `dist/`, registros y experimentos locales están ignorados. Las cachés antiguas del experimento también están ignoradas; no son necesarias para clonar y compilar.

Variables opcionales: `DEVELOPER_DIR`, `JAVA_HOME` (JDK 21), `VULKAN_SDK` (carpeta que contiene `bin/dxc`), `RADIANCE_JOBS` (4 por defecto), `RADIANCE_BUILD_DIR` y `GRADLE_USER_HOME`. No hay rutas del autor requeridas en la compilación.

## Verificación y colaboración

```bash
./setup.sh --check
python3 scripts/probar-fsr1.py --metal-validation
python3 scripts/benchmark-rt.py --validation --width 127 --height 65 --repeats 3 --rounds 1
```

Las pruebas externas usan el runtime del paquete y no arrancan Minecraft. Sus resultados se guardan fuera de Git. Compilar y superar estas pruebas no certifica la imagen completa ni la estabilidad del juego.

Para contribuir, consulta [CONTRIBUTING.md](CONTRIBUTING.md), [el avance desde la pantalla roja](docs/avance-pantalla-roja.md) y [las licencias y procedencias](THIRD_PARTY.md). Conservamos los autores, historiales y licencias de los proyectos originales; los forks contienen nuestras modificaciones experimentales.

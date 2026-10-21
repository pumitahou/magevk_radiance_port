# Radiance / MageVK — port experimental para Apple Silicon

Este repositorio reúne el port de Radiance 0.1.6-alpha para Minecraft Java 1.21.1 con Fabric, MCVR y MageVK. La rama de trabajo es **`V0.1`**. Se ha renderizado un mundo en un MacBook Air M4 mediante Vulkan → MageVK → Metal.

**Estado experimental:** E16 corrige la composición de la refracción de vidrio y agua con NRD; el usuario confirmó la corrección en Minecraft. Persisten ruido con poca luz, rendimiento bajo y cierres pendientes de investigar. FSR 1 EASU + RCAS funciona; FSR 3 Frame Generation está desactivado. Los 22 FPS comunicados en una escena plana casi vacía y 8 FPS a 720p son observaciones del usuario, no un benchmark comparable a render nativo.

La compilación predeterminada prepara **E21-presets-nativos**: incluye los presets internos de 4, 3 y 2 rebotes y conserva los cambios anteriores. El runtime incorpora E20, una corrección experimental de la presentación en Metal; superó una prueba aislada de 60 segundos, pero sigue pendiente de confirmar en Minecraft. Cada experimento tiene su [carpeta, instrucciones y límites de verificación](experiments/README.md).

Los cambios posteriores incluyen [E17, retención de recursos Metal](docs/E17-vida-recursos-metal.md), y [E18, optimización de sombras y medición GPU](docs/E18-ray-tracing.md). **E18 sigue siendo inestable:** el usuario reportó otro cierre y el informe nativo muestra un fallo durante una notificación de Metal. SHARC también tiene un fallo pendiente. La mejora del 19% corresponde a una prueba aislada de sombras, no a los FPS de Minecraft. Para identificar explícitamente el paquete de esta prueba: `./compile.sh --no-install E18-ray-tracing`.

## Empezar

Necesitas macOS Apple Silicon, Xcode completo y Homebrew. El entorno probado usa macOS 26.6.2 y Xcode 26.3; la revisión de MageVK está elegida para ese entorno. No se necesita actualizar a la rama principal de MageVK.

```bash
git clone --branch V0.1 https://github.com/pumitahou/magevk_radiance_port.git
cd magevk_radiance_port
# Coloca el ZIP de Vulkan SDK para macOS en downloads/ antes del setup.
./setup.sh
./compile.sh --no-install
```

Descarga manualmente el **ZIP de Vulkan SDK para macOS** desde [LunarG](https://vulkan.lunarg.com/sdk/home/) y colócalo **sin descomprimir** en `downloads/`, conservando su nombre `vulkansdk-macos-*.zip`. Deja un solo ZIP en esa carpeta. El SDK utilizado durante el experimento fue 1.4.363.0; el instalador acepta el archivo que elijas, sin exigir esa versión ni comprobar un SHA.

`setup.sh` inicializa las revisiones exactas de los submódulos con historial superficial y dos descargas simultáneas, instala las herramientas Homebrew ausentes y reutiliza un Vulkan SDK indicado con `VULKAN_SDK`. Si no lo encuentra instalado, extrae el ZIP de `downloads/` y ejecuta su instalador local con aceptación de sus licencias. Si falta el ZIP, muestra dónde colocarlo y termina. Xcode y Homebrew se instalan por separado. `./setup.sh --check` solo comprueba el entorno. Si se interrumpe la descarga de un submódulo, vuelve a ejecutar `./setup.sh`: conserva los que ya se descargaron.

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

En la configuración de pipeline hay una fila superior de presets de rebotes:
**Default · 4**, **Equilibrado · 3** y **Low · 2**. Cambian solo el límite de
rebotes y conservan los demás ajustes. La selección se guarda por shader pack;
al salir de la pantalla se reconstruye el pipeline para aplicarla.

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
experiments/                una carpeta por experimento; fichas y fuentes públicas
docs/                       avance, arquitectura y contribuciones
requirements.json           herramientas probadas y revisiones auxiliares
```

Los submódulos guardan **commits concretos**. La configuración `branch = V0.1` identifica la rama de desarrollo; `setup.sh` no ejecuta `update --remote` ni avanza las dependencias automáticamente. ShaderMake, MathLib y sse2neon se suministran a CMake desde los submódulos para evitar descargas variables desde `main` o `master`. CPM descarga las dependencias de MageVK fijadas en sus archivos `ExternalRevisions`.

`build/`, `.cache/`, `.tools/`, `.local/`, `dist/`, registros y resultados locales de experimentos están ignorados. Las fichas y fuentes reproducibles de `experiments/` sí se publican mediante una lista explícita de archivos permitidos. Las cachés antiguas del experimento también están ignoradas; no son necesarias para clonar y compilar.

Variables opcionales: `DEVELOPER_DIR`, `JAVA_HOME` (JDK 21), `VULKAN_SDK` (carpeta que contiene `bin/dxc`), `RADIANCE_JOBS` (4 por defecto), `RADIANCE_GIT_JOBS` (2 por defecto), `RADIANCE_BUILD_DIR` y `GRADLE_USER_HOME`. No hay rutas del autor requeridas en la compilación.

## Verificación y colaboración

```bash
./setup.sh --check
python3 scripts/probar-fsr1.py --metal-validation
python3 scripts/benchmark-rt.py --validation --width 127 --height 65 --repeats 3 --rounds 1
python3 scripts/probar-transparencia-nrd.py --validation
python3 scripts/probar-opacidad-rt.py --validation
```

Las pruebas externas usan el runtime del paquete y no arrancan Minecraft. Sus resultados se guardan fuera de Git. Compilar y superar estas pruebas no certifica la imagen completa ni la estabilidad del juego. El historial superficial reduce la descarga; los SDK de terceros todavía pueden ocupar bastante espacio.

Para contribuir, consulta [CONTRIBUTING.md](CONTRIBUTING.md), [el avance desde la pantalla roja](docs/avance-pantalla-roja.md) y [las licencias y procedencias](THIRD_PARTY.md). Conservamos los autores, historiales y licencias de los proyectos originales; los forks contienen nuestras modificaciones experimentales.

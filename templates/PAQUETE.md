# Radiance + MageVK para Apple Silicon — V0.1 experimental

Fuentes y compilación: https://github.com/pumitahou/magevk_radiance_port/tree/V0.1

Requiere macOS Apple Silicon, Java 21 y Minecraft 1.21.1 con Fabric. Hay problemas conocidos de transparencia, ruido en oscuridad, rendimiento y estabilidad. FSR 1 está integrado; Frame Generation permanece desactivado.

1. Coloca los dos JAR de `mods` en la carpeta de mods de tu instancia, evitando otro Radiance duplicado.
2. Configura el ejecutable Java del perfil con la ruta absoluta de `bin/java-magevk` de este paquete. Conserva toda la carpeta del paquete en esa ubicación.
3. Inicia Minecraft desde tu Launcher. El wrapper utiliza el runtime Vulkan/MageVK local y guarda sus registros en `logs/runs`.

Si compilaste desde fuentes y configuraste el perfil del Launcher oficial, `./compile.sh` instala automáticamente los dos JAR con respaldo y verificación. El paquete no inicia el juego por su cuenta.

Con el juego cerrado, los accesos 09, 10 y 11 seleccionan FSR 1 a 4×, 2× o resolución nativa tras el primer arranque. Estos accesos de configuración requieren Python 3. Los ajustes se respaldan. 4× por eje reduce el trazado a aproximadamente 1/16 de los píxeles y puede perder detalle. Comprueba la resolución `Display` de F3 en pantallas Retina.

Los modos magenta y verde son diagnósticos de interfaz, no modos de juego. El acceso 01 restaura la interfaz normal. Para activar validación Vulkan hace falta un Vulkan SDK y definir `VULKAN_SDK` o conservar la estructura del repositorio con su SDK local.

`revisiones.json` identifica los commits utilizados y `SHA256SUMS.txt` los archivos generados. Las licencias originales están en `licenses`. El ZIP no contiene mundos, cuentas ni registros personales.

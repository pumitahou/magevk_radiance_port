# E21 — Presets nativos de rebotes

La configuración del pipeline muestra una fila superior con **Default (4)**,
**Equilibrado (3)** y **Low (2)** en ambos modos de edición. Cambian solo el límite
de rebotes, conservando las demás opciones y las conexiones del pipeline.

La marca activa refleja el valor real. Los valores manuales distintos no se
identifican como un preset y los límites incompatibles se desactivan. Se guarda
por shader pack y se aplica al salir de la pantalla, al reconstruir el pipeline.
Hay etiquetas en inglés, español de España/México y chino simplificado.

Implementación en Radiance: `RayBouncePreset`, `Pipeline` y
`RenderPipelineScreen`. Las comprobaciones están en
`src/pipelineTest/java/com/radiance/client/pipeline/RayBouncePresetCheck.java`.

Desde la raíz del port, con el entorno preparado:

```bash
./compile.sh --no-install
# Solo la comprobación Java, sin iniciar Minecraft ni el render nativo:
./Radiance/gradlew -p Radiance verifyRayBouncePresets -PradianceUiBackend=vulkan
```

La compilación completa y la prueba de selección, persistencia, conservación de
otras opciones, valores personalizados y límites incompatibles pasaron. Se
comprobó el JAR instalado y que los archivos de ajustes existentes permanecieran
intactos. Se conserva el runtime E20. Falta revisar la pantalla y el cambio de
preset dentro de Minecraft; no se midió una mejora de FPS.

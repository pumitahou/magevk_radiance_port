# E20 — Presentación de imágenes en Metal

Los cierres observados compartían `objc_msgSend`, `MTLDispatchListApply` y la
notificación de programación de un command buffer. El reporte de macOS y el
desensamblado de E18 situaron la llamada en `CAMetalDrawable addPresentedHandler:`.
Esto identifica el punto de fallo; no demuestra el origen de la corrupción.

El cambio experimental en MageVK usa `MTLCommandBuffer presentDrawable:` en vez
del bloque programado anterior, retiene el drawable bajo su mutex y evita
acceder a la imagen después de soltar su última referencia. No cambia calidad.
Puede alterar el ritmo o latencia de presentación.

## Reproducir la prueba aislada

Desde la raíz, con el paquete compilado y Xcode disponible, en una sesión Bash:

```bash
source scripts/entorno.sh
mkdir -p experiments/E20-presentacion-metal/resultados
clang++ -O2 -std=c++17 -Wno-nullability-completeness \
  experiments/E20-presentacion-metal/present_stress.mm \
  -I "$VULKAN_SDK/include" -L "$VULKAN_SDK/lib" \
  -Wl,-rpath,"$VULKAN_SDK/lib" -lvulkan.1 \
  -framework Cocoa -framework QuartzCore -framework Metal \
  -o experiments/E20-presentacion-metal/resultados/present_stress
VK_DRIVER_FILES="$PWD/dist/Radiance-M4-experimental/runtime/magevk_icd.json" \
DYLD_LIBRARY_PATH="$PWD/dist/Radiance-M4-experimental/runtime:$VULKAN_SDK/lib" \
VK_LAYER_PATH="$VULKAN_SDK/share/vulkan/explicit_layer.d" \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation MTL_DEBUG_LAYER=1 \
  experiments/E20-presentacion-metal/resultados/present_stress 60 \
  > experiments/E20-presentacion-metal/resultados/presentacion.log 2>&1
```

Abre una ventana pequeña durante un minuto; no ejecuta Minecraft. Presenta
imágenes de 320×180, con dos frames en vuelo, y recrea el swapchain cada dos
segundos. Una salida correcta termina con `PRESENT_RESULT`; revisar también
que no haya errores ni avisos de validación, no solo el código de salida.

La prueba previa presentó 3.602 imágenes y recreó 30 swapchains en 60 segundos,
sin cierres ni errores/avisos de validación Vulkan/Metal. No reproduce el render
del juego ni demuestra que E18 falle bajo esa carga. La estabilidad dentro de
Minecraft sigue pendiente de confirmación por el usuario. No implica más FPS.
Los resultados y registros completos permanecen ignorados.

# Avance esencial: de la pantalla roja al mundo visible

Fecha de referencia: 19 de octubre de 2026. Radiance 0.1.6-alpha, Minecraft 1.21.1/Fabric, Java 21 arm64 y MacBook Air M4. Este documento resume observaciones del usuario y los experimentos locales; los registros completos y perfiles personales quedan fuera de Git.

## Resultado

En E10 el usuario confirmó que Minecraft renderizaba un mundo por la ruta Java → JNI/MCVR → Vulkan → MageVK/SPIRV-Cross → Metal. Inicialmente comunicó 1 FPS a 480p y 4 FPS a 144p, con ruido. La compilación funcionaba, pero no ofrecía todavía una experiencia de juego satisfactoria.

El desbloqueo se obtuvo adaptando el uso de Vulkan a las capacidades reales de MageVK. Las correcciones se probaron juntas: no hay una comparación A/B que cuantifique la aportación individual de cada parche.

## Secuencia que permitió localizar el fallo

| Experimento | Cambio y resultado | Alcance |
|---|---|---|
| Anterior a E06 | Pantalla roja, botones audibles e invisibles; un clear magenta directo al swapchain sí se veía. | Había presentación básica, sin demostrar dibujo de interfaz. |
| E06 | Layouts y barreras de imágenes internas corregidos. Normal seguía rojo; rellenar el origen de UI de magenta se veía mediante el blit original. | Copia y presentación de un origen conocido comprobadas en esa base. |
| E07 | Fragmento verde opaco, manteniendo la geometría y los vertex shaders. Seguía rojo. | Simplificar el fragmento no bastaba. |
| E08 | Validación Vulkan realmente activada: ocho tipos de error. | Se identificaron capacidades y layouts de descriptores no soportados. |
| E09 | Estados de color estáticos y correcciones de descriptores. Se veían formas verdes y avanzaba la barra. | Los dibujos llegaban a la imagen presentada. Quedaba un tipo de error de descriptores. |
| E10 | Fragmentos originales restaurados y última tabla de postprocesado corregida. El usuario veía el mundo. | Primer resultado funcional; el registro de esa ejecución no mostró errores ni avisos de validación. |

## Cambios exactos

1. **Imágenes internas de UI:** en Apple se sustituyó `PRESENT_SRC_KHR` por `COLOR_ATTACHMENT_OPTIMAL` en los render passes y el seguimiento de imágenes internas. Se ampliaron barreras para las escrituras de adjuntos de color. El swapchain conserva su presentación y el blit final usa `VK_FILTER_LINEAR`. Archivos: [ui_module.cpp](../MCVR/src/core/render/modules/ui_module.cpp) y [render_framework.cpp](../MCVR/src/core/render/render_framework.cpp). Los flags de transferencia se hicieron explícitos, pero ya eran añadidos por `DeviceLocalImage`; no se demostró una falta efectiva de esos permisos.
2. **Estados de color:** MageVK no exponía `extendedDynamicState3ColorBlendEnable`, `ColorBlendEquation` y `ColorWriteMask`. Se dejaron de declarar y emitir esos estados dinámicos cuando no están soportados. La mezcla y máscara RGBA pasan a variantes estáticas de pipeline conservadas en caché. Archivos: [device.cpp](../MCVR/src/core/vulkan/device.cpp), [dynamic_pipeline.cpp](../MCVR/src/core/vulkan/dynamic_pipeline.cpp), UI y sus cabeceras.
3. **Capacidad de texturas:** los layouts originales pedían 4.096 texturas más otro binding, 4.097 descriptores en total. Se consultó el layout real con `vkGetDescriptorSetLayoutSupport`; en esa revisión el resultado fue 1.210 texturas más el binding adicional. UI, mundo, IDs y subidas comparten ese límite y rechazan IDs fuera de rango. E10 corrigió también la tabla restante de [post_render_module.cpp](../MCVR/src/core/render/modules/world/post_render/post_render_module.cpp). Véase además [textures.cpp](../MCVR/src/core/render/textures.cpp).
4. **Estructuras de aceleración:** cuando `descriptorBindingAccelerationStructureUpdateAfterBind` no está soportado, sus conjuntos de descriptores son estáticos. Archivo: [descriptor.cpp](../MCVR/src/core/vulkan/descriptor.cpp).
5. **Salida normal e instalación comprobada:** se cambió de `flat-ui` a `normal` para restaurar los fragmentos originales. La compilación comenzó a instalar los dos JAR, con respaldo y SHA-256, en el perfil del Launcher que utiliza `java-magevk`.

El JAR histórico de E10 tuvo SHA-256 `a54c6aba2227a90d83b49eee57eef7b05115eebb59778ec075b4c2a00af7a44f`. El paquete actual incluye cambios posteriores y tendrá otro hash. Cambiar una etiqueta de experimento no restaura fuentes antiguas: para reproducir una versión se utiliza su commit superior y los gitlinks guardados.

## Trabajo posterior y límites actuales

E11 añadió tiempos de GPU y observación del historial NRD. E12 pasó el despacho directo de trazado de grupos de 1 a 32 hilos, conservando el caso indirecto. E13 añadió retención de recursos Metal y conversión de excepciones nativas a Java. E14 transporta materiales en el payload de Vanilla PT y añade parámetros LabPBR de vidrio. Estos cambios no demuestran que se hayan corregido todos los artefactos o cierres.

E15 integra FSR 1 espacial EASU + RCAS y un modo experimental 4× por eje. El usuario comunicó unos 22 FPS en un mundo plano casi vacío y 8 FPS a 720p, con transparencia todavía incorrecta y mala imagen en oscuridad. No es una comparación controlada de aceleración a igual resolución interna. El último registro examinado confirmaba FSR y NRD activos; un historial válido no prueba que todos los datos del denoiser sean correctos. FSR 3 Frame Generation sigue desactivado.

La rama `V0.1` publica esta base experimental y su organización reproducible. Los autores y licencias originales se conservan; los forks contienen las modificaciones del port.

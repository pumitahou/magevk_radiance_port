# E18: eliminar trabajo de sombras y medir el ray tracing

**Estado: experimental, con cierre pendiente.** El usuario reportó otro cierre
después de instalar E18. Las pruebas aisladas superadas no certifican la
estabilidad del juego.

## Referencia observada

El registro del juego con E17 muestra aproximadamente 65–75 ms en el módulo
principal, frente a unos 3 ms de NRD, con trazado de 349×243 y salida de 698×486.
El módulo principal incluye preparación de la escena y estructuras de
aceleración, pases gráficos y trazado. Su tiempo no permite atribuir todo el
coste a los rayos. El error final de atomics de 64 bits corresponde a la
activación posterior de SHARC, que queda fuera de este experimento.

Los benchmarks de grupos GPU no mostraron una mejora consistente al pasar de
32 a 64 hilos. Se conserva el grupo de 32. La prueba ampliada acepta
`--layers 16` para medir varias capas de triángulos y rechazos any-hit; sigue
siendo una escena sintética y no un benchmark de Minecraft.

## Cambios

Los rayos de sombra solo necesitan saber si existe un bloqueo aceptado. Sus
shaders any-hit siguen ignorando huecos y acumulando transmisión en superficies
transparentes; su closest-hit está vacío. E18 solicita terminar al encontrar
un bloqueo aceptado y omitir ese closest-hit. Se conservan las intersecciones
rechazadas, los índices SBT y los shaders miss de iluminación.

En el any-hit principal se omite el cálculo y muestreo de alpha cuando el modo
del material ya lo fuerza a uno. Se mantienen UV, LOD y flags de agua. El ahorro
esperado de esta segunda optimización es pequeño: la geometría totalmente
opaca suele omitir any-hit de todos modos.

No se reducen resolución, rebotes ni calidad de materiales. Se conservan las
correcciones de E16 y E17. SHARC permanece fuera de la prueba.

## Verificación

Compilaron los 23 stages de Vanilla PT, el código nativo y el paquete E18.
El probe `scripts/probar-sombras-rt.py --validation` comprobó radiancia final
equivalente con bloqueo, huecos alpha, transmisión acumulada seguida de miss,
geometría opaca y ausencia de intersección. No se exige el mismo número de
visitas any-hit antes de un bloqueo: el orden de recorrido no está garantizado
y terminar antes es precisamente la optimización.

A 16.962×5 píxeles (84.810 píxeles), cuatro rayos por píxel y siete repeticiones,
la mediana GPU del probe pasó de 6,496 a 5,259 ms, aproximadamente un 19% menos.
No hubo errores ni avisos de validación Vulkan. Es una prueba aislada de sombras
con cuatro capas: no representa toda la escena de Minecraft ni establece un
19% más de FPS en el juego. En una prueba menor pasó de 0,646 a 0,569 ms.

Los JAR de E18 quedaron instalados con SHA-256 verificado y respaldo anterior.

## Medición dentro del juego

Con `MCVR_PERFORMANCE_CAPTURE=1`, E18 agrega intervalos GPU de `world_prepare`,
`frame_graph` y el primer despacho de rayos del cuadro. Los queries se reciclan
por slot después de su fence y se leen sin esperar activamente. Las ramas
asíncronas no escriben en el pool primario. El primer despacho no representa
todos los rayos si un shader pack ejecuta varios pases o ramas asíncronas.

El registro usa `[RADIANCE-E18-PERF]` e incluye tamaño interno y tamaño de
despacho. Son intervalos inclusivos: no se deben sumar etapas anidadas.
`scripts/analizar-perf-log.py` calcula medianas y percentiles agrupados por
tamaño y etapa, sin publicar registros personales.

Para comparar, utilizar el mismo mundo, posición, resolución, ajustes y clima
de E17; esperar a que termine la carga de terreno y mantener la cámara quieta
antes de anotar FPS. Después moverse, mirar vidrio/agua y comprobar sombras.
La mejora de FPS en Minecraft requiere esta prueba del usuario.

## Resultado posterior en Minecraft

La sesión del 20 de octubre de 2026 terminó con SIGSEGV después de unos
192 segundos. El stack registra `objc_msgSend` → `MTLDispatchListApply` →
`_MTLCommandBuffer didScheduleWithStartTime:endTime:error:` en el hilo de
notificaciones de Metal. Es distinto del stack `useResources` observado antes
de E17; esto no demuestra una causa nueva ni permite atribuirlo al cambio de
sombras. El cierre sigue pendiente de investigación.

Los nuevos intervalos sí separan el coste dominante del trazado de la
preparación de escena en esta sesión:

| Resolución interna | Muestras | Mediana primer despacho RT | Mediana world prepare |
|---|---:|---:|---:|
| 279×184 | 42 | 31,631 ms | 0,084 ms |
| 432×254 | 18 | 61,438 ms | 0,426 ms |
| 564×373 | 24 | 125,406 ms | 0,354 ms |

Las resoluciones y la escena variaron durante la sesión. No es una comparación
controlada E17/E18 ni una medición de mejora de FPS. Los registros completos y
los informes JVM/macOS no se publican porque pueden contener datos personales.

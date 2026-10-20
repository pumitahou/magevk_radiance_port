# E17: vida útil de recursos Metal durante el envío compute

El usuario confirmó mejor estabilidad al jugar con E17 sin SHARC. Informó un
nuevo cierre al activar SHARC; la investigación de ese modo queda pendiente
por petición del usuario y no se considera resuelta por este parche.

Una sesión posterior de E18, que conserva este parche, volvió a cerrarse en
una notificación de Metal. La retención supera el caso de estrés descrito
abajo, pero la estabilidad completa del juego continúa pendiente; véase
[el resultado de E18](E18-ray-tracing.md).

## Evidencia de los cierres

Los dos cierres de E16 del 20 de octubre de 2026 muestran el mismo camino
nativo: `objc_retain` → `AGX ... useResources:count:usage:` →
`MVKMetalComputeCommandEncoderState::prepareComputeDispatch` →
`MVKCmdDispatch::encode` → envío asíncrono de la cola de MageVK.
Ocurrieron a los 53,57 y 210,47 segundos desde el inicio del proceso.
El primer informe registra SIGBUS y el segundo SIGSEGV. El código de salida 6
corresponde al abort posterior del manejador fatal; no identifica la causa.

El stack apunta a una referencia de recurso inválida durante la preparación
de un despacho compute. No identifica un shader concreto ni demuestra que NRD
sea el origen. La retención de referencias del command buffer ya estaba activa.

## Carrera identificada en el código

`MVKDevice::encodeGPUAddressableBuffers` enumera buffers bajo `_rezLock`, pero
`MVKUseResourceHelper::add` guardaba referencias Metal sin adquirir ownership.
Las llamadas a Metal `useResources` se realizan más tarde, fuera de ese lock.
Si otro hilo libera un buffer entre ambas operaciones, la lista diferida puede
contener una referencia vencida. Retener recursos al crear el command buffer
no cubre ese intervalo anterior a su registro en Metal.

## Cambio experimental

El helper adquiere una referencia por cada recurso Metal único y la conserva
hasta el reset o destrucción del encoder. La deduplicación evita adquirir una
referencia por cada despacho. El reset también elimina las listas pendientes;
el helper no permite copias que duplicarían la liberación de referencias.
Las entradas nulas se ignoran antes de enviarlas a Metal.

La retención del command buffer se mantiene activa. E16 y sus correcciones de
transparencia se conservan; este experimento cambia MageVK, no los shaders de
vidrio y agua. El registro debe mostrar
`[RADIANCE-E17] use_resource_retention=enabled`.

El instalador admite identificar el experimento en el motor nativo del JAR o
en la biblioteca MageVK del paquete, ya que E17 modifica esta última. El perfil
existente utiliza el runtime del paquete mediante `bin/java-magevk`.

## Verificación y prueba en el juego

MageVK y el paquete E17 compilaron correctamente. El probe
`scripts/probar-vida-recursos.py` usa un contador persistente por dirección GPU
y crea/destruye buffers independientes en otro hilo. El shader nunca utiliza
esos buffers transitorios: su destrucción no invalida comandos de la aplicación,
pero sí ejercita el registro global de direcciones de MageVK.

Una ejecución de E17 de 15 segundos con validación Vulkan y Metal completó
387.264 despachos y 975.672 ciclos de creación/destrucción. El contador GPU
coincidió con el esperado y apareció el marcador E17. La prueba del compositor
de E16 también pasó sobre el runtime nuevo. Estos resultados no certifican la
estabilidad completa de Minecraft ni excluyen otros fallos.

La comparación posterior utilizó la misma fuente de probe con una barrera
explícita shader→host. El runtime E16 abortó antes de producir resultado:
Metal API Validation rechazó `useResources` porque el recurso del índice 1
era nulo. E17, con las mismas validaciones, completó diez segundos,
242.816 despachos y 649.180 ciclos de buffers con contador correcto y sin
errores de validación. Esto reproduce un defecto de la ruta de registro de
recursos BDA de E16 y verifica que E17 supera esa carga aislada.

El instalador copió los JAR con SHA-256 verificado y conservó un respaldo.
Para la prueba visual, iniciar el mismo perfil y mundo de E16, conservar los
ajustes y jugar al menos cinco minutos. Moverse para cargar terreno, observar
agua/vidrio y abrir/cerrar el menú de pausa. Si vuelve a fallar, comparar los
nuevos logs y el stack con los de E16 antes de atribuirlo al mismo problema.

Los informes originales y registros completos quedan fuera de Git porque
pueden contener datos personales y credenciales de sesión. Este documento
conserva únicamente los hechos técnicos necesarios para investigar.

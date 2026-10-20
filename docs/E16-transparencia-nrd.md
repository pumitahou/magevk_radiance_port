# E16: transparencia de vidrio y agua con NRD

Este experimento recupera la radiancia refractada que Vanilla PT ya calcula,
pero que el compositor NRD no recibía. Con el split de transparencia, NRD
recibía el reflejo y perdía la imagen del fondo. También se marca el material
transmisivo en la guía de NRD para distinguirlo de una superficie opaca.

La nueva entrada se conecta desde `first_hit_refraction` hasta el compositor.
Solo se suma cuando su alpha indica una muestra válida; los píxeles cuya
radiancia completa ya llega por la salida clear no la suman de nuevo.
Los perfiles guardados deben conservar sus conexiones al cargar el nuevo mod.

El 20 de octubre de 2026 el usuario confirmó que la corrección funcionó al
ejecutar Minecraft con el paquete compilado e instalado de E16. Es una
confirmación visual del problema reportado; no establece una mejora de FPS
ni certifica todas las escenas o materiales.

## Probar en Minecraft

1. Iniciar el perfil Fabric 1.21.1 habitual, después de instalar el paquete E16.
2. Usar Vanilla PT y el preset con NRD. Conservar la resolución, FSR y demás
   ajustes de la prueba anterior para facilitar la comparación.
3. Mirar bloques de colores o terreno a través de vidrio claro y vidrio
   tintado, tanto de frente como en ángulo. El fondo debe seguir siendo visible
   junto al reflejo; los bordes pintados del vidrio pueden ser opacos.
4. Mirar el fondo de agua poco profunda, de frente y en ángulo. Comprobar
   también desde debajo de la superficie y con la cámara en movimiento.
5. Comprobar que piedra y otros bloques opacos conservan su aspecto.

El experimento no cambia la resolución ni el número de rebotes. No se afirma
una mejora de FPS: la confirmación visual y de rendimiento corresponde a la
prueba del usuario en el juego.

El registro técnico debe incluir `[RADIANCE-E16]` al construir el compositor.
El instalador respalda los JAR anteriores y verifica las copias con SHA-256.
El paquete anterior también queda guardado en `.local/package-backups/`.

## Verificación automática

`python3 scripts/probar-transparencia-nrd.py --validation` ejecutó el shader
real del compositor en la GPU M4 con entradas sintéticas. La versión anterior
produjo `[0.1000, 0.2000, 0.3000]` (solo emisión); E16 produjo
`[0.3499, 0.6997, 1.2998]`, incorporando el fondo refractado esperado
`[0.25, 0.5, 1.0]`. También pasaron los casos alpha cero/negativo y la rama
clear sin duplicación. El compositor half-rate compiló; esta prueba no ejecuta
su resolución temporal completa ni el juego.

La prueba de opacidad confirmó las cuatro combinaciones de geometría opaca,
no opaca y overrides de instancia. La prueba de materiales confirmó el payload
con cero muestras incorrectas en grupos de 1 y 32 hilos. Los 23 shaders de
trazado de Vanilla PT compilaron.

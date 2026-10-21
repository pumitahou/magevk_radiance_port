# Experimentos

Cada experimento tiene una carpeta propia con instrucciones, fuentes auxiliares
cuando corresponde, resultados resumidos y límites de lo comprobado.

| Experimento | Objetivo | Estado observado |
|---|---|---|
| [E16 — transparencia](E16-transparencia-nrd/) | Componer refracción de vidrio y agua con NRD | Corrección visual confirmada por el usuario |
| [E17 — recursos Metal](E17-vida-recursos-metal/) | Retener recursos usados por comandos diferidos | Prueba aislada superada; persistieron otros cierres |
| [E18 — sombras y medición](E18-ray-tracing/) | Reducir trabajo de sombras y medir etapas GPU | Mejoró un probe; el juego siguió inestable |
| [E19 — muy low cost](E19-lowcost/) | Reducir calidad mediante parámetros existentes | Preset preparado; sin mejora de FPS demostrada |
| [E20 — presentación Metal](E20-presentacion-metal/) | Evitar el callback de presentación que figura en los cierres | Prueba aislada superada; pendiente de prueba del usuario |
| [E21 — presets nativos](E21-presets-nativos/) | Seleccionar 4, 3 o 2 rebotes desde el mod | Compilación y persistencia verificadas; revisión visual pendiente |

La compilación actual incorpora estos cambios; `compile.sh` prepara E21 por
defecto. Pasar otro identificador solo cambia la etiqueta del paquete: no
revierte los fuentes. Para comparar revisiones anteriores utiliza sus commits
y las revisiones de submódulos correspondientes.

## Archivos públicos y locales

`.gitignore` permite únicamente los README de estas carpetas y las fuentes o
definiciones indicadas expresamente. Los demás archivos permanecen ignorados,
incluidos resultados, registros, ejecutables, volcados y el directorio privado
`investigacion-rendimiento-2026-10-20/`. No hay que forzar su incorporación a Git.

Las implementaciones del mod y sus bibliotecas siguen en sus repositorios
Radiance, MCVR y MageVK. Las carpetas de experimentos reúnen las instrucciones
y pruebas sin duplicar esos fuentes. Los datos históricos locales conservan sus
rutas para no romper las herramientas ni los informes de investigación.

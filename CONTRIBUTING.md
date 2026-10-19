# Contribuir

La rama del port es `V0.1`. Los cambios de Java pertenecen a `Radiance`, los del motor y shaders a `MCVR`, los del traductor Vulkan → Metal a `magevk`, y los de la biblioteca NRD a `MCVR/extern/nrd`. Los scripts, empaquetado, pruebas y documentación pertenecen a este repositorio superior.

Después de clonar, Git deja normalmente los submódulos en un commit separado de una rama. Antes de editar un fork, cambia a su rama, por ejemplo `git -C MCVR switch V0.1`.

Publica de dentro hacia fuera:

1. Si cambias NRD, guarda y publica su commit en tu fork y actualiza el gitlink `extern/nrd` en MCVR.
2. Guarda y publica los cambios de MCVR, Radiance o MageVK en sus repositorios.
3. En el repositorio superior, añade los directorios de submódulos cuyos commits cambiaron, guarda el commit y publícalo.

No publiques un gitlink cuyo commit todavía no esté disponible en el remoto del submódulo. No uses `git submodule update --remote` para preparar una compilación reproducible. Para recuperar las revisiones guardadas en el proyecto, utiliza `git submodule update --init --recursive` con los cambios propios ya guardados.

Comprueba `./setup.sh --check`, compila con `./compile.sh --no-install` y elige las pruebas GPU relevantes. Para comunicar problemas del juego incluye el commit superior, resolución `Display` de F3, resolución interna del trazado, modo FSR, escena y comportamiento observado. Los registros pueden contener datos personales: adjunta únicamente el fragmento técnico necesario.

No añadas bibliotecas compiladas, SDK, caches, mundos, perfiles del Launcher ni registros completos al código fuente. Conserva las licencias y avisos originales. La licencia del repositorio superior se aplica a su código propio; las dependencias mantienen sus respectivas condiciones.

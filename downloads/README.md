# Vulkan SDK para macOS

Descarga manualmente el ZIP desde https://vulkan.lunarg.com/sdk/home/ y colócalo en esta carpeta **sin descomprimir**, con su nombre original `vulkansdk-macos-*.zip`.

Deja un solo instalador y ejecuta `./setup.sh` desde la raíz del proyecto. El script lo instalará en `.tools/vulkan-sdk`. Se probó el SDK 1.4.363.0; no se fija una versión ni se comprueba un SHA del archivo.

El ZIP queda ignorado por Git. Si ya tienes el SDK instalado, también puedes indicar su carpeta mediante `VULKAN_SDK`.

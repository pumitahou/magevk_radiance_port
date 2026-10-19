# Procedencia y licencias

Este proyecto integra modificaciones experimentales de proyectos existentes. Cambiar el remoto a un fork no cambia la autoría ni la licencia de sus fuentes. Se conservan sus historiales, copyright y avisos. El código propio de integración de este repositorio se distribuye bajo GPL-3.0; véase `LICENSE`.

| Componente | Fuente utilizada | Licencia / aviso conservado |
|---|---|---|
| Radiance | https://github.com/pumitahou/Radiance — original Minecraft-Radiance/Radiance | `Radiance/LICENCE`, GPL-3.0 |
| MCVR | https://github.com/pumitahou/MCVR — original Minecraft-Radiance/MCVR | `MCVR/LICENSE.md`, GPL-3.0 y excepciones Apache/MIT indicadas por archivo |
| MageVK | https://github.com/pumitahou/magevk — original dttdrv/magevk / MoltenVK | `magevk/LICENSE`, Apache-2.0 |
| SPIRV-Cross de MageVK | https://github.com/dttdrv/magevk-spirv-cross | `magevk-spirv-cross/LICENSE` y `LICENSES/` |
| NRD | https://github.com/pumitahou/NRD — base Ljiong201108/NRD | `MCVR/extern/nrd/LICENSE.txt`, NVIDIA RTX SDKs License; no se relicencia como GPL o MIT |
| FidelityFX / FSR 1 | https://github.com/Interstellarss/FidelityFX-SDK | `MCVR/extern/FidelityFX-SDK/LICENSE.txt` y `sdk/LICENSE.txt`, MIT |
| ShaderMake | https://github.com/NVIDIA-RTX/ShaderMake | `third_party/ShaderMake/LICENSE.txt` y `ThirdPartyLicenses.txt` |
| MathLib | https://github.com/NVIDIA-RTX/MathLib | `third_party/MathLib/LICENSE.txt` |
| sse2neon | https://github.com/DLTcollab/sse2neon | `third_party/sse2neon/LICENSE` |
| Vulkan Loader / SDK | https://github.com/KhronosGroup/Vulkan-Loader / https://vulkan.lunarg.com/ | Avisos suministrados por el SDK, incluidos en el paquete |
| Fabric API | https://github.com/FabricMC/fabric | Apache-2.0; el JAR oficial conserva sus avisos |

Las demás dependencias de MCVR conservan sus archivos de licencia en `MCVR/extern`: GLFW, Volk, VulkanMemoryAllocator, GLM, Vulkan-Headers, stb, DLSS, SHARC, minizip-ng, JSON, XeSS y tinyexpr. Que una dependencia esté presente como submódulo no significa que su runtime esté activado: esta compilación desactiva DLSS, XeSS, SHARC y el backend completo de FSR 3.

El empaquetador copia los avisos disponibles de los componentes integrados a `dist/Radiance-M4-experimental/licenses`. No se incluyen el juego Minecraft, Java, Xcode ni binarios propietarios de DLSS. `revisiones.json` permite identificar las fuentes exactas del paquete. Los submódulos y sus licencias son parte necesaria de la distribución de fuentes.

# E18 — sombras y medición GPU

El [informe técnico](../../docs/E18-ray-tracing.md) describe la implementación,
los resultados observados y sus límites. Los fuentes de las pruebas están en
`probes/` y su lanzador en `scripts/probar-sombras-rt.py`.

Con el paquete y el entorno preparados, desde la raíz:

```bash
python3 scripts/probar-sombras-rt.py --validation
```

Los registros y resultados generados se conservan en rutas locales ignoradas.
Consulta el [índice](../README.md) para distinguir la revisión del código de la
etiqueta del paquete; esta prueba aislada no certifica estabilidad en Minecraft.

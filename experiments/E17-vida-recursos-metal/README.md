# E17 — retención de recursos Metal

El [informe técnico](../../docs/E17-vida-recursos-metal.md) describe la implementación,
los resultados observados y sus límites. Los fuentes de las pruebas están en
`probes/` y su lanzador en `scripts/probar-vida-recursos.py`.

Con el paquete y el entorno preparados, desde la raíz:

```bash
python3 scripts/probar-vida-recursos.py --seconds 10 --validation --metal-validation
```

Los registros y resultados generados se conservan en rutas locales ignoradas.
Consulta el [índice](../README.md) para distinguir la revisión del código de la
etiqueta del paquete; esta prueba aislada no certifica estabilidad en Minecraft.

# E19 — Muy low cost

Experimento de reducción de calidad usando parámetros existentes. No cambia
algoritmos de render. Su definición pública está en [preset.json](preset.json);
el empaquetador la instala como `bin/muy-lowcost.json`.

Reduce a 2 rebotes, apaga parallax/nubes/motion blur, usa agua y luz volumétrica
vanilla y transparencia estocástica. Conserva NRD y FSR 1 a 4× por eje, con
menos enfoque; SHARC y Frame Generation quedan apagados. También reduce
terreno, simulación, entidades y partículas. Puede perder detalle y luz.

Con Minecraft cerrado y tras configurar el perfil del paquete:

```bash
python3 scripts/configurar-lowcost.py apply --check
python3 scripts/configurar-lowcost.py apply
# Tras la prueba, si se desea recuperar el estado anterior:
python3 scripts/configurar-lowcost.py restore
```

Los accesos 12 y 13 del paquete hacen lo mismo. La restauración devuelve cuatro
archivos completos a su estado previo, incluidos cambios manuales posteriores.
Se verificó una aplicación/restauración byte a byte y se volvió a activar.
No hay una comparación controlada de FPS ni se corrigen los cierres con E19.

El preset nativo **Low de E21** cambia solo los rebotes; no aplica todas las
reducciones de este experimento. Los respaldos y resultados permanecen locales.

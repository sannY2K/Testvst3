# Granello 3

Secuenciador granular VST3 (instrumento) para Windows 64 bits, pensado para Ableton Live.
Corta un audio en 16 chops, cada uno es un paso del secuenciador y suena sincronizado con el tempo del proyecto.

## Cómo se usa
1. Arrastra un audio sobre la pantalla (o `library` → *Cargar audio...*). Se divide en 16 cortes.
2. Pon **play** en Ableton: el modo `step` sigue el transporte.
3. Elige un paso (botones 1–16 o clic en la tira de onda) y mueve las siete perillas: space, chaos, length, grains, position, tune, output.
   El icono de cadena junto a cada perilla decide si mueve todos los pasos a la vez (activado) o solo el seleccionado.
4. Modos: `hold` repite el paso seleccionado, `step` recorre los pasos, `keys` toca el chop con las notas MIDI C3–D#4 (60–75) sin necesitar transporte.
5. `sync / free` cambia *grains* entre divisiones de tempo (1/8T…) y granos por segundo.
6. `env` (ADSR, arrastra los puntos) y `filter` (arrastra los triángulos: paso alto/bajo, vertical = resonancia).
7. Clic derecho en un paso: copiar, pegar, desactivar, copiar a todos. `reset`, deshacer/rehacer y 8 presets en `library`.

El audio cargado, los pasos y los ajustes se guardan con el set de Ableton.

## Compilar en GitHub
1. Sube el proyecto a un repositorio (abajo).
2. Pestaña **Actions** → *Build Granello 3*; se lanza con cada push.
3. Descarga el artefacto `Granello3-VST3-Windows` (zip con `Granello 3.vst3`).
4. Release: `git tag v1.0.0 && git push origin v1.0.0`.

## Instalar en Ableton
1. Copia `Granello 3.vst3` a `C:\Program Files\Common Files\VST3`.
2. Ableton → Preferencias → Plug-ins → *Use VST3 Plug-in System Folders* → *Rescan*.
3. Instruments → Plug-ins → Granello 3.

## Subir a GitHub
```
git init
git add .
git commit -m "Granello 3"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/Granello3.git
git push -u origin main
```
(o `gh repo create Granello3 --private --source=. --push`)

## Estética
Los colores están en `namespace Theme` de `Source/PluginEditor.h`.

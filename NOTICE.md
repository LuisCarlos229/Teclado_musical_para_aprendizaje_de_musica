# Avisos y atribuciones de terceros

Este repositorio incluye o deriva de materiales de terceros. Esos materiales
NO están cubiertos por las licencias MIT, CERN-OHL-P-2.0 ni CC BY 4.0 de este
proyecto: se rigen por sus propias licencias o condiciones, indicadas abajo.

## Muestras de audio

### Salamander Grand Piano V3
- Autor: Alexander Holm
- Fuente: https://sfzinstruments.github.io/pianos/salamander/
  (consultada el 13/11/2026)
- Licencia: Creative Commons Attribution 3.0 Unported (CC BY 3.0)
  https://creativecommons.org/licenses/by/3.0/
- Modificaciones: los wavetables y envolventes de este repositorio son una
  adaptación de las muestras originales. De cada nota se extrajeron un ciclo
  de la forma de onda y su envolvente de amplitud para síntesis DDS en el
  ESP32. Aproximan el tono y el timbre de cada nota del instrumento, pero no
  son las grabaciones originales.

### Philharmonia Orchestra — Sound Samples
- Fuente: https://philharmonia.co.uk/resources/sound-samples/
  (consultada el 13/11/2026)
- Condiciones: uso libre, incluso en obras comerciales, con la restricción de
  que las muestras no pueden venderse ni ponerse a disposición "tal cual"
  (como muestras o como instrumento sampler).
- Las muestras originales no se redistribuyen en este repositorio. Para
  regenerar las wavetables, descargarlas desde la fuente indicada.
- Modificaciones: a partir de las muestras originales se extrajeron
  wavetables de un ciclo y envolventes de amplitud. Aproximan el tono y el
  timbre de cada nota del instrumento, pero no son las muestras originales ni
  se ofrecen como muestras o como instrumento sampler: cada archivo contiene
  solo un ciclo de forma de onda o una envolvente de amplitud.

## Software de terceros

### ESP-IDF
- Autor: Espressif Systems
- Licencia: Apache License 2.0
  https://www.apache.org/licenses/LICENSE-2.0
- El firmware se compila con ESP-IDF; el binario resultante incluye
  componentes de este framework.

### u8g2
- Autor: Oliver Kraus — https://github.com/olikraus/u8g2
- Componente ESP-IDF utilizado: Nixy4/u8g2 v0.1.4 (https://github.com/Nixy4/u8g2,
  commit ce5c862), adaptación de u8g2 para ESP-IDF.
- Ubicación en este repositorio: `firmware/components/u8g2/`
- Licencia del código: BSD de 2 cláusulas ("Simplified BSD"). El texto completo
  está en `firmware/components/u8g2/LICENSE`.
- Modificaciones: en `firmware/components/u8g2/CMakeLists.txt` se agregó
  `esp_driver_gpio` a la lista `REQUIRES` para compilar con ESP-IDF 6. El resto
  del componente se incluye sin cambios.
- Quien distribuya el firmware compilado debe acompañarlo con este aviso y con
  la licencia de u8g2.

#### Fuentes de u8g2 usadas por el firmware
Las fuentes de u8g2 tienen licencias propias, distintas de la del código
(detalle en https://github.com/olikraus/u8g2/wiki/fntgrp).

- `u8g2_font_6x10_tr`: fuente Misc-Fixed de X11. Dominio público.
- `u8g2_font_ncenB08_tr`: fuente New Century Schoolbook Bold de X11.
  Copyright (c) 1984, 1987 Adobe Systems Incorporated.
  Copyright (c) 1988, 1991 Digital Equipment Corporation.
  Licencia permisiva de X11: se permite usarla, copiarla y distribuirla sin
  cargo, siempre que se conserven estos avisos de copyright y no se usen los
  nombres de Adobe ni de Digital en publicidad sin su permiso.

### led_strip
- Autor: Espressif Systems
- Componente: espressif/led_strip v3.0.3 (ESP Component Registry,
  https://components.espressif.com/components/espressif/led_strip).
  El gestor de componentes de ESP-IDF lo descarga al compilar; no se incluye
  en este repositorio.
- Licencia: Apache License 2.0 — https://www.apache.org/licenses/LICENSE-2.0

title: Hoja de ruta
nav: Hoja de ruta
blurb: Los hitos que hay detrás del motor, lo que falta para la versión 1.0 y lo que viene después.
lead: Kingbble se construyó hito a hito, cada uno cerrado con una medición. Esto es lo que añadió cada uno, lo que sigue abierto y el orden previsto para lo que queda.
---
## Dónde está

**Candidata a versión 1.0.** El código del motor está completo. La versión 1.0 se etiqueta cuando terminen las ejecuciones finales, las cuatro ablaciones. Después viene la publicación de la app de escritorio, con una lista de palabras libre y sus propios leaves.

## Los hitos

| | Hito | Qué añadió | Cómo se cerró |
|---|---|---|---|
| M0–M1 | **Un motor propio** | el diccionario como autómata, el generador de jugadas, la puntuación | 0 diferencias con Macondo |
| M2 | **Búsqueda** | equity estática con leaves, simulación Monte Carlo, final exacto, pre-final exacto y aproximado | 0 diferencias en 34 finales; +4,8 puntos por partida |
| M3 | **Datos** | parser, descargador, un almacén de turnos, jugadores seudonimizados, un reparto por jugador | 8.738 partidas humanas, más de 230.000 turnos con atril |
| M4 | **Inferencia del atril** | el modelo humano de jugadas, el filtro de atriles, el baseline RangeFinder | log-loss de 5,74 frente a 12,50; 58,5 % de las partidas |
| M5 | **Riesgo y tiempo** | RiskBot, simulación por tiempo con parada por confianza, el banco para los matches | riesgo: no se detecta ganancia |
| M6 | **Entrenamiento** | niveles de datos, el gate, MLflow, el bucle Auto, leaves propios | el nivel 0 pasa su gate |
| M7 | **Torneos reproducibles** | bots con inferencia en self-play, manifiestos, semillas por partida, ablaciones corregidas por Holm | código completo; ejecuciones pendientes |
| M8 | **Interfaz y publicación** | la interfaz, la app de escritorio, esta web y la demo web | en marcha |

Cada uno enlaza con su capítulo: [generador](movegen), [simulación](simulation), [final](endgame), [datos](data), [inferencia](inference), [riesgo](risk), [entrenamiento](training), [método](method), [la interfaz](interface).

## Antes de la versión 1.0

| Tarea | Tamaño | Estado |
|---|---|---|
| Las cuatro ablaciones, 400 partidas emparejadas cada una | unas 22 horas de máquina | pendiente |
| El match contra el ajuste más fuerte de Quackle, después de verificar su nivel | de 25 a 30 horas | parado en 73 partidas; no se da por bueno |
| La tabla de resultados de cada comando de `docs/experiments.md` | horas | pendiente de las ejecuciones |
| Etiquetar `v1.0` | minutos | lo último |

## Problemas abiertos en el motor

**El pre-final con dos fichas en la bolsa es lento.** De 24 posiciones de prueba, 11 no terminan en 600 segundos. Cada jugada candidata obliga a resolver un final por cada orden en que se pueden robar las fichas, y un pase multiplica el árbol. El plan: perfilarlo, reutilizar los finales que se repiten entre ordenaciones, mejorar el orden de jugadas y la poda, y acotar antes el pase.

**Una diferencia sin explicar.** En una posición con dos fichas en la bolsa, una jugada saca el mismo número de victorias que en Macondo y 18 puntos menos de spread total. Es pequeña, es un solo caso y no se entiende, así que sigue en la lista.

**Las partidas ajustadas contra Macondo.** A igual tiempo el match es un empate en partidas y en puntos, con más partidas ajustadas perdidas que ganadas (17 frente a 27). El trabajo en los últimos turnos, donde eso se decide, continúa: mira [referencias](references).

**La profundidad de la simulación y el rival de los playouts.** Más iteraciones no ayudaron y tres plies empató con dos. Lo que no se ha probado a escala es un rival simulado que juegue mejor que una jugada estática.

## La publicación

Los resultados se midieron con una lista de torneo con licencia. Una app pública no puede incluirla, así que la publicación se monta sobre lo que sí se puede regalar.

| Paso | Estado |
|---|---|
| Lista de palabras libre (ENABLE) | hecho |
| Leaves propios para ENABLE, entrenados desde cero | hecho; más generaciones en marcha |
| Elegir el nivel Extremo midiendo en ENABLE, unas 400 partidas emparejadas por pareja de candidatos | pendiente |
| Empaquetar la app sin exigir Python en la máquina del usuario | pendiente |
| Pantalla «Acerca de» con la licencia, el código y los créditos | pendiente |
| Limpieza de marca y aviso de no afiliación | hecho en esta web; pendiente en el repositorio |
| Revisión final: ningún fichero con licencia en el paquete, sumas de la lista de palabras, avisos incluidos | pendiente |
| Versiones de Windows y Linux en la [página de descarga](download) | pendiente |
| Esta web en GitHub Pages | compilada; aún sin publicar |

Hay tres preguntas abiertas con terceros, y hasta que se respondan se toma la opción prudente: bajo qué licencia están las tablas de leaves publicadas; si se puede redistribuir un modelo ajustado con partidas de una plataforma; y si se puede incluir una segunda lista de palabras libre.

## Después de la 1.0

- **Más listas de palabras**, cada una con leaves entrenados desde cero con el botón de la app.
- **Español.** Faltan dos cosas: el juego de fichas español, con su Ñ y sus fichas de dos letras, y una lista de palabras que se pueda distribuir.
- **Un rival simulado más fuerte**, usando el modelo bot aprendido del self-play.
- **Riesgo, otra vez**, con una varianza medida respecto a la mejor candidata y un parámetro derivado de la teoría, ahora que hay un banco capaz de detectar efectos pequeños.
- **Análisis de una partida terminada**: marcar las jugadas en las que la simulación discrepa de lo jugado, y por cuánto.
- **Hilos en el navegador**, cuando el alojamiento permita las cabeceras que piden los hilos de WebAssembly.

## Lo que no está previsto

- **Juego en línea entre personas.** Kingbble es un motor y su interfaz. No hay cuentas ni servidores.
- **Una app para móvil.** La demo web funciona en el móvil.
- **Incluir listas de palabras con licencia.** No sin permiso de sus titulares.

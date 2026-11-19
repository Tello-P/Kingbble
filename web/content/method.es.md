title: Cómo se mide
nav: Medir
blurb: Partidas emparejadas, intervalos de confianza, semillas independientes y un test nulo. Por qué dos «hallazgos» del proyecto eran ruido.
lead: En un juego con tanta suerte, medir es la parte difícil. Partidas emparejadas, intervalos de confianza, semillas independientes, torneos reproducibles y los tests automáticos que mantienen honesto al propio banco de pruebas.
---
## El problema de la suerte

Dos bots juegan 200 partidas y uno gana 104. ¿Es mejor?

No hay forma de saberlo. Con 200 partidas independientes, el intervalo del 95 % del porcentaje de victorias es de unos ±7 puntos: un 52 % es compatible con cualquier cosa entre 45 y 59. Y las diferencias que merece la pena encontrar en este proyecto son pequeñas. Un final exacto vale alrededor de 1,5 puntos de porcentaje de victorias. Una buena idea vale 2 o 3.

Para ver 2 puntos con partidas independientes harían falta unas 5.000 por comparación, y una partida con simulación tarda unos 30 segundos en el hardware del proyecto: un ordenador personal con dos o tres núcleos, a baja prioridad. Así que el método tenía que ser más fino, no solo más grande.

## Partidas emparejadas

La mayor fuente de suerte es el robo: a quién le tocan los comodines, las `S`, la `Q`. Las partidas emparejadas la eliminan.

**Cada reparto se juega dos veces con los asientos cambiados.** El mismo orden de bolsa, los mismos atriles iniciales. En la primera partida, el bot A se sienta en el asiento 1 y recibe esas fichas; en la segunda, las recibe el bot B. Si A gana las dos, o gana por más de lo que pierde, la diferencia viene de las decisiones, porque las fichas fueron las mismas.

Las estadísticas se calculan entonces **sobre parejas**, no sobre partidas. El intervalo sale de cuánto varían los resultados de las parejas, que es mucho menos de lo que varían las partidas sueltas.

| Experimento | Sin emparejar, 200 partidas | Emparejado |
|---|---|---|
| Final exacto contra equity | «47 %», ±7 puntos: parece un bug | 51,4 % (50,4–52,5) y +4,8 de spread (+4,0 a +5,6) en 600 partidas |
| Pre-final aproximado | 44 % o 60 % según la semilla | 51,5 % (49,3–53,7) en 400 partidas |

La primera fila es la historia que cuenta el [capítulo de finales](endgame): una sospecha de bug que era ruido. La segunda es la misma lección una semana después, pillada a tiempo.

## Siempre un intervalo

Toda cifra de este manual que sale de partidas lleva al lado su intervalo de confianza del 95 %. Un resultado se llama **significativo** cuando el intervalo excluye «no hay diferencia»: el 50 % en porcentaje de victorias, cero en spread. En las gráficas, esas filas van en dorado.

Un resultado cuyo intervalo incluye «no hay diferencia» se cuenta como *neutro*, no como una pequeña victoria. Varias piezas del motor son honestamente neutras: el pre-final aproximado, los playouts a tres plies, todos los ajustes de riesgo.

## Semillas independientes

Cada tanda de partidas arranca de una semilla. En la primera versión, la partida `g` usaba `seed + g`, de modo que las tandas con semilla 1 y semilla 2 compartían 199 de sus 200 partidas. Dos ejecuciones «independientes» coincidían sospechosamente bien, 109 a 91 las dos veces.

Ahora la semilla de cada partida se mezcla con SplitMix64. Y con `--per-game-seeds` una partida depende solo de la semilla de la ejecución, del reparto y del asiento, nunca de las partidas que se jugaron antes. Una ejecución que se para y se reanuda vuelve a jugar exactamente las mismas partidas.

## El test nulo

El test más útil del proyecto compara un bot **consigo mismo**.

Dos bots idénticos que juegan una pareja de partidas, el mismo reparto con los asientos cambiados, tienen que repartírsela, una victoria cada uno, siempre. Si no, el banco está añadiendo un ruido que el emparejamiento debía cancelar.

Este test no existía cuando un banco de posiciones dijo que los playouts a tres plies ganaban el 56,5 % a los de dos. Los números aleatorios del banco iban ligados *al bot* (A o B) y no *al asiento*. Con bots idénticos, una pareja no salía siempre uno a uno, y el ruido sobrante se repetía en todas las pruebas hechas con ese banco. Con los flujos aleatorios ligados al asiento, la misma comparación da un 51,0 % (47,5–54,5). La mejora era un artefacto.

Ahora el test nulo se ejecuta de forma automática.

## Un banco de posiciones

Las partidas enteras pasan casi todo el tiempo en posiciones corrientes en las que cualquier bot sensato juega lo mismo. Para estudiar una fase, el proyecto usa un **banco de posiciones**: posiciones sacadas de partidas en un momento elegido (por ejemplo, de 10 a 25 fichas en la bolsa y el marcador a menos de 80 puntos), cada una jugada hasta el final dos veces, una desde cada lado. Concentra las partidas donde pueden mostrar una diferencia.

Para posiciones cercanas al final hay además un **juez exacto**: con la bolsa pequeña, dos jugadas candidatas se pueden comparar resolviendo la posición de forma exacta en lugar de jugándola.

## Torneos reproducibles

En la evaluación final, cada experimento es un fichero de configuración, y cada fichero de resultados lleva al lado un **manifiesto**.

| En el manifiesto | Para qué |
|---|---|
| Commit del código | qué motor jugó |
| Hash de la configuración | qué experimento era |
| Hashes del léxico, los leaves y los modelos | con qué datos jugó |
| Semillas, hilos, opciones | todo lo demás que puede cambiar una partida |

Reanudar una ejecución con otra configuración se rechaza. La simulación usa un número fijo de iteraciones y el final un límite de profundidad en vez de un límite de tiempo, así que la misma configuración repite las mismas partidas en cualquier máquina. Un test de determinismo comprueba justo eso.

## Muchas comparaciones a la vez

Si haces cuatro experimentos y das cada uno por significativo al 5 %, la probabilidad de que al menos uno sea una falsa alarma se acerca al 19 %. El script de análisis aplica la **corrección de Holm** entre los experimentos de un torneo: los p-valores se ajustan para que el 5 % valga para toda la familia.

`python -m ml.eval.tournament RESULTADOS...` convierte cualquier fichero de resultados emparejados en porcentaje de victorias y spread con intervalos emparejados, p-valores corregidos por Holm, tablas en Markdown y CSV, y una gráfica.

## Log-loss, acierto y calibración

Las partidas no son la única medida. Los modelos también se puntúan directamente:

- **Log-loss** (log-verosimilitud negativa): menos el logaritmo de la probabilidad que el modelo dio a lo que pasó de verdad, promediado. Premia acertar y castiga equivocarse con seguridad. Menos es mejor.
- **Acierto top-1**: cuántas veces la opción más probable era la verdadera.
- **Calibración (ECE)**: cuando el modelo dice 30 %, ¿pasa el 30 % de las veces? El error de calibración esperado es la distancia media.

## Tests e integración continua

Cada pull request ejecuta, en GitHub Actions:

| Comprobación | Qué hace |
|---|---|
| `g++` y `clang++` | compila el motor con los avisos como errores y pasa los tests de C++ (más de 140) |
| `clang-format` | el formato del C++ |
| `ruff` | el lint y el formato del Python |
| `pytest` | los tests de Python |

La rama principal está protegida por estas comprobaciones. Un workflow aparte, que se lanza a mano, ejecuta los experimentos largos en fragmentos paralelos, cada uno con su semilla, y junta los resultados en un solo artefacto.

La norma de trabajo es issue, rama, pull request: cada experimento tiene una issue con su resultado escrito. Las cifras de esta web son esas issues.

## La lección

> Dar siempre los resultados con partidas emparejadas y un intervalo de confianza. Dos de las «señales» del proyecto, un bug en el final y una mejora por playouts más profundos, eran ruido estadístico o un fallo de la medición.

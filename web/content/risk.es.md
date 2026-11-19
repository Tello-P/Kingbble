title: Riesgo y tiempo
nav: Riesgo y tiempo
blurb: ¿Debe arriesgar un bot que va perdiendo? RiskBot, y por qué no ganó más.
lead: Quien va por detrás debe arriesgar y quien va por delante no. RiskBot intenta convertir esa intuición en una regla. No ganó más partidas, y los motivos justifican el capítulo.
---
## Maximizar la media no es lo mismo que ganar

La simulación da a cada candidata un resultado medio. Elegir la mejor media es lo correcto si quieres el máximo de puntos a lo largo de muchas partidas. Pero una partida se gana acabando por delante, por un punto o por cien.

Imagina que vas 60 puntos por detrás y quedan pocas fichas. Una jugada segura que promedia +5 y casi nunca se dispara pierde la partida con certeza. Una jugada que promedia −2 pero una de cada cinco veces acaba en bingo es la única que puede ganar. Con 60 puntos de ventaja, la lógica se invierte: la varianza es lo único que te puede hacer daño.

Esa es la intuición detrás de la tercera hipótesis del proyecto.

## Dos formas de elegir

RiskBot va encima de la simulación. Toma las mismas candidatas y los mismos playouts, y sustituye el último paso, «quedarse con la mejor media», por una de dos reglas.

**Media-varianza (`risk`).** Cada candidata se puntúa con su media menos λ veces su varianza. Un λ positivo evita el riesgo; uno negativo lo busca. λ cambia con el estado de la partida: depende del marcador y crece a medida que se vacía la bolsa, así que al principio el bot juega casi igual que la simulación normal.

**Probabilidad de victoria (`riskwin`).** Cada candidata se puntúa directamente con una estimación de la probabilidad de ganar la partida: los resultados simulados se suman al marcador actual, y la incertidumbre del resto de la partida se modela como una dispersión que se reduce a medida que se acaban las fichas.

## Qué pasó

RiskBot contra el bot de simulación normal, partidas emparejadas en CSW24, 8 candidatas y 50 iteraciones, para varios valores de λ.

:::bars min=30 max=70 ref=50 unit=% caption=Porcentaje de victorias de RiskBot contra el bot de simulación normal para varios ajustes de riesgo. Partidas emparejadas, intervalos del 95 % sobre las parejas. Ninguno se distingue del 50 %.
λ = 0,002 | 50.5 | 42.3 | 58.7 | 100 partidas
λ = 0,005 | 51.5 | 43.1 | 59.9 | 100 partidas
λ = 0,01 | 50.0 | 37.7 | 62.3 | 40 partidas
λ = 0,02 | 48.0 | 38.7 | 57.3 | 100 partidas
riskwin | 47.5 | 34.2 | 60.8 | 40 partidas
:::

| λ | Partidas | % de victorias | Spread por partida |
|---|---|---|---|
| 0,002 | 100 | 50,5 (42,3–58,7) | −0,3 (−16,6 a +15,9) |
| 0,005 | 100 | 51,5 (43,1–59,9) | −7,2 (−23,3 a +8,8) |
| 0,01 | 40 | 50,0 (37,7–62,3) | −12,0 (−34,3 a +10,3) |
| 0,02 | 100 | 48,0 (38,7–57,3) | −9,2 (−24,9 a +6,4) |
| riskwin | 40 | 47,5 (34,2–60,8) | −14,1 (−38,7 a +10,6) |

Ningún ajuste mejora el porcentaje de victorias. Todos quedan entre el 48 y el 51,5 %, con intervalos de ±8 o 9 puntos. La pérdida de spread tampoco es significativa, aunque hay una tendencia débil: cuanto más se penaliza el riesgo, más spread se deja.

## Por qué no gana

El diagnóstico importa más que el resultado.

1. **La varianza que se penaliza no es el riesgo que importa.** Casi toda la varianza de un playout viene del atril del rival y del robo. Es casi la misma para todas las candidatas. Penalizarla apenas cambia el orden.
2. **La varianza está mal estimada.** Con 50 iteraciones lleva un error de alrededor del 20 %. Con un λ alto, ese ruido decide la jugada.
3. **El calendario apenas actúa.** El factor que escala λ con la bolsa lo deja cerca de cero durante la primera mitad de la partida.
4. **El modelo del «resto de la partida» no está calibrado.** La dispersión que se supone para los turnos que quedan se fijó a ojo, y con ella `riskwin` ordena las candidatas casi igual que la media.
5. **El efecto, si existe, es pequeño.** Una ganancia realista es de 1 a 3 puntos de porcentaje de victorias. Detectar +2 puntos exigiría unas 5.000 partidas por ajuste. La evaluación completa prevista solo habría detectado +5.

Por eso no se lanzó la ejecución completa de casi siete horas: lo esperable era «no se detecta ganancia», a un precio alto.

## Qué lo haría funcionar

Por orden de coste y beneficio probable:

- **Medir primero**: en qué porcentaje de turnos RiskBot elige una jugada distinta de la simulación normal, y cuánta media deja cuando lo hace.
- **Un λ teórico**, derivado de la ventaja y de la dispersión del resto de la partida, sin ningún parámetro puesto a ojo.
- **Calibrar la dispersión del resto de la partida** con los turnos humanos o con self-play.
- **Varianza relativa**: la varianza de *candidata menos mejor* sobre los mismos repartos, que elimina el ruido común a todas.
- **Una ventana**: aplicar el riesgo solo con la bolsa entre 7 y 25 fichas y la partida sin decidir.
- **Una política aprendida**: que el bot aprenda la corrección por refuerzo. Está implementada como política de riesgo aprendida en el código de entrenamiento y sigue aparcada hasta que haya algún indicio de que el riesgo aporta.

## El tiempo, la otra mitad del hito

El mismo hito convirtió la simulación de «un número fijo de iteraciones» en algo que gestiona el tiempo, como se cuenta en [simulación](simulation): un presupuesto en segundos por jugada, una parada en cuanto una candidata va claramente por delante y la garantía de que un límite de tiempo que no se alcanza no cambia nada. Con un número fijo de iteraciones el resultado es determinista, y los tests lo comprueban: un límite de 1.000 segundos que nunca se alcanza da exactamente las mismas medias, iteraciones y jugada que sin límite.

Cada capa tiene su propio presupuesto por jugada:

| Capa | Opción |
|---|---|
| Simulación | `--sim-time` |
| Pre-final exacto | `--peg-time` |
| Pre-final aproximado | `--apeg-time` |
| Final | `--endgame-time` |

Lo que **no** está implementado es una reserva para un reloj de partida completa, por ejemplo 25 minutos por jugador, en la que el tiempo ahorrado en las jugadas fáciles se gaste en las difíciles. Las apps del proyecto juegan siempre con tiempo por jugada. La interfaz sí tiene un reloj de partida para ti; el motor simplemente no planifica en torno a él.

title: El problema y la idea
nav: La idea
blurb: Por qué este juego es difícil para una máquina y la hipótesis que hay detrás de Kingbble.
lead: Por qué un juego de palabras cruzadas es difícil para una máquina, cómo lo resuelven los motores más fuertes y la hipótesis que Kingbble se construyó para comprobar.
---
## Por qué este juego es difícil para una máquina

A un buen motor de ajedrez ya no le gana nadie: ve todo el tablero y calcula más lejos que cualquiera. Aquí un jugador fuerte todavía gana una buena parte de las partidas a los mejores motores, porque se juntan tres obstáculos.

- **Información oculta.** No sabes qué siete fichas hay en el otro atril ni el orden de las que quedan en la bolsa. Dos posiciones iguales sobre el tablero pueden ser partidas completamente distintas.
- **Azar.** Lo que robas cambia mucho el resultado. Dos partidas con las mismas decisiones pueden acabar a 80 puntos de distancia. Eso también hace difícil *medir* si un cambio en el motor es una mejora: mira [cómo se mide](method).
- **Muchas jugadas.** Hay del orden de 500 jugadas legales por turno, y los puntos de una jugada son solo la mitad de su valor. La otra mitad son las fichas con las que te quedas, el *leave*: no es lo mismo jugar `QI` por 22 y quedarte `AEINRS` que hacer 30 y quedarte `UUVW`.

## Cómo juegan los motores fuertes

Quackle y Macondo, los dos motores de referencia, comparten una receta:

1. Un **generador de jugadas** muy rápido lista todas las jugadas legales.
2. Una **evaluación estática** las ordena: puntos más el valor del leave, leído de una tabla.
3. Una **simulación Monte Carlo** toma las mejores candidatas y, para cada una, imagina muchos futuros: un atril para el rival, su respuesta, tu siguiente jugada. Gana la candidata con mejor media.
4. Cerca del final, cuando la bolsa está vacía, la partida ya no tiene secretos y entra un **solver exacto**.

En el paso 3, el atril del rival tiene que salir de algún sitio. Por defecto se roba al azar de las fichas que no ves. Quackle tiene además RangeFinder, que descarta los atriles que no encajan con lo que el rival acaba de jugar, suponiendo que el rival juega como un motor.

## La hipótesis

> Las personas no juegan como un motor: cometen errores sistemáticos que dependen de su nivel y del marcador. Si aprendo cómo eligen jugada las personas de verdad, cada jugada del rival se convierte en una pista sobre las fichas que se ha quedado.

En símbolos, Kingbble aprende de partidas reales la probabilidad de una jugada dado el atril, el rating del jugador y el estado de la partida. Después, durante la partida, aplica la regla de Bayes: ganan peso los atriles con los que la jugada que se acaba de hacer habría sido probable, y lo pierden aquellos con los que habría sido rara. Simular contra esos atriles más realistas debería dar mejores decisiones.

Kingbble se planteó tres hipótesis medibles:

| Hipótesis | Cómo se mide | Estado |
|---|---|---|
| H1. Un modelo aprendido de personas predice mejor la jugada real que suponer un motor | Log-loss en jugadores que el modelo no vio | Confirmada |
| H2. Usar ese modelo en un filtro bayesiano de varios turnos mejora el juego | Calibración y porcentaje de victorias | Confirmada frente a RangeFinder; mejora modesta frente a no inferir |
| H3. Una política de riesgo aprendida por refuerzo mejora a maximizar la media | Cambio en el porcentaje de victorias | No demostrada: RiskBot no gana más; la parte de aprendizaje está aparcada |

El objetivo no es ganar a Macondo. Es **medir cuánto vale la información sobre el rival**, con un modelo calibrado en personas. Por eso una parte tan grande de este manual trata de medir.

## Lo que hubo que construir

No se tomó nada de otro motor. Cada pieza de abajo tiene su capítulo.

:::flow
Diccionario | GADDAG y KWG: todas las palabras válidas en un autómata
Generador | todas las jugadas legales y su puntuación
Equity | puntos más el valor del leave
Simulación | Monte Carlo sobre las mejores candidatas
Finales | solvers exactos con la bolsa vacía o casi vacía
:::

:::flow
Datos | 8.738 partidas humanas con el atril de quien mueve
Modelo humano | probabilidad de cada jugada, según el rating
Filtro de atriles | un posterior del atril rival, turno a turno
Simulación | reparte al rival atriles de ese posterior
:::

La primera fila es el motor, en C++20, donde vive todo lo que tiene que ser rápido. La segunda es la parte que lee al rival: los datos y el aprendizaje están en Python, y el filtro corre dentro del motor. Alrededor de las dos hay una tercera parte, menos visible: el banco de pruebas que decide si algo de esto funciona.

## Qué es nuevo y qué no

No es nuevo: la generación de jugadas con GADDAG, la equity estática con tablas de leaves, la simulación a dos plies y los finales exactos son técnicas conocidas, y Kingbble las implementa como las describe la literatura. Su valor aquí es que están verificadas contra el motor de referencia, así que los experimentos que se apoyan en ellas son fiables.

Es nuevo: el modelo del rival ajustado con partidas humanas y condicionado por rating, su uso dentro de un filtro de partículas que sigue el atril rival durante varios turnos, la protección frente a jugadas raras y la evaluación de todo ello con partidas emparejadas.

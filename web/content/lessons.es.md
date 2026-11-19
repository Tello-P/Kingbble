title: Lo que no funcionó
nav: Lo que no funcionó
blurb: Cinco ideas que no dieron nada, un falso positivo y los fallos que más enseñaron.
lead: Un proyecto que solo cuenta sus aciertos esconde la mitad de lo que aprendió. Este capítulo recoge las ideas que no dieron nada, un resultado que resultó ser ruido y los fallos que cambiaron la forma de medir.
---
## Más cálculo no ganó

La primera sospecha cuando un motor pierde contra otro más fuerte es que piensa poco. Así que a la simulación se le dio más: más iteraciones por candidata, más candidatas o las dos cosas, contra la base de 8 candidatas y 50 iteraciones.

:::bars min=30 max=65 ref=50 unit=% caption=Porcentaje de victorias del bot con más cálculo contra la base 8×50. 60 partidas cada uno en un banco de 30 posiciones con 10 a 25 fichas en la bolsa. Intervalos del 95 %.
8 candidatas × 200 iteraciones | 46.7 | 39.0 | 54.4 | 4 veces más iteraciones
8 candidatas × 800 iteraciones | 46.7 | 39.0 | 54.4 | 16 veces más iteraciones
15 candidatas × 50 iteraciones | 42.5 | 36.2 | 48.8 | spread −11,6
15 candidatas × 200 iteraciones | 50.8 | 43.2 | 58.4 | las dos cosas
:::

Nada gana. Pasar de 200 a 800 iteraciones no cambia nada, y **más candidatas con pocas iteraciones pierde**. Esto último es la *maldición del ganador*: con más candidatas en la carrera, es más probable que alguna salga con una media inflada por la suerte, y el bot la elige. Con 200 iteraciones el efecto desaparece.

La lectura: a partir de una cantidad pequeña, el límite de la simulación no es el ruido, es el sesgo. Juega dos plies hacia delante contra un rival que responde con una jugada estática. Más muestras de una estimación sesgada dan una estimación sesgada más precisa.

## Simular más profundo: un falso positivo

Si el problema es el sesgo, hay que mirar más lejos. Tres plies en vez de dos. En el mismo banco, con las mismas iteraciones, el resultado parecía la primera mejora real del proyecto:

| Medición | Partidas | % de victorias (intervalo del 95 %) |
|---|---|---|
| Primeras 60 partidas | 60 | 56,7 (48,9–64,4) |
| Ampliado a 200 | 200 | 56,5 (51,7–61,3) |
| Solo las 140 nuevas | 140 | 56,4 (50,4–62,5) |
| Partidas completas | 80 | 53,1 (45,2–61,0) |

Significativo, replicado en partidas nuevas y barato: un 13 % más de tiempo. Tres plies pasó a ser el valor por defecto.

Dos días después se vino abajo. El banco de posiciones tenía un fallo: **los números aleatorios iban ligados al bot y no al asiento**. En una pareja de partidas, el mismo reparto con los asientos cambiados, dos bots idénticos no quedaban siempre 1–1. Eso dejaba un ruido que no se cancelaba entre las partidas de una pareja, y como el banco era el mismo en todas las pruebas, el mismo ruido se repetía en cada una. La «replicación» replicaba el fallo.

| Tres plies contra dos | Partidas | % de victorias | Spread |
|---|---|---|---|
| Banco antiguo | 200 | 56,5 (51,7–61,3) | +2,1 (−2,9 a +7,2) |
| **Banco corregido** | **200** | **51,0 (47,5–54,5)** | **−0,5 (−4,6 a +3,7)** |

Con la medición limpia, tres plies empata con dos.

:::note Lo que quedó de aquello
Un **test nulo** que ahora corre de forma automática: dos bots idénticos en el banco tienen que sacar exactamente el 50 %. Si un banco de pruebas no supera un test con respuesta conocida, ninguna de sus otras respuestas cuenta. Mira [cómo se mide](method).
:::

A igual tiempo el cuadro era el mismo desde el principio: lo que el bot profundo gana por ver más lejos lo pierde por hacer menos iteraciones. Cuatro plies no fue mejor que tres, porque cada ply de más se juega con una respuesta estática y añade tanto error como quita.

## Riesgo: la media fue difícil de batir

La idea de [RiskBot](risk): cuando vas por detrás, preferir jugadas con resultados más dispersos; cuando vas por delante, menos. Se probaron todos los ajustes del parámetro de riesgo.

:::bars min=30 max=70 ref=50 unit=% caption=RiskBot contra SimBot (8×50), partidas emparejadas, intervalos del 95 %.
λ = 0,002 | 50.5 | 42.3 | 58.7 | 100 partidas
λ = 0,005 | 51.5 | 43.1 | 59.9 | 100 partidas
λ = 0,01 | 50.0 | 37.7 | 62.3 | 40 partidas
λ = 0,02 | 48.0 | 38.7 | 57.3 | 100 partidas
Probabilidad de victoria | 47.5 | 34.2 | 60.8 | 40 partidas
:::

Todo entre el 48 y el 51,5 %. Por qué:

1. **La varianza que se penaliza no es el riesgo que importa.** Casi toda la varianza de una jugada viene del atril del rival y del robo, y es la misma para todas las candidatas.
2. **La varianza está mal estimada.** Con 50 iteraciones tiene un error de alrededor del 20 %. Con una λ alta, ese ruido decide la jugada.
3. **El calendario apenas actúa.** El factor que activa el riesgo según se vacía la bolsa lo deja casi a cero durante la primera mitad de la partida.
4. **El efecto que se busca es pequeño**, de 1 a 3 puntos de porcentaje de victorias. Detectar +2 pide unas 5.000 partidas por ajuste.

La tercera hipótesis del proyecto, que una política de riesgo aprendida mejora a la media, se da como **no demostrada**. El aprendizaje por refuerzo del parámetro de riesgo está aparcado por eso.

## El pre-final aproximado

Con tres a seis fichas en la bolsa el solver exacto no llega, así que uno aproximado muestrea ordenaciones de la bolsa y resuelve cada una. Su efecto medido: **51,5 % (49,3–53,7) y +0,5 puntos de spread** en 400 partidas emparejadas. Neutro. Sigue en el motor como opción y no está activado por defecto en ningún nivel.

## Fallos que importaron

| Fallo | Cómo se notó | Qué cambió |
|---|---|---|
| **Una fuga en el grupo A de características** del modelo humano | el grupo que debía ayudar empeoraba el modelo | arreglado; ahora A mejora la pérdida en unos 0,06, y se comprueba que las características solo usen lo que el jugador podía ver |
| **19.131 turnos humanos descartados** por el parser | menos turnos con atril de los que daban las partidas | recuperados; el modelo se reajustó con más de 230.000 turnos |
| **«El final no bate a equity»** | 46,5–47 % contra equity en 200 partidas sin emparejar | no era un fallo: 200 partidas sin emparejar tienen un intervalo de ±7 puntos. Emparejadas, el solver gana +4,8 puntos por partida. Desde entonces todas las comparaciones van emparejadas |
| **Números aleatorios ligados al bot** en el banco | el falso positivo de arriba | semillas por asiento y un test nulo automático |
| **Quackle en el tablero equivocado** | resultados poco creíbles | el programa auxiliar le daba un tablero sin casillas de premio y después uno de tamaño equivocado; los dos arreglados, y el resultado sigue sin verificar |
| **Un nombre de bot desconocido cerraba el motor** | la interfaz se quedaba colgada | los nombres se validan antes de llegar al motor |

## Lo que dicen los fallos juntos

- **En este juego la suerte es grande y los efectos son pequeños.** Un experimento honesto pide partidas emparejadas, cientos de ellas, e intervalos. Sesenta partidas pueden enseñar casi cualquier cosa.
- **Un resultado no está replicado si la replicación comparte el instrumento.** El resultado de tres plies se mantuvo en partidas nuevas porque el fallo estaba en el banco, no en las partidas.
- **Un fallo sospechado también puede ser ruido.** El solver de finales pareció roto durante una semana por culpa de 200 partidas sin emparejar. Fallaba la medición, no el solver.
- **La distancia con la referencia está en el modelo, no en la máquina.** Más iteraciones no la cerraron. Una idea mejor de lo que tiene el rival sí movió la aguja, y de eso trata este proyecto.

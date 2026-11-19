title: El pre-final
nav: Pre-finales
blurb: Una o dos fichas en la bolsa: resolver un final por cada forma en que se pueden robar.
lead: Las últimas fichas de la bolsa son la parte de la partida más difícil de jugar bien. Kingbble tiene un solver exacto para una o dos fichas y uno aproximado para tres a seis.
---
## Por qué las últimas fichas son especiales

Con una ficha en la bolsa, el rival tiene siete de las ocho fichas que no ves. No sabes cuáles siete, pero solo hay ocho posibilidades. Cada una lleva a un final distinto, y cada uno de esos finales se puede [resolver de forma exacta](endgame).

Una jugada en esta fase decide además quién vacía la bolsa y, con ello, quién juega primero el final con toda la información. A veces la mejor jugada es una pequeña que deja una ficha en la bolsa a propósito. La equity estática y la simulación a dos plies juegan mal esta fase, porque lo que importa es justo lo que ellas promedian.

## El solver exacto: una o dos fichas

Para cada jugada candidata, el solver exacto de pre-finales enumera **todas las formas posibles de repartir las fichas no vistas** entre el atril rival y la bolsa, en todos los órdenes. Para cada una juega la candidata, deja que se produzca el robo y resuelve de forma exacta el final que resulta. Cada candidata acaba con un recuento de victorias, empates y derrotas sobre todos los casos, y con un spread total.

Las jugadas se ordenan **primero por victorias**. Entre las empatadas en victorias decide el spread total. Aquí el objetivo es ganar la partida, no ganarla por más.

Detalles que lo hacen abordable:

- **Candidatas.** Solo se analizan a fondo las mejores jugadas por equity estática, opcionalmente con el pase.
- **Corte temprano.** Una jugada se abandona en cuanto acumula más derrotas de las que aún podría acabar teniendo la mejor hasta el momento.
- **Resoluciones anidadas.** Una jugada que deja fichas en la bolsa lleva a otro pre-final al turno siguiente. Esos se resuelven hasta una profundidad limitada.
- **El pase.** En las posiciones anidadas solo se considera pasar justo después de un turno sin puntos, porque es cuando puede terminar la partida.
- **Hilos.** Las jugadas y los robos se reparten entre hilos, cada uno con su propio solver de finales.

## Comprobado contra Macondo

Posiciones con una y dos fichas en la bolsa, sobre ENABLE, 600 segundos por posición y lado. En cada posición se comparan las tres mejores jugadas y el pase: victorias y spread total sobre todas las ordenaciones de la bolsa.

:::bars min=0 max=100 unit=% caption=Coincidencia exacta con Macondo en pre-finales, según las fichas en la bolsa.
1 ficha en la bolsa | 100 | | | 28 de 28 jugadas, 7 posiciones
2 fichas en la bolsa | 95.8 | | | 23 de 24 jugadas, 6 posiciones
:::

| Bolsa | Posiciones comparadas | Jugadas comparadas | Diferencias | Saltadas por tiempo |
|---|---|---|---|---|
| 1 ficha | 7 | 28 | **0** | 5 |
| 2 fichas | 6 | 24 | **1** | 6 |

Con una ficha en la bolsa, los dos solvers coinciden en todo lo comparado.

Con dos fichas hay una diferencia, y aquí se cuenta tal cual. Para la jugada `M5 BLADE (24)` en una posición, los dos solvers dan el mismo número de victorias, cero, pero el spread total sobre todas las ordenaciones difiere en 18 puntos: −8.732 Kingbble y −8.714 Macondo. Lo más probable es que en alguna ordenación concreta uno de los dos encuentre una línea de final un poco mejor, o que alguna regla se aplique distinto, como el tratamiento de un pase al final del todo. Que las victorias coincidan dice que el efecto es pequeño, pero dos solvers exactos no deberían diferir en nada, así que sigue abierta.

:::note El límite, sin adornos
Con dos fichas en la bolsa, 11 de 24 posiciones no terminan en 600 segundos. Cada candidata necesita un final por cada orden del robo, y permitir el pase multiplica el árbol. Acelerarlo, reutilizando los finales que se repiten entre ordenaciones y cortando antes el pase, está en la [hoja de ruta](roadmap).
:::

## El solver aproximado: de tres a seis fichas

Con más fichas en la bolsa hay demasiados casos para enumerarlos. El pre-final aproximado, `apeg`, los muestrea:

1. Toma las 10 mejores jugadas por equity estática.
2. Reparte al azar el atril rival y el orden de la bolsa a partir de las fichas no vistas, 100 veces. Todas las jugadas se prueban contra **los mismos repartos**.
3. Para cada reparto hace la jugada, deja que los dos lados jueguen su mejor jugada por equity estática hasta vaciar la bolsa y resuelve el final con una búsqueda corta de 4 plies.
4. Ordena las jugadas por la proporción de repartos ganados y, después, por el spread final medio.

## Cuánto vale

Las dos capas juntas, `equity+apeg+peg+eg`, contra un bot solo con el final exacto, `equity+eg`. Partidas emparejadas, dos semillas.

| | % de victorias | Spread por partida |
|---|---|---|
| Semilla 1 | 53,0 (49,4–56,6) | +1,8 (−1,9 a +5,4) |
| Semilla 2 | 50,0 (47,6–52,4) | −0,8 (−4,1 a +2,5) |
| **Las dos, 400 partidas** | **51,5 (49,3–53,7)** | **+0,5 (−2,0 a +2,9)** |

En cristiano: **no se detecta ni ganancia ni pérdida**. Lo que sí se puede afirmar con confianza es que no empeora al bot y que, si ayuda, es en menos de unos 3 puntos por partida.

Tiene sentido. Estas capas solo actúan en los dos o tres últimos turnos, y un bot que ya resuelve el final de forma exacta juega bien esos turnos. Ver un efecto tan pequeño exigiría unas 2.000 partidas emparejadas.

Aquí es también donde se pierde por poco el match contra Macondo: Kingbble perdió allí más partidas ajustadas de las que ganó (17 frente a 27), y las partidas ajustadas se deciden en estos últimos turnos. Mira [referencias](references).

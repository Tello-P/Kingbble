title: Inferir el atril del rival
nav: Inferencia del atril
blurb: Una nube de atriles posibles que se repondera con cada jugada del rival. El núcleo de Kingbble.
lead: Una nube de atriles que el rival podría tener, reponderada cada vez que mueve. Es el núcleo de Kingbble: cómo funciona el filtro, cómo se protege de las sorpresas y cuánto vale.
---
## La idea en un ejemplo

El rival podía hacer un bingo de 80 puntos por una `E` abierta. En su lugar cambia tres fichas.

Antes de esa jugada, cualquier combinación de siete fichas no vistas era un atril posible. Después, los atriles que contenían ese bingo son muy improbables: casi nadie deja pasar 80 puntos a propósito. Los atriles con tres fichas incómodas y cuatro buenas se han vuelto probables, y las cuatro que se ha quedado seguramente son buenas.

Eso es la regla de Bayes, y Kingbble la aplica después de cada jugada del rival:

```
P(atril | jugada)  ∝  P(jugada | atril) × P(atril)
```

`P(atril)` es lo que se creía antes de la jugada. `P(jugada | atril)` sale de un modelo de jugadas: el [modelo humano](human-model) cuando el rival es una persona. El producto, normalizado, es lo que se cree ahora.

## Tres modelos de jugada

El filtro es el mismo sea cual sea el modelo que tenga detrás. En todo el manual se comparan tres.

| Modelo | Probabilidad de la jugada dado el atril | Nombre en los bots |
|---|---|---|
| **Ninguno** | igual para todos los atriles: de la jugada no se aprende nada | `inf:none` |
| **RangeFinder** | 1 si la jugada es la mejor opción de ese atril por equity (o está dentro de una tolerancia), 0 si no | `inf:rf` |
| **Modelo humano** | la probabilidad de que una persona de ese rating la eligiera | `inf:human` |

RangeFinder es el método clásico de Quackle y Macondo. Es el modelo correcto para un rival que siempre juega la mejor jugada por equity, y uno muy duro para cualquier otro: un atril es del todo compatible con la jugada o se tira.

Cuando el rival es un programa, Kingbble usa un **modelo bot** aparte, ajustado con turnos de bots, en lugar del humano. En modo `auto` elige entre los dos según quién mueve.

## Por qué un filtro de partículas

Con 60 fichas sin ver hay millones de atriles posibles de siete. Ponderar cada uno supondría generar y ordenar las jugadas de todos los atriles, en cada turno. Demasiado lento.

Así que el filtro guarda una **muestra**: por defecto 500 hipótesis, cada una un atril completo con un peso. Son las *partículas*. Un turno del rival va así:

:::flow
Observar | la jugada del rival y el tablero anterior
Reponderar | cada partícula por P(jugada | su atril)
Remuestrear | si el peso se concentra en pocas partículas
Avanzar | quitar las fichas jugadas, robar otras de la bolsa
Resincronizar | tras mi jugada y mi robo, arreglar lo que ya no encaja
:::

**Reponderar.** Para cada partícula, el motor ordena las opciones de ese atril desde el punto de vista del rival y le pide al modelo la probabilidad de la jugada que se hizo de verdad. Una partícula cuyo atril no podía hacer esa jugada recibe cero.

**Remuestrear.** A los pocos turnos, casi todo el peso está en unas pocas partículas. Cuando el número *efectivo* de partículas baja de la mitad, la nube se vuelve a sacar en proporción a los pesos: las pesadas se copian y las ligeras desaparecen.

**Avanzar.** Las fichas que jugó el rival salen de todas las partículas. Lo que queda es la apuesta de cada partícula sobre el **leave**, y esa es la parte valiosa: las fichas nuevas se roban al azar, pero las que se quedó pasan al turno siguiente.

**Resincronizar.** Cuando yo muevo y robo, veo fichas nuevas, y algunas partículas contienen ahora fichas que no pueden estar en el atril rival. Esas se *rejuvenecen*: se conservan sus fichas que siguen encajando y el resto se vuelve a robar.

## Exacto cuando se puede

Avanzada la partida, el número de atriles posibles se desploma. Cuando hay como mucho 3.000, el filtro deja de muestrear, **enumera todos los atriles** y actualiza cada uno de forma exacta. En el cambio, cada atril arranca con una parte del prior mezclada, para no descartar un atril que a la muestra se le hubiera escapado.

## La sorpresa

Un filtro que se fía del todo de su modelo es frágil. Una jugada extraña, un clic equivocado, un farol o simplemente una palabra que el modelo no esperaba, y todas las partículas reciben un peso casi nulo. Sobrevive lo que explica la jugada extraña menos mal, y el seguidor acaba equivocado y muy seguro.

Kingbble mide lo sorprendente que es cada jugada, en bits: cuánto más probable es la jugada bajo el *prior* (atriles uniformes) que bajo el posterior que se viene siguiendo. Por encima de 4 bits, el posterior se **ensancha antes de actualizar**: la mitad se sustituye por el prior. Una jugada rara cuesta entonces algo de nitidez, en vez de hundir toda la creencia.

Eso es lo que comprueba el test de estrés de [resultados](results): un rival que el 30 % de las veces juega algo fuera de sus diez mejores jugadas.

## Cómo se usa

El seguidor le da a la [simulación](simulation) un muestreador de atriles. En cada iteración simulada, el 80 % de las veces el atril del rival sale del posterior y el 20 % de forma uniforme de las fichas no vistas. Nada más cambia en la simulación.

## Cómo de bien adivina

La prueba: en turnos reservados, ¿cuánta probabilidad puso el seguidor en las fichas que el jugador **se quedó de verdad**? Se mide como log-loss, en nats por turno; menos es mejor.

:::bars min=0 max=13 caption=Log-loss del leave que se quedó de verdad, en 2.000 turnos humanos reservados de jugadores que el modelo nunca vio.
Kingbble, modelo humano | 5.74 | | | 
Sin inferencia | 5.87 | | | robo uniforme de las fichas no vistas
RangeFinder | 12.50 | | | 
:::

:::bars min=0 max=13 caption=Lo mismo en 1.000 turnos jugados por bots, con el modelo bot. Compara solo dentro de cada panel: los bots son más predecibles, así que aquí todo sale más bajo.
Kingbble, modelo bot | 4.46 | | | 
Sin inferencia | 5.40 | | | 
RangeFinder | 9.08 | | | 
:::

| En 2.000 turnos humanos | Modelo propio | Sin inferencia | RangeFinder |
|---|---|---|---|
| Log-loss | **5,737** | 5,865 | 12,497 |
| Acierto top-1 | 21,4 % | 20,2 % | 20,6 % |
| Error de calibración por ficha (ECE) | 0,012 | 0,003 | 0,045 |

Leído con honestidad:

- Frente a **no inferir nada**, la ganancia en turnos humanos es real y pequeña: 0,13 nats por turno. La gente es difícil de predecir.
- Frente a **RangeFinder**, la diferencia es grande. RangeFinder es demasiado tajante: descarta atriles que eran perfectamente posibles, y lo paga caro cada vez que el jugador no hizo la mejor jugada del motor.
- Con **bots**, que son predecibles, la ganancia frente a no inferir es mucho mayor: 0,94 nats.
- Las probabilidades por ficha están **bien calibradas**: cuando el seguidor dice que una ficha está en el atril el 30 % de las veces, está más o menos el 30 % de las veces. El error ronda el 1 %.

## ¿Gana partidas?

Cada lado juega con simulación y sigue el atril del otro con su modelo.

:::bars min=40 max=70 ref=50 unit=% caption=Porcentaje de victorias de la inferencia con el modelo de Kingbble contra la inferencia con RangeFinder. 200 partidas con dos semillas, intervalo del 95 %. La línea discontinua es el 50 %.
Modelo propio contra RangeFinder | 58.5 | 51.7 | 65.3 | 117 de 200 partidas
:::

**117 de 200 partidas, un 58,5 %, con un intervalo del 95 % de 51,7 a 65,3.** Significativamente mejor que la igualdad. Es el resultado central del proyecto.

Por semilla salió un 54 % (spread medio +2,9) y un 63 % (+30,6), lo que también enseña cuánto pueden diferir dos tandas de 100 partidas.

Y bajo estrés, contra un rival que el 30 % de las veces juega una jugada al azar fuera de sus diez mejores: el modelo de Kingbble gana el 89 % y RangeFinder el 85 %, con 100 partidas cada uno. La diferencia no es significativa (±9 puntos). Lo que importa del test es lo otro: el modelo no se rompe cuando el rival deja de tener sentido.

| Criterio de salida del hito | Resultado | Cumple |
|---|---|---|
| Mejor que RangeFinder en log-loss | 5,74 frente a 12,50 | sí |
| Porcentaje de victorias combinado de al menos 50 | 58,5 % | sí |
| Contra jugadas raras, no peor que el baseline | 89 % frente a 85 % | sí |

:::note El resumen honesto
La ventaja en partidas viene sobre todo de **no equivocarse tanto como RangeFinder**, y menos de saber mucho más que nada. Por eso los resultados dan las tres cifras, ninguno, RangeFinder y modelo propio, cada una con su intervalo.
:::

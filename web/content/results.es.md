title: Resultados
nav: Resultados
blurb: Todas las cifras del proyecto en un sitio: corrección, predicción y partidas.
lead: Todas las mediciones del proyecto en un sitio, agrupadas por la pregunta que responden: si el motor es correcto, si la inferencia predice mejor y si algo de esto gana más partidas.
---
## ¿El motor es correcto?

Antes de medir la fuerza, el motor tiene que estar bien. Se exportaron posiciones, se le pasaron a Macondo y se compararon jugada a jugada y valor a valor.

:::bars min=0 max=100 unit=% caption=Coincidencia exacta con Macondo. Generador: 10.000 posiciones con cada una de dos semillas. Final: posiciones que los dos solvers terminan en 600 s. Pre-final: victorias y spread total de las mejores jugadas sobre todas las ordenaciones de la bolsa.
Generador de jugadas | 100 | | | 20.000 de 20.000 posiciones
Final exacto | 100 | | | 34 de 34 posiciones
Pre-final, 1 ficha en la bolsa | 100 | | | 28 de 28 jugadas
Pre-final, 2 fichas en la bolsa | 95.8 | | | 23 de 24 jugadas
:::

La única diferencia es una sola jugada en una sola posición, 18 puntos de spread total sobre todas las ordenaciones con las mismas victorias, y está en investigación. Los detalles, en [pre-finales](preendgame).

Un dato más de la comparación de finales: dentro del tope de 600 segundos, el solver de Kingbble dejó 5 posiciones sin terminar y el de Macondo 11.

## ¿La inferencia predice mejor?

Cuánta probabilidad pone cada método en las fichas que el jugador se quedó de verdad. Compara solo dentro de cada panel: los dos paneles son conjuntos de turnos distintos.

:::bars min=0 max=13 caption=2.000 turnos humanos reservados. Log-loss en nats por turno, menos es mejor.
Kingbble, modelo humano | 5.74 | | | 
Sin inferencia | 5.87 | | | 
RangeFinder | 12.50 | | | 
:::

:::bars min=0 max=13 caption=1.000 turnos jugados por bots, con el modelo bot.
Kingbble, modelo bot | 4.46 | | | 
Sin inferencia | 5.40 | | | 
RangeFinder | 9.08 | | | 
:::

Y el modelo de jugadas por sí solo, prediciendo la jugada que hace una persona:

:::bars min=0 max=7 caption=Log-verosimilitud negativa en jugadores de test, menos es mejor.
Modelo humano (MLP, solo partidas reales) | 3.62 | | | 
Elección uniforme entre las opciones legales | 6.38 | | | 
:::

Frente a no inferir, la ganancia con personas es pequeña (0,13 nats por turno). La gran diferencia es frente a RangeFinder, que es demasiado tajante. Con bots, más predecibles, la ganancia frente a no inferir es mucho mayor (0,94 nats). Las probabilidades por ficha están bien calibradas, con un error de alrededor del 1 %. Más en [inferencia del atril](inference).

## ¿Gana más partidas?

Cada fila es un experimento distinto con su propio rival.

:::bars min=38 max=70 ref=50 unit=% caption=Porcentaje de victorias con intervalos del 95 %. Las filas en dorado excluyen el 50 %. La línea discontinua es el 50 %.
Inferencia: modelo propio contra RangeFinder | 58.5 | 51.7 | 65.3 | 200 partidas
Final exacto contra equity | 51.4 | 50.4 | 52.5 | 600 partidas emparejadas
Pre-final aproximado + exacto | 51.5 | 49.3 | 53.7 | 400 partidas emparejadas
Simulación, 3 plies contra 2 | 51.0 | 47.5 | 54.5 | 200 partidas, banco corregido
Kingbble contra Macondo, 5 s por jugada | 47.0 | 41.7 | 52.3 | 300 partidas emparejadas
:::

- **Inferencia contra RangeFinder: 117 de 200 partidas**, significativamente por encima del 50 %. El resultado central.
- **Final exacto**: pocas partidas más (+1,4 puntos), porque solo decide las igualadas, pero con un efecto claro en spread, abajo.
- **Pre-final y tres plies**: neutros. No hacen daño; no se detecta ganancia.
- **Contra Macondo**: empate estadístico. Más partidas ajustadas perdidas que ganadas (17 frente a 27), lo que encaja con que los últimos turnos son la parte que queda por pulir. Mira [referencias](references).

## Lo que suma el final en puntos

:::bars min=-3 max=7 ref=0 caption=Spread medio por partida frente a un bot solo con equity estática (la última fila: frente a un bot solo con el final exacto). Partidas emparejadas, intervalos del 95 %.
Final exacto, 1 s | 4.8 | 4.0 | 5.6 | 600 partidas
Final exacto, 10 s | 5.1 | 4.3 | 6.0 | 600 partidas
1 s, y la jugada de equity si no es exacto | 2.7 | 2.0 | 3.3 | 600 partidas
Pre-final aproximado + exacto | 0.5 | -2.0 | 2.9 | 400 partidas
:::

## Bajo estrés

Un rival que el 30 % de las veces juega una jugada al azar fuera de sus diez mejores.

:::bars min=60 max=100 unit=% caption=Porcentaje de victorias contra el rival errático, 100 partidas por lado. La diferencia no es significativa (±9 puntos); lo importante es que la inferencia con el modelo humano no empeora.
Modelo de Kingbble | 89 | | | +140 de spread
RangeFinder | 85 | | | +128 de spread
:::

## Las tres hipótesis

| Hipótesis | Veredicto | Evidencia |
|---|---|---|
| H1. Un modelo aprendido de personas predice mejor la jugada real | **Confirmada** | NLL de 3,62 frente a 6,38 en jugadores no vistos |
| H2. Usarlo en un filtro de varios turnos mejora el juego | **Confirmada frente a RangeFinder**, modesta frente a no inferir | log-loss de 5,74 frente a 12,50 y 5,87; 58,5 % de las partidas |
| H3. Una política de riesgo aprendida mejora a la media | **No demostrada** | todos los ajustes de riesgo entre el 48 y el 51,5 % |

## Las cuatro ablaciones

La evaluación final toma el bot completo, `sim+inf:human+eg`, y lo enfrenta a sí mismo quitándole una pieza. 400 partidas emparejadas cada una, un número fijo de iteraciones de simulación, semillas por partida y un final limitado en profundidad, de modo que la misma configuración repite las mismas partidas. Los p-valores van corregidos por Holm entre las cuatro.

| Experimento | El otro bot | La pregunta |
|---|---|---|
| `inference` | `sim+inf:none+eg` | ¿ayuda seguir el atril del rival? |
| `human_vs_rangefinder` | `sim+inf:rf+eg` | ¿modelo humano o RangeFinder? |
| `sim_vs_equity` | `equity+eg` | ¿qué añade la simulación? |
| `endgame` | `sim+inf:human` | ¿qué añade el solver de finales? |

El código y las configuraciones están terminados. Las ejecuciones son unas 22 horas de máquina y son el último paso antes de etiquetar la versión 1.0: mira la [hoja de ruta](roadmap). Cuando terminen, su tabla irá aquí.

## Reproducir una cifra

Cada tabla de esta página es un comando, listado en `docs/experiments.md` en el repositorio. Todos los scripts guardan en disco cada partida terminada, así que una ejecución parada sigue donde se quedó; tienen un modo `--quick` que comprueba el proceso en minutos; y corren a la prioridad más baja con un número fijo de núcleos. Por ejemplo, el diagnóstico del final:

```
for seed in 1 2 3; do
  build/engine/kingbble_selfplay --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2 \
    --a equity+eg --b equity --paired --games 200 --seed $seed --threads 2 \
    --endgame-time 1 --endgame-stats --results issue_48_s$seed.txt
done
python -m ml.eval.tournament issue_48_s*.txt
```

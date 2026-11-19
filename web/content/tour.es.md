title: Una jugada, de principio a fin
nav: Una jugada, paso a paso
blurb: Qué pasa dentro del motor entre tu jugada y su respuesta.
lead: Qué pasa dentro del motor desde que juegas hasta que responde. Un recorrido por todas las capas, con el enlace al capítulo que explica cada una.
---
## La versión corta

Cuando le toca a Kingbble tiene siete fichas, ve el tablero y sabe qué fichas no han salido todavía. A partir de ahí hace cinco cosas, cada una apoyada en la anterior.

:::flow
Generar | todas las jugadas legales: unas 500 por turno
Ordenar | por puntos más el valor de las fichas que se queda
Simular | las 10 mejores, más o menos, contra respuestas posibles
Resolver | de forma exacta, con la bolsa vacía o casi vacía
Jugar | y contarle al seguidor de atriles lo que acaba de pasar
:::

Cuántos de estos pasos se ejecutan depende del [nivel](levels): el nivel fácil se queda en el primero, el medio en el segundo, y así sucesivamente.

## 1. Generar

El [diccionario](lexicon) no es una lista de palabras. Es un autómata, un GADDAG, en el que cada palabra se guarda una vez por cada una de sus letras, para poder construirla hacia fuera desde cualquier ficha que ya esté en el tablero. El [generador de jugadas](movegen) recorre ese autómata desde cada casilla pegada a una ficha y coloca solo letras que estén en el atril y formen palabras válidas en perpendicular.

El resultado es la lista completa de colocaciones legales, cada una con su [puntuación](scoring). A esa lista el motor añade los cambios de fichas posibles y el pase.

## 2. Ordenar

Los puntos de una jugada dicen lo que gana ahora. Su [equity](equity) suma lo que valdrán después las fichas que se queda: el *leave*. Quedarse `ERS` vale varios puntos porque esas fichas combinan bien al turno siguiente; quedarse `VVU` cuesta puntos. Los valores salen de una tabla con un número para cada uno de los 914.624 leaves posibles de una a seis fichas.

Ordenar por equity ya es un jugador decente. Además es instantáneo: el nivel medio es exactamente esto.

## 3. Simular

La equity no ve que una jugada le abre al rival una casilla de triple palabra, ni que bloquea la línea que necesitabas. La [simulación](simulation) sí. Para cada una de las mejores candidatas el motor repite muchas veces el mismo experimento pequeño: reparte un atril al rival, juega la candidata, deja que el rival haga su mejor respuesta, hace su mejor continuación y apunta la diferencia de puntos más el valor de los leaves que quedan. Se elige la candidata con mejor media.

Dos detalles lo hacen asumible. La simulación va por lotes y, después de cada uno, descarta las candidatas que van claramente por detrás. Y trabaja con un presupuesto de tiempo: cinco segundos son cinco segundos.

Este es el paso donde importa saber algo del atril rival. Con la [inferencia](inference) activada, la mayoría de los atriles que se reparten al rival salen del posterior del seguidor y no de un robo uniforme.

## 4. Resolver

Con la bolsa vacía no queda nada que adivinar: el rival tiene exactamente las fichas que no han salido. El [solver de finales](endgame) busca en el árbol de jugadas y respuestas hasta el final y juega la línea que maximiza la diferencia final. Con una o dos fichas en la bolsa, el [solver de pre-finales](preendgame) hace lo mismo para cada orden en que esas fichas podrían robarse.

## 5. Jugar y actualizar lo que se sabe

La jugada va al tablero. Si el bot usa inferencia, su seguidor observa también cada jugada *tuya*: antes de tu jugada tiene una nube de atriles que podrías tener; después, pesan más los atriles que explican tu jugada. Las fichas que has jugado salen de esos atriles, se roban otras nuevas y la nube queda lista para el turno siguiente.

## Dónde vive cada parte

```
engine/      C++20: include/, src/, apps/ (self-play, matches, evaluación, bridge), tests/
ml/          Python: data/, human_model/, rl/, eval/, tests/
app/         menú de entrenamiento (línea de comandos y Streamlit) y el bucle Auto
ui/          la interfaz: página estática y servidor local
desktop/     el envoltorio de escritorio
web/         esta web y la compilación del motor a WebAssembly
scripts/     herramientas de léxico, ayudas para Quackle y Macondo, un script por experimento
configs/     configuraciones de torneo
docs/        los experimentos detrás de las tablas de resultados
```

El motor y la capa de Python se hablan mediante bindings y ficheros de resultados. La interfaz habla con el motor a través de un programa pequeño, el bridge: entra un comando de texto, sale un objeto JSON. Por eso la misma página funciona sobre un servidor local, dentro de la app de escritorio y, en la [demo](play), sobre el motor compilado a WebAssembly sin ningún servidor.

## Los nombres de los bots

Los bots se llaman por sus piezas, y esos nombres aparecen por todo el manual.

| Nombre | Qué hace |
|---|---|
| `greedy` | la jugada con más puntos |
| `equity` | la jugada con mayor equity estática |
| `sim` | simulación Monte Carlo sobre las mejores candidatas por equity |
| `sim+inf:human` | simulación con atriles del rival sacados del filtro con el modelo humano |
| `sim+inf:rf` | lo mismo con el baseline RangeFinder |
| `sim+inf:none` | lo mismo con atriles uniformes: el `sim` de siempre |
| `risk`, `riskwin` | elección entre candidatas simuladas por media y varianza, o por probabilidad de victoria |
| `+eg` | añade el solver exacto de finales |
| `+peg` | añade el solver exacto de pre-finales |
| `+apeg` | añade el pre-final aproximado |

Así, `sim+inf:human+eg` es el bot completo de los experimentos: simulación, inferencia con el modelo humano y finales exactos.

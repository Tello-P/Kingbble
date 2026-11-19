title: Glosario
nav: Glosario
blurb: Todos los términos de este manual, en una línea cada uno.
lead: Las palabras del juego y del motor, cada una en una o dos líneas, con el capítulo donde se explica. Los términos técnicos que la comunidad usa en inglés se dejan en inglés.
---
## El juego

| Término | Significado |
|---|---|
| **Atril** | las fichas que tiene un jugador, hasta siete |
| **Bolsa** | las fichas que aún no se han robado |
| **Comodín** | una ficha que vale cero y hace de cualquier letra |
| **Casilla de premio** | una casilla que dobla o triplica una letra o una palabra, una sola vez, para las fichas colocadas en ella ese turno |
| **Bingo** | una jugada que usa las siete fichas; +50 puntos |
| **Gancho** | una letra añadida delante o detrás de una palabra del tablero para formar otra |
| **Palabra cruzada** | una palabra formada en perpendicular a la principal por una de sus fichas nuevas |
| **Cambio** | ceder el turno para cambiar fichas con la bolsa; se permite con siete o más fichas en ella |
| **Cerrar** | jugar tu última ficha con la bolsa vacía; ganas el doble del valor del atril del rival |
| **Spread** | tus puntos menos los del rival |
| **Fichas no vistas** | las que no están ni en el tablero ni en tu atril: la bolsa más el otro atril |
| **Diccionario, léxico** | la lista de palabras válidas. Mira [el diccionario](lexicon) |

## Notación y formatos

| Término | Significado |
|---|---|
| **8H** | una casilla: fila 8, columna H. La fila primero indica horizontal; `H8` indica vertical |
| **`?`** | un comodín en un atril. En el tablero, una letra minúscula es un comodín jugado como esa letra |
| **CGP** | una línea de texto que describe una posición entera: tablero, atriles, marcadores |
| **GCG** | el formato de texto de una partida entera, jugada a jugada |
| **KWG** | el formato binario de un grafo de palabras, compartido con los motores de referencia |
| **KLV, KLV2** | formatos binarios de una tabla de leaves |

## El motor

| Término | Significado | Capítulo |
|---|---|---|
| **DAWG** | un grafo que guarda una lista de palabras compartiendo prefijos y sufijos | [diccionario](lexicon) |
| **GADDAG** | un grafo que guarda cada palabra empezando por cada una de sus letras, para construir jugadas hacia fuera desde una ficha del tablero | [diccionario](lexicon) |
| **Ancla** | una casilla vacía junto a una ficha, donde puede engancharse una jugada | [generador](movegen) |
| **Cross-check** | para una casilla, el conjunto de letras que forman ahí una palabra cruzada válida | [generador](movegen) |
| **Leave** | las fichas que te quedas después de una jugada | [equity](equity) |
| **Equity** | los puntos de una jugada más el valor de su leave | [equity](equity) |
| **Estática** | calculada solo con la posición, sin mirar hacia delante | [equity](equity) |
| **Simulación, playout** | jugar la partida hacia delante con robos al azar para valorar una jugada | [simulación](simulation) |
| **Ply** | una jugada de un jugador | [simulación](simulation) |
| **Iteración** | un playout de cada candidata con los mismos robos al azar | [simulación](simulation) |
| **Final** | la partida desde que la bolsa se vacía: información perfecta | [final](endgame) |
| **Pre-final** | la partida con pocas fichas en la bolsa | [pre-final](preendgame) |
| **Alfa-beta** | una búsqueda que se salta las ramas que no pueden cambiar el resultado | [final](endgame) |
| **Tabla de transposición** | una memoria de posiciones ya buscadas | [final](endgame) |
| **Lazy SMP** | varios hilos buscando el mismo árbol y compartiendo esa memoria | [final](endgame) |
| **Bridge** | el programa por el que la interfaz habla con el motor, una línea de entrada y una línea de JSON de salida | [interfaz](interface) |

## Inferencia y aprendizaje

| Término | Significado | Capítulo |
|---|---|---|
| **Inferencia del atril** | estimar las fichas del rival a partir de sus jugadas | [inferencia](inference) |
| **Prior** | lo que se cree antes de ver la jugada: cualquier atril que permitan las fichas no vistas | [inferencia](inference) |
| **Verosimilitud** | lo probable que es la jugada hecha, para cada atril que pudo tener | [inferencia](inference) |
| **Posterior** | lo que se cree después: prior por verosimilitud | [inferencia](inference) |
| **Filtro de partículas** | un conjunto de atriles candidatos con peso, actualizado tras cada jugada | [inferencia](inference) |
| **RangeFinder** | el método clásico de Quackle: quedarse con los atriles para los que la jugada hecha era casi la mejor | [inferencia](inference) |
| **Modelo humano** | un modelo de qué jugada elige una persona, según su rating | [modelo humano](human-model) |
| **Logit condicional** | un modelo que da a cada opción una nota y convierte las notas en probabilidades | [modelo humano](human-model) |
| **MLP** | una red neuronal pequeña | [modelo humano](human-model) |
| **Self-play** | el motor jugando contra sí mismo para producir datos | [entrenamiento](training) |
| **Gate** | una versión nueva no puede perder contra la actual antes de sustituirla | [entrenamiento](training) |

## Medir

| Término | Significado | Capítulo |
|---|---|---|
| **Partidas emparejadas** | cada reparto se juega dos veces cambiando los asientos, para que la suerte del robo se anule | [método](method) |
| **Intervalo del 95 %** | el rango que contendría el valor real en 95 de cada 100 repeticiones | [método](method) |
| **Significativo** | el intervalo no incluye «no hay diferencia» | [método](method) |
| **Log-loss, NLL** | cuánto le sorprende a un modelo lo que pasó; menos es mejor | [método](method) |
| **Nat** | la unidad de la log-loss con logaritmos naturales | [método](method) |
| **Calibración** | cuando el modelo dice 30 %, pasa el 30 % de las veces | [método](method) |
| **Ablación** | quitar una pieza para medir lo que aporta | [resultados](results) |
| **Corrección de Holm** | un ajuste por contrastar varias hipótesis a la vez | [método](method) |
| **Test nulo** | un experimento con respuesta conocida, hecho para comprobar el instrumento | [lo que no funcionó](lessons) |
| **Semilla** | el número que fija los sorteos, para poder repetir una ejecución | [método](method) |

## Nombres de los bots

| Nombre | Qué es |
|---|---|
| `greedy` | la máxima puntuación |
| `equity` | equity estática |
| `sim` | simulación Monte Carlo |
| `sim+inf:human` | simulación con inferencia del atril por el modelo humano; `:rf` para RangeFinder, `:none` para ninguna |
| `risk`, `riskwin` | selección por media y varianza, o por probabilidad de victoria |
| `+eg` | con el final exacto |
| `+peg` | con el pre-final exacto |
| `+apeg` | con el pre-final aproximado |

Así, `sim+inf:human+eg` es simulación, inferencia con el modelo humano y el final exacto.

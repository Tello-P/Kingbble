title: El modelo de juego humano
nav: El modelo humano
blurb: La probabilidad de que una persona con este atril, a este rating, haga esta jugada.
lead: La probabilidad de que una persona con este atril, a este rating y en esta posición haga esta jugada. Qué mira el modelo, cómo se ajusta y cómo de bien predice a jugadores que nunca ha visto.
---
## La pregunta

Dados una posición, un atril y un jugador, el modelo responde: *de todas las jugadas legales, ¿cómo de probable es que se elija cada una?*

Un motor tiene una respuesta simple: la mejor jugada, siempre. Una persona no. La gente no ve bingos que no conoce, prefiere palabras de las que está segura, se guarda una `S` más de la cuenta, juega seguro cuando va ganando y arriesga cuando va perdiendo. Esas tendencias no son ruido. Son regulares, dependen del nivel del jugador y se pueden aprender.

## Una elección entre opciones

El modelo es un **logit condicional**, la herramienta habitual para elegir un elemento de un conjunto. Cada opción recibe una utilidad, que es una suma ponderada de sus features, y la probabilidad de una opción es su parte del total después de exponenciar:

```
P(opción i) = exp(u_i) / Σ exp(u_j)        u = w · features
```

Todo lo que es igual para todas las opciones de un turno se cancela en esa fracción. Así que el modelo no puede aprender «la gente hace 35 puntos de media», solo *qué distingue a la jugada elegida de las demás con las que competía*. Por eso la equity entra como distancia a la mejor opción del turno, y por eso los datos del turno en conjunto (el marcador, la fase de la partida) entran sobre todo a través de interacciones con features de la opción.

En cada turno el modelo mira las mejores opciones por equity estática, hasta un tope `k`, más la opción que se jugó de verdad.

## Qué mira

Las features van por grupos, y cada grupo responde a una pregunta sobre cómo elige la gente.

| Grupo | Pregunta | Ejemplos |
|---|---|---|
| **Base** | ¿Cómo de buena es la opción según el propio motor? | distancia a la mejor equity, puntos, valor del leave, puesto, fichas jugadas, si es un bingo, un cambio o un pase |
| **A. La palabra** | ¿Conocería el jugador esta palabra? | frecuencia en el inglés corriente, veces jugada en las partidas de entrenamiento, si es desconocida por las dos vías, longitud, probabilidad de robar sus letras, anagramas del atril |
| **B. Bingos perdidos** | ¿Había un bingo disponible y esta opción no lo es? | si había alguno, cuántos, lo común que es el más fácil, cuánta equity se dejó |
| **C. El leave** | ¿Con qué se queda el jugador? | tamaño, vocales y consonantes, duplicados, una `S`, un comodín, `Q` sin `U`, sinergia entre pares, probabilidad de bingo al turno siguiente, una cuenta por letra |
| **D. Forma en el tablero** | ¿Dónde va? | casillas de premio usadas, palabras cruzadas, ganchos, fichas atravesadas, distancia al centro, dirección |
| **E. Defensa** | ¿Qué abre? | triples palabra que quedan al alcance, líneas de bingo que se abren o se cierran |
| **F. Puesto en el turno** | ¿Cuánto destaca? | puesto por puntos, equity estandarizada en el turno, distancia entre las dos mejores opciones |
| **G. Contexto** | ¿Cómo va la partida? | diferencia en el marcador, fichas en la bolsa, número de turno, bolsa vacía, cambio en el turno anterior, rating |
| **H. Interacciones** | ¿Cambia lo anterior con la fase, la ventaja o el rating? | distancia de equity × fase de la partida, distancia × rating, bingo perdido × rating, frecuencia de la palabra × rating, puntos × ventaja |

En las interacciones vive la idea de Kingbble. «Distancia × rating» permite al modelo aprender que un jugador de 2.000 casi nunca regala 15 puntos de equity y que uno de 1.400 lo hace a menudo. «Frecuencia de la palabra × rating» le permite aprender que las palabras raras son indicio de un jugador fuerte.

## Por bandas de rating

Los jugadores se agrupan en bandas por rating, con cortes en 1.400, 1.700 y 2.000. Se ajusta un juego de pesos con todos y, después, cada banda recibe sus propios pesos, **encogidos hacia los comunes**. Con cuánta fuerza se decide con los datos de validación. Una banda con pocos turnos se queda cerca del modelo común; una con muchos puede diferenciarse. Un rating por debajo de todas las bandas, o desconocido, usa la primera.

## Dos válvulas de seguridad

Un logit puro puede dar a una jugada una probabilidad tan pequeña que una sola jugada sorprendente borre todo lo que creía el seguidor. Dos términos de mezcla lo evitan, ajustados por banda:

- **epsilon**: con una probabilidad pequeña, el jugador elige de forma uniforme entre las mejores opciones, tengan la utilidad que tengan.
- **residual**: con una probabilidad pequeña, el jugador hace algo fuera de las opciones que mira el modelo.

Son una forma de decir «la gente a veces hace cosas que no sé explicar», con un número puesto.

## La red y el modelo de árboles

Dos alternativas comparten las mismas features y la misma salida:

- **MLP.** Una red neuronal pequeña con una capa oculta sobre las features estandarizadas. Su salida, dividida por una temperatura, sustituye a la utilidad del logit. Puede captar efectos no lineales que al logit se le escapan. El motor la ejecuta directamente desde un fichero de texto: sin ninguna librería de aprendizaje automático al jugar.
- **LightGBM.** Un ordenador de opciones basado en árboles con gradient boosting, disponible en el menú de entrenamiento como alternativa en el nivel de datos intermedio.

## Cómo de bueno es

:::bars min=0 max=7 caption=Log-verosimilitud negativa de la jugada que se hizo de verdad, en jugadores de test (menos es mejor). El modelo es el MLP ajustado solo con partidas humanas reales.
Modelo humano | 3.62 | | | ajustado con partidas reales, probado en jugadores no vistos
Elección uniforme | 6.38 | | | todas las opciones legales igual de probables
:::

Una log-verosimilitud negativa de 3,62 frente a 6,38 significa que el modelo pone, de media, unas **16 veces más probabilidad** en la jugada que se hizo de verdad que una elección uniforme entre las opciones.

Esa es una medida del modelo solo. Lo que le importa al motor es si ayuda a adivinar el *atril*, que es el capítulo siguiente.

## Por qué solo partidas reales

Una versión anterior completaba el conjunto de entrenamiento con partidas de self-play de un bot «tipo humano». El modelo final se ajusta solo con partidas humanas reales: los humanos sintéticos le enseñaban al modelo la idea que el bot tiene de un humano. Los datos de self-play siguen teniendo su sitio, en el modelo bot y en la mezcla de datos del [menú de entrenamiento](training).

## El fichero

El modelo ajustado es un fichero de texto: una línea de versión, el tipo de modelo, los nombres de las features en orden, el tope de candidatas, los términos de mezcla y una línea de pesos por banda de rating. Los nombres de las features son un contrato entre el motor en C++ que las calcula y el código en Python que las ajusta. Un fichero de modelo nombra sus features, así que un modelo antiguo sigue funcionando cuando se añaden features nuevas.

:::note No va en la versión pública
El modelo humano se ajustó con partidas de Woogles, y si se puede redistribuir un modelo entrenado con ellas es una pregunta abierta planteada a la plataforma. Hasta que se aclare, el fichero del modelo queda fuera de la versión pública, y el nivel Extremo público se construye solo con piezas que no necesitan datos humanos. Los resultados de esta web están medidos con el modelo humano.
:::

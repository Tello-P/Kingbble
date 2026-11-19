title: Los cinco niveles
nav: Niveles
blurb: De la máxima puntuación a la inferencia del atril: qué calcula cada nivel y cómo se nota al jugar.
lead: Un nivel no es un hándicap que se le pone a un mismo bot. Cada nivel es una forma distinta de elegir la jugada, con una pieza más del motor encendida que el anterior.
---
:::raw
<div class="levels">
  <div class="level"><span class="ctile" aria-hidden="true">F<i>4</i></span><div><h3>Fácil</h3><p>Juega la jugada con más puntos. Nombre en el motor: <code>greedy</code>.</p></div><span class="where">Web y escritorio</span></div>
  <div class="level"><span class="ctile" aria-hidden="true">M<i>3</i></span><div><h3>Medio</h3><p>Puntos más el valor de las fichas que se queda. Nombre en el motor: <code>equity</code>.</p></div><span class="where">Web y escritorio</span></div>
  <div class="level"><span class="ctile" aria-hidden="true">F<i>4</i></span><div><h3>Fuerte</h3><p>Simula lo que pasa después de cada una de sus mejores candidatas. Nombre en el motor: <code>sim</code>.</p></div><span class="where">Web y escritorio</span></div>
  <div class="level"><span class="ctile" aria-hidden="true">M<i>3</i></span><div><h3>Muy fuerte</h3><p>Simulación, y la solución exacta cuando la bolsa está vacía. Nombre en el motor: <code>sim+eg</code>.</p></div><span class="where">Web y escritorio</span></div>
  <div class="level locked"><span class="ctile" aria-hidden="true">X<i>8</i></span><div><h3>Extremo</h3><p>Todo lo anterior, más una estimación continua de las fichas que tienes, con todos los núcleos del ordenador.</p></div><span class="where">Solo escritorio</span></div>
</div>
:::

## Fácil: la máxima puntuación

El bot genera todas las jugadas legales y juega la que vale más puntos. Nada más.

Es rápido, nunca hace una jugada ilegal y encuentra palabras largas que a una persona se le escaparían, porque las ve todas. También es corto de vista: gasta un comodín por cuatro puntos más, se queda con un atril de tres íes y una Q, y te abre una casilla de triple palabra sin pensarlo. Contra él se gana como explica el resto de este manual: cuidando tu atril y esperando a que estropee el suyo.

Cómo se encuentran las jugadas: [el generador de jugadas](movegen).

## Medio: equity estática

Ahora el bot suma, a los puntos de cada jugada, el valor de las fichas que se quedaría. Una jugada de 22 puntos que conserva `ERS?` es mejor que una de 30 que conserva `UUVW`, y la tabla de leaves sabe por cuánto.

Este único cambio es el mayor salto de fuerza de los cinco. El bot empieza a jugar como alguien que conoce el juego: guarda comodines y eses, se quita las fichas repetidas y cambia cuando su atril no tiene arreglo. Todavía no mira el tablero desde tu lado: no sabe lo que su jugada te abre.

Cómo se obtienen los valores: [equity y leaves](equity).

## Fuerte: simulación

El bot toma sus mejores candidatas por equity y, para cada una, juega la partida hacia delante muchas veces: te reparte un atril al azar de las fichas que no ha visto, te deja responder, hace su siguiente jugada y apunta cómo quedó el marcador. Gana la candidata con mejor media.

Aquí empieza a defender. Una jugada que deja abierta una triple palabra con las fichas para aprovecharla todavía en la bolsa sale con mala media, y el bot deja de jugarla, sin que haya escrita en ningún sitio una regla sobre casillas de triple palabra.

El tiempo para pensar que elijas se gasta aquí. La simulación para antes cuando una candidata va claramente por delante, así que con una jugada evidente responde antes de que se acabe el tiempo.

Cómo funciona: [simulación](simulation).

## Muy fuerte: simulación más el final exacto

Igual que Fuerte hasta que la bolsa se vacía. A partir de ahí el bot conoce tu atril exacto, porque es todo lo que no ve, y la partida pasa a ser de información perfecta. El motor la busca hasta el final y juega la jugada que es demostrablemente la mejor, o la mejor que ha demostrado cuando se acaba el tiempo.

Se nota en las partidas ajustadas. Te bloquea tu última salida, cierra un turno antes de lo que esperabas, o te deja con la Q en la mano. Medido contra la equity estática, el final exacto vale unos 4,8 puntos por partida.

Cómo funciona: [el final](endgame).

## Extremo: te lee el atril

El nivel más alto añade la pieza para la que se construyó este proyecto. Después de cada jugada tuya, el motor se pregunta con qué atriles esa jugada habría sido una elección razonable, y mantiene un conjunto ponderado de los atriles que puedes tener ahora. Cuando simula, te reparte atriles de ese conjunto en vez de atriles al azar.

Si juegas una palabra corta por pocos puntos y te quedas cinco fichas, supone que estás preparando una jugada de siete, y empieza a cerrar las calles. Si cambias, deja de temer a tus comodines.

Además usa todos los núcleos del ordenador para la simulación y el final, y admite tiempos para pensar más largos. Por eso solo está en la app de escritorio: una pestaña del navegador tiene un hilo para esto, y un nivel que necesita minutos del único hilo de un móvil no sería un nivel, sería un cuelgue.

:::note Qué inferencia usa el Extremo público
En los experimentos, la inferencia usa un modelo de juego humano ajustado con partidas reales. Si ese fichero de modelo se puede redistribuir es una pregunta abierta, así que la versión pública no lo incluye. El nivel Extremo de la app pública se monta solo con piezas que no necesitan datos humanos, elegidas midiendo las candidatas entre sí en ENABLE. Mira [el modelo humano](human-model) y [la inferencia del atril](inference).
:::

## Tiempo para pensar

| | Demo web | App de escritorio |
|---|---|---|
| Fácil, Medio | al instante | al instante |
| Fuerte, Muy fuerte | 0,5, 1, 2, 3 o 5 segundos | de 0,5 a 60 segundos |
| Extremo | no disponible | de 0,5 a 60 segundos |

Más tiempo significa más simulaciones por candidata, no una búsqueda más profunda. La ganancia se aplana pronto: en las mediciones del propio proyecto, multiplicar las iteraciones por 16 no ganó más partidas (mira [lo que no funcionó](lessons)). Uno o dos segundos es un buen ajuste. Usa más cuando quieras el análisis de una posición, no para una partida entera.

## Cuál elegir

- **Para aprender el juego**: Fácil y después Medio. Cuando Medio deje de ganarte, habrás aprendido a gestionar un atril.
- **Para una partida de verdad**: Fuerte a uno o dos segundos.
- **Para practicar finales**: Muy fuerte. Repasa después la partida y mira las cuatro últimas jugadas.
- **Para ver la idea del proyecto en marcha**: Extremo, en la [app de escritorio](download).

En el modo **Bot contra bot** puedes enfrentar dos niveles cualesquiera y ver en el tablero qué aspecto tiene la diferencia entre ellos.

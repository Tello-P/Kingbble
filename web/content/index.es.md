title: Kingbble
nav: Inicio
headline: El motor que intenta adivinar tu atril
lead: Kingbble es un motor de palabras cruzadas escrito desde cero en C++. Genera todas las jugadas legales, simula el futuro, resuelve los finales de forma exacta y lee cada jugada tuya como una pista sobre las fichas que te has quedado.
read: Leer el manual
toc: Qué hay en este manual
---
:::stats
0 diferencias | con Macondo, el motor de referencia, en 20.000 posiciones del generador de jugadas y 34 finales exactos
58,5 % | de victorias de la inferencia con modelo humano frente al método clásico RangeFinder (intervalo del 95 %: 51,7–65,3)
5,74 vs 12,50 | log-loss al predecir las fichas que se queda una persona: modelo propio frente a RangeFinder
47 % | contra el bot de simulación de Macondo a igual tiempo por jugada: empate estadístico (41,7–52,3)
:::

## Por qué un juego de palabras, y no ajedrez

A un buen motor de ajedrez ya no le gana nadie: la máquina ve todo el tablero y calcula más lejos que cualquier persona. Un juego de palabras cruzadas es distinto. Ves tu atril y nunca el otro, el reparto de fichas decide mucho, y un jugador fuerte todavía le gana una buena parte de las partidas a los mejores motores. Así que aquí la máquina no puede limitarse a calcular más que tú. Tiene que entenderte.

Los motores más fuertes simulan el futuro suponiendo que el rival tiene fichas al azar, o que juega como una máquina. Kingbble parte de otra idea: **las personas no juegan como máquinas, y su forma de jugar delata sus fichas**. Si alguien podía hacer un bingo de 80 puntos y en su lugar cambia fichas, casi seguro que no tenía las letras.

Para comprobar esa idea primero tenía que existir el motor entero: un diccionario guardado como autómata, un generador de jugadas que coincide con el motor de referencia posición a posición, simulación Monte Carlo, solvers exactos para el final de la partida, un modelo de juego humano ajustado con más de 230.000 turnos reales, y una forma de medir que aguante la cantidad de azar de este juego.

Esta web es el manual de todo eso. Cada capítulo explica una pieza: qué hace, cómo está construida, cómo se comprobó y qué dicen los números, incluidos los experimentos que no funcionaron.

:::shot game-es.webp | La interfaz: tablero, atril y el panel con las mejores jugadas por equity estática.
:::

## Tres formas de entrar

- **Jugar.** La [demo](play) ejecuta el motor de verdad en tu navegador, compilado a WebAssembly, en cuatro niveles. El quinto, Extremo, está en la app de escritorio.
- **Instalarlo.** La [página de descarga](download) tiene las versiones de Windows y Linux de la app de escritorio.
- **Entenderlo.** Empieza por [la idea](idea), o ve directo a la parte que te interese en el contenido de abajo.

:::note Cifras honestas
Todas las cifras de esta web salen de una ejecución real del proyecto, con su número de partidas y su intervalo del 95 %. Cuando un resultado es neutro o negativo se cuenta así: mira [lo que no funcionó](lessons).
:::

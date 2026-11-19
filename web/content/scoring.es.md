title: Reglas, puntuación y la partida
nav: Reglas y puntuación
blurb: Casillas de premio, palabras cruzadas, los 50 puntos de bonus y cómo termina una partida.
lead: Cómo se puntúa una jugada, cómo transcurre una partida desde el primer robo hasta la última ficha y los formatos de texto que el motor lee y escribe.
---
## El tablero y las fichas

El tablero tiene 15 por 15 casillas. Sesenta y una llevan premio, que la interfaz muestra con un rótulo y una trama además del color, para que el tablero se lea sin depender del color:

| Rótulo | Premio | Casillas |
|---|---|---|
| `2L` | doble letra | 24 |
| `3L` | triple letra | 12 |
| `×2` | doble palabra (el centro es una) | 17 |
| `×3` | triple palabra | 8 |

La bolsa inglesa tiene 100 fichas: 98 letras, cada una con un valor de 1 (`E`, `A`, `I`…) a 10 (`Q`, `Z`), y dos comodines que valen cero y pueden ser cualquier letra.

## Puntuar una jugada

La puntuación de una jugada es la suma de todas las palabras que forma: la palabra principal en la línea de juego, más una palabra perpendicular por cada ficha nueva que toque fichas por encima o por debajo.

Para cada palabra:

1. Se suman los valores de sus letras. Una letra sobre un premio de **letra** cuenta doble o triple, pero solo si la ficha de esa casilla se ha colocado en este turno.
2. La suma se multiplica por cada premio de **palabra** que quede bajo una ficha colocada en este turno. Dos premios de palabra se multiplican entre sí: dos dobles hacen ×4 y dos triples ×9.
3. Los comodines cuentan cero, sea cual sea la letra que representen, aunque un comodín sobre un premio de palabra sigue multiplicando la palabra.

Por último, una jugada que coloca las siete fichas del atril gana **50 puntos extra**, el bonus que los jugadores llaman bingo.

:::note Ejemplos resueltos, sacados de los tests
`QI` sobre una doble palabra: (10 + 1) × 2 = **22**. `RETAINs` de salida con un comodín en la S, las siete fichas: **64** con el bonus de 50 puntos. Una palabra cruzada `.I` formada por una sola ficha nueva junto a otra que ya estaba: **11**.
:::

La interfaz enseña esta cuenta para cada jugada: la pestaña Puntos desglosa cada palabra letra a letra, con el premio que hay bajo cada ficha.

:::shot breakdown.webp | La puntuación de una jugada, desglosada palabra a palabra y ficha a ficha.
:::

## Una partida, de la bolsa a la última ficha

El motor sigue las reglas de torneo de NASPA.

- **Inicio.** La bolsa se baraja con una semilla y cada jugador roba siete fichas. La misma semilla da siempre la misma bolsa, y eso es lo que permite repetir los experimentos.
- **Un turno** es una de tres cosas: colocar fichas, cambiar parte del atril o pasar. Después de colocar, el jugador roba hasta volver a tener siete.
- **Los cambios** solo se permiten mientras queden al menos siete fichas en la bolsa.
- **La partida termina** de una de dos formas:
  - Un jugador vacía su atril con la bolsa vacía. Ese jugador suma el **doble** del valor de las fichas que le quedan al rival.
  - **Seis turnos seguidos sin puntuar** (pases, cambios o jugadas de cero puntos). Cada jugador resta entonces el valor de su propio atril.

La diferencia entre los dos marcadores finales es el **spread**. Casi todo este manual da dos cifras por experimento: el porcentaje de partidas ganadas y el spread medio por partida.

## Las semillas, bien hechas

Cada partida de una tanda necesita su propia semilla. La primera versión usaba `seed + g` para la partida `g`, de modo que `--seed 1` y `--seed 2` compartían 199 de sus 200 partidas, y dos ejecuciones «independientes» devolvían casi siempre el mismo resultado.

Ahora cada semilla pasa por SplitMix64, una función de mezcla, y las tandas son independientes. Con `--per-game-seeds` una partida depende solo de la semilla de la ejecución, del reparto y del asiento, así que una ejecución que se para y se reanuda vuelve a jugar exactamente las mismas partidas.

## Los formatos que habla el motor

| Formato | Qué es | Para qué se usa |
|---|---|---|
| **GCG** | el registro de partida en texto de Quackle y Woogles: `>Ana: AEINRST 8D RETINAS +72 72` | leer las partidas humanas, guardar las tuyas desde la interfaz |
| **CGP** | una línea que describe una posición: tablero, atriles, marcador | enviar posiciones a Macondo para comparar |
| **KWG** | diccionario binario | compartir listas de palabras con wolges y Macondo |
| **KLV / KLV2 / CSV** | tablas de leaves | leer valores de leave publicados y propios |

El parser de GCG lee jugadores, jugadas, cambios, pases, jugadas retiradas (`--`), bonus por desafío, puntos del atril final y penalizaciones por tiempo. Reproduce la partida en un tablero y comprueba que cada puntuación y cada total acumulado coincidan con el fichero, lo que además es una segunda comprobación del código de puntuación contra miles de partidas reales.

## El primer bot: greedy

`greedy` juega la jugada con más puntos. Si no hay ninguna colocación posible cambia el atril entero cuando la bolsa lo permite, y si no, pasa. Es un jugador flojo y muy útil: es la forma más rápida de ejercitar el generador, la puntuación y el bucle de partida en millones de posiciones, y es el [nivel](levels) Fácil.

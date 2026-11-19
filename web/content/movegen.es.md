title: El generador de jugadas
nav: Generación de jugadas
blurb: Anchors, cross-checks y un recorrido por el autómata: todas las jugadas legales, 460.000 por segundo.
lead: Anchors, cross-checks y un recorrido por el autómata. Cómo lista el motor todas las jugadas legales de una posición y cómo se demostró que lista exactamente las correctas.
---
## El trabajo

Dados un tablero y un atril, devolver todas las colocaciones legales: cada palabra que se puede poner en una fila o una columna, tocando lo que ya hay, usando solo fichas del atril y de forma que todas las palabras perpendiculares que se formen sean también válidas. Una posición típica de medio juego tiene unas 500.

Todo lo demás del motor llama a este código sin parar. Una sola jugada simulada necesita una generación para la respuesta del rival y otra para la continuación, miles de veces por segundo. Así que tiene que ser exacto y rápido a la vez. Kingbble sigue el algoritmo que Steven Gordon publicó en 1994, sobre el [GADDAG](lexicon).

## Anchors

Toda palabra nueva tiene que conectar con las fichas del tablero (o, en la primera jugada, cubrir la casilla central). Por eso la búsqueda no parte de todas las casillas. Parte de los **anchors**: las casillas vacías pegadas a una ficha.

:::board 5 4 | Fichas en el tablero y, marcados con un aro, los anchors de esta posición: las únicas casillas desde las que se puede empezar a construir una jugada.
..+++++..
.+KANES+.
..+E+++..
..+R+....
...+.....
:::

En un tablero vacío hay un solo anchor, el centro.

## Cross-checks

Una ficha colocada en una fila pasa también a formar parte de una columna. Si hay fichas justo encima o debajo de la casilla, la letra que se ponga ahí tiene que completar una palabra vertical válida.

Para cada casilla vacía y cada dirección, el generador calcula de antemano un **cross-check**: una máscara de 26 bits con un bit por cada letra que sería válida ahí, junto con los puntos de la palabra perpendicular que se formaría. Las casillas sin nada encima ni debajo admiten las 26 letras.

Con las máscaras hechas, comprobar si una letra puede ir en una casilla mientras se construye una palabra es mirar un bit, no consultar el diccionario. Después de cada jugada solo se recalculan las máscaras de las casillas cercanas a las fichas nuevas.

## El recorrido

Desde cada anchor, el generador recorre a la vez el autómata y el tablero:

1. Empieza en el anchor y avanza hacia la **izquierda** (o hacia arriba, en las jugadas verticales). En cada casilla usa la ficha que ya está en el tablero o coloca una del atril, y toma la transición correspondiente del GADDAG. Una letra del atril solo se prueba si el cross-check de esa casilla la permite.
2. En cualquier punto donde el GADDAG tenga una transición de separador puede **dar la vuelta**: salta a la casilla a la derecha del anchor y sigue hacia la derecha del mismo modo.
3. Cada vez que el autómata dice «aquí termina una palabra» y la casilla siguiente del tablero está vacía o fuera del borde, se ha encontrado una jugada legal.

Como el GADDAG guarda cada palabra una vez por letra, cada palabra se encuentra justo en el anchor que le corresponde, y la búsqueda nunca explora un prefijo que no se pueda completar.

## Comodines, duplicados y la primera jugada

- **Comodines.** Un comodín puede ser cualquier letra. Durante el recorrido, si el atril tiene un comodín el generador lo prueba como cada una de las letras que permiten el estado actual y el cross-check, y apunta la ficha como comodín, que vale cero.
- **Duplicados.** Una jugada de una sola ficha forma «palabra» en horizontal y en vertical, y saldría dos veces. Se devuelve una sola.
- **La salida.** En un tablero vacío las salidas horizontales y verticales son simétricas. El motor lista por defecto una sola dirección, como hace Macondo; la interfaz pide las dos, para que puedas salir en vertical si quieres.

## Cambios y pases

Cambiar fichas es legal mientras queden al menos siete en la bolsa. El motor puede listar como cambio cada subconjunto distinto del atril. Pasar siempre es posible.

## Velocidad

En self-play con el bot greedy, el generador produce unas **460.000 jugadas por segundo** en un núcleo de un portátil corriente, con unas 500 jugadas legales por turno y una media de unos 396 puntos por jugador y partida.

## Cómo se comprobó

Dos comprobaciones independientes, las dos automáticas.

**Contra la fuerza bruta.** Sobre un léxico pequeño, un segundo generador ingenuo prueba todas las combinaciones de fichas en todas las líneas. Los dos dan exactamente las mismas jugadas.

**Contra Macondo.** Esta es la que importa.

1. `kingbble_positions` saca posiciones de partidas de self-play y escribe todas las jugadas que Kingbble encuentra en cada una.
2. Cada posición se exporta en CGP, el formato de texto que lee Macondo.
3. Un programa pequeño en Go le pide a Macondo todas sus jugadas en esa misma posición.
4. Un script compara jugada a jugada y puntuación a puntuación.

:::bars min=0 max=100 unit=% caption=Coincidencia exacta con Macondo. El generador se comparó en 10.000 posiciones con la semilla 1 y otras 10.000 con la semilla 2.
Generador de jugadas | 100 | | | 20.000 de 20.000 posiciones
:::

**Cero diferencias en 20.000 posiciones.** Los totales brutos no coinciden (5,44 millones de jugadas frente a 5,19 millones) solo porque, con el tablero vacío, Macondo lista la salida en una única dirección; se cuentan como la misma jugada.

Para repetirlo:

```
python3 scripts/compare_macondo.py --count 10000 --seed 1
```

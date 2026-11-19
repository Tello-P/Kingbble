title: El diccionario: GADDAG y KWG
nav: El diccionario
blurb: Cómo 168.551 palabras se convierten en un autómata que se puede leer desde cualquier letra.
lead: Cómo una lista de palabras se convierte en un autómata en el que se puede entrar por cualquier letra y leer en las dos direcciones, que es justo lo que necesita este juego.
---
## Por qué no basta con una lista

Un diccionario responde a una pregunta: ¿es válida esta palabra? El juego hace otra más difícil: *¿qué palabras puedo construir alrededor de las fichas que ya están puestas?* Una ficha del tablero puede acabar siendo la primera letra de la palabra nueva, la última o cualquiera de en medio.

Un trie normal (un árbol con una rama por letra, leído desde la primera) solo ayuda cuando la palabra empieza en esa ficha. Para todo lo demás habría que probar cada casilla de inicio a la izquierda y ver si las letras cuadran. El GADDAG elimina ese tanteo.

## El truco: guardar todas las rotaciones

Para cada palabra, un GADDAG guarda un camino por cada una de sus letras. El camino empieza en esa letra, lee **hacia atrás** hasta el principio de la palabra, cruza un separador y luego lee **hacia delante** hasta el final.

Por ejemplo, `CASA` se guarda cuatro veces:

| Camino | Se lee así |
|---|---|
| `C^ASA` | empiezo en la C; a su izquierda no hay nada; luego A, S, A hacia la derecha |
| `AC^SA` | empiezo en la primera A; C a su izquierda; luego S, A hacia la derecha |
| `SAC^A` | empiezo en la S; A, C a su izquierda; luego A hacia la derecha |
| `ASAC` | empiezo en la última A; S, A, C a su izquierda; la palabra acaba ahí |

:::tiles CASA
:::

Así, desde cualquier ficha del tablero, o desde cualquier casilla vacía donde haya que poner una, el generador puede extender hacia la izquierda letra a letra y, cuando quiera, dar la vuelta y extender hacia la derecha. Cada paso es una sola transición del autómata. Nunca hay que adivinar una posición.

## Que no ocupe demasiado

Guardado sin más, eso son tantos caminos como letras hay en toda la lista. Dos cosas lo mantienen manejable.

**Minimización.** Muchos caminos acaban igual: todas las palabras que terminan en `-ING` comparten esos últimos estados. El autómata se construye ya minimizado con el algoritmo incremental de Daciuk, que fusiona los sufijos iguales a medida que se añaden las palabras en orden, de modo que el árbol completo sin minimizar nunca llega a existir en memoria.

**Un array plano.** En memoria el autómata es un único array: cada nodo apunta a un rango contiguo de aristas, y cada arista guarda una letra y el índice del nodo siguiente. No hay punteros repartidos por la memoria, así que recorrerlo no se sale de la caché del procesador.

Con la lista ENABLE salen **401.729 nodos y 836.835 aristas para 168.551 palabras, construidos en unos 1,5 segundos**.

:::note Nodo y arista
Un *nodo* es un estado: «hasta aquí he leído estas letras». Una *arista* es una transición con una letra. Menos nodos significa menos memoria y, lo que aquí importa más, más autómata en caché a la vez.
:::

## KWG: hablar el mismo formato que los demás

KWG es el formato binario de diccionario que usan wolges y Macondo. Es un array de enteros de 32 bits, cada uno una arista: su letra, si ahí termina una palabra, si es la última de sus hermanas y el índice de sus hijas. Un fichero contiene dos autómatas, un DAWG para comprobar palabras y un GADDAG para generar jugadas.

Kingbble lee y escribe KWG y lo convierte a su propio GADDAG. Se comprobó en los dos sentidos:

- El `ENABLE.kwg` que genera wolges, la herramienta de referencia, da **las mismas 168.551 palabras** leído por Kingbble.
- Macondo carga sin problemas el KWG que escribe Kingbble.

La herramienta es `kingbble_buildkwg`. Un fixture pequeño de 2.576 palabras (`lexica/fixtures/enable_small.kwg`, 40 KB) permite pasar los tests sin ninguna lista con licencia.

## Anagramas: el WordMap

Junto al autómata hay un segundo índice, más simple. Su clave son las letras de una palabra en orden alfabético (`RETAINS` pasa a ser `AEINRST`) y su valor es la lista de palabras que tienen exactamente esas letras.

Dadas unas fichas, comodines incluidos, devuelve las palabras que las usan todas, o todas las que usan al menos dos. Sirve para valorar atriles y cambios y para encontrar bingos rápido, y alimenta varias features del [modelo humano](human-model), como cuántas palabras de siete letras puede formar un atril. Coincide con la fuerza bruta en 200 atriles aleatorios.

## Podar el diccionario para una posición

Cerca del final de la partida casi todo el diccionario es peso muerto: una palabra que necesita una `Z` no se puede jugar si ningún jugador tiene `Z` y no hay ninguna al alcance en el tablero. Antes de una búsqueda de [final](endgame) el motor construye un autómata podado solo con las palabras que podrían formar las fichas que siguen en juego. La búsqueda recorre entonces una estructura mucho más pequeña.

## Qué listas de palabras

| Lista | Palabras | Licencia | Para qué se usa |
|---|---|---|---|
| ENABLE | 168.551 | dominio público | desarrollo, tests, la demo web y la versión pública |
| CSW24 | — | con licencia (Collins) | los experimentos; nunca se distribuye |
| NWL23 | — | con licencia (NASPA) | comparaciones; nunca se distribuye |

Las listas oficiales y sus tablas de leaves tienen licencia y no están en el repositorio. `scripts/fetch_lexicon.py` construye una lista normalizada (mayúsculas, solo de la A a la Z, sin duplicados) a partir de un fichero que ya tengas, o descarga ENABLE.

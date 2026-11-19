title: Frente a Macondo y Quackle
nav: Macondo y Quackle
blurb: Los dos motores de referencia: primero como regla de medir, después como rivales.
lead: Macondo y Quackle son los dos motores de referencia de este juego. Kingbble los usa dos veces: primero como regla, para demostrar que sus respuestas son correctas, y después como rivales.
---
## Dos referencias

**Quackle** es el programa de análisis clásico, en C++, que los jugadores de torneo usan desde hace dos décadas. Su ajuste más fuerte, Championship Player, simula; el más flojo, Speedy Player, juega de forma estática. Introdujo RangeFinder, el baseline con el que se mide la [inferencia](inference) de Kingbble.

**Macondo** es la referencia actual, en Go, el motor que hay detrás de los bots de la plataforma Woogles. Tiene la simulación más fuerte y los solvers de finales y pre-finales más completos que hay disponibles en abierto, y lee los mismos formatos de léxico y de leaves que Kingbble.

Ninguno va incluido, enlazado ni copiado. Kingbble habla con ellos desde fuera: un programa auxiliar pequeño para cada uno, que se lanza como proceso aparte.

## Como regla

Si dos programas implementan las mismas reglas y los dos dicen ser exactos, sus respuestas tienen que ser idénticas. Eso convierte a un motor de referencia en el test más fuerte posible.

| Qué | Cómo se compara | Resultado |
|---|---|---|
| Diccionario | el KWG que escribe wolges leído por Kingbble; el KWG que escribe Kingbble cargado por Macondo | las mismas 168.551 palabras |
| Generador de jugadas | todas las jugadas y puntuaciones en 20.000 posiciones de self-play, exportadas en CGP | 0 diferencias |
| Final | el valor exacto de 34 posiciones que terminan los dos solvers | 0 diferencias |
| Pre-final, 1 ficha | victorias y spread de las mejores jugadas sobre todas las ordenaciones | 0 diferencias en 28 jugadas |
| Pre-final, 2 fichas | lo mismo | 1 diferencia en 24 jugadas, en investigación |

Estas comparaciones son scripts del repositorio (`compare_macondo.py`, `compare_endgame.py`, `compare_preendgame.py`) con un tope de tiempo configurable y un reparto dinámico de las posiciones entre trabajadores, para que una posición lenta no frene a las demás.

## Como rival: Macondo

El match: `sim+peg+eg` contra el bot de simulación de Macondo, en CSW24, a **igual tiempo por jugada e iguales hilos**. Cinco segundos por jugada, dos núcleos, 300 partidas emparejadas.

:::bars min=38 max=62 ref=50 unit=% caption=Kingbble contra el bot de simulación de Macondo a 5 segundos por jugada, 300 partidas emparejadas, intervalo del 95 %.
Kingbble contra Macondo | 47.0 | 41.7 | 52.3 | spread +4,8 (−6,3 a +15,9)
:::

**Un 47,0 % de las partidas, con un intervalo de 41,7 a 52,3, y un spread medio de +4,8 puntos a favor de Kingbble** (−6,3 a +15,9). Un empate estadístico: el intervalo incluye el 50 %, y el spread cae del lado de Kingbble.

El detalle interesante está en las partidas ajustadas. De las que se decidieron por poco margen, Kingbble **ganó 17 y perdió 27**. Hace tantos puntos como Macondo a lo largo de la partida y después pierde más finales apretados. Eso señala a los últimos turnos: el [pre-final](preendgame), donde el solver exacto todavía es lento con dos fichas en la bolsa y donde queda una diferencia sin explicar.

De eso trató la línea de trabajo siguiente: la gestión del tiempo en la simulación, gastar todo el presupuesto en más candidatas, valorar los leaves de los dos jugadores al final de un playout, y un *duelo de posiciones*, una herramienta que toma las posiciones en las que los dos motores eligieron jugadas distintas y las juzga, de forma exacta cuando la bolsa es pequeña.

:::note Por qué la comparación justa es a igual tiempo
Dar a los dos motores el mismo número de iteraciones compararía implementaciones. Darles los mismos segundos en los mismos núcleos compara lo que recibiría una persona sentada delante de cada uno.
:::

## Como rival: Quackle

Contra Quackle el cuadro está incompleto, y se cuenta como está.

| Rival | Partidas | Resultado | Estado |
|---|---|---|---|
| Speedy Player (el ajuste flojo) | 10 | 80 % ± 25 | una prueba de humo, no un resultado |
| Championship Player | 73 de 300 | 82 % en el momento de pararla | **todavía no se da por buena** |

La ejecución contra Championship se paró en 73 partidas con Kingbble en el 82 %. Es demasiado bueno para creérselo sin más contra el ajuste más fuerte de Quackle, así que antes de que cuente hay que comprobar en el banco de pruebas que Quackle está jugando de verdad a nivel Championship. Por el camino ya se encontraron y arreglaron dos fallos en ese programa auxiliar: durante un tiempo le daba a Quackle un tablero sin casillas de premio, y después uno de tamaño equivocado.

El criterio de salida escrito para este match es modesto: un porcentaje de victorias de al menos 45 contra Championship Player. Hasta que se verifique el nivel, no se afirma ninguna cifra.

## Qué dicen las comparaciones

- En todo lo que tiene una respuesta correcta, jugadas, puntuaciones, finales, Kingbble da la respuesta de la referencia.
- En juego, a igual tiempo, está a la par del bot de simulación de la referencia dentro de la incertidumbre de 300 partidas.
- La distancia que queda está en los turnos de cierre, no en el medio juego ni en el cálculo bruto.

Todo esto lo consiguió un único motor escrito desde cero por una sola persona, que hizo sus experimentos en un ordenador personal a baja prioridad. La comparación no está para reclamar un puesto. Está para que los resultados sobre la inferencia se apoyen en un motor que se sabe sólido.

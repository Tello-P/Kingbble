title: Los datos: 8.738 partidas humanas
nav: Los datos
blurb: De dónde salen las partidas humanas, cómo se guardan y cómo se mantiene a los jugadores anónimos y separados.
lead: Un modelo de cómo juega la gente necesita gente jugando. De dónde salen las partidas, qué se guarda de cada turno, cómo se anonimiza a los jugadores y por qué la separación entre entrenamiento y test va por jugador.
---
## Lo que necesita un modelo de juego humano

Para aprender *qué jugada elige una persona con un atril dado*, cada ejemplo necesita tres cosas: la posición, el atril que tenía el jugador y la jugada que hizo. La primera y la última están en cualquier registro de partida. El atril es lo raro, porque en casi todos los registros solo se ven las fichas que se jugaron.

Woogles, una plataforma abierta para jugar en línea, registra el atril completo de quien mueve en cada turno. Eso es lo que hace posible este conjunto de datos.

## De un registro de partida a una fila

:::flow
Descargar | partidas terminadas en GCG, con sus metadatos
Parsear | reproducir cada partida y comprobar cada puntuación
Guardar | una fila por turno, en Parquet
Separar | entrenamiento, validación y test, por jugador
Features | cada opción legal de cada turno, descrita con números
:::

**Descargar.** Un rastreador parte de unos pocos jugadores semilla, lista las partidas recientes de cada uno, guarda cada partida clásica terminada que aún no tenga y encola a los rivales humanos que va encontrando. Las peticiones van espaciadas, y volver a lanzarlo solo trae lo que falta.

**Parsear.** Cada partida se reproduce en un tablero con el [parser de GCG](scoring). Una partida cuyas puntuaciones no cuadran se descarta, así que el almacén solo tiene partidas que el motor reproduce jugada a jugada.

**Guardar.** Los turnos se escriben en ficheros Parquet, particionados por léxico, y se consultan con DuckDB. Cada fila tiene la posición antes de la jugada, el atril de quien mueve, la jugada, los puntos, el estado de la bolsa, si quien mueve es un programa y su rating.

## Las cifras

:::stats
8.738 | partidas humanas en el léxico CSW24
230.000+ | turnos humanos con el atril de quien mueve, tras la limpieza
3 | particiones: entrenamiento, validación y test, sin ningún jugador en dos de ellas
:::

En la plataforma también juegan bots. Sus turnos se guardan marcados: quedan fuera del modelo humano y sirven para ajustar un *modelo bot* aparte, el rival que supone la inferencia cuando el oponente es un programa.

## Jugadores anónimos

El almacén no guarda nombres. Cada apodo se sustituye por un **seudónimo**: un hash SHA-256 con sal, recortado. El mismo jugador recibe el mismo identificador en cada construcción, así que sus turnos se pueden seguir agrupando, pero el identificador no se puede convertir de vuelta en un nombre. La sal es un secreto que vive fuera del repositorio.

## Por qué la separación va por jugador

La forma habitual de reservar datos de test es apartar al azar una décima parte de las filas. Aquí sería un error. Las costumbres de un jugador se repiten en sus partidas: las mismas palabras favoritas, los mismos puntos ciegos. Un modelo probado con otros turnos *de la misma gente* con la que entrenó parece mejor de lo que es.

Por eso la separación es por jugador. Un hash del seudónimo manda a cada jugador, con todos sus turnos, a una sola partición: entrenamiento, validación o test, con un 80 %, un 10 % y un 10 %. La asignación no cambia cuando se añaden más partidas. Todas las cifras sobre el modelo humano de este manual están medidas en **jugadores que el modelo nunca vio**.

## Un bug que merece contarse: la fuga en la familiaridad

Un grupo de features describe lo familiar que es una palabra: entre otras cosas, cuántas veces se jugó en las partidas de entrenamiento. La primera versión contaba sobre todas las partidas de entrenamiento, *incluido el turno que se quería predecir*. En entrenamiento, la jugada elegida llevaba siempre una cuenta más que las demás opciones. El modelo aprendió que «la jugada elegida tiene el contador más alto», una regla que no existe fuera del conjunto de entrenamiento.

El síntoma fue que quitar el grupo entero *mejoraba* la validación en 0,058. El arreglo fue contar con partidas disjuntas de las que se ajustan. Con la fuga cerrada el grupo ayuda, en unos 0,06.

Es un ejemplo pequeño de para qué sirve el conjunto de validación, y de una norma que se siguió en todo el proyecto: cuando un número sale raro, averiguar por qué antes de usarlo.

## Las features en disco

Para cada turno el motor genera todas las opciones legales y describe cada una con un vector de números: las [features](human-model). Con cientos de opciones por turno y cientos de miles de turnos, esta es la parte voluminosa: decenas de gigabytes en los niveles de datos grandes. Para que sea manejable, por turno solo se guardan las mejores opciones por equity (más la que se eligió de verdad), las filas se almacenan como flotantes de 16 bits y los ficheros se mapean en memoria, de modo que el ajuste nunca los carga enteros.

## Dos fuentes, un formato

Las partidas humanas son una fuente. La otra es el **self-play**: partidas que el motor juega contra sí mismo, escritas en el mismo formato GCG y guardadas igual. El self-play alimenta el modelo bot y los [valores de leave](equity), y se puede mezclar con datos humanos en el [menú de entrenamiento](training). En self-play la separación va por partida, porque todas tienen los mismos dos «jugadores».

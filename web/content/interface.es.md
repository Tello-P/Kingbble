title: La interfaz
nav: La interfaz
blurb: Modos, controles, ayudas, repaso y ajustes, uno por uno.
lead: La interfaz es una página que no sabe nada del juego. Dibuja lo que el motor le dice y le devuelve lo que haces. Este capítulo recorre todo lo que hay en la pantalla.
---
## La pantalla

:::shot game-es.webp | El tablero a la izquierda, el panel a la derecha, el atril y las acciones bajo el tablero.
:::

- **La barra superior**: partida nueva, descargar la partida y ajustes.
- **Las barras de jugador**: nombre, puntos y reloj de cada uno, con el que mueve marcado.
- **El tablero**: quince por quince, con las casillas de premio en color y, si las quieres, coordenadas.
- **El atril**: tus siete fichas, con las acciones al lado: jugar, recoger, mezclar, cambiar, pasar.
- **El panel**: pestañas para las mejores jugadas, la lista de jugadas, las fichas no vistas y la ayuda de puntuación.

En el móvil el panel pasa debajo del tablero y el atril queda al alcance del pulgar.

:::shot mobile.webp | La misma página en un móvil.
:::

## Tres modos

| Modo | Para qué sirve |
|---|---|
| **Tú contra Kingbble** | una partida contra el motor con el nivel y el tiempo para pensar que elijas |
| **Bot contra bot** | ver jugar a dos bots, con pausa, jugada siguiente y control de velocidad |
| **Análisis** | dos jugadores en una pantalla, todos los atriles a la vista, con las sugerencias del motor para quien mueve |

Al empezar una partida eliges el rival, su tiempo para pensar, quién sale (tú, el bot o al azar), la lista de palabras y, si quieres, una **semilla**. La misma semilla reparte las mismas fichas, así que una partida se puede repetir o compartir.

## Colocar fichas

Hay tres formas y puedes dejarlas todas activas o elegir una en los ajustes.

- **Arrastra** una ficha del atril a una casilla. Toca una ficha colocada para recogerla.
- **Clic y teclado**: haz clic en una casilla para poner ahí el cursor y escribe las letras. Otro clic en la misma casilla, o Espacio, cambia entre horizontal y vertical. El cursor salta las fichas que ya están en el tablero.
- **Haz clic en una jugada sugerida** del panel para que se coloque sola.

Un **comodín** pregunta qué letra representa. Mayús mientras escribes una letra usa un comodín a propósito, aunque tengas la ficha de verdad.

## Teclado

| Tecla | Qué hace |
|---|---|
| Letras | coloca esa ficha en el cursor |
| Mayús + letra | coloca un comodín como esa letra |
| Flechas | mueven el cursor por el tablero |
| Espacio | cambia la dirección, horizontal o vertical |
| Retroceso | recoge la última ficha colocada |
| Intro | juega la jugada |
| Esc | recoge todas las fichas; en el repaso, vuelve a la partida |
| Flechas izquierda y derecha | sin cursor en el tablero, recorren las jugadas de la partida |

## Ayudas

Cada una se puede apagar en los ajustes, para una partida sin ayuda.

- **Panel de mejores jugadas.** Las jugadas para tu atril ordenadas por equity estática, con sus puntos y las fichas que conservan. Es el nivel Medio pensando en voz alta, no la simulación.
- **Vista previa de puntos.** Mientras hay fichas en el tablero, los puntos que haría la jugada y si es válida.
- **Fichas no vistas.** Las fichas que no están ni en el tablero ni en tu atril: la bolsa más el otro atril.
- **Coordenadas.** Letras y números en los bordes del tablero, como en la notación de jugadas: `8H` es la fila 8, columna H.

## Cómo puntuó una jugada

Haz clic en una jugada de la lista para ver de dónde salieron sus puntos: cada palabra formada, cada letra con su multiplicador, los multiplicadores de palabra y el premio por usar las siete fichas.

:::shot breakdown.webp | El desglose de una jugada: cada palabra, letra a letra.
:::

La pestaña **Puntuación** explica las casillas de premio, las reglas de puntuación en seis líneas con un ejemplo de cada una, y el valor de todas las fichas. Las reglas que hay detrás están en [puntuación](scoring).

## Repasar una partida

Durante la partida o al terminarla, recórrela jugada a jugada. El tablero, los atriles y los marcadores enseñan la posición después de cada jugada, y la lista marca dónde estás.

:::shot review.webp | Repaso: el tablero como estaba después de una jugada anterior.
:::

- **Primera, anterior, siguiente, actual** recorren la partida.
- **Jugar desde aquí** rebobina la partida hasta esa posición y sigue desde ella. Es una forma de probar la otra jugada.
- **Deshacer** retira tu última jugada y la respuesta del bot.
- **Descargar partida (GCG)** guarda la partida en el formato estándar que leen los programas de análisis.

## Ajustes

:::shot settings.webp | El diálogo de ajustes.
:::

| Ajuste | Opciones |
|---|---|
| Idioma | inglés, español |
| Aspecto | automático (sigue al sistema), claro, oscuro |
| Colocar fichas | arrastrar, clic y teclado; solo arrastrar; solo clic y teclado |
| Ayudas | panel de mejores jugadas, vista previa de puntos, fichas no vistas, coordenadas |
| Sonido | sí o no |
| Reloj de partida | apagado, o un tiempo para cada jugador; agotarlo termina la partida |
| Animación de entrada y su sonido | sí o no; calma, caja de música, respiración o ninguno |
| Leaves (app de escritorio) | la tabla incluida o una entrenada aquí: mira [entrenamiento](training) |

Los ajustes se guardan en tu dispositivo entre visitas. Cambiar el idioma cambia la interfaz; la lista de palabras sigue siendo la que elegiste.

## Accesibilidad

- Todo se puede hacer con el teclado: el tablero es una rejilla que se recorre con las flechas, el atril y las pestañas siguen los patrones habituales, y el foco siempre se ve.
- Los lectores de pantalla oyen cada casilla como su coordenada, su premio y su ficha, y cada ficha como su letra y sus puntos. Las jugadas y los cambios de turno se anuncian.
- Los colores cumplen el contraste necesario para el texto en los dos temas, y las casillas de premio llevan etiqueta además de color.
- Con «reducir movimiento» activado en el sistema, las animaciones y la entrada se apagan.
- Las zonas táctiles son lo bastante grandes para los dedos, y la página funciona desde un móvil estrecho hasta un monitor ancho.

## Mensajes

El motor se explica con palabras. Si rechaza una jugada, la interfaz dice por qué: fichas que no están en una fila o columna, la primera jugada que no cubre el centro, un hueco, una jugada sin conectar con el resto o una palabra que no está en la lista. Un cambio con menos de siete fichas en la bolsa se rechaza con ese motivo.

## Cómo habla con el motor

La página envía una orden por línea y recibe una línea de JSON:

```
new 7 human sim+eg 0 0
check 7,7,C 7,8,A 7,9,R 7,10,B
{"ok":true,"score":16,"move":{"kind":"place","text":"8H CARB (16)",...}}
```

En la app de escritorio un pequeño servicio local pasa esas líneas al motor nativo. En la demo web las mismas líneas van al motor compilado a WebAssembly, en un Web Worker. La página no nota la diferencia, y por eso la demo y la app se ven iguales. Más en [compilar desde el código](build).

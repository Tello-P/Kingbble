title: Preguntas
nav: Preguntas
blurb: Respuestas cortas sobre la demo, la app, el motor y las cifras.
lead: Respuestas cortas a lo que la gente pregunta primero, cada una con el capítulo que da la larga.
---
## Sobre jugar

### ¿Es gratis?

Sí. Kingbble es software libre bajo la GPL-3.0: puedes usarlo, estudiarlo, cambiarlo y compartirlo, y todo lo que se construya a partir de él tiene que seguir bajo la misma licencia. No hay cuentas, ni anuncios, ni nada que pagar.

### ¿Por qué me ha rechazado una palabra que sé que vale?

La demo y la app pública usan ENABLE, una lista de palabras inglesas de dominio público. Se parece a las listas de torneo y no es igual: faltan algunas palabras que allí valen, y unas pocas de ENABLE faltan allí. Las listas de torneo tienen licencia y no se pueden incluir. Mira [el diccionario](lexicon).

### ¿Por qué Extremo está bloqueado en el navegador?

Usa todos los núcleos del procesador y tiempos para pensar más largos, y una pestaña del navegador tiene un hilo para el motor. Está en la [app de escritorio](download). Los otros cuatro niveles son iguales en los dos sitios.

### ¿Cuánto juega?

A igual tiempo, el motor completo está a la par del bot de simulación de Macondo dentro de la incertidumbre de 300 partidas: un 47,0 % de las partidas (41,7–52,3) con un spread de +4,8 puntos a su favor. Eso se midió con una lista de torneo y con el motor de escritorio. La demo del navegador va con un hilo, así que en los mismos segundos calcula menos. Mira [referencias](references).

### ¿El bot hace trampas? ¿Ve mis fichas?

No. Un bot recibe lo que vería un jugador: el tablero, su propio atril, los marcadores y el número de fichas de la bolsa. El nivel Extremo *estima* tu atril a partir de tus jugadas, que es lo que hace también un rival humano fuerte, y puede equivocarse. Solo en el final, con la bolsa vacía, conoce tu atril exacto, igual que tú conoces el suyo, porque es todo lo que no está en el tablero. Mira [inferencia del atril](inference).

### ¿Puedo jugar contra otra persona?

En la misma pantalla, sí: el modo Análisis son dos jugadores con los dos atriles a la vista. No hay juego en línea ni está previsto.

### ¿Puedo jugar en español?

La interfaz está en español y en inglés. Las palabras son inglesas: no hay una lista de palabras en español que se pueda distribuir libremente, y el motor todavía no tiene el juego de fichas español. Mira la [hoja de ruta](roadmap).

### ¿Funciona en el móvil?

Sí. La demo funciona en cualquier navegador móvil actual. En un procesador pequeño, elige Medio o un tiempo para pensar corto.

### ¿Funciona sin conexión?

La app de escritorio, siempre. La demo, una vez cargada, sigue funcionando sin conexión mientras la pestaña esté abierta, porque no se le pide nada a ningún servidor.

### ¿Se guardan mis partidas?

No. Los ajustes se guardan en tu dispositivo; las partidas, no. Descarga la partida en GCG si quieres conservarla.

## Sobre el motor

### ¿En qué se diferencia de otros motores?

En tres cosas. Está escrito desde cero, sin compartir código con los motores de referencia y coincidiendo con ellos en todas las posiciones comparadas. Estima el atril del rival a lo largo de varios turnos con un modelo de cómo juegan **las personas**, donde el método clásico supone un rival casi perfecto. Y cada afirmación viene con su número de partidas y su intervalo, también las que fallaron.

### ¿De verdad ayuda la inferencia?

Frente al método clásico, RangeFinder, sí: un 58,5 % de las partidas (51,7–65,3) y una predicción mucho mejor de las fichas que se queda el rival (5,74 frente a 12,50). Frente a no inferir nada, la ganancia en predicción con personas es pequeña (5,74 frente a 5,87), y su efecto en partidas es una de las cuatro ablaciones finales, todavía por ejecutar. Mira [resultados](results).

### ¿Es una red neuronal?

En su mayor parte, no. El generador de jugadas, la puntuación, la simulación y el final son algoritmos clásicos. Hay dos piezas aprendidas: los valores de los leaves, una tabla; y el modelo de jugadas, un logit o una red pequeña con unas pocas decenas de entradas. Ninguna pieza del motor es un modelo grande, y nada necesita tarjeta gráfica.

### ¿Por qué no simular más y ya está?

Se probó. Dieciséis veces más iteraciones no ganaron más partidas, y más candidatas con pocas iteraciones perdieron. El límite está en lo que la simulación supone del rival, no en cuántas veces se ejecuta. Mira [lo que no funcionó](lessons).

### ¿Por qué C++?

El generador de jugadas se ejecuta millones de veces en una simulación y el final busca millones de posiciones. El motor es C++20 sin dependencias; Python se usa alrededor, para los datos, el entrenamiento y el análisis.

### ¿Por qué «Kingbble»?

Es el nombre propio del proyecto, con su logo y sus colores. No está afiliado a ningún fabricante de juegos de mesa.

## Sobre las cifras

### ¿Por qué todas las cifras llevan un intervalo?

Porque en este juego la suerte del robo es grande. Dos bots idénticos que jueguen 100 partidas rara vez acabarán 50–50. Un resultado sin su intervalo no se distingue de la suerte. Mira [cómo se mide](method).

### ¿Qué quiere decir «emparejadas»?

Cada reparto se juega dos veces cambiando los asientos. La suerte que tuviera el reparto le toca una vez a cada bot y se anula, así que hacen falta muchas menos partidas para ver una diferencia.

### ¿Por qué algunos resultados no se dan por buenos?

Una ejecución contra el ajuste más fuerte de Quackle se paró con Kingbble en el 82 %. Es demasiado bueno para creérselo sin comprobar que el rival jugaba a plena fuerza, así que no cuenta. Una cifra que favorece se comprueba más, no menos.

### ¿Puedo reproducir un resultado?

Sí: cada tabla es un comando en `docs/experiments.md`, con semillas fijas y un manifiesto del código y los ficheros exactos que se usaron. Esas ejecuciones necesitan la lista de torneo, que tienes que conseguir por tu cuenta. Mira [compilar desde el código](build).

## Sobre el proyecto

### ¿Quién lo ha hecho?

Es un proyecto personal de una sola persona. Mira [acerca de](about).

### ¿Puedo contribuir?

Sí. El repositorio admite issues y pull requests; el flujo es issue, rama, pull request, tests en verde, merge. Hay buenos puntos de partida en la [hoja de ruta](roadmap).

### ¿Puedo usar el motor en mi programa?

En los términos de la GPL-3.0, sí: si distribuyes un programa que lo incluye, ese programa tiene que estar también bajo la GPL-3.0, con su código.

### He encontrado un fallo

Abre una issue en el [repositorio](repo) con la posición, como fichero GCG o con la semilla de la partida, y lo que esperabas.

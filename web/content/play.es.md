title: Jugar en el navegador
nav: Jugar en el navegador
blurb: El motor real, compilado a WebAssembly, en cuatro niveles. Sin instalar nada.
lead: La demo es el motor real, el mismo C++ compilado a WebAssembly, funcionando dentro de esta pestaña. No se instala nada y no se envía nada a un servidor: el bot piensa en tu dispositivo.
---
:::raw
<div class="launch">
  <a class="tilebtn" href="../demo/index.html?lang=es"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M5 3.2v9.6L12.6 8z"/></svg><span>Jugar la demo</span></a>
  <small>Unos 4 MB la primera vez (el motor, la lista de palabras y los leaves). Después carga desde la caché del navegador.</small>
</div>
:::

## Qué es la demo

La misma interfaz que la app de escritorio y el mismo motor debajo: el generador de jugadas, la puntuación, la equity estática con leaves, la simulación Monte Carlo y el solver exacto de finales. Cuando haces una jugada, el motor la comprueba con la lista de palabras; cuando le toca al bot, piensa el tiempo que le has dado y juega.

Tiene tres modos:

- **Tú contra Kingbble.** Una partida contra el motor en el nivel que elijas.
- **Bot contra bot.** Dos bots juegan entre sí mientras miras, con pausa, paso a paso y velocidad.
- **Análisis.** Dos jugadores en una pantalla con todos los atriles a la vista y las mejores jugadas del motor.

:::shot game-es.webp | La demo es esta interfaz, funcionando en el navegador.
:::

## Los niveles

| Nivel | Qué hace | En la demo |
|---|---|---|
| **Fácil** | juega la jugada con más puntos | sí |
| **Medio** | equity estática: puntos más el valor de las fichas que se queda | sí |
| **Fuerte** | simulación Monte Carlo | sí, hasta 5 s por jugada |
| **Muy fuerte** | simulación más el final exacto | sí, hasta 5 s por jugada |
| **Extremo** | lo anterior más la inferencia del atril, con todos los núcleos | **bloqueado**: solo en la app de escritorio |

Cada uno se explica en [niveles](levels). Extremo aparece en la lista con un candado. Necesita hilos, más memoria y más tiempo de los que debería ocupar una pestaña del navegador, así que vive en la [app de escritorio](download).

## En qué se diferencia de la app de escritorio

| | Demo web | App de escritorio |
|---|---|---|
| Niveles | de Fácil a Muy fuerte | los cinco |
| Tiempo para pensar por jugada | de 0,5 a 5 segundos | hasta 60 segundos |
| Procesador | un hilo | todos los núcleos |
| Lista de palabras | ENABLE | ENABLE y tus propias listas |
| Leaves | la tabla incluida | la incluida, o entrenar la tuya |
| Entrenamiento | no | [entrenamiento de leaves](training) con gate |
| Se guarda entre visitas | solo los ajustes | los ajustes |
| Funciona sin conexión | después de la primera carga | siempre |

En el navegador el motor va con un solo hilo, así que en los mismos segundos hace menos simulaciones que la app de escritorio. Las jugadas siguen siendo legales, las puntuaciones exactas, y el final se sigue resolviendo, no estimando.

## La lista de palabras

La demo juega con **ENABLE**, una lista de dominio público de 168.551 palabras inglesas (se usan las de hasta 15 letras). Las listas de torneo tienen licencia y no se pueden regalar, así que no están en ninguna parte de esta web. Si te rechaza una palabra que conoces, ese es el motivo. Más en [el diccionario](lexicon).

Los leaves, los valores de las fichas que te quedas, se entrenaron desde cero para ENABLE con el motor jugando contra sí mismo. No se usa ninguna tabla publicada.

## Cómo se juega

- **Coloca fichas** arrastrándolas, o haz clic en una casilla y escribe. Otro clic en la misma casilla cambia la dirección.
- **Intro** juega la jugada. **Esc** recoge las fichas.
- **Cambiar** te deja elegir fichas para cambiarlas, mientras la bolsa tenga siete o más.
- Un **comodín** pregunta qué letra representa.
- La **vista previa de puntos** enseña lo que vale la jugada antes de jugarla, y el panel lista las mejores jugadas si lo activas en los ajustes.
- Al terminar la partida, **repásala** jugada a jugada, vuelve a jugar desde cualquier posición o descárgala en GCG.

La lista completa de controles está en [la interfaz](interface).

## Si no carga

| Síntoma | Causa | Qué hacer |
|---|---|---|
| Se queda en «Cargando el motor» | una conexión lenta: la primera carga son unos 4 MB | espera, o recarga |
| Página en blanco | un navegador muy antiguo sin excepciones de WebAssembly | usa un Chrome, Edge, Firefox o Safari actual |
| El bot tarda mucho en el móvil | un hilo en un procesador pequeño | baja el tiempo para pensar, o elige Medio |
| Un nivel sale en gris | es Extremo | está en la [app de escritorio](download) |

## Privacidad

No hay cuenta, ni analítica, ni servidor detrás de la demo. El motor corre en un Web Worker de tu navegador. Tus ajustes se guardan en el almacenamiento local del navegador y en ningún otro sitio.

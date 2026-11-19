title: El final exacto
nav: Finales
blurb: Con la bolsa vacía no hay secretos. Una búsqueda que juega el final de la partida de forma perfecta.
lead: Cuando la bolsa está vacía, la partida ya no tiene secretos: el rival tiene exactamente las fichas que no han salido. A partir de ahí Kingbble deja de estimar y busca.
---
## Otro juego

Durante casi toda la partida se razona con probabilidades. En cuanto se roba la última ficha, eso se acaba. El atril rival se obtiene restando, y desde ese momento es un juego de información perfecta, como el ajedrez, con pocas jugadas por delante.

Un buen final no es lo mismo que un final de muchos puntos. Está lleno de ideas que la equity estática no ve:

- **Cerrar primero.** Vaciar el atril termina la partida y suma el doble del valor de lo que le quede al rival.
- **Dejar al rival atascado.** Si tiene una `Q` sin sitio donde jugarla, el plan correcto puede ser jugar de ficha en ficha e ir sumando mientras él pasa.
- **Bloquear.** Una jugada que puntúa poco pero le quita al rival su única casilla buena.
- **Preparar.** Una jugada cuya finalidad es la que hace posible al turno siguiente.

Todo eso sale solo al buscar en el árbol de jugadas y respuestas hasta el final.

## La búsqueda

El solver es una **búsqueda negamax con poda alfa-beta**. En cristiano: prueba una jugada, luego todas las respuestas a ella, luego todas las respuestas a esas, hasta el final de la partida, y puntúa cada posición final por la diferencia de puntos. Al volver hacia arriba, se supone que cada jugador elige lo mejor para sí. Alfa-beta se salta las ramas que no pueden cambiar la respuesta: en cuanto aparece una réplica que ya hace que una jugada sea peor que otra alternativa, no hace falta mirar las demás réplicas a esa jugada.

Lo que la hace viable:

| Técnica | Qué hace |
|---|---|
| **Profundización iterativa** | busca a profundidad 1, luego 2, luego 3… Cada pasada es barata comparada con la siguiente y le dice a la siguiente qué jugadas probar primero. Además, siempre hay una respuesta completa a la que recurrir si se acaba el tiempo. |
| **Orden de jugadas** | probar primero la mejor jugada es lo que hace que alfa-beta pode. Se ordenan por la mejor línea de la iteración anterior y por puntos. |
| **Tabla de transposiciones** | a una misma posición se puede llegar con distintos órdenes de jugadas. Su resultado se guarda la primera vez y se reutiliza. |
| **Poda del léxico** | antes de buscar, el [diccionario](lexicon) se recorta a las palabras que podrían formar las fichas que siguen en juego. La generación de jugadas dentro de la búsqueda recorre entonces un autómata mucho más pequeño. |
| **Lazy SMP** | con varios hilos, los ayudantes buscan la misma posición sobre la tabla compartida, la mitad de ellos un ply más profundo. El valor es el mismo; la respuesta llega antes. La tabla está partida en fragmentos, cada uno con su propio cerrojo. |

La búsqueda se detiene en la primera iteración **exacta**, es decir, en la que ninguna línea se cortó por el límite de profundidad, o al llegar al límite de tiempo. El valor que maximiza es el **spread** final.

Siguiendo la convención de finales de Macondo, dos turnos seguidos sin puntuar terminan la partida dentro de la búsqueda, y cada jugador pierde el valor de su atril.

## ¿Es correcto?

La comprobación es la misma que para el generador: comparar con la referencia. Si dos solvers son exactos, el valor de una posición con juego perfecto tiene que ser idéntico.

Cincuenta finales reales, con hasta 14 fichas entre los dos atriles, se resolvieron con los dos solvers con un tope de 600 segundos por posición y lado.

:::bars min=0 max=100 unit=% caption=Coincidencia exacta con Macondo en finales: 34 posiciones que los dos solvers terminaron en 600 s, cero diferencias de valor.
Valor del final | 100 | | | 34 de 34 posiciones
:::

| | Finales |
|---|---|
| Resueltos | 50 |
| Comparados (terminan los dos lados) | 34 |
| Diferencias de valor | **0** |
| Saltados por tiempo | 16: 5 que no termina Kingbble y 11 que no termina Macondo |

Es decir, en todos los finales que terminan los dos, los valores son idénticos, y dentro del tope Kingbble terminó más. Tres posiciones se resolvieron otra vez desde cero en los dos lados como comprobación adicional y dieron los mismos valores.

## ¿Merece la pena?

Esta pregunta tiene su historia. Una prueba temprana mostraba que el bot con final exacto solo ganaba el 47 % contra el mismo bot sin él. Eso habría sido un bug en el solver, del que dependen todos los bots más fuertes.

No era un bug. La prueba tenía 200 partidas sin emparejar, y con tan pocas el intervalo es de ±7 puntos: «47 %» era compatible con «mejor». Repetida con [partidas emparejadas](method), en las que cada reparto se juega dos veces cambiando los asientos para que la suerte del robo se cancele:

:::bars min=-1 max=7 ref=0 caption=Spread medio por partida ganado frente a un bot solo con equity estática. 600 partidas emparejadas por fila, intervalos del 95 %.
Final exacto, 1 s | 4.8 | 4.0 | 5.6
Final exacto, 10 s | 5.1 | 4.3 | 6.0
1 s, y si no es exacto, la jugada de equity | 2.7 | 2.0 | 3.3
:::

| Configuración | % de victorias | Spread por partida |
|---|---|---|
| 1 s por final | 51,4 (50,4–52,5) | +4,8 (+4,0 a +5,6) |
| 10 s por final | 51,8 (50,8–52,9) | +5,1 (+4,3 a +6,0) |
| 1 s, y la jugada de equity si no es exacto | 51,0 (50,2–51,8) | +2,7 (+2,0 a +3,3) |

Lo que dice esto:

- El final exacto **vale unos 5 puntos por partida**, de forma significativa.
- Gana pocas partidas más (alrededor de 1,5 puntos de porcentaje de victorias) porque el final solo decide el resultado cuando la partida ya está igualada.
- Con un segundo se resuelve de forma exacta **el 80 % de los finales**, con una profundidad media de unos 4 plies. Con diez segundos, el 91 %. El tiempo extra apenas se nota en los resultados.
- El solver juega algo distinto de equity en cerca del 36 % de los finales.
- Cuando se acaba el tiempo, usar la jugada de la **última iteración completa** es mejor que rendirse y jugar la de equity (+4,8 frente a +2,7). Así que eso es lo que hace el motor.

## En los niveles

El final exacto es la diferencia entre los [niveles](levels) Fuerte y Muy fuerte de la demo: `sim` frente a `sim+eg`. En el navegador corre en un hilo con el tiempo de reflexión que elijas; en la app de escritorio usa varios.

title: Equity y leaves
nav: Equity y leaves
blurb: Una jugada vale sus puntos más las fichas que se queda. De dónde salen esos valores.
lead: Una jugada vale los puntos que hace más lo que valdrán después las fichas que se queda. Este capítulo trata de ese segundo número: qué es un leave, de dónde sale su valor y cómo construye Kingbble su propia tabla partiendo de cero.
---
## Los puntos son la mitad de la historia

Imagina dos jugadas. Una hace 30 puntos y te deja con `UUVW`. La otra hace 22 y te deja `AEINRS`. Casi cualquier jugador fuerte elige la de 22: con esas seis fichas es probable un bingo al turno siguiente, y eso son 50 puntos además de la palabra.

Las fichas que quedan en el atril después de una jugada son su **leave**. La **equity estática** de una jugada es

```
equity = puntos de la jugada + valor del leave
```

y el bot `equity` juega sin más la jugada de mayor equity. Es «estática» porque no se simula nada: los dos términos se consultan o se calculan en el momento. Por eso es instantánea, y es la primera capa de decisión sobre la que se apoya todo lo demás.

## Qué es una tabla de leaves

Una tabla de leaves tiene un número para cada combinación de una a seis fichas que permite la bolsa. Son **914.624**. El número responde a esto: *comparado con un robo medio, ¿cuántos puntos mejor o peor quedo, en lo que resta de partida, por quedarme exactamente con estas fichas?*

Algunas formas típicas, con cualquier tabla:

- Un comodín vale mucho más que cualquier letra: entre 20 y 25 puntos.
- Una `S` vale varios puntos; la segunda `S` vale menos que la primera.
- La `Q` sin `U` es muy negativa.
- Los duplicados hacen daño: `II` y `UU` son malos, `VV` es terrible.
- El equilibrio importa: tres vocales y tres consonantes son mejores que seis de una clase.
- Las combinaciones no son sumas. `ERS` vale más que `E` + `R` + `S`, y por eso la tabla tiene una entrada para cada combinación y no una por ficha.

## Tres formatos de tabla

| Formato | Qué es |
|---|---|
| **KLV** | binario: un autómata pequeño que lista los leaves en orden y, después, un valor de 16 bits por leave |
| **KLV2** | lo mismo con valores en coma flotante de 32 bits; el formato de Macondo y wolges |
| **CSV** | líneas `LEAVE,valor`; lo que escriben las herramientas de ajuste de Kingbble |

El motor lee los tres. En memoria un leave se empaqueta en una clave de 64 bits (cinco bits por ficha, ordenadas), y consultarlo es una búsqueda binaria. Un leave que no está en la tabla vale cero.

## El resto de la fórmula

Dos ajustes hacen que la equity se comporte bien en los dos extremos de la partida.

**Bolsa vacía.** Sin fichas por robar, un leave ya no es una promesa: es una carga. Cerrar gana el doble del valor del atril rival, y cada ficha que te quedas cuesta el doble de su valor más una constante, como hace Macondo. En la práctica, el [solver de finales](endgame) sustituye esta estimación siempre que está activado.

**Bolsa casi vacía.** Con pocas fichas, los valores de la tabla son demasiado optimistas: puede que no dé tiempo a usar ese comodín. Una mezcla opcional lleva el valor del leave desde el de la tabla hacia menos el valor facial de las fichas a medida que se vacía la bolsa.

Los cambios pasan por la misma fórmula: un cambio puntúa cero y su leave es lo que te quedas, así que el motor puede comparar «cambiar cinco» con «jugar por 12» en igualdad de condiciones.

## De dónde salen los valores

Las tablas publicadas para las listas de torneo tienen licencia y no se distribuyen con Kingbble. Así que el motor aprendió a hacer las suyas, partiendo de cero.

El método es el que usan wolges y Magpie, a veces llamado de *atril completo*:

1. **Jugar.** El bot `equity` juega contra sí mismo cientos de miles de partidas rápidas con la tabla actual. En cada turno que empieza con el atril completo y al menos siete fichas en la bolsa, la partida apunta las siete fichas y la equity de la jugada que se hizo.
2. **Promediar por atril.** Cada atril distinto acaba con una media: lo que valió, de media, tener esas siete fichas.
3. **Derivar los leaves.** El valor de un leave es la media de los valores de todos los atriles completos a los que podría llegar, ponderada por la probabilidad de robar cada combinación, menos el valor de un atril robado de una bolsa llena.
4. **Encoger.** Un leave visto en pocos atriles tiene una estimación ruidosa. Los datos se parten en dos mitades independientes; lo que coincide entre las mitades es señal y el resto es ruido, y cada corrección se encoge hacia cero en esa medida (una estimación de Bayes empírico).
5. **Repetir.** La tabla corregida se usa para jugar la generación siguiente, porque unos leaves mejores cambian qué jugadas se hacen y, con ello, cuánto vale cada atril.

Los atriles se *reparten*, no se eligen, así que la muestra no arrastra ningún sesgo de la política que los jugó.

La primera generación parte de una tabla en la que todos los leaves valen cero, que es el jugador greedy. Con una generación ya se recupera lo evidente, y las siguientes lo afinan. La tabla que acompaña a la [demo web](play) llevó cinco generaciones de 600.000 partidas cada una, tres millones en total:

| Leave | Gen. 1 | Gen. 2 | Gen. 3 | Gen. 4 | Gen. 5 |
|---|---|---|---|---|---|
| `?` (un comodín) | +21,0 | +26,6 | +28,6 | +29,3 | +29,5 |
| `S` | +5,7 | +7,6 | +8,2 | +8,5 | +8,6 |
| `Q` | −5,2 | −7,9 | −9,1 | −9,5 | −9,6 |

Cada generación mueve los valores menos que la anterior. En la quinta los cambios son de unas décimas, que es donde el método ha convergido.

:::note La tabla de la demo y de la versión pública
Se ajustó desde cero sobre ENABLE con este proceso, sin ningún valor publicado en ningún punto. Es propia de Kingbble y se distribuye con él. La app de escritorio puede además seguir entrenándola en tu máquina: mira [entrenamiento](training).
:::

## Cuánto vale la equity sola

Lo bastante para ser un rival serio para la mayoría de la gente, y ciega de una forma concreta: no mira el tablero. No distingue que una jugada le regala al rival una triple palabra, ni que los 40 puntos disponibles ahora seguirán ahí al turno siguiente. Eso es lo que ve la [simulación](simulation).

Como referencia, el [capítulo de finales](endgame) mide `equity` con final exacto contra `equity` a secas: solo el final vale 4,8 puntos por partida.

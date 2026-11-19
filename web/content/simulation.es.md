title: Simulación Monte Carlo
nav: Simulación
blurb: Imaginar muchas veces el atril rival, jugar cada candidata y quedarse con la mejor media.
lead: Imaginar muchas veces el atril rival, jugar cada candidata y quedarse con la de mejor media. Cómo funciona SimBot, en qué gasta el tiempo y qué se probó para hacerlo más fuerte.
---
## Lo que la equity no ve

La equity estática ordena una jugada por lo que gana y lo que se queda. No sabe nada de lo que la jugada le hace al tablero: si abre una línea hacia una triple palabra, si prepara un gancho que solo tú puedes usar o si deja una casilla de premio que se llevará el rival. Para valorar eso hay que mirar lo que pasa después.

No puedes saber lo que pasa después, porque no conoces las fichas del rival. Pero puedes **muestrearlo**.

## Una iteración

Para una jugada candidata, una iteración de la simulación es:

1. **Repartir.** Dar al rival un atril sacado de las fichas que no ves y barajar el resto como bolsa.
2. **Jugar la candidata.** Poner la jugada en una copia del tablero y robar las fichas de reposición.
3. **El rival responde** con su mejor jugada por equity estática.
4. **Tú continúas** con tu mejor jugada por equity estática.
5. **Puntuar.** El valor de la iteración es la diferencia de puntos en esos plies, más el valor del leave que queda al final.

Eso es un playout a *dos plies*: la respuesta del rival y tu continuación. Repítelo unos cientos de veces con repartos distintos y la media te dice cómo le va a la candidata frente al abanico de cosas que el rival puede tener.

:::flow
Candidatas | las 10 mejores por equity estática
Reparto | un atril para el rival, un orden de bolsa
Playout | candidata, respuesta, continuación
Media | por candidata, con su error estándar
Poda | fuera las que van claramente por detrás; repetir
:::

Todas las candidatas se evalúan contra **los mismos repartos**. La suerte que tenga un reparto afecta a todas por igual, así que la comparación entre ellas es mucho más fina que cada media por separado.

## Los parámetros

| Parámetro | Por defecto | Significado |
|---|---|---|
| `candidates` | 10 | cuántas de las mejores jugadas por equity se simulan |
| `max_iterations` | 200 | repartos por candidata, como máximo |
| `min_iterations` | 32 | repartos antes de poder podar nada |
| `batch` | 16 | repartos entre comprobaciones de poda |
| `prune_z` | 2,58 | una candidata se descarta cuando va por detrás de la mejor por este número de errores estándar (99 %) |
| `plies` | 2 | jugadas que se hacen después de la candidata en cada playout |
| `time_limit_s` | 0 | segundos por jugada; 0 es sin límite |
| `posterior_share` | 0,8 | con un seguidor de atriles, la parte de atriles del rival que sale de él; el resto son uniformes |

## Gastar el tiempo donde importa

**Poda por confianza.** Después de cada lote se descarta toda candidata cuya media vaya por detrás de la mejor por más de 2,58 errores estándar combinados. Las iteraciones que quedan van a las candidatas que siguen en liza. En cuanto solo queda una, la simulación termina aunque sobre tiempo: una jugada obvia tarda una fracción de segundo.

**Un presupuesto de tiempo.** Con un límite de, por ejemplo, cinco segundos, la simulación hace lote tras lote hasta gastar el tiempo, siempre después del mínimo de 32 iteraciones. El reloj se mira en cada iteración, así que un lote largo sobre muchas candidatas no puede pasarse. El precio es que el resultado depende entonces de lo rápida que sea la máquina; para experimentos reproducibles, las ejecuciones publicadas usan un número fijo de iteraciones.

**Reducción a la mitad.** Una alternativa a la poda que gasta todo el presupuesto: a intervalos iguales del límite de tiempo se descarta la peor mitad de las candidatas. Permite sopesar más candidatas en un tiempo fijo.

**Hilos.** Las iteraciones son independientes, así que se reparten entre hilos, cada uno con su generador y su copia de la posición. En la app de escritorio la simulación usa los núcleos que le des. En el navegador corre en uno.

## Simular mejor al rival

Dos refinamientos cambian *quién* es el rival simulado.

**Su atril.** Por defecto, el atril del rival es un robo uniforme de las fichas no vistas. Con [inferencia](inference), el 80 % de los atriles sale del posterior del seguidor: los atriles que encajan con cómo viene jugando el rival. El otro 20 % sigue siendo uniforme, como seguro frente a un seguidor que se equivoque con mucha confianza.

**Su respuesta.** Por defecto, el rival simulado juega la mejor jugada por equity. Opcionalmente elige entre sus mejores respuestas con probabilidades que favorecen a las mejores: un rival más blando, que no siempre encuentra la jugada óptima, más parecido a cómo responde una persona.

## Cerca del final

Los valores de leave describen un futuro medio. Con pocas fichas sin ver ya no queda un futuro medio, solo este. Por eso, a partir de cierto número de fichas no vistas, los playouts pueden seguir **hasta el final de la partida** en lugar de parar a los dos plies, como hace Macondo. Y cuando la bolsa está vacía, la simulación deja paso al [solver de finales](endgame).

## Qué se probó y qué salió

La simulación es donde se probaron casi todas las mejoras «obvias». Casi todas salieron neutras, y una parecía una victoria hasta que se arregló la medición.

:::bars min=30 max=70 ref=50 unit=% caption=Porcentaje de victorias de cada variante contra la simulación base (8 candidatas × 50 iteraciones) en un banco de 30 posiciones jugadas desde los dos lados. Intervalos del 95 %; la línea discontinua es el 50 %.
8 × 200 iteraciones | 46.7 | 39.0 | 54.4 | 4 veces más iteraciones
8 × 800 iteraciones | 46.7 | 39.0 | 54.4 | 16 veces más iteraciones
15 × 50 | 42.5 | 36.2 | 48.8 | más candidatas, mismas iteraciones
15 × 200 | 50.8 | 43.2 | 58.4 | más de las dos cosas
3 plies, banco corregido | 51.0 | 47.5 | 54.5 | 200 partidas
:::

- **Más iteraciones no ganan.** Pasar de 50 a 200 u 800 iteraciones por candidata no cambia nada medible. El ruido de la media no es el límite.
- **Más candidatas con pocas iteraciones pierden.** Es la *maldición del ganador*: entre 15 medias ruidosas, alguna sale inflada por suerte, y esa es la que se elige. Con más iteraciones por candidata el efecto desaparece.
- **Los playouts más profundos empatan.** Tres plies midieron primero un 56,5 %, que parecía la primera mejora real. Era un artefacto del banco de pruebas: mira [lo que no funcionó](lessons). En el banco corregido es un 51,0 %, un empate, con más tiempo por jugada.

La conclusión que marcó el resto del proyecto: en estas posiciones, la distancia con los motores más fuertes no es cuestión de calcular más. Es el sesgo del modelo dentro de los playouts, un horizonte de dos plies contra un rival que juega de forma estática, y la precisión de los finales.

## Cuánto aporta la simulación

El bot `sim` contra `equity` es una de las cuatro ablaciones de la evaluación final ([resultados](results)). En los niveles de la [demo](play), la simulación es la diferencia entre Medio y Fuerte.

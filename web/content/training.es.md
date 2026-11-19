title: Entrenamiento
nav: Entrenamiento
blurb: Entrena tus propios leaves desde la app, y el menú de entrenamiento de los modelos: niveles, gate, MLflow y bucle Auto.
lead: En Kingbble hay dos cosas que se aprenden de partidas en vez de escribirse a mano: los valores de los leaves y el modelo de jugadas. Las dos se pueden reentrenar, y las dos solo se conservan si pasan un gate.
---
## Dos cosas que se aprenden

| | Valores de leaves | Modelo de jugadas |
|---|---|---|
| Qué es | un número por cada conjunto de fichas que te quedas | la probabilidad de que un jugador elija cada jugada |
| Quién lo usa | todos los niveles desde Medio | la inferencia del atril |
| De qué aprende | del motor jugando contra sí mismo | de partidas humanas, de self-play o de una mezcla |
| Dónde se entrena | en la app de escritorio, con un botón | en el menú de entrenamiento, desde la línea de comandos |
| Se explica en | [equity y leaves](equity) | [el modelo humano](human-model) |

## Entrenar leaves desde la app

La app de escritorio tiene un botón **Entrenar leaves** en los ajustes, para la lista de palabras en uso. Existe porque una tabla de leaves pertenece a su lista: una tabla hecha para una lista está un poco equivocada para otra, y las tablas publicadas para listas de torneo no se pueden regalar. Con el botón, cualquier lista que tengas consigue la suya.

| Entrenamiento | Cuánto tarda | Para qué sirve |
|---|---|---|
| **Rápido** | minutos | para verlo funcionar; una tabla tosca |
| **Normal** | de una a dos horas | una tabla suficiente para jugar |
| **Máximo** | una noche | lo mejor que da el método en tu máquina |

Qué pasa al pulsarlo:

:::flow
Self-play | el motor juega partidas rápidas contra sí mismo con la tabla actual
Crédito | a cada atril completo repartido se le apunta la equity de la jugada hecha con él
Derivar | cada leave recibe el valor medio de los atriles a los que se puede completar
Encoger | los valores respaldados por pocos atriles se acercan a cero, usando dos mitades independientes de los datos
Repetir | la tabla nueva juega la generación siguiente
Gate | la tabla nueva juega contra la actual en partidas emparejadas
:::

Mientras corre, la app enseña la etapa y el tiempo que falta. **Parar conserva el progreso**: la siguiente ejecución sigue desde la última generación terminada.

### El gate

Una tabla nueva no se usa por ser nueva. Juega partidas emparejadas contra la tabla en uso, y la app da el resultado con su intervalo:

- **Mejores que los actuales**: el intervalo queda por encima del 50 %. Se ofrece la tabla nueva.
- **Igual de buenos que los actuales**: el intervalo incluye el 50 %.
- **Peores que los actuales**: el intervalo queda por debajo del 50 %. Se queda la tabla actual.

Tú eliges qué tabla se usa, la incluida o la tuya, y puedes volver atrás cuando quieras. Cambiar los leaves reinicia el motor para esa lista de palabras, así que una partida en curso se pierde: la app te avisa.

:::note La tabla que viene con la demo
La tabla de ENABLE de la demo web y de la app se hizo exactamente así, partiendo de una tabla de ceros. No se tomó ningún valor de ninguna tabla publicada.
:::

## El menú de entrenamiento

El modelo de jugadas, y los experimentos a su alrededor, se entrenan desde el **menú de entrenamiento** del repositorio: `python -m app.cli`, o una página de Streamlit con las mismas opciones. Es una herramienta del proyecto, no forma parte de la app de escritorio.

### Niveles de datos

Un nivel es una cantidad de datos. Cada uno es diez veces el anterior.

| Nivel | Nombre | Partidas de self-play | Modelo | Para qué sirve |
|---|---|---|---|---|
| 0 | smoke | 1.000 | logit | comprueba que todo el proceso funciona, en minutos |
| 1 | test | 10.000 | logit | un primer modelo utilizable |
| 2 | base | 100.000 | logit y una red de 32 unidades | horas de generación |
| 3 | full | 1.000.000 | una red de 64 unidades | días de generación |

Los niveles grandes guardan una muestra compacta de sus características y la borran después de entrenar, para caber en un disco normal.

### Los comandos

```
python -m app.cli                    # el menú interactivo
python -m app.cli train --level 1    # genera lo que falta, ajusta y guarda una versión nueva
python -m app.cli evaluate           # pasa el gate de la versión nueva contra la anterior
python -m app.cli improve            # sube de nivel si la curva de aprendizaje sigue bajando
python -m app.cli auto --budget-cpu-hours 20 --budget-gb 30
python -m app.cli status
python -m app.cli revert v003        # vuelve a activar una versión anterior
```

- **train** genera las partidas de self-play que le faltan al nivel, ajusta el modelo con su **curva de aprendizaje** (la pérdida de validación con fracciones crecientes de los datos) y guarda una versión numerada.
- **evaluate** es el gate: la log-loss de la inferencia del atril en turnos reservados y un match emparejado contra la versión anterior. El veredicto es *peor*, *no mejor* o *mejor*.
- **improve** mira la curva de aprendizaje. Si más datos seguían bajando la pérdida, sube de nivel. Si la curva está plana, más datos serían un gasto inútil, y lo dice.

### El bucle Auto

`auto` repite train y evaluate por su cuenta dentro de un presupuesto de horas de procesador y de disco.

1. Entrena en el nivel actual.
2. Pasa el gate de la versión nueva contra la mejor hasta ahora. La conserva salvo que pierda.
3. Si la curva de aprendizaje sigue bajando, sube de nivel.
4. Para cuando se gasta el presupuesto, cuando llega al último nivel, cuando dos niveles seguidos no traen mejora o cuando una versión nueva pierde.

La mejor versión es siempre el modelo activo. El estado se guarda después de cada paso, así que el mismo comando lo retoma tras un apagado. Antes de empezar un nivel, el bucle estima las horas y los gigas que necesita y se niega si no caben en lo que queda.

### Protecciones

- Cada paso pesado corre con **límites de memoria, núcleos y prioridad**, a la prioridad más baja, para que el ordenador siga siendo utilizable.
- Una ejecución pesada **se niega a empezar mientras otra está activa**, salvo que se fuerce.
- Cada ejecución queda **registrada en MLflow**: parámetros, curva de aprendizaje, resultado del gate y ficheros producidos, para poder comparar dos versiones meses después.
- Cualquier versión anterior se puede volver a activar con `revert`.

## Lo que el entrenamiento no puede hacer

- No hace más exacto al motor donde ya es exacto. El generador de jugadas, la puntuación y el final no se aprenden.
- Más datos solo ayudan mientras la curva de aprendizaje baja. El nivel 0 pasó su gate; si el nivel 3 merece sus días de máquina es una pregunta abierta en la [hoja de ruta](roadmap).
- Un modelo ajustado con partidas de otras personas arrastra las condiciones de esas partidas. Por eso la app pública entrena leaves solo con self-play y no incluye el modelo humano.

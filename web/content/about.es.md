title: Acerca de, licencia y créditos
nav: Acerca de y licencia
blurb: Quién lo ha hecho, con qué licencia, qué incluye, qué deja fuera y a quién le debe.
lead: Kingbble es un proyecto de una sola persona que creció hasta ser un motor, una app y este manual. Esta página dice en qué condiciones puedes usarlo, qué incluye y qué no, y sobre el trabajo de quién se apoya.
---
## El proyecto

Kingbble empezó con una observación: a un buen motor de ajedrez ya no le gana nadie, pero en este juego un jugador fuerte todavía gana a menudo a los mejores motores, porque las fichas están ocultas y el reparto decide mucho. De ahí salió una pregunta: ¿puede un motor jugar mejor si modela cómo eligen sus jugadas *las personas* y lo usa para adivinar las fichas que tienen? Responderla obligó a construir primero todo lo de debajo, y a medir cada pieza contra los motores en los que confía la comunidad.

Es el trabajo de una persona, [Tello-P](https://github.com/Tello-P) en GitHub, desarrollado en abierto: cada cambio pasó por una issue, una rama y un pull request con tests.

- **Código fuente**: [github.com/Tello-P/Kingbble](repo)
- **Preguntas y fallos**: las issues del repositorio
- **Historia**: el changelog del repositorio, hito a hito

## La licencia

Kingbble es software libre bajo la **Licencia Pública General de GNU, versión 3**.

En resumen, y sin sustituir al texto de la licencia:

- Puedes **usarlo** para cualquier fin.
- Puedes **estudiarlo** y **cambiarlo**.
- Puedes **compartirlo**, cambiado o no.
- Si lo distribuyes, o distribuyes un programa que lo incluye, tienes que hacerlo **bajo la misma licencia**, con el código fuente, y conservar los avisos.
- Viene **sin ninguna garantía**.

El texto completo es el fichero `LICENSE` del repositorio y va incluido en todas las descargas.

El contenido de esta web, su texto y sus figuras, se publica en las mismas condiciones que el código que documenta.

## Qué incluye

| Elemento | Origen | Condiciones |
|---|---|---|
| El código de Kingbble: motor, interfaz, herramientas, esta web | este proyecto | GPL-3.0 |
| Lista de palabras ENABLE | Alan Beale y otros | dominio público |
| Tabla de leaves para ENABLE | entrenada desde cero por este proyecto, con self-play | GPL-3.0, con el código |
| Tipografía Young Serif | Bastien Sozeau | SIL Open Font License 1.1 |
| Tipografía IBM Plex Sans | IBM | SIL Open Font License 1.1 |
| Electron, en la app de escritorio | OpenJS Foundation y colaboradores | MIT |
| Lista de palabras y leaves de prueba (`lexica/fixtures`) | hechas para este proyecto | GPL-3.0 |

## Qué se deja fuera, y por qué

| Elemento | Por qué no se incluye |
|---|---|
| Listas de palabras de torneo (Collins Scrabble Words, la NASPA Word List) | tienen licencia de sus titulares |
| Tablas de leaves publicadas por otros proyectos | sus condiciones de redistribución no están confirmadas |
| El modelo humano de jugadas de los experimentos | se ajustó con partidas de una plataforma en línea; si se puede redistribuir es una pregunta abierta |
| Las propias partidas humanas | son de sus jugadores y de la plataforma; el proyecto las guarda seudonimizadas y no las publica |
| Una lista de palabras en español | no se conoce ninguna que se pueda distribuir libremente |

Los experimentos se midieron con una lista de torneo en uso local, citada y no redistribuida. Todo lo que un visitante puede descargar de aquí está hecho solo con la primera tabla.

## Marcas y afiliación

Kingbble es un proyecto independiente. **No está afiliado, patrocinado ni respaldado por Hasbro, Mattel ni ningún titular de marcas de juegos de palabras cruzadas.** SCRABBLE® es una marca registrada de sus respectivos titulares: Hasbro, Inc. en Estados Unidos y Canadá, y Mattel, Inc. en el resto del mundo. Los nombres de listas de palabras y de otros motores aparecen en esta web solo para describir con qué se comparó Kingbble y con qué es compatible.

El nombre, el logo y el diseño visual de Kingbble son propios.

## Sobre el trabajo de otros

El código es de Kingbble. Las ideas no, y el proyecto no existiría sin quienes publicaron las suyas.

| | Lo que Kingbble le debe |
|---|---|
| **Andrew Appel y Guy Jacobson** | el algoritmo para generar jugadas desde un grafo de palabras con anclas y cross-checks (1988) |
| **Steven Gordon** | el GADDAG, que permite construir jugadas hacia fuera desde cualquier letra (1994) |
| **Brian Sheppard** (Maven) | la simulación, los valores de leaves y el final exacto como arquitectura de un motor fuerte |
| **Quackle** (Jason Katz-Brown, John O'Laughlin y colaboradores) | la referencia durante dos décadas, y RangeFinder, el baseline de la inferencia |
| **Macondo** (César Del Solar y colaboradores) | la referencia con la que Kingbble se comprueba posición a posición, y el rival de sus matches |
| **wolges** (Andy Kurnia) | los formatos KWG y KLV, y el método de atril completo para los valores de leaves |
| **Magpie** | el mismo método de leaves, e ideas para un motor rápido |
| **Woogles** | la plataforma abierta cuyas partidas públicas hicieron posible un modelo de juego humano |
| **Los jugadores** | cada partida registrada son horas de pensamiento de alguien |

Kingbble se comunica con Quackle y Macondo solo como programas aparte, para comparar. No se incluye ni se enlaza nada de su código.

## Privacidad

- **Esta web** no tiene analítica, ni cookies, ni scripts de terceros. Las tipografías se sirven desde la propia web.
- **La demo web** se ejecuta en tu navegador. Nada de lo que hagas en ella se envía a ningún sitio.
- **La app de escritorio** no hace conexiones.
- Lo único que se guarda son tus **ajustes**, en tu propio dispositivo.
- Si la web se aloja en GitHub Pages, GitHub, como alojamiento, puede registrar las direcciones de los visitantes según sus propias condiciones de privacidad.

## Cómo está hecha esta web

HTML y CSS planos, generados por un script de Python a partir de ficheros de texto, con unas pocas líneas de JavaScript para el tema, el menú y el contenido. Sin framework, sin rastreadores, sin ninguna petición externa. Está compuesta en Young Serif e IBM Plex Sans, las mismas tipografías y colores que la app, para que se vea que el manual y el juego son la misma cosa.

El código de todas las páginas está en la carpeta `web/` del repositorio. Las correcciones son bienvenidas como issues o pull requests.

## Cómo citarlo

Si usas Kingbble en un trabajo publicado, cita el repositorio y la versión:

```
Kingbble: a crossword-game engine with opponent-rack inference
calibrated on human play. Version 1.0, 2026.
https://github.com/Tello-P/Kingbble
```

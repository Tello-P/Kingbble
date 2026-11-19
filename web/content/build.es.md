title: Compilar desde el código
nav: Compilar desde el código
blurb: Compila el motor, arranca la interfaz, construye la demo web y publica esta web.
lead: Todo lo que hay en esta web sale de un solo repositorio: el motor, la interfaz, la app de escritorio, la demo web y estas páginas. Este capítulo es la lista de comandos de cada cosa.
---
## Qué necesitas

| Para | Requisitos |
|---|---|
| El motor | CMake 3.20 o posterior, GCC 12 o Clang 15 o posterior (C++20), git |
| Las herramientas y el entrenamiento | Python 3.11 o posterior |
| La app de escritorio | Node.js con npm |
| La demo web | Emscripten (emsdk) |
| Esta web | Python 3.11 con Pillow |

## El repositorio

```
engine/      C++: include/, src/, apps/ (self-play, matches, evaluación, bridge), tests/
ml/          Python: data/, human_model/, rl/, eval/, tests/
app/         el menú de entrenamiento (CLI y Streamlit) y el bucle Auto
ui/          la interfaz: server.py y static/ (la página)
desktop/     la ventana de Electron
web/         esta web: content/, static/, wasm/, build.py
scripts/     diccionario, auxiliares para los motores de referencia, scripts por experimento
configs/     configuraciones de torneo
docs/        los experimentos que hay detrás de las tablas
```

## El motor

```
git clone https://github.com/Tello-P/Kingbble
cd Kingbble
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Los tests no necesitan ningún fichero con licencia: usan una lista de palabras y una tabla de leaves pequeñas hechas para eso, en `lexica/fixtures/`.

Para la parte de Python:

```
pip install -e ".[dev,eval]"
ruff check . && pytest -q
```

## Una lista de palabras

Las listas de torneo y sus tablas de leaves tienen licencia y no están en el repositorio. La lista libre es un comando:

```
python scripts/fetch_lexicon.py --preset enable
```

Esto descarga ENABLE en `lexica/`. Una lista que tengas derecho a usar se añade con `--name` y `--source`. La tabla de leaves se busca por nombre: un fichero junto a la lista con el mismo nombre acabado en `.klv2`, `.klv` o `.csv`. Una lista sin leaves solo ofrece el nivel Fácil.

## La interfaz en un navegador

```
cmake --build build -j --target kingbble_bridge
python ui/server.py --threads 2
```

Después abre `http://localhost:8765`. El servidor solo usa la biblioteca estándar de Python. Arranca un proceso del motor por visitante, le pasa las órdenes de la página y rechaza cualquier orden que no conozca antes de que llegue al motor.

## La app de escritorio

```
cd desktop
npm install
npm start -- --threads 4
```

La app arranca el mismo servidor local en un puerto libre y lo abre en su propia ventana. Todo lo que va después de `--` se le pasa al servidor.

## La demo web

La demo es el bridge compilado con Emscripten. Con emsdk instalado y activado:

```
web/wasm/build.sh
```

Esto escribe `engine.js` y `engine.wasm` (unos 570 KB) en `web/static/demo/`. La compilación es de un solo hilo y usa excepciones de WebAssembly. En la página, tres ficheros pequeños ocupan el sitio del servidor:

| Fichero | Qué hace |
|---|---|
| `worker.js` | carga el motor en un Web Worker, descarga la lista de palabras y los leaves, y responde a las órdenes |
| `shim.js` | responde a las llamadas `/api` de la página desde el worker, limita los niveles y el tiempo para pensar, y enseña Extremo como bloqueado |
| `demo.css` | los pocos estilos que añade la demo |

La interfaz en sí se copia sin cambios desde `ui/static/`. Ese es el motivo del diseño de [la interfaz](interface): una página, dos motores detrás.

La demo necesita dos ficheros de datos que no están en el repositorio: `web/data/ENABLE.txt` y `web/data/ENABLE-leaves.txt`. El fichero `web/README.md` dice cómo producir cada uno.

## Esta web

```
python web/build.py            # todo en web/dist/
python web/build.py --no-demo  # solo las páginas
python -m http.server -d web/dist 8000
```

El generador no tiene más dependencias que Pillow, para las capturas. Cada capítulo son dos ficheros de texto, `web/content/SLUG.en.md` y `SLUG.es.md`. Un enlace a un capítulo que no existe para la compilación.

La salida son ficheros planos con enlaces solo relativos, así que funciona desde cualquier carpeta de cualquier alojamiento estático:

```
dist/
  index.html      te manda a en/ o es/ según el idioma del navegador
  en/  es/        los capítulos
  demo/           la demo jugable
  assets/         estilos, script, tipografías, imágenes
  404.html
  .nojekyll
```

## Publicar en GitHub Pages

Como todos los enlaces son relativos, el mismo `dist/` sirve como web de usuario (`https://NOMBRE.github.io/`) y como web de proyecto (`https://NOMBRE.github.io/Kingbble/`) sin cambios.

**Como web de usuario.** Copia el contenido de `web/dist/` a la raíz del repositorio `NOMBRE.github.io` y haz push. En *Settings, Pages*, el origen es la rama principal, carpeta raíz.

**Como web de proyecto, a mano.** Copia `web/dist/` a una rama `gh-pages` del repositorio y elige esa rama en *Settings, Pages*.

**Con una acción.** Un workflow que compila y publica en cada push a main:

```
name: site
on: { push: { branches: [main] } }
permissions: { contents: read, pages: write, id-token: write }
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: "3.12" }
      - run: pip install pillow
      - run: python web/build.py
      - uses: actions/upload-pages-artifact@v3
        with: { path: web/dist }
  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment: github-pages
    steps:
      - uses: actions/deploy-pages@v4
```

Para que la acción compile la demo, el motor compilado y los dos ficheros de datos tienen que estar a su alcance: en el repositorio, o descargados de una release en un paso anterior.

:::note El fichero .nojekyll
GitHub Pages pasa las webs por Jekyll salvo que se le diga que no, y Jekyll se salta algunos ficheros. El fichero vacío `.nojekyll` en la raíz de `dist/` lo desactiva. No lo quites.
:::

La web ocupa unos 14 MB en disco con la demo, casi todo la lista de palabras y los leaves. GitHub Pages los sirve comprimidos, así que un visitante descarga unos 4 MB para la demo y unas decenas de kilobytes por página.

## Lanzar los experimentos

Cada tabla de resultados es un comando, listado en `docs/experiments.md`. Necesitan la lista de torneo con licencia, porque con ella se midieron los resultados, y corren a la prioridad más baja con un número fijo de núcleos. Cada uno tiene un modo `--quick` que comprueba el proceso en minutos. Mira [resultados](results).

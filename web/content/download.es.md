title: Descargar
nav: Descargar
blurb: La app de escritorio para Windows y Linux: los cinco niveles, todos los núcleos y entrenamiento de leaves.
lead: La app de escritorio es el Kingbble completo: los cinco niveles con Extremo incluido, todos los núcleos del procesador, tiempos para pensar más largos y entrenamiento de leaves. Funciona entera en tu ordenador y no necesita conexión.
---
:::raw
<div class="downloads">
  <section class="dl">
    <h3>Windows</h3>
    <p>Windows 10 u 11, 64 bits.</p>
    <ul><li>Instalador (.exe)</li><li>No pide permisos de administrador</li></ul>
    <a class="dlbtn" href="#" data-os="windows" aria-disabled="true" tabindex="-1"><span>Descargar para Windows</span><small>próximamente</small></a>
    <p class="alt"><a href="#" data-os="windows-zip" aria-disabled="true" tabindex="-1">.zip portable</a> · <a href="#" data-os="windows-sha" aria-disabled="true" tabindex="-1">SHA-256</a></p>
  </section>
  <section class="dl">
    <h3>Linux</h3>
    <p>Cualquier distribución actual de 64 bits.</p>
    <ul><li>AppImage: un fichero, sin instalación</li><li>También como paquete .deb</li></ul>
    <a class="dlbtn" href="#" data-os="linux" aria-disabled="true" tabindex="-1"><span>Descargar para Linux</span><small>próximamente</small></a>
    <p class="alt"><a href="#" data-os="linux-deb" aria-disabled="true" tabindex="-1">Paquete .deb</a> · <a href="#" data-os="linux-sha" aria-disabled="true" tabindex="-1">SHA-256</a></p>
  </section>
</div>
:::

:::note Las versiones aún no están publicadas
Los botones están en su sitio y apuntarán a la primera versión pública cuando se etiquete. Los formatos y los nombres de fichero de esta página son los previstos y pueden cambiar con ella. Hasta entonces hay dos formas de jugar: [la demo del navegador](play), que no necesita nada, y [compilar desde el código](build), que te lo da todo hoy.
:::

## Lo que tiene y la demo web no

| | Demo web | App de escritorio |
|---|---|---|
| **Nivel Extremo** (inferencia del atril) | bloqueado | sí |
| Tiempo para pensar por jugada | hasta 5 segundos | hasta 60 segundos |
| Procesador | un hilo | todos los núcleos |
| Leaves | la tabla incluida | la incluida, o [entrenar la tuya](training) |
| Listas de palabras | ENABLE | ENABLE, más cualquier lista que tengas derecho a usar |
| Conexión | hace falta para la primera carga | nunca |

## Qué lleva dentro

- **La interfaz**, la misma de la demo, en una ventana de Electron.
- **El motor**, un programa nativo para tu sistema. La interfaz habla con él en líneas de JSON a través de un pequeño servicio local que solo escucha en tu propia máquina. Nada sale del ordenador.
- **ENABLE**, la lista de palabras de dominio público de 168.551 palabras.
- **Una tabla de leaves para ENABLE**, entrenada desde cero con el motor jugando contra sí mismo.
- **La licencia y los avisos de terceros.**

Lo que no lleva, a propósito: listas de palabras de torneo, tablas de leaves publicadas por otros y cualquier modelo ajustado con partidas de otras personas. Mira [acerca de](about).

## Instalación

**Windows.** Ejecuta el instalador. Puede que Windows enseñe un aviso azul de «editor desconocido», porque el instalador no está firmado con un certificado de pago: elige *Más información* y después *Ejecutar de todas formas*. El .zip portable no necesita instalación: descomprímelo y ejecuta `Kingbble.exe`.

**Linux.** Da permiso de ejecución al AppImage y ábrelo:

```
chmod +x Kingbble-x86_64.AppImage
./Kingbble-x86_64.AppImage
```

O instala el paquete: `sudo apt install ./kingbble_amd64.deb`.

## Requisitos

Cifras previstas, a confirmar con la primera versión.

| | Mínimo | Recomendado |
|---|---|---|
| Procesador | 64 bits, 2 núcleos | 4 núcleos o más |
| Memoria | 2 GB libres | 4 GB libres |
| Disco | 400 MB | 1 GB si entrenas leaves |
| Sistema | Windows 10, o un Linux actual de 64 bits | |

El nivel Extremo y el solver de finales usan todos los núcleos que se les den. En un portátil con batería, baja el tiempo para pensar.

## Comprobar la descarga

Cada fichero se publica con su SHA-256. Para comprobarlo:

```
sha256sum Kingbble-x86_64.AppImage          # Linux
certutil -hashfile Kingbble-Setup.exe SHA256   # Windows
```

El resultado tiene que coincidir con el publicado junto a la descarga. Cada versión se etiqueta además en el repositorio con el código fuente exacto del que salieron sus binarios, como exige la licencia.

## macOS

No hay versión de macOS prevista para la primera publicación. El motor y la interfaz se compilan desde el código en macOS con los mismos pasos que en Linux: mira [compilar desde el código](build).

## Desinstalar

**Windows**: *Configuración, Aplicaciones*, o borra la carpeta de la versión portable. **Linux**: borra el AppImage, o `sudo apt remove kingbble`. Los ajustes y los leaves que hayas entrenado están en la carpeta de datos de la app (`%APPDATA%\Kingbble` en Windows, `~/.config/Kingbble` en Linux); bórrala para no dejar nada.

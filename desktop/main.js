// Kingbble desktop app: starts the local service (server.js, which starts the
// engine) on a free port and shows the interface in its own window. No Python.
//
//   Development: build the bridge (cmake --build build --target kingbble_bridge), put a
//   word list and its leaves in lexica/ (e.g. ENABLE.txt and ENABLE.csv), then
//   cd desktop && npm install && npm start -- [--threads 4] [--lexica DIR] [--human-params FILE]
//
//   Packaged: the bridge, the word list and the leaves are in the app's resources.

const { app, BrowserWindow, Menu, shell, dialog } = require("electron");
const os = require("os");
const path = require("path");
const { createServer, bridgeName } = require("./server");

let service = null;

function option(args, name) {
  const i = args.indexOf(name);
  return i >= 0 && i + 1 < args.length ? args[i + 1] : undefined;
}

function options() {
  // `npm start -- ARGS` runs `electron . ARGS`: everything after the app path.
  const args = process.argv.slice(app.isPackaged ? 1 : 2).filter((a) => a !== "--");
  const cores = typeof os.availableParallelism === "function" ? os.availableParallelism() : os.cpus().length;
  const threads = Number(option(args, "--threads")) || Math.max(1, Math.min(cores, 16));
  if (app.isPackaged) {
    const res = process.resourcesPath;
    return {
      staticDir: path.join(res, "ui"),
      bridge: path.join(res, "engine", bridgeName()),
      lexicaDir: option(args, "--lexica") || path.join(res, "lexica"),
      threads,
      human: option(args, "--human-params"),
    };
  }
  const root = path.resolve(__dirname, "..");
  return {
    staticDir: path.join(root, "ui", "static"),
    bridge: option(args, "--bridge") || path.join(root, "build", "engine", bridgeName()),
    lexicaDir: option(args, "--lexica") || path.join(root, "lexica"),
    threads,
    human: option(args, "--human-params"),
  };
}

async function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 900,
    minWidth: 420,
    minHeight: 640,
    title: "Kingbble",
    icon: path.join(__dirname, "icon.png"),
    backgroundColor: "#0b0b10",
    titleBarStyle: process.platform === "darwin" ? "hiddenInset" : "default",
    autoHideMenuBar: true,
    // the intro plays its sound before any click
    webPreferences: { autoplayPolicy: "no-user-gesture-required" },
  });
  win.webContents.setWindowOpenHandler(({ url }) => {
    if (/^https?:/.test(url)) shell.openExternal(url);
    return { action: "deny" };
  });
  try {
    service = createServer(options());
    win.loadURL(await service.listen(0));
  } catch (e) {
    const msg = `Kingbble could not start its engine.\n\n${e.message}`;
    dialog.showErrorBox("Kingbble", msg);
    win.loadURL(`data:text/html,<body style="font:16px system-ui;padding:40px;white-space:pre-wrap">${encodeURIComponent(msg)}</body>`);
  }
}

if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on("second-instance", () => {
    const [win] = BrowserWindow.getAllWindows();
    if (win) {
      if (win.isMinimized()) win.restore();
      win.focus();
    }
  });
  Menu.setApplicationMenu(null);
  app.whenReady().then(createWindow);
  app.on("window-all-closed", () => app.quit());
  app.on("quit", () => service && service.close());
}

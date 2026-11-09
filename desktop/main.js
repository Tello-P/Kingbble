// Kingbble desktop app: starts the local server (ui/server.py, which starts the
// engine) on a free port and shows the interface in its own window.
//
//   cd desktop && npm install && npm start -- [server options, e.g. --threads 4]
//
// KINGBBLE_PYTHON picks the Python interpreter (default python3, or python on Windows).

const { app, BrowserWindow, Menu, shell } = require("electron");
const { spawn } = require("child_process");
const path = require("path");

const root = path.resolve(__dirname, "..");
let server = null;

function startServer() {
  return new Promise((resolve, reject) => {
    const python = process.env.KINGBBLE_PYTHON || (process.platform === "win32" ? "python" : "python3");
    const dash = process.argv.indexOf("--");
    const extra = dash >= 0 ? process.argv.slice(dash + 1) : [];
    server = spawn(python, [path.join(root, "ui", "server.py"), "--port", "0", ...extra], { cwd: root });
    let out = "";
    server.stdout.on("data", (d) => {
      out += d;
      const m = out.match(/http:\/\/[\d.]+:\d+/);
      if (m) resolve(m[0]);
    });
    server.stderr.on("data", (d) => process.stderr.write(d));
    server.on("exit", (code) => reject(new Error(`the Kingbble server stopped (code ${code})`)));
  });
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
  });
  try {
    win.loadURL(await startServer());
  } catch (e) {
    win.loadURL(`data:text/html,<body style="font:16px system-ui;padding:40px">${encodeURIComponent(e.message)}</body>`);
  }
  win.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: "deny" };
  });
}

Menu.setApplicationMenu(null);
app.whenReady().then(createWindow);
app.on("window-all-closed", () => app.quit());
app.on("quit", () => server && server.kill());

const { app, BrowserWindow, ipcMain } = require('electron')
const { spawn, execFile }             = require('child_process')
const readline                        = require('readline')
const path                            = require('path')

const VISOR_EXE = path.join(__dirname, '..', 'visor.exe')

let mainWindow   = null
let activeProc   = null

/* ── Window ─────────────────────────────────────────────────────────────── */

function createWindow() {
    mainWindow = new BrowserWindow({
        width:  1280,
        height: 820,
        minWidth:  900,
        minHeight: 600,
        backgroundColor: '#0f1117',
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            nodeIntegration: false,
            contextIsolation: true,
        },
        title: 'Visor Consola',
        show: false,
    })
    mainWindow.loadFile(path.join(__dirname, 'index.html'))
    mainWindow.once('ready-to-show', () => mainWindow.show())
    mainWindow.on('closed', () => { killActive(); mainWindow = null })
}

app.whenReady().then(createWindow)
app.on('window-all-closed', () => { killActive(); app.quit() })

/* ── Process management ─────────────────────────────────────────────────── */

function killActive() {
    if (activeProc) {
        try { activeProc.kill('SIGTERM') } catch (_) {}
        activeProc = null
    }
}

function runVisor(args, onEvent) {
    killActive()

    const proc = spawn(VISOR_EXE, ['--json', ...args], {
        cwd: path.join(__dirname, '..'),
    })
    activeProc = proc

    const rl = readline.createInterface({ input: proc.stdout, crlfDelay: Infinity })
    rl.on('line', line => {
        line = line.trim()
        if (!line) return
        if (line.startsWith('{')) {
            try { onEvent(JSON.parse(line)) }
            catch (_) { sendLog(line) }
        } else {
            sendLog(line)
        }
    })

    proc.stderr.on('data', d => sendLog(d.toString().trim()))
    proc.on('close', code => {
        if (activeProc === proc) activeProc = null
        onEvent({ t: 'proc_closed', code })
    })

    return proc
}

function sendLog(line) {
    if (mainWindow && line) mainWindow.webContents.send('log', line)
}

function sendEvent(e) {
    if (mainWindow) mainWindow.webContents.send('visor-event', e)
}

/* ── IPC handlers ───────────────────────────────────────────────────────── */

ipcMain.handle('list-ports', () =>
    new Promise(resolve => {
        let result = []
        runVisor(['list-ports'], e => {
            if (e.t === 'ports')      result = e.list
            if (e.t === 'proc_closed') resolve(result)
        })
    })
)

ipcMain.handle('get-config', () =>
    new Promise(resolve => {
        let result = null
        runVisor(['get-config'], e => {
            if (e.t === 'config')     result = e
            if (e.t === 'proc_closed') resolve(result)
        })
    })
)

ipcMain.handle('set-config', (_, key, value) =>
    new Promise(resolve => {
        runVisor(['set', key, String(value)], e => {
            if (e.t === 'proc_closed') resolve(true)
        })
    })
)

ipcMain.on('start-monitor', () => {
    runVisor(['monitor'], e => {
        if (e.t !== 'proc_closed') sendEvent(e)
        else                       sendEvent({ t: 'disconnected' })
    })
})

ipcMain.on('start-diagnostic', () => {
    runVisor(['diagnostic'], e => {
        if (e.t !== 'proc_closed') sendEvent(e)
        else                       sendEvent({ t: 'diag_closed' })
    })
})

ipcMain.on('restore', () => {
    runVisor(['restore'], e => {
        if (e.t !== 'proc_closed') sendEvent(e)
    })
})

ipcMain.on('stop', () => {
    killActive()
    sendEvent({ t: 'stopped' })
})

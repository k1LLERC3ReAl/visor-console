const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('visor', {
  listPorts:       ()          => ipcRenderer.invoke('list-ports'),
  getConfig:       ()          => ipcRenderer.invoke('get-config'),
  setConfig:       (key, val)  => ipcRenderer.invoke('set-config', key, val),
  startMonitor:    ()          => ipcRenderer.send('start-monitor'),
  startDiagnostic: ()          => ipcRenderer.send('start-diagnostic'),
  restore:         ()          => ipcRenderer.send('restore'),
  stop:            ()          => ipcRenderer.send('stop'),
  onEvent:  (cb) => ipcRenderer.on('visor-event', (_, e) => cb(e)),
  onLog:    (cb) => ipcRenderer.on('log', (_, line) => cb(line)),
  removeAllListeners: () => {
    ipcRenderer.removeAllListeners('visor-event')
    ipcRenderer.removeAllListeners('log')
  }
})

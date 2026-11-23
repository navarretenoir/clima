// Puente JS→host con la misma forma que WebKitGTK: window.webkit.messageHandlers.bridge.postMessage(str)
const { ipcRenderer } = require("electron");
window.webkit = { messageHandlers: { bridge: { postMessage: (s) => ipcRenderer.send("bridge", String(s)) } } };

//
// Application Singleton
//
import {RPC} from "./rpc.js";

const RPC_URL = "/rpc1";

// Creates RPC manager for
const rpc = new RPC(RPC_URL);
export {rpc};

// Internal private state
const tabViews = new Map();
const activeTabViews = new Map();

export function setTabViewFunc(id, getFunc) {

    tabViews.set(id, getFunc);
}

export function getTabViewFunc(id) {

    return tabViews.get(id);
}

export function isTabViewActive(id) {

    if (activeTabViews.get(id)) {
        return true;;
    }
    return false;
}

export function setTabViewActive(id, state) {

    activeTabViews.set(id, state);
}

// // Exported Application Events
// class Events extends EventTarget {
//     constructor() {
//         super();
//     }
// }
// export const events = new Events();
// export const EvWebSocketOpen   = "websocket.open";
// export const EvWebSocketClose  = "websocket.close";
// export const EvWebSocketError  = "websocket.error";
// export const EvWebSocketMsg    = "websocket.msg";
//
// // Internal private state
// const webSockets = new Map();
// const tabViews = new Map();
// const activeTabViews = new Map();
//
//
// // // Sends request to the specified url with JSON body .
// // // The data sent include 'mid' field for compatibility with WebSocket messages.
// // // Returns promise.
// // export function postJson(url, data) {
// //
// //     const req = {
// //         mid: 0,
// //         msg: data,
// //     }
// //     const body = JSON.stringify(req);
// //     const promise = fetch(url, {
// //         method: "POST",
// //         mode:   "cors",
// //         cache:  "no-cache",
// //         headers: {
// //             "Content-Type": "Application/json",
// //         },
// //         body: body,
// //     });
// //     return promise.then((resp) => resp.json());
// // }
//
// export function rpcCall(url, name, params=null, cb=undefined) {
//
//     // Get the WebSocket for this url
//     const ws = webSockets.get(url);
//     if (!ws || ws.wsocket.readyState >= WebSocket.CLOSING) {
//         return `WebSocket:${url} connection not open`;
//     }
//
//     // Builds request with user message
//     const req = {
//         mid:    ws.mid,
//         call:   name,
//         params: params,
//     };
//     const json = JSON.stringify(req);
//     console.log("JSON", req, json);
//     ws.wsocket.send(json);
//
//     // If callback not defined
//     if (cb === undefined) {
//         return;
//     }
//
//     // Saves callback associated with message id
//     // TODO timeout for responses ??
//     ws.callbacks.set(ws.mid, cb);
//     ws.mid += 1;
// }
//
// // Open WebSocket at the specified relative URL.
// // Returns message on error.
// export function openWebSocket(url) {
//
//     // Checks if websocket already created
//     const ws = webSockets.get(url);
//     if (ws && ws.wsocket.readyState <= WebSocket.OPEN) {
//         return "WebSocket connection already open";
//     }
//
//     // Open web socket connection with server using the supplied relative URL
//     const address = 'ws://' + document.location.host + "/" + url;
//     const wsocket = new WebSocket(address);
//
//     // Add listener for all this WebSocket events
//     wsocket.addEventListener("open", webSocketListener);
//     wsocket.addEventListener("close", webSocketListener);
//     wsocket.addEventListener("error", webSocketListener);
//     wsocket.addEventListener("message", webSocketListener);
//
//     // Maps this WebSocket url with info object:
//     webSockets.set(url, {
//         wsocket:    wsocket,    // WebSocket object
//         mid:        1,          // Next message id
//         callbacks:  new Map(),  // Maps sent message 'mid' with associated callback
//     });
// }
//
// // Closes connection with the specified relative url
// // Returns error message on errors.
// export function closeWebSocket(url) {
//
//     // Get the WebSocket for this url
//     const ws = webSockets.get(url);
//     if (!ws || ws.wsocket.readyState >= WebSocket.CLOSING) {
//         return "WebSocket already closed";
//     }
//     ws.wsocket.close();
// }
//
// // Returns the WebSocket connection status for the specified url.
// // Returns null if WebSocket not previously created.
// export function getWebSocketStatus(url) {
//
//     const ws = webSockets.get(url);
//     if (!ws) {
//         return null;
//     }
//     return ws.readyState;
// }
//
// // // Sends message through the WebSocket for the specified url.
// // // Returns error message on errors.
// // export function sendWebSocketMsg(url, msg) {
// //
// //     // Get the WebSocket for this url
// //     const ws = webSockets.get(url);
// //     if (!ws || ws.wsocket.readyState >= WebSocket.CLOSING) {
// //         return `WebSocket:${url} connection not open`;
// //     }
// //     ws.wsocket.send(msg);
// // }
// //
// // // Sends request through the WebSocket for the specified url
// // // Returns error message on errors.
// // export function sendWebSocketJson(url, msg, cb = undefined) {
// //
// //     // Get the WebSocket for this url
// //     const ws = webSockets.get(url);
// //     if (!ws || ws.wsocket.readyState >= WebSocket.CLOSING) {
// //         return `WebSocket:${url} connection not open`;
// //     }
// //
// //     // If callback not defined, converts msg to JSON and sends it
// //     if (cb === undefined) {
// //         const json = JSON.stringify(msg);
// //         ws.wsocket.send(json);
// //         return;
// //     }
// //
// //     // Builds request with user message
// //     const req = {
// //         mid:    ws.mid,
// //         msg:    msg,
// //     };
// //     const json = JSON.stringify(req);
// //     ws.wsocket.send(json);
// //
// //     // Saves callback associated with message id
// //     // TODO timeout for responses ??
// //     ws.callbacks.set(ws.mid, cb);
// //     ws.mid += 1;
// // }
//
// // Internal listener for WebSocket events
// function webSocketListener(ev) {
//
//     // Get WebSocket associated with this event url
//     const url = ev.target.url.substring(ev.target.url.lastIndexOf('/'));
//     const ws = webSockets.get(url);
//     if (!ws) {
//         console.log("Event for WebSocket not registered");
//         return;
//     }
//
//     // Dispatch WebSocket event
//     let appEvent;
//     switch (ev.type) {
//         case "open":
//             appEvent = EvWebSocketOpen;
//             break;
//         case "close":
//             appEvent = EvWebSocketClose;
//             ws.wsocket.removeEventListener("open", webSocketListener);
//             ws.wsocket.removeEventListener("close", webSocketListener);
//             ws.wsocket.removeEventListener("error", webSocketListener);
//             ws.wsocket.removeEventListener("message", webSocketListener);
//             webSockets.delete(url);
//             break;
//         case "error":
//             appEvent = EvWebSocketError;
//             break;
//         case "message":
//             appEvent = EvWebSocketMsg;
//             const msg = JSON.parse(ev.data);
//             if (msg.mid !== undefined) {
//                 const cb = ws.callbacks.get(msg.mid);
//                 if (!cb) {
//                     console.log("WebSocket callback not found", resp);
//                     return;
//                 }
//                 cb(msg.resp);
//                 ws.callbacks.delete(msg.mid);
//             }
//             break;
//     }
//     const customEvent = new CustomEvent(appEvent, {
//         detail: { url: url, data: ev.data},
//     });
//     events.dispatchEvent(customEvent);
//
// }
//
//
//

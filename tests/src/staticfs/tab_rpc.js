import * as App from "./app.js";
import {RPC} from "./rpc.js";

const VIEW_ID = "tab.wsocket";

function logEvent(msg) {

    const now = new Date().toISOString().substring(11,23);
    $$('list.wsocket').add({time:now, event:msg}, 0);
}
                    
function logRPC(ev) {

    switch (ev.type) {
        case RPC.EV_OPENED:
            logEvent(`RPC: ${ev.detail.url} opened`);
            break;
        case RPC.EV_CLOSED:
            logEvent(`RPC: ${ev.detail.url} closed`);
            break;
        case RPC.EV_ERROR:
            logEvent(`RPC: ${ev.detail.url} error`);
            break;
    }
}

function getLines(count, len) {

    const emsg = App.rpc.call("get_lines", {count, len}, (resp)=> {
        if (resp.err) {
            logEvent(`ERROR: ${resp.err}`);
            return;
        }
        const elapsed = App.rpc.lastCallElapsed();
        const lines = resp.data;
        let lineCount = 0;
        let colLen = 0;
        for (let i = 0; i < lines.length; i++) {
             lineCount++;
             colLen += lines[i].length
        }
        logEvent(`lines: ${lineCount}, len: ${colLen/lineCount}  elapsed: ${elapsed.toFixed(3)}ms`);
    });
    if (emsg) {
        logEvent(`ERROR: ${emsg}`);
    }
}

App.rpc.bind("client_callback", function(params) {

    logEvent(`client_callback: ${params}`);
    return {data: 'client_callback_response'};
});

// Set the menu option ids
const optionClear       = "rpc.clear";
const optionOpen        = "rpc.open";
const optionClose       = "rpc.close";
const optionGetTime     = "rpc.get_time";
const optionGetLines1   = "rpc.get_lines1";
const optionGetLines2   = "rpc.get_lines2";
const optionGetLines3   = "rpc.get_lines3";
const optionCallClient  = "rpc.call_client";

const optionSumBinArray = "rpc.sum_bin_array";

// Builds map of menu option id to function handler
const optionMap = new Map();

optionMap.set(optionClear, function() {
    $$('list.wsocket').clearAll();
});

optionMap.set(optionOpen, function() {
    const emsg = App.rpc.open();
    if (emsg) {
        logEvent(emsg);
        return;
    }
    App.rpc.addEventListener(RPC.EV_OPENED, logRPC);
    App.rpc.addEventListener(RPC.EV_CLOSED, logRPC);
    App.rpc.addEventListener(RPC.EV_ERROR, logRPC);
});

optionMap.set(optionClose, function() {
    const emsg = App.rpc.close();
    if (emsg) {
        logEvent(emsg);
        return;
    }
});

optionMap.set(optionGetTime, function() {

    const emsg = App.rpc.call("get_time", null, (resp)=> {
        if (resp.err) {
            logEvent("ERROR", resp.err);
            return;
        }
        const elapsed = App.rpc.lastCallElapsed();
        logEvent(`${resp.data}  elapsed:${elapsed.toFixed(3)}ms`);
    });
    if (emsg) {
        logEvent(`ERROR: ${emsg}`);
        return;
    }
});

optionMap.set(optionGetLines1, () => getLines(1, 1024));
optionMap.set(optionGetLines2, () => getLines(100, 1024));
optionMap.set(optionGetLines3, () => getLines(10000, 1024));

optionMap.set(optionCallClient, () => {

    const emsg = App.rpc.call("call_client", {name: 'client_callback', value: 42}, (resp)=> {
        if (resp.err) {
            logEvent(`ERROR: ${emsg}`);
            return;
        }
        logEvent(`call_client resp: ${resp.data}`);
    });
    if (emsg) {
        logEvent(`ERROR: ${emsg}`);
    }
});

optionMap.set(optionSumBinArray, () => {

    console.log("------------------------------------");

    const buf1 = new Uint8Array(3);
    for (let i  = 0; i < buf1.length; i++) {
        buf1[i] = i + 10;
    }
    const buf2 = new Uint16Array(3);
    for (let i  = 0; i < buf2.length; i++) {
        buf2[i] = i + 10;
    }
    const buf3 = new Uint32Array(3);
    for (let i  = 0; i < buf3.length; i++) {
        buf3[i] = i + 10;
    }
    const buf4 = new Float32Array(3);
    for (let i  = 0; i < buf4.length; i++) {
        buf4[i] = i + 10;
    }
    const buf5 = new Float64Array(3);
    for (let i  = 0; i < buf5.length; i++) {
        buf5[i] = i + 10;
    }
    const emsg = App.rpc.call_bin("sum_bin_array", {array:0}, [buf1, buf2, buf3, buf4, buf5], (resp)=> {
    });
    if (emsg) {
        logEvent(`ERROR: ${emsg}`);
    }
});

// Returns this tab view
export function getView() {

    const tabView = {
        header: "RPC",
        close: true,
        body: {
            id: VIEW_ID,
            on: {
                onDestruct: function() {
                    App.setTabViewActive(VIEW_ID, false);
                }
            },
            cols: [
                // Siderbar menu
                {
                    view: "sidebar",
                    id: "rpc.sidebar",
                    data: [
                        { id: optionOpen,  value: "Open RPC connection", icon:"open"},
                        { id: optionClose, value: "Close RPC connection", icon:"close"},
                        { id: optionClear, value: "Clear log", icon:"clear"},
                        { value: "JSON", icon:"json",
                            data: [
                                { id: optionGetTime,   value: "get_time()"},
                                { id: optionGetLines1, value: "get_lines(1x1024)"},
                                { id: optionGetLines2, value: "get_lines(100x1024)"},
                                { id: optionGetLines3, value: "get_lines(10.000x1024)"},
                                { id: optionCallClient, value: "call_client()"},
                            ],
                        },
                        { value: "Binary", icon:"binary",
                            data: [
                                { id: optionSumBinArray,   value: "sum_bin_array()"},
                            ],
                        }
                    ],
                    on: {
                        onItemClick: function(id) {
                            //console.log(id);
                            const handler = optionMap.get(id);
                            if (handler) {
                                handler();
                            }
                        }
                    }
                },
                // List with events
                {
                    view: "list",
                    id: "list.wsocket",
                    template: "#time# | #event#"
                },
            ],
        }
    };
    return {viewId: VIEW_ID, view: tabView};
}

import * as App from "./app.js";
import {RPC} from "./rpc.js";

const VIEW_ID = "tab.rpc";
const COMBO_COUNT_ID = "tab.rpc.combo.count";
const COMBO_SIZE_ID = "tab.rpc.combo.size";
const TEXT_COUNT_ID = "tab.rpc.text.count";
const TEXT_SIZE_ID = "tab.rpc.text.size";
const CHECK_ASYNC = "tab.rpc.check.async";

// Creates RPC manager
const RPC_URL = "/rpc1";
const rpc = new RPC(RPC_URL);

function getRandomInt(max) {

  return Math.floor(Math.random() * max);
}

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


function callTextMsg(count, size, async) {

    // Build call parameters
    const params = {size};
    params.arr = [];
    for (let i = 0; i < size; i++) {
        params.arr.push(getRandomInt(1000));
    }

    const time_start = performance.now();
    let resp_count = 0;

    const response = function(resp) {
        // Checks response
        const resp_size = resp.data.size;
        if (resp_size != size) {
            logEvent(`ERROR: invalid response size`);
            return;
        }
        for (let i = 0 ; i < size; i++) {
            if (resp.data.arr[i] != params.arr[i]+1) {
                logEvent(`ERROR: invalid response array`);
            }
        }

        resp_count++;
        if (resp_count == count) {
            const elapsed = (performance.now() - time_start).toFixed(2);
            logEvent(`Received: ${resp_count} responses in ${elapsed}`);
            return;
        }

        if (!async) {
            if (call_remote()) {
                return;
            }
        }
    };

    const call_remote = function() {
        const emsg = rpc.call("rpc_server_text_msg", params, response);
        if (emsg) {
            logEvent(`ERROR: ${emsg}`);
        }
    }

    if (async) {
        for (let c = 0; c < count; c++) {
            if (call_remote()) {
                return;
            }
        }
    } else {
        call_remote();
    }
}

function callBinaryMsg(count, len) {

    const u8 = new Uint8Array(len);
    for (let i  = 0; i < u8.length; i++) {
         u8[i] = i;
    }

    const u16 = new Uint16Array(len);
    for (let i  = 0; i < u16.length; i++) {
         u16[i] = i;
    }

    const u32 = new Uint32Array(len);
    for (let i  = 0; i < u32.length; i++) {
         u32[i] = i;
    }

    const f32 = new Float32Array(len);
    for (let i  = 0; i < f32.length; i++) {
         f32[i] = i;
    }

    const f64 = new Float64Array(len);
    for (let i  = 0; i < f64.length; i++) {
         f64[i] = i;
    }

    const emsg = rpc.call("sum_bin_arrays", {u8, u16, u32, f32, f64});
    if (emsg) {
        logEvent(`ERROR: ${emsg}`);
    }
}


rpc.bind("test_bin", function(params) {

    console.log("test_bin", params);

    const u32 = new Uint32Array(params.u32);
    for (let i = 0; i < u32.length; i++) {
        u32[i] += 1;
    }

    const f32 = new Float32Array(params.f32);
    for (let i = 0; i < f32.length; i++) {
        f32[i] += 1;
    }

    const f64 = new Float64Array(params.f64);
    for (let i = 0; i < f64.length; i++) {
        f64[i] += 1;
    }

    return {data: {u32, f32, f64}};
});

rpc.bind("client_callback", function(params) {

    logEvent(`client_callback: ${params}`);
    return {data: 'client_callback_response'};
});

// optionMap.set(optionClear, function() {
//     $$('list.wsocket').clearAll();
// });
//
// optionMap.set(optionOpen, function() {
//     const emsg = rpc.open();
//     if (emsg) {
//         logEvent(emsg);
//         return;
//     }
//     rpc.addEventListener(RPC.EV_OPENED, logRPC);
//     rpc.addEventListener(RPC.EV_CLOSED, logRPC);
//     rpc.addEventListener(RPC.EV_ERROR, logRPC);
// });
//
// optionMap.set(optionClose, function() {
//     const emsg = rpc.close();
//     if (emsg) {
//         logEvent(emsg);
//         return;
//     }
// });
//
// optionMap.set(optionGetTime, function() {
//
//     const emsg = rpc.call("get_time", null, (resp)=> {
//         if (resp.err) {
//             logEvent("ERROR", resp.err);
//             return;
//         }
//         const elapsed = rpc.lastCallElapsed();
//         logEvent(`${resp.data}  elapsed:${elapsed.toFixed(3)}ms`);
//     });
//     if (emsg) {
//         logEvent(`ERROR: ${emsg}`);
//         return;
//     }
// });
//
// optionMap.set(optionGetLines1, () => getLines(1, 1024));
// optionMap.set(optionGetLines2, () => getLines(100, 1024));
// optionMap.set(optionGetLines3, () => getLines(10000, 1024));
//
// optionMap.set(optionCallClient, () => {
//
//     const emsg = rpc.call("call_client", {name: 'client_callback', value: 42}, (resp)=> {
//         if (resp.err) {
//             logEvent(`ERROR: ${emsg}`);
//             return;
//         }
//         logEvent(`call_client resp: ${resp.data}`);
//     });
//     if (emsg) {
//         logEvent(`ERROR: ${emsg}`);
//     }
// });
//
// optionMap.set(optionSumBinArrays, () => {
//
//     sumBinArrays(1_000_000);
// });

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
            rows: [
                {
                    view: "toolbar",
                    cols: [
                        {
                            view:   "button",
                            label:  "Open",
                            css:    "webix_primary",
                            autowidth:  true,
                            click: function() {
                                const emsg = rpc.open();
                                if (emsg) {
                                    logEvent(emsg);
                                    return;
                                }
                                rpc.addEventListener(RPC.EV_OPENED, logRPC);
                                rpc.addEventListener(RPC.EV_CLOSED, logRPC);
                                rpc.addEventListener(RPC.EV_ERROR, logRPC);
                            }
                        },
                        {
                            view:   "button",
                            label:  "Close",
                            css:    "webix_primary",
                            autowidth:  true,
                            click: function() {
                                const emsg = rpc.close();
                                if (emsg) {
                                    logEvent(emsg);
                                    return;
                                }
                            }
                        },
                        {
                            view:   "button",
                            label:  "Clear log",
                            css:    "webix_alert",
                            autowidth:  true,
                            click: function() {
                                $$('list.wsocket').clearAll();
                            }
                        },
                        { width: 20},
                        {
                            view:   "button",
                            label:  "Call Text",
                            css:    "webix_danger",
                            autowidth:  true,
                            click: function() {
                                const count = parseInt($$(TEXT_COUNT_ID).getValue());
                                const size = parseInt($$(TEXT_SIZE_ID).getValue());
                                const async = $$(CHECK_ASYNC).getValue();
                                if (count == NaN || size == NaN) { return;}
                                console.log("count", count, "size", size, "async", async);
                                callTextMsg(count, size, async);
                            }
                        },
                        { width: 20},
                        {
                            view:   "button",
                            label:  "Call Binary",
                            css:    "webix_danger",
                            autowidth:  true,
                            click: function() {
                                const count = parseInt($$(TEXT_COUNT_ID).getValue());
                                const size = parseInt($$(TEXT_SIZE_ID).getValue());
                                const async = $$(CHECK_ASYNC).getValue();
                                if (count == NaN || size == NaN) { return;}
                                console.log("count", count, "size", size, "async", async);
                            }
                        },
                        {
                            view:   "text",
                            id: TEXT_COUNT_ID,
                            label: "Message count:",
                            labelAlign: "right",
                            labelWidth: 120,
                            placeholder: "count",
                            value: "1",
                            width: 200,
                        },
                        {
                            view:   "text",
                            id: TEXT_SIZE_ID,
                            label: "Array size:",
                            labelAlign: "right",
                            placeholder: "size",
                            value: "1",
                            width: 200,
                        },
                        {
                            view:   "checkbox",
                            id: CHECK_ASYNC,
                            labelAlign: "right",
                            label: "Async:",
                            value: 0,
                            width: 200,
                        },
                    ],
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

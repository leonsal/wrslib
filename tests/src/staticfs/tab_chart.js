import * as App from "./app.js";
import {RPC} from "./rpc.js";

const RPC_URL = "/rpc2";
const VIEW_ID = "tab.chartjs";
const SLIDER_FREQ_ID = "tab.chartjs.slider.freq";
const SLIDER_NOISE_ID = "tab.chartjs.noise.freq";
const SLIDER_POINTS_ID = "tab.chartjs.slider.points";
const SLIDER_FPS_ID = "tab.chartjs.slider.fps";
const CHART_ID = "tab.chartjs.chart";
const SLIDER_WIDTH = 160;
const rpc = new RPC(RPC_URL);

let timeoutId = null;
let lastRequest = null;
let lastFPS = null;

function requestChart() {

    //console.log(`elapsed": ${(performance.now() - lastRequest).toFixed(2)}`);
    lastRequest = performance.now();

    rpc.call("rpc_server_chart_run", {}, (resp) => {
 
        // Get chart labels and signal
        const label = new Float32Array(resp.data.label);
        const signal = new Float32Array(resp.data.signal);

        // Updates chart
        $$(CHART_ID).chart.data.labels = Array.from(label);
        $$(CHART_ID).chart.data.datasets[0].data = Array.from(signal);
        $$(CHART_ID).chart.update();
    });

    // Changes update interval if FPS changed 
    const fps = $$(SLIDER_FPS_ID).getValue();
    if (timeoutId === null || fps != lastFPS) {
        clearTimeout(timeoutId);
        const delayMs = (1.0 / fps) * 1000;
        timeoutId = setInterval(requestChart, delayMs);
        lastFPS = fps;
    }
}

function rpcEvents(ev) {

    if (ev.type == RPC.EV_OPENED) {

        // Updates chart parameters
        const freq = $$(SLIDER_FREQ_ID).getValue();
        const noise=  $$(SLIDER_NOISE_ID).getValue();
        const npoints =  $$(SLIDER_POINTS_ID).getValue();
        lastFPS = $$(SLIDER_FPS_ID).getValue();
        rpc.call("rpc_server_chart_set", {freq, noise, npoints});

        // Starts chart request
        lastRequest = performance.now();
        requestChart();
        return;
    }

    if (ev.type == RPC.EV_CLOSED) {
        // Stops chart request
        clearTimeout(timeoutId);
        timeoutId = null;
        return;
    }
}


// Returns this view
export function getView() {

    const tabView = {
        header: "Chart",
        close: true,
        body: {
            id: VIEW_ID,
            on: {
                onDestruct: function() {
                    rpc.close();
                    App.setTabViewActive(VIEW_ID, false);
                }
            },
            rows: [
                // Toolbar with buttons
                {
                    view: "toolbar",
                    cols: [
                        {
                            view:   "button",
                            label:  "Start",
                            css:    "webix_primary",
                            autowidth:  true,
                            click: function() {
                                const emsg = rpc.open();
                                if (emsg) {
                                    console.log(emsg);
                                    return;
                                }
                                rpc.addEventListener(RPC.EV_OPENED, rpcEvents);
                                rpc.addEventListener(RPC.EV_CLOSED, rpcEvents);
                                rpc.addEventListener(RPC.EV_ERROR, rpcEvents);
                            },
                        },
                        {
                            view:   "button",
                            label:  "Stop",
                            css:    "webix_primary",
                            autowidth:  true,
                            click: function() {
                                const emsg = rpc.close();
                                if (emsg) {
                                    console.log(emsg);
                                    return;
                                }
                            },
                        },
                        {
                            view:   "slider",
                            id:     SLIDER_FREQ_ID,
                            width:  SLIDER_WIDTH,
                            title:  webix.template("freq: #value#Hz"),
                            moveTitle: false,
                            value:  '300',
                            min:    10,
                            max:    8000,
                            on: {
                                onSliderDrag: function() {
                                    rpc.call("rpc_server_chart_set", {freq: this.getValue()});
                                },
                                onChange:function(){
                                    rpc.call("rpc_server_chart_set", {freq: this.getValue()});
                                },
                            },
                        },
                        {
                            view:   "slider",
                            id:     SLIDER_NOISE_ID,
                            width:  SLIDER_WIDTH,
                            title:  webix.template("noise: #value#%"),
                            moveTitle: false,
                            value:  '0',
                            min:    0,
                            max:    50,
                            on: {
                                onSliderDrag: function() {
                                    rpc.call("rpc_server_chart_set", {noise: this.getValue()});
                                },
                                onChange:function(){
                                    rpc.call("rpc_server_chart_set", {noise: this.getValue()});
                                },
                            },
                        },
                        {
                            view:   "slider",
                            id:     SLIDER_POINTS_ID,
                            width:  SLIDER_WIDTH,
                            title:  webix.template("npoints: #value#"),
                            moveTitle: false,
                            value:  '1024',
                            min:    16,
                            max:    2048,
                            on: {
                                onSliderDrag: function() {
                                    rpc.call("rpc_server_chart_set", {npoints: this.getValue()});
                                },
                                onChange:function(){
                                    rpc.call("rpc_server_chart_set", {npoints: this.getValue()});
                                },
                            },
                        },
                        {
                            view:   "slider",
                            id:     SLIDER_FPS_ID,
                            width:  SLIDER_WIDTH,
                            title:  webix.template("fps: #value#"),
                            moveTitle: false,
                            value:  '20',
                            min:    2,
                            max:    60,
                        },
                    ],
                },
                // Chart
                {
                    view: "chartjs",
                    id:   CHART_ID,
		            css:  {"background-color": "white"}, // canvas backgroud
                    // Start of Chart.js configuration
                    config: {
			            type: "line",
			            data: {
				            labels: ['1','2','3','4','5'],
				            datasets: [
                                {
                                    label: "signal",
                                    data:  [1,2,3,4,5],
                                    borderColor: "blue",
                                    borderWidth: 1,
                                    pointRadius: 0,
                                },
                            ]
					    },
                        options: {
                            animation: false,
                            maintainAspectRatio: false,
                            plugins: {
                                title: {
                                    display: true,
                                    text:    "Chart1 title",
                                },
                            },
                            legend: {
                                display: false,
                            },
                            scales: {
                                "x": {
                                    "type": "linear",
                                },
                            },
                        },
                    },
			    },
            ],
        }
    };
    return {viewId: VIEW_ID, view: tabView};
}


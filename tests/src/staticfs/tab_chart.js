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

class AudioStream extends EventTarget {

    constructor(bufSize=4410, minBufs=2) {

        super();
        this.#bufSize = bufSize;
        this.#minBufs = minBufs;
    }

    open() {

        this.#ctx = new AudioContext();
    }

    close() {

        if (this.#ctx === null) {
            return;
        }
        this.#ctx.close();
        this.#ctx = null;
    }

    bufferSize() {

        return this.#bufSize;
    }

    appendData(arrayf32) {

        console.log("append", arrayf32.byteLength/4);
        this.#bufList.push(arrayf32);
        if (!this.#playing) {
            this.start();
        }
    }

    // Starts playing
    start() {

        if (this.#playing) {
            console.log("start(): already playing");
            return;
        }

        if (this.#bufList.length == 0) {
            console.log("start(): request data");
            const cev = new CustomEvent(AudioStream.EV_NEED_DATA, {
                detail: {bufSize: this.#bufSize}
            });
            this.dispatchEvent(cev); 
            return;
        }

        // Get next array from the list and create buffer
        const buffer = this.#ctx.createBuffer(
            1,                      // channels,
            this.#bufSize,          // length
            this.#ctx.sampleRate    // sample rate
        );
        console.log("buffer", buffer);
        const bufData = this.#bufList.shift();
        buffer.copyToChannel(bufData, 0);

        // Creates audio buffer source and set the buffer as the source
        const source = this.#ctx.createBufferSource();
        source.buffer = buffer;

        // Connects the buffer source to the destination
        source.connect(this.#ctx.destination);

        // Register handler to start playing again when current buffer ends.
        source.addEventListener('ended', (_) => {
            // Play next buffer 
            this.#playing = false;
            this.start();
            // If number of available buffers less than minimum,
            // requests more data.
            if (this.#bufList.length <= this.#minBufs) {
                const cev = new CustomEvent(AudioStream.EV_NEED_DATA, {
                    detail: {bufSize: this.#bufSize}
                });
                this.dispatchEvent(cev); 
            }
        });

        source.start();
        console.log("buffer list", this.#bufList.length);
        this.#playing = true;
    }

    // Stops playing
    stop() {

        this.#playing = false;
        this.#bufList = [];
    }

    // Public static properties
    static EV_NEED_DATA = "audiostream.need_data";

    // Private instance properties
    #bufSize    = 4410;
    #minBufs    = 2;
    #ctx        = null; // WebAudio context
    #bufList    = [];   // Buffer list
    #playing    = false;

};

let audio = new AudioStream(4410, 3);
audio.addEventListener(AudioStream.EV_NEED_DATA, () => {
    requestAudio();
    requestAudio();
});

function requestAudio() {

    //console.log(`elapsed": ${(performance.now() - lastRequest).toFixed(2)}`);
    lastRequest = performance.now();

    rpc.call("rpc_server_chart_run", {}, (resp) => {
 
        // Get chart labels and signal
        const label = new Float32Array(resp.data.label);
        const signal = new Float32Array(resp.data.signal);
        audio.appendData(signal);

        // Updates chart
        $$(CHART_ID).chart.data.labels = Array.from(label);
        $$(CHART_ID).chart.data.datasets[0].data = Array.from(signal);
        $$(CHART_ID).chart.update();
    });

    // // Changes update interval if FPS changed 
    // const fps = $$(SLIDER_FPS_ID).getValue();
    // if (timeoutId === null || fps != lastFPS) {
    //     clearTimeout(timeoutId);
    //     const delayMs = (1.0 / fps) * 1000;
    //     timeoutId = setInterval(requestAudio, delayMs);
    //     lastFPS = fps;
    // }
}

function rpcEvents(ev) {

    if (ev.type == RPC.EV_OPENED) {

        // Updates chart parameters
        const freq = $$(SLIDER_FREQ_ID).getValue();
        const noise=  $$(SLIDER_NOISE_ID).getValue();
        const npoints =  audio.bufferSize();
        lastFPS = $$(SLIDER_FPS_ID).getValue();
        rpc.call("rpc_server_chart_set", {freq, noise, npoints});

        audio.open();
        audio.start();
        // // Starts chart request
        // lastRequest = performance.now();
        // requestAudio();
        return;
    }

    if (ev.type == RPC.EV_CLOSED) {
        audio.stop();
        // // Stops chart request
        // clearTimeout(timeoutId);
        // timeoutId = null;
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


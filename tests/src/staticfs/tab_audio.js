import * as App from "./app.js";
import {RPC} from "./rpc.js";

const RPC_URL = "/rpc2";
const VIEW_ID = "tab.chartjs";
const COMBO_BUFSIZE_ID = "tab.audio.combo.bsize";
const COMBO_BUFCOUNT_ID = "tab.audio.combo.bcount";
const SLIDER_GAIN_ID = "tab.audio.slider.gain";
const SLIDER_FREQ_ID = "tab.audio.slider.freq";
const SLIDER_NOISE_ID = "tab.audio.slider.noise";
const SLIDER_FPS_ID = "tab.audio.slider.fps";
const CHART_ID = "tab.audio.chart";
const SLIDER_WIDTH = 160;
const rpc = new RPC(RPC_URL);


class AudioStream extends EventTarget {

    constructor() {

        super();
    }

    open(bufSize=4410, minBufs=2) {

        this.#bufSize = bufSize;
        this.#minBufs = minBufs;
        this.#ctx = new AudioContext();
    }

    close() {

        if (this.#ctx === null) {
            return;
        }
        this.#ctx.close();
        this.#ctx = null;
    }

    get sampleRate() {
        if (this.#ctx === null) {
            return null;
        }
        return this.#ctx.sampleRate;
    }

    get bufSize() {
        return this.#bufSize;
    }

    appendData(arrayf32) {

        if (arrayf32.length != this.#bufSize) {
            throw new Error('appendData() requires Float32Typed array with correct size');
        }

        if (this.#playTime === null) {
            this.#playTime = this.#ctx.currentTime;
        }

        // Creates buffer and copy data to buffer channel
        const buffer = this.#ctx.createBuffer(
            1,                      // channels,
            this.#bufSize,          // length
            this.#ctx.sampleRate    // sample rate
        );
        buffer.copyToChannel(arrayf32, 0);

        // Creates audio buffer source and set the buffer as the source
        const source = this.#ctx.createBufferSource();
        source.buffer = buffer;
        this.#bufList.push(source);
   
        // Schedule the time for this source to start to play
        source.connect(this.#ctx.destination);
        source.start(this.#playTime);
        this.#playTime += buffer.duration;

        // Sets handler to process when current buffer has ended playing.
        source.addEventListener('ended', (_) => {
            // Remove buffer from list
            this.#bufList.shift();
            // Request more data if necessary
            const bufCount = this.#bufList.length;
            if (bufCount == 0) {
                this.#playTime = null;
            }
            if (bufCount < this.#minBufs) {
                const cev = new CustomEvent(AudioStream.EV_NEED_DATA, {
                    detail: {bufCount: this.#minBufs - bufCount}
                });
                this.dispatchEvent(cev); 
            }
        });
    }

    // Starts playing
    start() {

        if (this.#playTime != null) {
            console.log("start(): already playing");
            return;
        }
        // Dispatch event to request audio data
        const cev = new CustomEvent(AudioStream.EV_NEED_DATA, {detail: {bufCount: this.#minBufs}});
        this.dispatchEvent(cev); 
    }

    // Stops playing
    stop() {

        // Stops all enqueued buffers
        for (let i = 0; i < this.#bufList.length; i++) {
            this.#bufList[i].stop();
        }
        this.#bufList = [];
        this.#playTime = null;
    }

    // Public static properties
    static EV_NEED_DATA = "audiostream.need_data";

    // Private instance properties
    #bufSize    = 4410;     // Buffer size in number of float32 samples
    #bufList    = [];       // List of buffers scheduled to play
    #minBufs    = 2;        // Minimum number of buffers to keep in the list of buffers
    #ctx        = null;     // WebAudio context
    #playTime   = null;     // WebAudio play time

};

let audio = new AudioStream();
audio.addEventListener(AudioStream.EV_NEED_DATA, (ev) => {
    //console.log("request audio bufCount:", ev.detail.bufCount);
    while (ev.detail.bufCount > 0) {
        requestAudio();
        ev.detail.bufCount--;
    }
});

let chartUpdate = false;
let chartLastUpdate = null;
let lastLabels = null;
let lastSignal = null;

function updateChart() {

    if (!chartUpdate || !lastLabels) {
        return;
    }

    // Calculates rate from fps and checks for elapsed time
    const fps = parseInt($$(SLIDER_FPS_ID).getValue());
    const rateMS = (1.0/fps) * 1000.0;
    const elapsed = performance.now() - chartLastUpdate;
    if (elapsed < rateMS) {
        return;
    }
    //console.log(`elapsed: ${elapsed.toFixed(2)}`);

    // Updates the chart
    $$(CHART_ID).chart.data.labels = Array.from(lastLabels);
    $$(CHART_ID).chart.data.datasets[0].data = Array.from(lastSignal);
    $$(CHART_ID).chart.update();

    chartLastUpdate = performance.now();
    requestAnimationFrame(updateChart);
}

function startUpdateChart() {

    chartUpdate = true;
    chartLastUpdate = performance.now()
    requestAnimationFrame(updateChart);
}

function stopUpdateChart() {

    chartUpdate = false;
}


function requestAudio() {

    rpc.call("rpc_server_audio_run", {}, (resp) => {
 
        // Get audio labels and signal
        lastLabels = new Float32Array(resp.data.label);
        lastSignal = new Float32Array(resp.data.signal);
        audio.appendData(lastSignal);

        updateChart();
    });
}

function rpcEvents(ev) {

    if (ev.type == RPC.EV_OPENED) {

        // Updates audio parameters
        const nsamples = parseInt($$(COMBO_BUFSIZE_ID).getValue());
        const bufCount = parseInt($$(COMBO_BUFCOUNT_ID).getValue());
        audio.open(nsamples, bufCount);
        const sample_rate = audio.sampleRate;
        const gain = $$(SLIDER_GAIN_ID).getValue();
        const freq = $$(SLIDER_FREQ_ID).getValue();
        const noise=  $$(SLIDER_NOISE_ID).getValue();
        rpc.call("rpc_server_audio_set", {sample_rate, nsamples, gain, freq, noise});

        audio.start();
        startUpdateChart();
        return;
    }

    if (ev.type == RPC.EV_CLOSED) {
        stopUpdateChart();
        audio.stop();
        audio.close();
        return;
    }
}


// Returns this view
export function getView() {

    const tabView = {
        header: "Audio",
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
                                $$(COMBO_BUFSIZE_ID).disable();
                                $$(COMBO_BUFCOUNT_ID).disable();
                            },
                        },
                        {
                            view:   "button",
                            label:  "Stop",
                            css:    "webix_primary",
                            autowidth:  true,
                            click: function() {
                                const emsg = rpc.close();
                                $$(COMBO_BUFSIZE_ID).enable();
                                $$(COMBO_BUFCOUNT_ID).enable();
                                if (emsg) {
                                    console.log(emsg);
                                    return;
                                }
                            },
                        },
                        {
                            view:   "combo",
                            id:     COMBO_BUFSIZE_ID,
                            width:  200,
                            label:  "buffer size:",
                            value:  '512',
                            options: ['256','512','1024','2048','4096'],
                        },
                        {
                            view:       "combo",
                            id:         COMBO_BUFCOUNT_ID,
                            width:      200,
                            labelAlign: "right",
                            labelWidth: 100,
                            label:      "buffer count:",
                            value:      '3',
                            options:    ['2','3','4','5','6'],
                        },
                        {
                            view:   "slider",
                            id:     SLIDER_GAIN_ID,
                            width:  SLIDER_WIDTH,
                            title:  webix.template("gain: #value#%"),
                            moveTitle: false,
                            value:  '20',
                            min:    0,
                            max:    100,
                            on: {
                                onSliderDrag: function() {
                                    rpc.call("rpc_server_audio_set", {gain: this.getValue()});
                                },
                                onChange:function(){
                                    rpc.call("rpc_server_audio_set", {gain: this.getValue()});
                                },
                            },
                        },
                        {
                            view:   "slider",
                            id:     SLIDER_FREQ_ID,
                            width:  SLIDER_WIDTH,
                            title:  webix.template("freq: #value#Hz"),
                            moveTitle: false,
                            value:  '100',
                            min:    100,
                            max:    8000,
                            on: {
                                onSliderDrag: function() {
                                    rpc.call("rpc_server_audio_set", {freq: this.getValue()});
                                },
                                onChange:function(){
                                    rpc.call("rpc_server_audio_set", {freq: this.getValue()});
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
                                    rpc.call("rpc_server_audio_set", {noise: this.getValue()});
                                },
                                onChange:function(){
                                    rpc.call("rpc_server_audio_set", {noise: this.getValue()});
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
                            min:    1,
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
				            labels: ['0','1'],
				            datasets: [
                                {
                                    label: "signal",
                                    data:  [0,0],
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
                                    text:    "Signal",
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


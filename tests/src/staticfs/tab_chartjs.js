import * as App from "./app.js";
import {RPC} from "./rpc.js";

const RPC_URL = "/rpc2";
const VIEW_ID = "tab.chartjs";
const SLIDER_FREQ_ID = "tab.chartjs.slider.freq";
const SLIDER_POINTS_ID = "tab.chartjs.slider.points";
const SLIDER_FPS_ID = "tab.chartjs.slider.fps";
const CHART_ID = "tab.chartjs.chart";

const rpc = new RPC(RPC_URL);

let timeoutId = null;

function requestChart() {

    const fps = $$(SLIDER_FPS_ID).getValue();
    const delayMs = (1.0/fps) * 1000;
    rpc.call("rpc_server_chart_run", {}, (resp) => {
  
        const label = new Float32Array(resp.data.label);
        const signal = new Float32Array(resp.data.signal);

        $$(CHART_ID).chart.data.labels = Array.from(label);
        $$(CHART_ID).chart.data.datasets[0].data = Array.from(signal);
        $$(CHART_ID).chart.update();

        timeoutId = setTimeout(requestChart, delayMs);
    });
}

function rpcEvents(ev) {

    console.log("RPC: %s url:%s", ev.type, ev.detail.url);
    if (ev.type == RPC.EV_OPENED) {
        const freq = $$(SLIDER_FREQ_ID).getValue();
        const npoints =  $$(SLIDER_POINTS_ID).getValue();
        rpc.call("rpc_server_chart_set", {freq, npoints});
        requestChart();
        return;
    }
}


// Returns this view
export function getView() {

    const tabView = {
        header: "ChartJS",
        close: true,
        body: {
            id: VIEW_ID,
            on: {
                onDestruct: function() {
                    App.closeWebSocket(WSOCKET_URL);
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
                                rpc.addEventListener(RPC.EV_OPENED, rpcEvents);
                                rpc.addEventListener(RPC.EV_OPENED, rpcEvents);
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
                            width:  200,
                            title:  webix.template("freq: #value#Hz"),
                            moveTitle: false,
                            value:  '60',
                            min:    10,
                            max:    1024,
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
                            id:     SLIDER_POINTS_ID,
                            width:  200,
                            title:  webix.template("npoints: #value#"),
                            moveTitle: false,
                            value:  '128',
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
                            width:  200,
                            title:  webix.template("fps: #value#"),
                            moveTitle: false,
                            value:  '2',
                            min:    2,
                            max:    60,
                            name:   "sfreq",
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
                                    label: "data 1",
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

	// chart1 = w.NewView(js.Obj{
	// 	"view": "chartjs",                           // component name
	// 	"id":   w.GenId(),                           // view id
	// 	"css":  js.Obj{"background-color": "white"}, // canvas backgroud
	// 	// Chartjs configuration starts here
	// 	"config": js.Obj{
	// 		"type": "line",
	// 		"data": js.Obj{
	// 			"labels": []float64{},
	// 			"datasets": js.Array{
	// 				js.Obj{
	// 					"label":       "data 1",
	// 					"data":        []float64{},
	// 					"borderColor": "blue",
	// 					"borderWidth": 1,
	// 					"pointRadius": 0,
	// 				},
	// 			},
	// 		},
	// 		"options": js.Obj{
	// 			"maintainAspectRatio": false,
	// 			"animation":           false,
	// 			"plugins": js.Obj{
	// 				"title": js.Obj{
	// 					"display": true,
	// 					"text":    "Chart1 title",
	// 				},
	// 			},
	// 			"legend": js.Obj{
	// 				"display": false,
	// 			},
	// 			"scales": js.Obj{
	// 				"x": js.Obj{
	// 					"type": "linear",
	// 					//	//"min":  0,
	// 					//	//"max":  10,
	// 					//	"ticks": js.Obj{
	// 					//		"source": "auto",
	// 					//	},
	// 				},
	// 			},
	// 		},
	// 	},
	// })
	// view.Add(chart1)
	//
	// chart2 := w.NewView(js.Obj{
	// 	"view": "chartjs",                           // component name
	// 	"id":   w.GenId(),                           // view id
	// 	"css":  js.Obj{"background-color": "white"}, // canvas backgroud
	// 	// Chartjs configuration starts here
	// 	"config": js.Obj{
	// 		"type": "line",
	// 		"data": js.Obj{
	// 			"labels": []float64{0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
	// 			"datasets": js.Array{
	// 				js.Obj{
	// 					"label":       "data 1",
	// 					"data":        []float64{-10, -8, -5, 0, 1, 2, 3, 4, 10, 4, 3, 2, 1, 0},
	// 					"borderColor": "black",
	// 				},
	// 			},
	// 		},
	// 		"options": js.Obj{
	// 			"maintainAspectRatio": false,
	// 			"animation":           false,
	// 			"plugins": js.Obj{
	// 				"title": js.Obj{
	// 					"display": true,
	// 					"text":    "Chart title",
	// 				},
	// 			},
	// 		},
	// 	},
	// })
	// view.Add(chart2)
	//

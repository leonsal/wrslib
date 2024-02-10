//
// chart.js Webix component
//
"use strict";

//
// Create component
//
webix.protoUI({

	name: "chartjs",

    // Creates and initializes chart from config
	$init: function(config) {

        // Creates canvas and appends it to Webix view
		this._canvas = document.createElement("canvas");
		this.$view.appendChild(this._canvas);

        // Creates chart 
        const ctx = this._canvas.getContext('2d');
        this.chart = new Chart(ctx, config.config);
	},

    setChartData: function(labels, dataset, darray) {

        this.chart.data.labels = labels;
        this.chart.data.datasets[dataset].data = darray;
        this.chart.update();
    },

	$setSize: function(x, y){

		if (webix.ui.view.prototype.$setSize.call(this, x,y)) {
			this._canvas.width = x;
			this._canvas.height = y;
            this.chart.resize(x, y);
		}
	}

}, webix.ui.view, webix.EventSystem);




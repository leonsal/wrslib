//
// dygraph Webix component
//
"use strict";

//
// Create component
//
webix.protoUI({

	name: "dygraph",

    // Creates and initializes chart from config
	$init: function(config) {

        const data = [[0, 0]];
        this.chart = new Dygraph(this.$view, data, config.config);
	},

	$setSize: function(x, y){

		if (webix.ui.view.prototype.$setSize.call(this, x,y)) {
            this.chart.resize(x, y);
		}
	}

}, webix.ui.view, webix.EventSystem);



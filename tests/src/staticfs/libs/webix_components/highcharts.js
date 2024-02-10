//
// Highcharts Webix component
//
"use strict";

//
// Create component
//
webix.protoUI({

	name: "highstock",

    // Creates and initializes chart from config
	$init: function(config) {

        this.chart = Highcharts.stockChart(this.$view, config);
        // Forces resize of the chart
        //setTimeout(() => this.chart.setSize(null, null), 1);
	},

    $setSize:function(){

        webix.ui.view.prototype.$setSize.apply(this, arguments);
        this.chart.setSize(null, null);
    },

}, webix.ui.view, webix.EventSystem);

//
// Create component
//
webix.protoUI({

	name: "highchart",

    // Creates and initializes chart from config
	$init: function(config) {

        this.chart = Highcharts.chart(this.$view, config);
        // Forces resize of the chart
        //setTimeout(() => this.chart.setSize(null, null), 1);
	},

    $setSize:function(){

        webix.ui.view.prototype.$setSize.apply(this, arguments);
        this.chart.setSize(null, null);
    },

}, webix.ui.view, webix.EventSystem);



import * as App from "./app.js";
import {RPC} from "./rpc.js";
import * as tabRPC from "./tab_rpc.js";
import * as tabChartJS from "./tab_chartjs.js";

// Associate menu ids with function with creates associated view
const MENUID_RPC = "menu.rpc";
const MENUID_CHARTJS = "menu.chartjs";
const MENUID_EXIT = "menu.exit";
App.setTabViewFunc(MENUID_RPC, tabRPC.getView);
App.setTabViewFunc(MENUID_CHARTJS, tabChartJS.getView);

const mainMenu = {
    view: "menu",
    id: "main.menu",
    autowidth: true,
    data: [
        {
            value: "Tests",
            submenu: [
                {id: MENUID_RPC, value: "RPC"},
                {id: MENUID_CHARTJS, value: "Chart JS"},
                {$template: "Separator"},
                {id: MENUID_EXIT, value: "Exit"},
            ]
        },
    ],
    on: {
        onMenuItemClick:function(menuId){
            if (menuId == MENUID_EXIT) {
                const rpc = new RPC('/rpc1');
                const emsg = rpc.open();
                if (emsg) {
                    console.log("EXIT", emsg);
                    return;
                }
                rpc.addEventListener(RPC.EV_OPENED, () => {
                    const emsg = rpc.call('rpc_server_exit');
                    if (emsg) {
                        console.log("EXIT2", emsg);
                    }
                });
                return;
            }
            // Get function associated with the menu id to create the tab view body
            const viewfn = App.getTabViewFunc(menuId);
            if (!viewfn) {
                return;
            }
            // Get the view id and the create view function
            // If tab already open, just sets its active.
            const {viewId, view} = viewfn();
            if (App.isTabViewActive(viewId)) {
                $$("main.tabview").setValue(viewId);
                return;
            }
            // Adds the new view to the tab view and show it.
            $$("main.tabview").addView(view);
            $$("main.tabview").setValue(viewId);
            App.setTabViewActive(viewId, true);
        }
    },
};

const mainToolbar = {
    view: "toolbar", 
    id: "main.toolbar",
    elements:[
        mainMenu,
        {
            view:  "label",
            label: "WebApp_Tests",
        },
    ]
};

const mainTabView = {
    view:   "tabview",
    id:     "main.tabview",
    type:   "clean",
    css:    "tabview",
    tabbar: {
        height:      34,
        close:      false,
        optionWidth: 210,
    },
    cells: [
        {
            header: "Info",
            body: {
            }
        },
    ],
};

// Shows initial UI
webix.ui({
    rows: [
        //{type: "clean", cols: [mainMenu, mainToolbar]},
        mainToolbar,
	    {height: 8, css: "tabview"},
        mainTabView,
    ],
});


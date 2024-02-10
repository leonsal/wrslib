//
// file selector Webix component
//
"use strict";

//
// Create component
//
webix.protoUI({

	name: "file_selector",

    // Creates and initializes chart from config
	$init: function(config) {
   
        // Creates <input> element and appends to the Webix view
        this._input = document.createElement("input");
        this._input.setAttribute("type", "file");
        this._input.setAttribute("id", config.id);
        this.$view.appendChild(this._input);

        // Configure the element attributes and style from the config
        if (config.accept) {
            this._input.setAttribute("accept", config.accept);
        }
        if (config.multiple) {
            this._input.setAttribute("multiple", "");
        }
        this._input.style = "display:none";

        // Adds event listener
        this._input.addEventListener("change", () => {

            // If no callback, nothing to do
            if (!this._callback) {
                return;
            }
            // Builds array with file objects
            const fileList = [];
            for (const file of this._input.files) {
                const fileObj = {
                    name:           file.name,
                    lastModified:   file.lastModified,
                    size:           file.size,
                };
                fileList.push(fileObj);
            }
            this._callback(fileList);
        });
	},

    // Show native file selector dialog with the specified config and callback.
    // If the config is null, the previous config will be used.
    // The specified callback will be called when dialog's OK button clicked
    // with an array of objects describing the selected files.
	showDialog: function(config, callback){

        if (config) {
            if (!config.accept) {
                this._input.removeAttribute("accept");
            } else {
                this._input.setAttribute("accept", config.accept);
            }
            if (!config.multiple) {
                this._input.removeAttribute("multiple");
            } else {
                this._input.setAttribute("multiple", "");
            }
        }

        this._callback = callback;
        this._input.showPicker();
	}

}, webix.ui.view, webix.EventSystem);


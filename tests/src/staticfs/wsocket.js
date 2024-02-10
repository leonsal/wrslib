//
// WebSocket class for communication with application server
//
export class WSocket {

    constructor() {
    }

    // Logs to console.log() messages with level >= current level
    log(level) {

        if (level < this.#logLevel) {
            return;
        }
        const args = ["WS:"+WSocket.level2str[level].toUpperCase()];
        for (let i = 1; i < arguments.length; i++) {
            args.push(arguments[i]);
        }
        console.log.apply(null, args);
    }

    // Logs debug message
    logDebug(...args) {
        this.log(0, ...args)
    }

    // Logs info message
    logInfo(...args) {
        this.log(1, ...args)
    }

    // Logs error message
    logError(...args) {
        this.log(2, ...args)
    }

    // Sets the log level by name
    setLog(lname) {

        let level = WSocket.str2level[lname];
        if (level == undefined) {
            this.logError("Invalid log level", lname)
            return;
        }
        this.#logLevel = level;
    }

    // Connects with server at the specified relative url
    connect(urlrel) {

        // If connection already open does nothing
        if (this.#socket) {
            this.logError("Connection already opened");
            return;
        }

        // Open web socket connection with server using the supplied relative URL
        const url = 'ws://' + document.location.host + "/" + urlrel;
        this.#socket = new WebSocket(url);
        this.logDebug("connecting to:", url);

        // Sets event handlers
        this.#socket.onopen    = ev => this.onOpen(ev);
        this.#socket.onerror   = ev => this.onError(ev);
        this.#socket.onclose   = ev => this.onClose(ev);
        this.#socket.onmessage = ev => this.onMessage(ev);
    }

    // Closes connection
    close() {

        // If connection already closed does nothing
        if (!this.#socket) {
            this.logError("Connection already closed");
            return;
        }
        this.#socket.close();

    }

    // Returns WebSocket connection status
    status() {

        if (!this.#socket) {
            return this.CLOSED;
        }
        return this.#socket.readyState;
    }

    // Sends message of specified type and body to server
    send(msg) {

        // Checks if connection is open
        if (!this.#socket || this.#socket.readyState != WSocket.OPEN) {
            const msg = "Connection with server is closed";
            this.logError(msg);
            this.errorAlert(msg);
            return
        }

        this.#socket.send(msg);
        this.logDebug("TX:", msg);
    }

    // Sends message of specified type and body to server
    sendJSON(obj) {

        // Encodes message to JSON and sends is as text
        const text = JSON.stringify(obj);
        this.send(text);
    }

    // Called when WebSocket connection is opened
    onOpen(ev) {

        this.logInfo("Connection opened");
    }

    // Called when error ocurrs with WebSocket communication
    onError(ev) {

        const msg = "Connection error";
        this.logError(msg, ev);
        this.errorAlert(msg);
    }

    // Called when the WebSocket connection is closed
    onClose(ev) {

        this.#socket = null;
        const msg = "Connection with server was closed";
        this.logInfo(msg, ev);
        this.errorAlert(msg);
    }

    // Called when message is received from WebSocket connection 
    onMessage(ev) {

        this.logDebug("onMessage", ev);
    }

    errorAlert(msg) {

    }

    // Private state
    #logLevel = 0;
    #socket = null;

    static str2level = {
        "debug":    0,
        "info":     1,
        "error":    2,
    };

    static level2str = {
        0: "debug",
        1: "info",
        2: "error",
    };

    // Websocket readyStates
    static CONNECTING = 0;
    static OPEN = 1;
    static CLOSING = 2;
    static CLOSED = 3;
}


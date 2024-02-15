//
// Remote Procedure Class for communication with Web Application Server
// Protocol description:
//
// Call remote function message:
// {
//    cid:  <id of next call>,
//    call: <name of remote function binding> ,
//    params: <any>,            // may be undefined if no parameters
// }
// 
// Response from call:
// {
//    rid: <id of the call>,
//    resp: {
//       err: <any>,            // Only present if error occurred
//       data: <any>            // Only present if NO error occured
//    }
// }

// Binary chunk types
const ChunkTypeMsg    = 1;
const ChunkTypeBuffer = 2;
const BufferTypes = new Map()
BufferTypes.set('ArrayBuffer',  true);
BufferTypes.set('Int8Array',    true);
BufferTypes.set('Uint8Array',   true);
BufferTypes.set('Int16Array',   true);
BufferTypes.set('Uint16Array',  true);
BufferTypes.set('Int32Array',   true);
BufferTypes.set('Uint32Array',  true);
BufferTypes.set('Float32Array', true);
BufferTypes.set('Float64Array', true);

// Align the specified offset to next multiple of 4 if necessary
function alignOffset(offset) {

    const mod = offset % 4;
    if (mod > 0) {
        return offset + 4 - mod;
    }
    return offset;
}

// Returns if buffer is TypedArray or ArrayBuffer.
function checkBuffer(buffer) {

    if (typeof(buffer) != 'object') {
        return false;
    }
    return BufferTypes.get(buffer.constructor.name);
}

export class RPC extends EventTarget {

    // Creates RPC manager for specified server URL
    // url - Server URL
    // retryMS - Optional number of milliseconds to retry connection
    constructor(url, retryMS) {
        super();
        this.#url = url;
        this.#retryMS = retryMS;
    }

    // Open RPC WebSocket connection 
    open() {

        // Checks if connection is already open
        if (this.#socket) {
            return "RPC connection already opened";
        }

        // Open web socket connection with server using the supplied relative URL
        const url = 'ws://' + document.location.host + "/" + this.#url;
        this.#socket = new WebSocket(url);
        this.#socket.binaryType = 'arraybuffer';

        // Sets event handlers
        this.#socket.onopen    = ev => this.#onOpen(ev);
        this.#socket.onerror   = ev => this.#onError(ev);
        this.#socket.onclose   = ev => this.#onClose(ev);
        this.#socket.onmessage = ev => this.#onMessage(ev);
    }

    close() {

        // Checks connection is open
        if (!this.#socket) {
            return "RPC connection not opened";
        }
        this.#socket.close();
        this.#closed = true;
    }

    call(remoteName, params=null, cb=null) {

        // Checks connection is open
        if (!this.#socket || this.#socket.readyState != WebSocket.OPEN) {
            return "RPC connection not opened";
        }

        // Builds RPC call message
        const req = {
            cid:    this.#cid,
            call:   remoteName,
            params: params,
        };
        const json = JSON.stringify(req);
        this.#socket.send(json);
        this.#callTime = performance.now();

        // If callback not defined
        if (!cb) {
            return;
        }

        // Saves callback associated with message id
        // TODO timeout for responses ??
        this.#callbacks.set(this.#cid, cb);
        this.#cid += 1;
    }

    // Call remote function
    call_bin(remoteName, params=null, buffers, cb=null) {

        // Checks connection is open
        if (!this.#socket || this.#socket.readyState != WebSocket.OPEN) {
            return "RPC connection not opened";
        }

        // Check buffers
        if (!buffers || !Array.isArray(buffers)) {
            throw "Buffers must be an array of typed arrays or array buffer";
        }
        for (let i = 0; i < buffers.length; i++) {
            if (!checkBuffer(buffers[i])) {
                throw `Buffers:${i} must be a typed array or array buffer`;
            }
        }

        // Builds RPC call message
        const req = {
            cid:    this.#cid,
            call:   remoteName,
            params: params,
        };
        const json = JSON.stringify(req);

        // Converts JSON string to typed array
        const encoder = new TextEncoder();
        const json_bytes = encoder.encode(json);

        // Calculates the total length in bytes of the data to send
        const chunkHeaderSize = 8;
        let totalByteLength = chunkHeaderSize + json_bytes.byteLength;
        totalByteLength = alignOffset(totalByteLength);
        for (let i = 0; i < buffers.length; i++) {
            totalByteLength += chunkHeaderSize + buffers[i].byteLength;
            totalByteLength = alignOffset(totalByteLength);
        }
        console.log("totalByteLength", totalByteLength);

        // Allocates message buffer with total size required
        const msgBuffer = new ArrayBuffer(totalByteLength);
        const msgView = new DataView(msgBuffer);
        const msgU8 = new Uint8Array(msgBuffer);

        // JSON header
        let byteOffset = 0;
        msgView.setUint32(byteOffset, ChunkTypeMsg, true);
        byteOffset += 4;
        msgView.setUint32(byteOffset, json_bytes.byteLength, true);
        byteOffset += 4;

        // JSON data
        msgU8.set(json_bytes, byteOffset);
        byteOffset += json_bytes.byteLength;
        byteOffset = alignOffset(byteOffset);
        console.log("offset", byteOffset);
        
        // Buffers
        for (let i = 0; i < buffers.length; i++) {
            const buffer = buffers[i];
            const bufU8 = new Uint8Array(buffer.buffer);
            // Buffer header
            msgView.setUint32(byteOffset, ChunkTypeBuffer, true);
            byteOffset += 4;
            msgView.setUint32(byteOffset, buffer.byteLength, true);
            byteOffset += 4;
            // Buffer data
            msgU8.set(bufU8, byteOffset);
            byteOffset += buffer.byteLength;
            byteOffset = alignOffset(byteOffset);
        }

        //console.log(msgBuffer);
        this.#socket.send(msgBuffer);
        this.#callTime = performance.now();

        // If callback not defined
        if (!cb) {
            return;
        }

        // Saves callback associated with message id
        // TODO timeout for responses ??
        this.#callbacks.set(this.#cid, cb);
        this.#cid += 1;
    }

    // Binds remote name string with local function
    bind(remoteName, fn) {
       
        if (fn) {
            this.#binds.set(remoteName, fn);
        } else {
            this.#binds.delete(remoteName);
        }
    }

    // Returns the time in milliseconds of the elapsed time between
    // the call request and the callback response.
    lastCallElapsed() {

        return this.#callElapsed;
    }

    #onOpen(ev) {

        const cev = new CustomEvent(RPC.EV_OPENED, {
            detail: {
                url: this.#url,
            },
        });
        this.dispatchEvent(cev); 
    }

    #onError(ev) {

        const cev = new CustomEvent(RPC.EV_ERROR, {
            detail: {
                url: this.#url,
            },
        });
        this.dispatchEvent(cev); 
    }

    #onClose(ev) {

        const cev = new CustomEvent(RPC.EV_CLOSED, {
            detail: {
                url: this.#url,
            },
        });
        this.dispatchEvent(cev); 

        this.#socket = null;
        if (this.#closed || this.#retryMS === undefined) {
            return;
        }
        setTimeout(this.#retryMS, this.open);
    }

    #onMessage(ev) {

        console.log("onMessage", typeof(ev.data), ev.data instanceof ArrayBuffer);
        if (typeof(ev.data) == 'string') {
            this.#onMessageText(ev); 
        } else {
            this.#onMessageBin(ev);
        }




    }

    #onMessageText(ev) {

        const msg = JSON.parse(ev.data);
        // Checks for response id from previous call
        if (msg.rid !== undefined) {
            // Get associated callback
            const cb = this.#callbacks.get(msg.rid);
            if (!cb) {
                console.log("RPC no callback found for: ${msg.rid} not found");
                return;
            }
            this.#callbacks.delete(msg.rid);
            // Checks response field
            const resp = msg.resp;
            if (!resp) {
                console.log("RPC no 'resp' field in response");
                return;
            }
            if (resp.err === undefined && resp.data === undefined) {
                console.log("RPC 'resp' field missing 'err' or' 'data'");
                return;
            }
            this.#callElapsed = performance.now() - this.#callTime;
            cb(msg.resp);
            return;
        }

        // Checks for call id
        if (msg.cid !== undefined) {
            if (msg.call === undefined) {
                console.log("RPC remote call without 'call' field");
                return;
            }
            const localFn = this.#binds.get(msg.call);
            if (localFn === undefined) {
                console.log(`RPC remote call ${msg.call} not binded`);
                return;
            }
            // Calls local function and if function returnsd result,
            // sends response back to caller
            const result = localFn(msg.params);
            if (result !== undefined) {
                const resp = {
                    rid: msg.cid,
                    resp: result,
                }
                const json = JSON.stringify(resp);
                this.#socket.send(json);
            }
            return;
        }
        console.log("RPC invalid JSON call or response");
    }

    #onMessageBin(ev) {

        const msg = ev.data;
        const fieldSize = 4;
        const headerSize = 2 * fieldSize;
        const msgView = new DataView(msg);
        const last = ev.data.byteLength;
        let curr = 0;
        let json_text = null;
    
        while (true) {
            // Checks for available size for a chunk header
            if (last - curr < headerSize) {
                console.log("no space for header");
                return;
            }

            // Get the chunk type and length in bytes
            const chunkType = msgView.getInt32(curr, true);
            curr += fieldSize;
            const chunkLen = msgView.getInt32(curr, true);
            curr += fieldSize;
            console.log("header", chunkType, chunkLen);

            // Checks chunk length
            if (chunkLen > last - curr) {
                console.log("invalid chunk length", chunkLen);
                return;
            }

            // Decodes JSON chunk
            if (chunkType == ChunkTypeMsg) {
                const chunkView = new DataView(msg, curr, chunkLen);
                const decoder = new TextDecoder(); // UTF-8
                json_text = decoder.decode(chunkView);
                console.log("TEXT", json_text); 
            // Decodes Buffer chunk
            } else if (chunkType == ChunkTypeBuffer) {
                const buf = msg.slice(curr, chunkLen);
                console.log("buf", buf);
            } else {
                console.log("invalid chunk type", chunkType);
                return;
            }
            curr += chunkLen;
            curr = alignOffset(curr);
            console.log(curr, last);
        }
    }


    // Public static properties
    static EV_OPENED    = "rpc.opened";
    static EV_CLOSED    = "rpc.closed";
    static EV_ERROR     = "rpc.error";

    // Private instance properties
    #url            = null;
    #retryMS        = null;
    #socket         = null;
    #closed         = false;
    #cid            = 1;            // Next call id
    #callbacks      = new Map();
    #binds          = new Map();
    #callTime       = undefined;    // Time of last call
    #callElapsed    = undefined;
};


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
const BufferPrefix = "\b\b\b\b\b\b";
const ChunkHeaderFieldSize = 4;
const ChunkHeaderSize = 2 * ChunkHeaderFieldSize;

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

    if (buffer === null || buffer === undefined) {
        return false;
    }
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
        const msg = {
            cid:    this.#cid,
            call:   remoteName,
            params: params,
        };
        this.#sendMsg(msg);

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

    // Encodes and sends call or response message
    #sendMsg(msg) {
  
        // Stringify JSON replacing references to arraybuffers or typed arrays
        // fields to a special string plus the buffer number.
        const buffers = [];
        const json = JSON.stringify(msg, (_, value) => {
            if (!checkBuffer(value)) {
                return value;
            }
            buffers.push(value);
            const replaced = BufferPrefix + (buffers.length-1).toString();
            return replaced;
        });

        // If no buffers found in the message, send as a simple text message.
        if (buffers.length == 0) {
            this.#socket.send(json);
            this.#callTime = performance.now();
            return;
        }

        // Converts JSON string to typed array
        const encoder = new TextEncoder();
        const json_bytes = encoder.encode(json);

        // Calculates the total length in bytes of the data to send
        let totalLength = ChunkHeaderSize + json_bytes.byteLength;
        totalLength = alignOffset(totalLength);
        for (let i = 0; i < buffers.length; i++) {
            totalLength += ChunkHeaderSize + buffers[i].byteLength;
            totalLength = alignOffset(totalLength);
        }
        console.log("totalByteLength", totalLength);

        // Allocates message buffer with total size required
        const msgBuffer = new ArrayBuffer(totalLength);
        const msgView = new DataView(msgBuffer);
        const msgU8 = new Uint8Array(msgBuffer);

        // JSON header
        let offset = 0;
        msgView.setUint32(offset, ChunkTypeMsg, true);
        offset += ChunkHeaderFieldSize;
        msgView.setUint32(offset, json_bytes.byteLength, true);
        offset += ChunkHeaderFieldSize;

        // JSON data
        msgU8.set(json_bytes, offset);
        offset += json_bytes.byteLength;
        offset = alignOffset(offset);
        
        // Buffers
        for (let i = 0; i < buffers.length; i++) {
            const buffer = buffers[i];
            const bufU8 = new Uint8Array(buffer.buffer);
            // Buffer header
            msgView.setUint32(offset, ChunkTypeBuffer, true);
            offset += ChunkHeaderFieldSize;
            msgView.setUint32(offset, buffer.byteLength, true);
            offset += ChunkHeaderFieldSize;
            // Buffer data
            msgU8.set(bufU8, offset);
            offset += buffer.byteLength;
            offset = alignOffset(offset);
        }
        this.#socket.send(msgBuffer);
        this.#callTime = performance.now();
    }

    #onMessage(ev) {

        if (typeof(ev.data) == 'string') {
            this.#decodeJSON(ev.data); 
        } else {
            this.#decodeBinMsg(ev);
        }
    }

    #decodeJSON(msgString, buffers=null) {

        // Decodes JSON string
        let msg = null;
        if (!buffers) {
             msg = JSON.parse(msgString);
        } else {
            msg = JSON.parse(msgString, (_, value) => {
                if (typeof(value) != 'string') {
                    return value;
                }
                if (!value.startsWith(BufferPrefix)) {
                    return value;
                }
                const bufnStr = value.slice(BufferPrefix.length);
                const bufn = parseInt(bufnStr);
                if (bufn >= buffers.length) {
                    console.log("reviver: invalid buffer number:", bufn);
                    return undefined;
                }
                return buffers[bufn];
            });
        }

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
            // Calls local function and if function returns result,
            // sends response back to caller
            const result = localFn(msg.params);
            if (result !== undefined) {
                const resp = {
                    rid: msg.cid,
                    resp: result,
                }
                this.#sendMsg(resp);
            }
            return;
        }
        console.log("RPC invalid JSON call or response");
    }

    #decodeBinMsg(ev) {

        const msg = ev.data;
        const msgView = new DataView(msg);
        const last = ev.data.byteLength;
        let curr = 0;
        let json_text = null;
        let buffers = [];
    
        while (curr != last) {
            // Checks for available size for a chunk header
            if (last - curr < ChunkHeaderSize) {
                console.log("no space for header");
                return;
            }

            // Get the chunk type and length in bytes
            const chunkType = msgView.getInt32(curr, true);
            curr += ChunkHeaderFieldSize;
            const chunkLen = msgView.getInt32(curr, true);
            curr += ChunkHeaderFieldSize;

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
            } else
            // Decodes Buffer chunk and saves in buffers array
            if (chunkType == ChunkTypeBuffer) {
                const buf = msg.slice(curr, curr + chunkLen);
                buffers.push(buf);
            } else {
                console.log("invalid chunk type", chunkType);
                return;
            }

            // Prepares for next chunk
            curr += chunkLen;
            curr = alignOffset(curr);
        }

        if (json_text === null) {
            console.log("NO JSON message received");
            return;
        }
        // Decode JSON with associated buffers
        this.#decodeJSON(json_text, buffers);
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


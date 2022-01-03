//-----------------------------------------------------------------------------
// Map any element with a given ID to a memory in Pluto
//-----------------------------------------------------------------------------
var WsClient = function () {
    var ui = {};
    var ws = null;
    var connectionCallback = new Map();
    var listMemoryCallback = null;
    var varToElement = {};
    var peerUrl = "";

    /**
     * @name publicMembers
     * Public Members
     */
    ///@{
    /**
     * Initialize memory to element mapper
     * format: init(connectionCallback);
     *
     * @param connectionCallback A callback that will be called when connection
     * status changes. It takes one boolean argument which is true for connected
     * and false for disconnected.
     */
    function init(callback) {
        connectionCallback.set(defaultConCallback, defaultConCallback);
        if (isFunction(callback))
            connectionCallback.set(callback, callback);
        ui.elOnInit = document.getElementsByClassName("mapElementToVar");
        ui.elButton = []; //document.getElementsByTagName("button");
        ui.elInput = []; //document.getElementsByTagName("input");
        bindUiActions();
        sendConnectionStatus(isConnected());
        setStatus("Initialized - Disconnected");
    }

    /**
     * List all available variables
     *
     * @param filter The variables to filter for. Currently only "*" is supported
     * @param callback The callback that will be called when list arrives
     * @return true if request was sent.
     */
    function listVariables(filter, callback) {
        listMemoryCallback = callback;
        sendData('{"cmd":"varList","data":"*"}');
    }

    /**
     * Register memory with callback
     *
     * Register a memory with callback that will be called when ever
     * a new value arrives.
     *
     * @param memory The memory to register
     * @param callback The callback that will be called on memory change
     * @return true if callback is valid, else false.
     */
    function subscribeVariable(memory, callback, ct) {
        if (!isFunction(callback))
            return false;

        varToElement[memory] = {
            setValue: callback,
            is: function (type) { return false; },
            context: ct
        };
        subscribeVar(memory);
        return true;
    }

    /**
     * Unregister memory with callback
     *
     * @param memory The memory to register
     */
    function unsubscribeVariable(memory) {
        releaseVariable(memory);
        varToElement[memory] = null;
    }

    /**
     * Send data to server
     *
     * @param name The memory name
     * @param value The memory value (can be an object)
     */
    function publishVariable(varname, val) {
        var msg = {
            cmd: "publish",
            data: {
                name: varname,
                value: val
            }
        };
        sendData(JSON.stringify(msg));
    }

    /**
     * Get memory with callback
     *
     * Get a memory with callback that will be called when
     * a new value is ready.
     *
     * @param memory The memory to register
     * @param callback The callback that will be called on memory ready
     * @return true if callback is valid, else false.
     */
    function requestVariable(memory, callback, ct) {
        if (!isFunction(callback))
            return false;

        varToElement[memory] = {
            setValue: callback,
            is: function (type) { return false; },
            context: ct
        };
        reqVar(memory);
        return true;
    }

    /** 
     * Connect to server.
     * Format: connect(url);
     *
     * @param url The url and portnumer like this: 1.2.3.4:8181.  If url is 
     * null, the host url will be used with port 8181.
     */
    function connect(url) {
        if ("WebSocket" in window) {
            if (location.host.length == 0 && url == null) {
                setStatus("No target url - not connecting");
                return false;
            }
            peerUrl = url || location.host + ":80";
            if (ws != null && ws.isConnected()) {
                ws.close();
            }
            createSocket(peerUrl);
            return true;
        }
        setStatus("Web sockets not supported");
        return false;
    }

    /**
     * Close connection.
     */
    function close() {
        if (ws != null) ws.close();
    }

    /**
     * Close connection.
     */
     function registerConnectionCallback(callback) {
        connectionCallback.set(callback, callback);
    }
    ///@}



    //-------------------------------------------------------------------------


    /**
     * @name privateMembers
     * Private Members
     */
    ///@{

    function sendConnectionStatus(status) {
        for (let cb of connectionCallback.values()){
            cb(status);
        }
    }

    function reqVar(memory) {
        if (isConnected()) {
            var msg = { cmd: "request", data: memory };
            sendData(JSON.stringify(msg));
        }
    }

    function subscribeVar(memory) {
        if (isConnected()) {
            var msg = { cmd: "subscribe", data: memory };
            sendData(JSON.stringify(msg));
        }
    }

    function releaseVariable(memory) {
        if (isConnected()) {
            var msg = { cmd: "unsubscribe", data: memory };
            sendData(JSON.stringify(msg))
        }
    }

    function onOpen(evt) {
        setStatus("Connected");
        for (var i = 0; i < ui.elOnInit.length; i++) {
            var el = ui.elOnInit[i];
            var id = el.getAttribute('id');
            varToElement[id] = el;
        }

        sendConnectionStatus(isConnected());

        for (key in varToElement) {
            subscribeVar(key);
        }
    }

    function onClose(evt) {
        setStatus("Disconnected (" + evt.code + "): " + evt.reason);
        sendConnectionStatus(isConnected());
        var id = setInterval(function () {
            createSocket(peerUrl);
            clearInterval(id);
        }, 1000);
    }

    function handleRequestResp(data) {
        var varName = data.name;
        var varValue = data.value;
        var el = varToElement[varName];
        if (el) {
            el.setValue(varValue, el.context);
        }
    }

    function handleNewstate(data) {
        var varName = data.name;
        var varValue = data.value;
        var el = varToElement[varName];
        if (el) {
            el.setValue(varValue, el.context);
        }
    }

    function handleSubscribeResp(data) {
        var varName = data.name;
        var varValue = data.value;
        var el = varToElement[varName];

        if (el && isFunction(el.setValue))
            el.setValue(varValue, el.context);
    }

    function handleUnsubscribeResp(data) {
        var varName = data.name;
        var el = varToElement[varName];
        if (el === null)
            return;

        el.setValue("?", el.context);
        varToElement[varName] = null;
    }

    function handleVarListResp(data) {
        var filter = data.filter;
        var list = data.list;
        listMemoryCallback(list);
    }

    function onMessage(evt) {
        if (evt.data === "")
            return;

        console.log("WsClient: Recieved: " + evt.data);
        var n = JSON.parse(evt.data);
        if (!n.cmd) {
            return;
        }

        switch (n.cmd) {
            case "newState":
                handleNewstate(n.data);
                break;
            case "subscribeResp":
                handleSubscribeResp(n.data);
                break;
            case "unsubscribeResp":
                handleUnsubscribeResp(n.data);
                break;
            case "requestResp":
                handleRequestResp(n.data);
                break;
            case "varListResp":
                handleVarListResp(n.data);
                break;
            default:
                console.log("WsClient: Recieved unknown command: " + n.cmd);
        }


    }

    function onError(evt) {
        var msg = "Error: " + evt.data;
        setStatus(msg);
    }

    function createSocket(url) {
        ws = new WebSocket("ws://" + url);
        ws.onopen = onOpen;
        ws.onclose = onClose;
        ws.onmessage = onMessage;
        ws.onerror = onError;
        setStatus("Web socket created to : " + url);
    }
    function setStatus(statusString) {
        console.log(statusString);
    }

    function isConnected() {
        return (ws != null && ws.readyState == ws.OPEN);
    }

    function sendData(data) {
        if (!isConnected()) {
            return;
        }

        ws.send(data);
    }

    function bindUiActions() {

        for (var i = 0; i < ui.elButton.length; i++) {
            var el = ui.elButton[i];
            el.onmousedown = function () {
                publishVariable(this.getAttribute('id'), 1);
                el.buttonPressed = true;
            }
            el.onmouseup = function () {
                publishVariable(this.getAttribute('id'), 0);
                el.buttonPressed = false;
            }
            el.onmouseleave = function () {
                if (el.buttonPressed == true)
                    publishVariable(this.getAttribute('id'), 0);
                el.buttonPressed = false;
            }
        }

        for (var i = 0; i < ui.elInput.length; i++) {
            var el = ui.elInput[i];
            el.onsubmit = function () {
                setMemory(this.getAttribute('id'), this.getValue());
            }
        }

    }

    function isFunction(check) {
        return Object.prototype.toString.call(check) == '[object Function]';
    }
    function defaultConCallback(state) { }
    ///@}
    return {
        init: init,
        connect: connect,
        close: close,
        subscribeVariable: subscribeVariable,
        unsubscribeVariable: unsubscribeVariable,
        publishVariable: publishVariable,
        requestVariable: requestVariable,
        listVariables: listVariables,
        registerConnectionCallback: registerConnectionCallback
    }
}();


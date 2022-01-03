$(document).ready(function () {

    var keyTargetUrl = "targetUrl";

    WsClient.init(onConnection);
    DataPlotter.init();

    var targetUrl = localStorage.getItem(keyTargetUrl);
    if (targetUrl == null)
        targetUrl = getNewTargetUrl();
    WsClient.connect(targetUrl);

    function onConnection(state) {
        $("connectionUrl").value = targetUrl;
        if (state) {
            $("#connectionLight").css("background-color", "green");
        }
        else {
            $("#connectionLight").css("background-color", "red");
        }
    }
    function getNewTargetUrl() {
        promptForName("Sláið inn URL fyrir Target ", "192.168.1.191:80/ws", function (name) {
            localStorage.setItem(keyTargetUrl, name);
            location.reload();
        });
    }
    $("#connectionUrl").click(function () {
        getNewTargetUrl();
    });
});



$(window).resize(function () {
});

function lightsMainSwitchCallback(value) {
    systemConfigHandler.setMainSwitchState(value);
    drawFrontHandler.drawLights();
}

function altitudeCallback(value) {
    value = Number(value);
    drawSideHandler.setHeight(value);
    $("#altitudeValue").text(value.toFixed(2));
}



function promptForName(caption, defaultText, successCallback) {
    var name = prompt(caption, defaultText);
    if (name == null || name.length == 0)
        return;
    successCallback(name);
}


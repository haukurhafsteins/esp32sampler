var DataPlotter = function () {
    var charts = [];
    var config = [];
    var graphData = [];
    var time_per_sample_us = 0.04;
    var show_points = false;
    var ui = {};
    var color = ["#CCCCCC", "#888888", "#FFFF00", "#0000FF", "#00FF00", "#00FFFF", "#FF0000", "#FF00FF"];

    function init() {
        ui.inpSampleTime = $("#inpSampleTime");
        ui.btnSetSampleTime = $("#btnSetSampleTime");
        ui.inpSampleFreq = $("#inpSampleFreq");
        ui.btnSetSampleFreq = $("#btnSetSampleFreq");
        ui.btnTogglePoints = $("#btnTogglePoints");

        bindUiActions();
        var i = 0;
        for (i = 0; i < 8; i++) {
            var el = 'ad' + i + 'Chart';
            var elById = document.getElementById(el);
            if (elById) {
                var title = "AD" + i + " Data";
                var label = "AD" + i;

                graphData[i] = new GraphData([{ borderWidth: 1, borderColor: color[i], label: label, data: [0.0] }]);
                config[i] = new GraphConfig(title, graphData[i]);
                charts[i] = new Chart(document.getElementById(el), config[i]);
            }
            else {
                graphData[i] = null;
                config[i] = null;
                charts[i] = null;
            }
        }

        // All graph
        graphData[8] = new GraphData([{ borderWidth: 1, borderColor: color[i], label: "--", data: [0.0] }]);
        config[8] = new GraphConfig("All AD's", graphData[8]);
        charts[8] = new Chart(document.getElementById("adAllChart"), config[8]);

        WsClient.subscribeVariable("ad1Data.0", ad10Callback);
        WsClient.subscribeVariable("ad1Data.3", ad13Callback);
        WsClient.subscribeVariable("ad1Data.4", ad14Callback);
        WsClient.subscribeVariable("ad1Data.5", ad15Callback);
        WsClient.subscribeVariable("ad1Data.6", ad16Callback);
        WsClient.subscribeVariable("ad1Data.7", ad17Callback);

        WsClient.registerConnectionCallback(connectionCallback);
    }

    function connectionCallback(value) {
        if (value) {
            var sample_time_us = Number(ui.inpSampleTime.val());
            WsClient.publishVariable("sampleTime", sample_time_us);
            var sample_freq = Number(ui.inpSampleFreq.val());
            WsClient.publishVariable("sampleFreq", sample_freq);
        }
    }
    function GraphData(datasets) {//label, data, color) {
        this.labels = [];
        this.datasets = datasets;

        for (let i = 0; i < datasets[0].data.length; i++)
            this.labels[i] = (time_per_sample_us * i).toFixed(0);
    }

    function GraphConfig(title, graphData) {
        this.type = 'line';
        this.data = graphData;
        this.options = {
            maintainAspectRatio: false,
            responsive: true,
            plugins: {
                legend: { position: 'top' },
                title: { display: true, text: title }
            },
            scales: {
                y: {
                    min: 0,
                    grid: { color: "#333333" },
                    ticks: { color: "#aaaaaa" }
                },
                x: {
                    min: 0,
                    grid: { color: "#333333" },
                    ticks: { color: "#aaaaaa" }
                }
            },
            animation: {
                duration: 0
            },
            elements: {
                point: {
                    radius: 0
                }
            }
        }
    }

    function adCallback(i, value) {
        var labels = [];
        for (let n = 0; n < value.length; n++)
            labels[n] = (time_per_sample_us * n).toFixed(0);
        charts[i].data.datasets[0].data = value;
        charts[i].data.labels = labels;
        charts[i].update();
    }

    function ad10Callback(value) {
        adCallback(0, value);
    }
    function ad11Callback(value) {
        adCallback(1, value);
    }
    function ad12Callback(value) {
        adCallback(2, value);
    }
    function ad13Callback(value) {
        adCallback(3, value);
    }
    function ad14Callback(value) {
        adCallback(4, value);
    }
    function ad15Callback(value) {
        adCallback(5, value);
    }
    function ad16Callback(value) {
        adCallback(6, value);
    }
    function ad17Callback(value) {
        adCallback(7, value);

        var datasets = [
            charts[0].data.datasets[0],
            charts[3].data.datasets[0],
            charts[4].data.datasets[0],
            charts[5].data.datasets[0],
            charts[6].data.datasets[0],
            charts[7].data.datasets[0]
        ];

        charts[8].data.datasets = datasets;
        var labels = [];
        for (let n = 0; n < value.length; n++)
            labels[n] = (time_per_sample_us * n).toFixed(0);
        charts[8].data.labels = labels;
        charts[8].update();
    }

    $("#requestData").click(function () {
        WsClient.requestVariable("ad1Data.all", ad10Callback, 0);
    });

    function bindUiActions() {
        ui.btnSetSampleTime.click(function () {
            var sample_time_us = Number(ui.inpSampleTime.val());
            WsClient.publishVariable("sampleTime", sample_time_us);
        });
        ui.btnSetSampleFreq.click(function () {
            sample_freq = Number(ui.inpSampleFreq.val());
            WsClient.publishVariable("sampleFreq", sample_freq);
            time_per_sample_us = 1000000/sample_freq;
        });
        ui.btnTogglePoints.click(function () {
            show_points = !show_points;
            var r = show_points ? 2 : 0;
            charts[0].options.elements.point.radius = r;
            charts[3].options.elements.point.radius = r;
            charts[4].options.elements.point.radius = r;
            charts[5].options.elements.point.radius = r;
            charts[6].options.elements.point.radius = r;
            charts[7].options.elements.point.radius = r;
            charts[8].options.elements.point.radius = r;
            charts[8].update();
        });
    }

    return {
        init: init,
    }
}();





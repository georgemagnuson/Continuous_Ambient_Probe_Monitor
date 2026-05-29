/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : WebTemplate.h
   PURPOSE   : Web UI running high-performance isolated data update pipelines
   ========================================================================== */

#ifndef WEBTEMPLATE_H
#define WEBTEMPLATE_H

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset='utf-8'>
    <title>CAPM Chart Station</title>
    <style>
        body { font-family: sans-serif; background: #eef2f3; padding: 20px; text-align: center; }
        .card { max-width: 500px; margin: 0 auto 20px auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        table { width: 100%; border-collapse: collapse; margin-top: 15px; }
        td { padding: 12px; text-align: left; border-bottom: 1px solid #eee; }
        td.val { text-align: right; font-weight: bold; color: #007bff; }
        .status { margin-top: 20px; padding: 10px; background: #f8f9fa; font-size: 13px; color: #666; border-radius: 5px; }
        .chart-box { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        .btn-group { display: flex; gap: 10px; justify-content: center; margin-top: 15px; }
        .nav-btn { flex: 1; padding: 10px 15px; font-size: 13px; font-weight: bold; color: white; border: none; border-radius: 5px; cursor: pointer; transition: background 0.2s ease; text-decoration: none; }
        .btn-data { background: #007bff; }
        .btn-data:hover { background: #0056b3; }
        .btn-history { background: #6c757d; }
        .btn-history:hover { background: #545b62; }
        #chartNotice { font-size: 12px; color: #aaa; margin-top: 10px; display: none; }
    </style>
</head>
<body>

<div class='card'>
    <h2>CAPM Live Monitor</h2>
    <table>
        <tr><td>Air Temperature</td><td id='t_dht' class='val'>--</td></tr>
        <tr><td>Air Humidity</td><td id='h_dht' class='val'>--</td></tr>
        <tr><td>Water Probe</td><td id='t_ds' class='val'>--</td></tr>
        <tr><td>Battery Monitor</td><td id='v_bat' class='val'>--</td></tr>
    </table>

    <div class='btn-group'>
        <button class='nav-btn btn-data' onclick="window.open('/data', '_blank')">View Raw Live Data</button>
        <button class='nav-btn btn-history' onclick="window.open('/full_data', '_blank')">View Raw History Logs</button>
    </div>

    <div id='statusBox' class='status'>Connecting to firmware...</div>
</div>

<div class='chart-box'>
    <h2>Historical Temperature Trends (C)</h2>
    <canvas id="telemetryChart" width="400" height="200"></canvas>
    <div id="chartNotice">Chart.js unavailable &mdash; connect to the internet to enable the graph.</div>
</div>

<!-- Chart.js at end of body so a slow/failed CDN load never blocks the table -->
<script src="https://cdn.jsdelivr.net/npm/chart.js@2.9.4/dist/Chart.min.js"></script>
<script>
    // Initialise chart only if Chart.js loaded successfully from CDN.
    // If it didn't load, the table still updates normally.
    let telemetryChart = null;
    try {
        const ctx = document.getElementById('telemetryChart').getContext('2d');
        telemetryChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Air Temp',
                        borderColor: '#ff9f43',
                        backgroundColor: 'rgba(255, 159, 67, 0.1)',
                        borderWidth: 2,
                        pointRadius: 3,
                        fill: true,
                        data: []
                    },
                    {
                        label: 'Water Probe',
                        borderColor: '#00cec9',
                        backgroundColor: 'rgba(0, 206, 201, 0.1)',
                        borderWidth: 2,
                        pointRadius: 3,
                        fill: true,
                        data: []
                    }
                ]
            },
            options: {
                responsive: true,
                scales: {
                    xAxes: [{
                        display: true,
                        gridLines: { display: false },
                        ticks: { fontSize: 10, fontColor: '#888' }
                    }],
                    yAxes: [{
                        display: true,
                        ticks: { min: 0, max: 40, stepSize: 10, fontSize: 10, fontColor: '#888' },
                        gridLines: { color: '#eee' }
                    }]
                }
            }
        });
    } catch(e) {
        document.getElementById('chartNotice').style.display = 'block';
    }

    async function updateDashboard() {
        const flag = document.getElementById('statusBox');

        /* PIPELINE 1: Live Text Metrics — always runs */
        try {
            let res = await fetch('/data');
            if (!res.ok) throw new Error("HTTP " + res.status);
            let json = await res.json();

            document.getElementById('t_dht').innerText = json.dht_temp.toFixed(1) + " C";
            document.getElementById('h_dht').innerText = json.dht_humidity.toFixed(0) + " %";
            document.getElementById('t_ds').innerText  = json.ds_probe.toFixed(1) + " C";
            document.getElementById('v_bat').innerText = json.battery_volts.toFixed(2) + "V (" + json.battery_percentage.toFixed(0) + "%)";

            flag.innerText = "Sync Active: " + (json.time_string || "Connected");
            flag.style.color = "green";
        } catch(err) {
            flag.innerText = "Telemetry Lag: Reconnecting...";
            flag.style.color = "#d97706";
        }

        /* PIPELINE 2: Historical Chart — only if Chart.js loaded */
        if (telemetryChart) {
            try {
                let resHistory = await fetch('/full_data');
                if (!resHistory.ok) throw new Error("HTTP " + resHistory.status);
                let historyData = await resHistory.json();

                if (Array.isArray(historyData) && historyData.length > 0) {
                    telemetryChart.data.labels           = historyData.map(p => p.time_string || "");
                    telemetryChart.data.datasets[0].data = historyData.map(p => p.dht_temp);
                    telemetryChart.data.datasets[1].data = historyData.map(p => p.ds_probe);
                    telemetryChart.update();
                }
            } catch(hErr) { /* chart update failure never blocks table */ }
        }

        // Schedule next update only after this one fully completes.
        // Using setTimeout instead of setInterval prevents a slow /full_data
        // response from stacking concurrent requests on the single-threaded server.
        setTimeout(updateDashboard, 4000);
    }

    window.onload = () => { updateDashboard(); };
</script>

</body>
</html>
)=====";

#endif

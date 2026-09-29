function switchTab(tabId) {
    document.querySelectorAll('.menu-item').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    if (event && event.target) {
        event.target.classList.add('active');
    }
    const tab = document.getElementById(tabId);
    if (tab) {
        tab.classList.add('active');
    }
}

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var isTestMode = false;
var isEspNowOn = false;

function initWebSocket() {
    websocket = new WebSocket(gateway);
    websocket.onopen = () => console.log('Connected');
    websocket.onclose = () => setTimeout(initWebSocket, 2000);
    websocket.onmessage = onMessage;
}

function onMessage(event) {
    var data = JSON.parse(event.data);
    if (data.type === "data") {
        document.getElementById('valQ').innerText = data.emgQ;
        document.getElementById('valT').innerText = data.emgT;
        document.getElementById('motorState').innerText = data.state;
        
        if (data.state === "EXTENDING") document.getElementById('motorState').style.color = "#03dac6";
        else if (data.state === "FLEXING") document.getElementById('motorState').style.color = "#bb86fc";
        else document.getElementById('motorState').style.color = "#cf6679";
        
        addData(data.emgQ, data.emgT, data.threshold);
        
        if (data.rxStatus === "Connected") {
            document.getElementById('rxStatus').innerText = "Connected";
            document.getElementById('rxStatus').style.color = "#03dac6";
            document.getElementById('rxMac').innerText = data.rxMac;
        } else {
            document.getElementById('rxStatus').innerText = "Searching...";
            document.getElementById('rxStatus').style.color = "#cf6679";
            document.getElementById('rxMac').innerText = "-";
        }
    } else if (data.type === "calib_done") {
        alert("Calibration complete!");
    }
}

function updateSettings() {
    var g = document.getElementById('gain').value;
    var t = document.getElementById('threshold').value;
    var r = document.getElementById('restAngle').value;
    var m = document.getElementById('minAngle').value;
    var mx = document.getElementById('maxAngle').value;
    
    document.getElementById('gainVal').innerText = g;
    document.getElementById('threshVal').innerText = t;
    document.getElementById('restVal').innerText = r;
    document.getElementById('minVal').innerText = m;
    document.getElementById('maxVal').innerText = mx;
    
    websocket.send(JSON.stringify({ 
        command: "settings", 
        gain: parseFloat(g), 
        threshold: parseInt(t), 
        restAngle: parseInt(r), 
        minAngle: parseInt(m), 
        maxAngle: parseInt(mx) 
    }));
}

function calibrate() {
    websocket.send(JSON.stringify({ command: "calibrate" }));
    alert("Calibration started... Please keep your muscles relaxed for 3 seconds.");
}

function toggleTestMode() {
    isTestMode = !isTestMode;
    var btn = document.getElementById('testBtn');
    var title = document.getElementById('modeTitle');
    if (isTestMode) {
        btn.innerText = "TEST MODE: ON";
        btn.classList.add('active');
        title.innerText = "Simulation Mode";
    } else {
        btn.innerText = "TEST MODE: OFF";
        btn.classList.remove('active');
        title.innerText = "Live EMG Signals";
    }
    websocket.send(JSON.stringify({ command: "test_mode", state: isTestMode }));
}

function toggleEspNow() {
    isEspNowOn = !isEspNowOn;
    var btn = document.getElementById('espNowBtn');
    if (isEspNowOn) {
        btn.innerText = "ESP-NOW: ON";
        btn.classList.add('active');
    } else {
        btn.innerText = "ESP-NOW: OFF";
        btn.classList.remove('active');
    }
    websocket.send(JSON.stringify({ command: "esp_now", state: isEspNowOn }));
}

window.addEventListener('load', () => { 
    initWebSocket(); 
    initGraph(); 
});

const canvas = document.getElementById('emgChart');
const ctx = canvas.getContext('2d');
let dataPointsQ = new Array(100).fill(0);
let dataPointsT = new Array(100).fill(0);
let currentThreshold = 1500;

function initGraph() { 
    canvas.width = canvas.parentElement.clientWidth; 
    canvas.height = 400; 
    drawGraph(); 
}

function addData(valQ, valT, thresh) { 
    dataPointsQ.push(valQ); 
    dataPointsQ.shift(); 
    dataPointsT.push(valT); 
    dataPointsT.shift(); 
    currentThreshold = thresh; 
    drawGraph(); 
}

function drawGraph() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.strokeStyle = '#333'; 
    ctx.beginPath();
    for (let i = 0; i < 5; i++) { 
        let y = (canvas.height / 4) * i; 
        ctx.moveTo(0, y); 
        ctx.lineTo(canvas.width, y); 
    }
    ctx.stroke();

    let maxGraphVal = 4000;
    let threshY = canvas.height - (currentThreshold / maxGraphVal) * canvas.height;
    ctx.strokeStyle = 'rgba(207, 102, 121, 0.5)'; 
    ctx.lineWidth = 2; 
    ctx.setLineDash([5, 5]);
    ctx.beginPath(); 
    ctx.moveTo(0, threshY); 
    ctx.lineTo(canvas.width, threshY); 
    ctx.stroke();
    ctx.setLineDash([]);

    ctx.strokeStyle = '#03dac6'; 
    ctx.lineWidth = 3; 
    ctx.beginPath();
    for (let i = 0; i < dataPointsQ.length; i++) {
        let x = (i / (dataPointsQ.length - 1)) * canvas.width;
        let y = canvas.height - (dataPointsQ[i] / maxGraphVal) * canvas.height;
        if (i === 0) ctx.moveTo(x, y); 
        else ctx.lineTo(x, y);
    }
    ctx.stroke();

    ctx.strokeStyle = '#bb86fc'; 
    ctx.lineWidth = 3; 
    ctx.beginPath();
    for (let i = 0; i < dataPointsT.length; i++) {
        let x = (i / (dataPointsT.length - 1)) * canvas.width;
        let y = canvas.height - (dataPointsT[i] / maxGraphVal) * canvas.height;
        if (i === 0) ctx.moveTo(x, y); 
        else ctx.lineTo(x, y);
    }
    ctx.stroke();
}

window.onresize = () => { 
    canvas.width = canvas.parentElement.clientWidth; 
    drawGraph(); 
};

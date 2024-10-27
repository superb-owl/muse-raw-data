const width = 800;
const height = 100;
const margin = { top: 10, right: 30, bottom: 20, left: 50 };
const graphGap = 20;

const totalHeight = (height + margin.top + margin.bottom + graphGap) * 7 + height + margin.top + margin.bottom;

const svg = d3.select("#graph")
    .append("svg")
    .attr("width", width + margin.left + margin.right)
    .attr("height", totalHeight)
    .append("g")
    .attr("transform", `translate(${margin.left},${margin.top})`);

const x = d3.scaleLinear().range([0, width]);
const y = d3.scaleLinear().range([height, 0]);

const line = d3.line()
    .x((d, i) => x(i))
    .y(d => y(d));

const colors = d3.schemeCategory10;

let data = {
    eeg: [[], [], [], []],
    ppg: [[], [], []],
    eegRaw: [[], [], [], []],
    ppgRaw: [[], [], []]
};

const maxDataPoints = 256 * 5; // Assuming 256Hz sampling rate
const averageWindow = 50; // Number of frames to average

function movingAverage(arr, window) {
    if (arr.length < window) {
        return arr;
    }
    const result = [];
    for (let i = window - 1; i < arr.length; i++) {
        const windowSlice = arr.slice(i - window + 1, i + 1);
        const validValues = windowSlice.filter(val => !isNaN(val));
        const avg = validValues.length > 0 ? validValues.reduce((acc, val) => acc + val, 0) / validValues.length : NaN;
        result.push(avg);
    }
    return result;
}


function createGraph(index, label) {
    const yPos = index * (height + margin.top + margin.bottom + graphGap);
    const g = svg.append("g")
        .attr("transform", `translate(0,${yPos})`);

    g.append("rect")
        .attr("width", width)
        .attr("height", height)
        .attr("fill", "none")
        .attr("stroke", "lightgray");

    g.append("g")
        .attr("transform", `translate(0,${height})`)
        .call(d3.axisBottom(x).ticks(5));

    g.append("g")
        .call(d3.axisLeft(y).ticks(5));

    g.append("text")
        .attr("x", -40)
        .attr("y", height / 2)
        .attr("text-anchor", "middle")
        .attr("transform", `rotate(-90, -40, ${height / 2})`)
        .text(label);

    return g.append("path")
        .attr("class", "line")
        .style("stroke", colors[index]);
}

const graphs = [
    ...Array(4).fill().map((_, i) => createGraph(i, `EEG ${i + 1}`)),
    ...Array(3).fill().map((_, i) => createGraph(i + 4, `PPG ${i + 1}`))
];

function updateGraph() {
    x.domain([0, maxDataPoints - 1]);

    const updateLine = (values, index) => {
        const smoothedValues = movingAverage(values, averageWindow);
        const filteredValues = smoothedValues.filter(val => !isNaN(val));
        if (filteredValues.length > 0) {
            y.domain([d3.min(filteredValues), d3.max(filteredValues)]);
            graphs[index].datum(filteredValues).attr("d", line);
        }
    };

    data.eeg.forEach((values, i) => updateLine(values, i));
    data.ppg.forEach((values, i) => updateLine(values, i + 4));
}

const ws = new WebSocket("ws://0.0.0.0:8765");

ws.onmessage = (event) => {
    const message = JSON.parse(event.data);
    
    message.eeg_channels.forEach((value, i) => {
        if (!isNaN(value)) {
            data.eegRaw[i].push(value);
            if (data.eegRaw[i].length > maxDataPoints) {
                data.eegRaw[i].shift();
            }
            data.eeg[i] = [...data.eegRaw[i]];
        }
    });

    message.ppg_channels.forEach((value, i) => {
        if (!isNaN(value)) {
            data.ppgRaw[i].push(value);
            if (data.ppgRaw[i].length > maxDataPoints) {
                data.ppgRaw[i].shift();
            }
            data.ppg[i] = [...data.ppgRaw[i]];
        }
    });

    updateGraph();
};

ws.onerror = (error) => {
    console.error("WebSocket error:", error);
};

ws.onclose = (event) => {
    if (event.wasClean) {
        console.log(`WebSocket connection closed cleanly, code=${event.code}, reason=${event.reason}`);
    } else {
        console.error('WebSocket connection died');
    }
};


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
    ppg: [[], [], []]
};

const maxDataPoints = 500; // Assuming 250Hz sampling rate, this will show 2 seconds of data

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
        y.domain([d3.min(values), d3.max(values)]);
        graphs[index].datum(values).attr("d", line);
    };

    data.eeg.forEach((values, i) => updateLine(values, i));
    data.ppg.forEach((values, i) => updateLine(values, i + 4));
}

const ws = new WebSocket("ws://0.0.0.0:8765");

ws.onmessage = (event) => {
    const message = JSON.parse(event.data);
    
    message.eeg_channels.forEach((value, i) => {
        data.eeg[i].push(value);
        if (data.eeg[i].length > maxDataPoints) {
            data.eeg[i].shift();
        }
    });

    message.ppg_channels.forEach((value, i) => {
        data.ppg[i].push(value);
        if (data.ppg[i].length > maxDataPoints) {
            data.ppg[i].shift();
        }
    });

    updateGraph();
};

ws.onerror = (error) => {
    console.error("WebSocket error:", error);
};

ws.onclose = () => {
    console.log("WebSocket connection closed");
};


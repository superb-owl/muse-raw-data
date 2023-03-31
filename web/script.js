window.data = null;
const labels = ['TP9', 'AF7', 'AF8', 'TP10'];
const bandColors = {
  delta: '#07136a',
  theta: '#0a1c9f',
  alpha: '#0d26d4',
  beta: '#2a42f0',
  gamma: '#5f71f4',
}
const bandAbbrevs = {
    delta: 'δ',
    theta: 'θ',
    alpha: 'α',
    beta: 'β',
    gamma: 'γ',
}
const bands = {
  delta: [1, 4],
  theta: [4, 8],
  alpha: [8, 12],
  beta: [12, 30],
  gamma: [30, 80],
}
const svg = d3.select('svg#RawData');
const margin = { top: 20, right: 20, bottom: 30, left: 50 };
const width = +svg.attr('width') - margin.left - margin.right;
const height = +svg.attr('height') - margin.top - margin.bottom;

const GRAPH_PAD = 20;
const GRAPH_HEIGHT = (height - (labels.length - 1) * GRAPH_PAD) / labels.length;
const GRAPH_OFFSET = GRAPH_HEIGHT + GRAPH_PAD;
const MAX_DATA_POINTS = 250;

function drawLines() {
  if (!window.data) {
    window.requestAnimationFrame(drawLines);
    return;
  }
  svg.selectAll('g').remove();
  const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);
  labels.forEach((_, sensorIdx) => {
    const offset = sensorIdx * GRAPH_OFFSET;
    const xScale = d3.scaleLinear().range([0, width]).domain([0, Math.min(MAX_DATA_POINTS, data.eeg_buffer.length)]);
    const yScale = d3.scaleLinear().range([offset + GRAPH_HEIGHT, offset]).domain([-1000, 1000]);

    const FREQ_WIDTH = 400;
    const SKIP_FREQUENCIES = 2; // exclude the 0 frequency due to log10 issues...what is that?
    const freqDomain = [data.frequency_buckets[SKIP_FREQUENCIES], data.frequency_buckets[data.frequency_buckets.length - 1]];
    const freqRange = d3.extent(data.fft.flat())

    const xFreqScale = d3.scaleLog()
      .domain(freqDomain)                     // domain refers to the x of the data
      .range([offset, GRAPH_HEIGHT + offset]) // range refers to the y of the (inverted) svg coordinates
    const yFreqScale = d3.scaleLinear()
      .domain(freqRange)                      // domain refers to the y of the data
      .range([0, FREQ_WIDTH])                 // range refers to the x of the (inverted) svg coordinates

    const freqLine = d3.line() // these x, y refer to svg coordinates. y coord maps to x coord of data
        .y((d, i) => xFreqScale(data.frequency_buckets[i + SKIP_FREQUENCIES]))
        .x(d => yFreqScale(d));

    const xAxis = g.append('g')
        .attr('transform', `translate(0,${offset + GRAPH_HEIGHT / 2})`)
        .call(d3.axisBottom(xScale));
    const yAxis = g.append('g')
        .call(
          // Skip some ticks to make the graph more readable
          d3.axisLeft(xFreqScale).tickFormat((x, i) => i % 3 > 0 ? '' : `${x.toFixed(1)}Hz`)
        );
    g.append("text")
        .attr("class", "y label")
        .attr("text-anchor", "end")
        .attr("dx", "-1em")
        .attr("dy", ".5em")
        .attr("y", offset + GRAPH_HEIGHT / 2)
        .text(labels[sensorIdx]);

    Object.keys(bands).forEach((band, bandIdx) => {
      const bandSize = data.bands[band][sensorIdx];
      g.append('rect')
          .attr('x', xFreqScale(0))
          .attr('y', xFreqScale(bands[band][0]))
          .attr('height', xFreqScale(bands[band][1]) - xFreqScale(bands[band][0]))
          .attr('width', yFreqScale(bandSize))
          .attr('fill', bandColors[band])
    })
    const line = d3.line()
        .x((d, i) => xScale(i))
        .y(d => yScale(d));

    const lineData = data.eeg_buffer.map(d => d[sensorIdx]);
    g.append('path')
        .datum(lineData)
        .attr('class', 'line')
        .attr('d', line)
        .attr('stroke', d3.schemeCategory10[6]); // Color from the predefined color scheme

    const fftData = data.fft.map(d => d[sensorIdx]).slice(SKIP_FREQUENCIES);
    g.append('path')
        .datum(fftData)
        .attr('class', 'line')
        .attr('d', freqLine)
        .attr('stroke', 'green'); // Color from the predefined color scheme

    Object.keys(bands).forEach((band, bandIdx) => {
      const barWidth = yFreqScale(data.bands[band][sensorIdx]);
      const xLoc = yFreqScale(0) + 10;
      const color = barWidth < xLoc + 10 ? "black" : "white";
      g.append("text")
          .attr("x", xFreqScale(0) + 10)
          .attr('y', .5 * (xFreqScale(bands[band][0]) + xFreqScale(bands[band][1])))
          .attr("dy", ".3em")
          .attr("dx", "1em")
          .attr("fill", color)
          .text(bandAbbrevs[band])
    })
  });
  document.getElementById('Debug').innerHTML = `Sample rate: ${data.sample_rate} samples/sec`;
  window.requestAnimationFrame(drawLines);
}
window.requestAnimationFrame(drawLines);


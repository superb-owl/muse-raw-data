window.data = null;
const labels = ['TP9', 'AF7', 'AF8', 'TP10', 'Right AUX'];
const bands = ['delta', 'theta', 'alpha', 'beta', 'gamma'];
const svg = d3.select('svg');
const margin = { top: 20, right: 20, bottom: 30, left: 50 };
const width = +svg.attr('width') - margin.left - margin.right;
const height = +svg.attr('height') - margin.top - margin.bottom;

const GRAPH_PAD = 20;
const GRAPH_HEIGHT = (height - (labels.length - 1) * GRAPH_PAD) / labels.length;
const GRAPH_OFFSET = GRAPH_HEIGHT + GRAPH_PAD;

function drawLines() {
  if (!window.data) {
    window.requestAnimationFrame(drawLines);
    return;
  }
  svg.selectAll('g').remove();
  const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);
  data.eeg_buffer[0].forEach((_, i) => {
    const offset = i * GRAPH_OFFSET;
    const xScale = d3.scaleLinear().range([0, width]).domain([0, data.eeg_buffer.length]);
    const yScale = d3.scaleLinear().range([offset + GRAPH_HEIGHT, offset]).domain([-1000, 1000]);

    // X and Y axes
    const xAxis = g.append('g')
        .attr('transform', `translate(0,${offset + GRAPH_HEIGHT / 2})`)
        .call(d3.axisBottom(xScale));
    const yAxis = g.append('g')
        .call(d3.axisLeft(yScale));
    g.append("text")
    .attr("class", "y label")
    .attr("text-anchor", "end")
    .attr("dx", "-1em")
    .attr("dy", ".2em")
    .attr("y", offset + GRAPH_HEIGHT / 2)
    .text(labels[i]);

    bands.forEach((band, bandIdx) => {
      bandSize = data.bands[band][i] * 100;
      g.append('rect')
      .attr('x', bandIdx * 100)
      .attr('y', yScale(bandSize))
      .attr('width', 50)
      .attr('height', yScale(0) - yScale(bandSize))
      .attr('fill', d3.schemeCategory10[bandIdx])
    })

    // Line generator
    const line = d3.line()
        .x((d, i) => xScale(i))
        .y(d => yScale(d));

    const lineData = data.eeg_buffer.map(d => d[i]);
    g.append('path')
        .datum(lineData)
        .attr('class', 'line')
        .attr('d', line)
        .attr('stroke', d3.schemeCategory10[i % 10]); // Color from the predefined color scheme

    const fftData = data.fft.map(d => d[i] * 100);
    g.append('path')
        .datum(fftData)
        .attr('class', 'line')
        .attr('d', line)
        .attr('stroke', d3.schemeCategory10[(2*i) % 10]); // Color from the predefined color scheme
  });
  document.getElementById('Debug').innerHTML = `Sample rate: ${data.sample_rate} samples/sec`;
  window.requestAnimationFrame(drawLines);
}
window.requestAnimationFrame(drawLines);


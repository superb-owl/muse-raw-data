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
  data.eeg_buffer[0].forEach((_, sensorIdx) => {
    const offset = sensorIdx * GRAPH_OFFSET;
    const xScale = d3.scaleLinear().range([0, width]).domain([0, data.eeg_buffer.length]);
    const yScale = d3.scaleLinear().range([offset + GRAPH_HEIGHT, offset]).domain([-1000, 1000]);

    const FREQ_WIDTH = 400;
    const SKIP_FREQUENCIES = 4; // exclude the 0 frequency due to log10 issues...what is that?
    freq_range = [data.frequency_buckets[SKIP_FREQUENCIES], data.frequency_buckets[data.frequency_buckets.length - 1]];

    // domain refers to the x,y of the data. Ranges are the (inverted) svg coordinates
    const xFreqScale = d3.scaleLog()
      .range([offset, GRAPH_HEIGHT + offset])
      .domain(freq_range)
    xFreqScale.ticks(3)
    const yFreqScale = d3.scaleLinear()
      .range([0, FREQ_WIDTH])
      .domain([0, 1000])
    // these x, y refer to svg coordinates. y coord maps to x coord of data
    const freqLine = d3.line()
        .y((d, i) => xFreqScale(data.frequency_buckets[i + SKIP_FREQUENCIES]))
        .x(d => yFreqScale(d));

    // X and Y axes
    const xAxis = g.append('g')
        .attr('transform', `translate(0,${offset + GRAPH_HEIGHT / 2})`)
        .call(d3.axisBottom(xScale));
    const yAxis = g.append('g')
        .call(
          d3.axisLeft(xFreqScale).ticks(3).tickFormat(x => `${x.toFixed(1)}Hz`)
        );
    g.append("text")
    .attr("class", "y label")
    .attr("text-anchor", "end")
    .attr("dx", "-1em")
    .attr("dy", ".2em")
    .attr("y", offset + GRAPH_HEIGHT / 2)
    .text(labels[sensorIdx]);

    bands.forEach((band, bandIdx) => {
      bandSize = data.bands[band][sensorIdx] * 100;
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

    const lineData = data.eeg_buffer.map(d => d[sensorIdx]);
    g.append('path')
        .datum(lineData)
        .attr('class', 'line')
        .attr('d', line)
        .attr('stroke', d3.schemeCategory10[6]); // Color from the predefined color scheme

    const fftData = data.fft.map(d => d[sensorIdx] * 10).slice(SKIP_FREQUENCIES);
    g.append('path')
        .datum(fftData)
        .attr('class', 'line')
        .attr('d', freqLine)
        .attr('stroke', d3.schemeCategory10[7]); // Color from the predefined color scheme
  });
  document.getElementById('Debug').innerHTML = `Sample rate: ${data.sample_rate} samples/sec`;
  window.requestAnimationFrame(drawLines);
}
window.requestAnimationFrame(drawLines);


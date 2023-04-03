(function() {
const svg = d3.select('svg#Heartbeat');
const margin = { top: 20, right: 20, bottom: 30, left: 50 };
const width = +svg.attr('width') - margin.left - margin.right;
const height = +svg.attr('height') - margin.top - margin.bottom;

const scales = [
  [37000,37700],
  [32500,32800],
  [0, 150],
]

const colors = [
  'pink', 'red', 'darkred',
]

let beatIdx = 0;
let lastTime = new Date().getTime();
function drawHeartbeat() {
  if (!window.data) {
    window.requestAnimationFrame(drawHeartbeat);
    return;
  }
  svg.selectAll('g').remove();
  const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);
  const xScale = d3.scaleLinear().range([0, width]).domain([0, data.ppg_buffer.length]);
  const xAxis = g.append('g')
      .attr('transform', `translate(0,${height / 2})`)
      .call(d3.axisBottom(xScale));
  for (let sensor = 0; sensor < 3; sensor ++) {
    const lineData = data.ppg_buffer.map(d => d[sensor]);
    const yScale = d3.scaleLinear().range([height, 0]).domain(d3.extent(lineData));
    const line = d3.line()
        .x((d, i) => xScale(i))
        .y(d => yScale(d));
    g.append('path')
        .attr('class', 'heartbeat-line')
        .datum(lineData)
        .attr('d', line)
        .attr('stroke', colors[sensor]);
  }

  const START_FREQ = 1;
  const END_FREQ = 6;
  const freqDomain = [data.ppg_frequency_buckets[START_FREQ], data.ppg_frequency_buckets[START_FREQ + END_FREQ]];
  const freqRange = d3.extent(data.ppg_fft.map(d => d[0]));
  const FREQ_WIDTH = 400;

  const xFreqScale = d3.scaleLog()
    .domain(freqDomain)                     // domain refers to the x of the data
    .range([0, height])                     // range refers to the y of the (inverted) svg coordinates
  const yFreqScale = d3.scaleLinear()
    .domain(freqRange)                      // domain refers to the y of the data
    .range([0, FREQ_WIDTH])                 // range refers to the x of the (inverted) svg coordinates

  const yAxis = g.append('g')
      .call(
        // Skip some ticks to make the graph more readable
        d3.axisLeft(xFreqScale).tickFormat((x, i) => `${x.toFixed(1)}Hz`)
      );
  const freqLine = d3.line() // these x, y refer to svg coordinates. y coord maps to x coord of data
      .y((d, i) => xFreqScale(data.ppg_frequency_buckets[START_FREQ + i]))
      .x(d => yFreqScale(d));

  const fftData = data.ppg_fft.map(d => d[0]).slice(START_FREQ, END_FREQ);
  g.append('path')
      .datum(fftData)
      .attr('class', 'frequency-line')
      .attr('d', freqLine)

  let heartrate = 0.0;
  let total = 0.0;
  fftData.forEach((amt, idx) => {
    const freq = data.ppg_frequency_buckets[START_FREQ + idx];
    heartrate += freq * amt;
    total += amt;
  });

  heartrate /= total;
  g.append("text")
      .attr("class", "y label")
      .attr("text-anchor", "end")
      .attr("dx", "1em")
      .attr("dy", "1em")
      .attr("x", width)
      .attr("y", 0)
      .text((heartrate * 60).toFixed(1) + "bpm")

  /*
  const now = new Date().getTime();
  const msPassed = now - lastTime;
  const msPerSample = 1000 / data.ppg_sample_rate;
  const idxDiff = Math.round(msPassed / msPerSample);
  beatIdx = (beatIdx + idxDiff) % data.ppg_buffer.length;
  console.log('beatIdx', beatIdx);
  lastTime = now;
  const size = data.ppg_buffer[beatIdx][2] / 100;
  const s1 = 100 * size;
  const s2 = 50 * size;
  const s3 = 45 * size;
  let heartPath = `M0 ${s1} v-${s1} h${s1}
    a${s2},${s2} ${s3} 0,1 0,${s1}
    a${s2},${s2} ${s3} 0,1 -${s1},0
    z`
  // Draw the heart
  g.append("path")
      .attr("d", heartPath)
      .attr("fill", "red")
      .attr("stroke", "black")
      .attr("stroke-width", 2)
      .attr('transform', "rotate(225,150,121)")
  */

  window.requestAnimationFrame(drawHeartbeat);
}
window.requestAnimationFrame(drawHeartbeat);
})();

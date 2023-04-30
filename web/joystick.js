(function() {
const svg = d3.select('svg#Joystick');
const margin = { top: 20, right: 20, bottom: 30, left: 50 };
const width = +svg.attr('width') - margin.left - margin.right;
const height = +svg.attr('height') - margin.top - margin.bottom;

const colors = [
  'lightblue', 'purple',
]

function drawJoystick() {
  if (!window.data) {
    window.requestAnimationFrame(drawJoystick);
    return;
  }
  console.log('draw js', data.joystick_buffer);
  svg.selectAll('g').remove();
  const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);
  const xScale = d3.scaleLinear().range([0, width]).domain([0, data.joystick_buffer.length])
  const xAxis = g.append('g')
      .attr('transform', `translate(0,${height / 2})`)
      .call(d3.axisBottom(xScale));
  for (let joystickAxis = 0; joystickAxis < 2; joystickAxis++) {
    const lineData = data.joystick_buffer.map(d => d[joystickAxis]);
    const yScale = d3.scaleLinear().range([height, 0]).domain([-1, 1]);
    const line = d3.line()
        .x((d, i) => xScale(i))
        .y(d => yScale(d));
    g.append('path')
        .attr('class', 'heartbeat-line')
        .datum(lineData)
        .attr('d', line)
        .attr('stroke', colors[joystickAxis]);
  }

    /*
  const START_FREQ = 0.5;
  const END_FREQ = 10.0;
  const START_FREQ_BUCKET = data.ppg_frequency_buckets.findIndex(f => f >= START_FREQ);
  const END_FREQ_BUCKET = data.ppg_frequency_buckets.findLastIndex(f => f <= END_FREQ);
  const freqDomain = [data.ppg_frequency_buckets[START_FREQ_BUCKET], data.ppg_frequency_buckets[START_FREQ_BUCKET + END_FREQ_BUCKET]];
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
      .y((d, i) => xFreqScale(data.ppg_frequency_buckets[START_FREQ_BUCKET + i]))
      .x(d => yFreqScale(d));

  const fftData = data.ppg_fft.map(d => d[0]).slice(START_FREQ_BUCKET, END_FREQ_BUCKET);
  g.append('path')
      .datum(fftData)
      .attr('class', 'frequency-line')
      .attr('d', freqLine)

  let heartrate = 0.0;
  let total = 0.0;
  fftData.forEach((amt, idx) => {
    const freq = data.ppg_frequency_buckets[START_FREQ_BUCKET + idx];
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

    */
  window.requestAnimationFrame(drawJoystick);
}
window.requestAnimationFrame(drawJoystick);
})();

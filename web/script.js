window.data = [];
const svg = d3.select('svg');
const margin = { top: 20, right: 20, bottom: 30, left: 50 };
const width = +svg.attr('width') - margin.left - margin.right;
const height = +svg.attr('height') - margin.top - margin.bottom;
const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);
// Scales
const BUFFER_SIZE = 1280;
const xScale = d3.scaleLinear().range([0, width]).domain([0, BUFFER_SIZE]);
const yScale = d3.scaleLinear().range([height, 0]).domain([-1000, 1000]);

// X and Y axes
const xAxis = g.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(d3.axisBottom(xScale));
const yAxis = g.append('g')
    .call(d3.axisLeft(yScale));

// Line generator
const line = d3.line()
    .x((d, i) => xScale(i))
    .y(d => yScale(d));

function drawLines() {
  if (!data.length) {
    window.requestAnimationFrame(drawLines);
    return;
  }
    g.selectAll('path').remove();

// Draw lines
  data[0].forEach((_, i) => {
      const lineData = data.map(d => d[i]);
      g.append('path')
          .datum(lineData)
          .attr('class', 'line')
          .attr('d', line)
          .attr('stroke', d3.schemeCategory10[i % 10]); // Color from the predefined color scheme
  });
  window.requestAnimationFrame(drawLines);
}
window.requestAnimationFrame(drawLines);


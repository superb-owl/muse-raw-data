(function() {
const svg = d3.select('svg#CenterOfMass');
const margin = { top: 20, right: 20, bottom: 30, left: 50 };
const width = +svg.attr('width') - margin.left - margin.right;
const height = +svg.attr('height') - margin.top - margin.bottom;

const bandColors = {
  all: 'black',
  delta: 'red',
  theta: 'orange',
  alpha: 'green',
  beta: 'blue',
  gamma: 'purple',
}

function drawCenterOfMass() {
  if (!window.data) {
    window.requestAnimationFrame(drawCenterOfMass);
    return;
  }
  svg.selectAll('g').remove();
  const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);
  const xScale = d3.scaleLinear().range([0, width]).domain([-100, 100]);
  const yScale = d3.scaleLinear().range([0, height]).domain([-100, 100]);

  g.append('circle') // nose
    .attr('cx', width / 2).attr('cy', 0)
    .attr('r', 15)
    .attr('stroke', 'black')
    .attr('fill', 'white')
  g.append('circle') // head
    .attr('cx', width / 2).attr('cy', height / 2)
    .attr('r', height / 2)
    .attr('stroke', 'black')
    .attr('fill', 'white')

  function adjustWeights(weights, data, sensor) {
      const weight = data.reduce((acc, next) => acc + Math.abs(next), 0) / data.length;
      if (sensor === 'TP9') {
        weights.back += weight;
        weights.left += weight;
      } else if (sensor === 'AF7') {
        weights.front += weight;
        weights.left += weight;
      } else if (sensor === 'AF8') {
        weights.front += weight;
        weights.right += weight;
      } else if (sensor === 'TP10') {
        weights.back += weight;
        weights.right += weight;
      }
  }

  const weights = {};
  Object.keys(bands).concat(['all']).forEach((band) => {
    weights[band] = {
      front: 0,
      back: 0,
      left: 0,
      right: 0,
    };
  })
  sensors.forEach((sensorLabel, sensorIdx) => {
    const allBandsData = Object.keys(bands).map(band => data.eeg_bands[band][sensorIdx])
    adjustWeights(weights.all, allBandsData, sensorLabel);
    Object.keys(bands).forEach((band, bandIdx) => {
      const bandData = [data.eeg_bands[band][sensorIdx]];
      adjustWeights(weights[band], bandData, sensorLabel);
    });
  });
  Object.keys(weights).forEach(band => {
    const xLoc = weights[band].right - weights[band].left;
    const yLoc = weights[band].back - weights[band].front;
    const total = weights[band].right + weights[band].left + weights[band].back + weights[band].front;
    g.append('circle')
      .attr('cx', xScale(xLoc)).attr('cy', yScale(yLoc))
      .attr('r', 20 * total / 100)
      .attr('stroke-width', 2)
      .attr('stroke', bandColors[band])
      .attr('fill', 'none')
    g.append('text')
      .attr('x', xScale(xLoc)).attr('y', yScale(yLoc))
      .attr('dx', '-.3em')
      .attr('font-size', 15)
      .attr('stroke', 'black')
      .text(bandAbbrevs[band])
  })

  window.requestAnimationFrame(drawCenterOfMass);
}
window.requestAnimationFrame(drawCenterOfMass);
})();

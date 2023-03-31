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
  const xScale = d3.scaleLinear().range([0, width]).domain([-1000, 1000]);
  const yScale = d3.scaleLinear().range([height, 0]).domain([-1000, 1000]);

  g.append('circle') // nose
    .attr('cx', width / 2).attr('cy', 0)
    .attr('r', 5)
    .attr('stroke', 'black')
    .attr('fill', 'none')
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

  const numTimeBuckets = 1;
  const bucketSize = Math.floor(data.eeg_buffer[0].length / numTimeBuckets);
  const buckets = [];
  for (let bucket = 0; bucket < numTimeBuckets; bucket++) {
    const bucketStart = bucket * bucketSize;
    const weights = {};
    Object.keys(bands).concat(['all']).forEach((band) => {
      weights[band] = {
        front: 0,
        back: 0,
        left: 0,
        right: 0,
      };
    })
    labels.forEach((sensorLabel, sensorIdx) => {
      const bucketData = data.eeg_buffer.map(d => d[sensorIdx]).slice(bucketStart, bucketStart + bucketSize);
      adjustWeights(weights.all, bucketData, sensorLabel);
      Object.keys(bands).forEach((band, bandIdx) => {
        const bucketBandData = [data.bands[band][sensorIdx] * 10];
        adjustWeights(weights[band], bucketBandData, sensorLabel);
      });
    });
    console.log('weights', weights);
    Object.keys(weights).forEach(band => {
      const xLoc = weights[band].right - weights[band].left;
      const yLoc = weights[band].back - weights[band].front;
      const total = weights[band].right + weights[band].left + weights[band].back + weights[band].front;
      g.append('circle')
        .attr('cx', xScale(xLoc)).attr('cy', yScale(yLoc))
        .attr('r', 20 * total / 4000)
        .attr('stroke-width', 2)
        .attr('stroke', bandColors[band])
        .attr('fill', 'none')
    })
  }

  window.requestAnimationFrame(drawCenterOfMass);
}
window.requestAnimationFrame(drawCenterOfMass);
})();

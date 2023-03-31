(function() {
const svg = d3.select('svg#CenterOfMass');
const margin = { top: 20, right: 20, bottom: 30, left: 50 };
const width = +svg.attr('width') - margin.left - margin.right;
const height = +svg.attr('height') - margin.top - margin.bottom;

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

  const numTimeBuckets = 1;
  const bucketSize = Math.floor(data.eeg_buffer[0].length / numTimeBuckets);
  const buckets = [];
  for (let bucket = 0; bucket < numTimeBuckets; bucket++) {
    const bucketStart = bucket * bucketSize;
    const weights = {
      front: 0,
      back: 0,
      left: 0,
      right: 0,
    }
    labels.forEach((label, sensorIdx) => {
      const bucketData = data.eeg_buffer.map(d => d[sensorIdx]).slice(bucketStart, bucketStart + bucketSize);
      const weight = bucketData.reduce((acc, next) => acc + Math.abs(next), 0) / bucketData.length;
      if (label === 'TP9') {
        weights.back += weight;
        weights.left += weight;
      } else if (label === 'AF7') {
        weights.front += weight;
        weights.left += weight;
      } else if (label === 'AF8') {
        weights.front += weight;
        weights.right += weight;
      } else if (label === 'TP10') {
        weights.back += weight;
        weights.right += weight;
      }
    });
    console.log(weights);
    const xLoc = weights.right - weights.left;
    const yLoc = weights.back - weights.front;
    console.log(xLoc, yLoc);
    g.append('circle')
      .attr('cx', xScale(xLoc)).attr('cy', yScale(yLoc))
      .attr('r', 2 * (bucket + 1))
      .attr('stroke', 'black')
      .attr('fill', 'none')
  }

  window.requestAnimationFrame(drawCenterOfMass);
}
window.requestAnimationFrame(drawCenterOfMass);
})();

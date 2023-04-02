window.data = null; const ratioGraphs = {
  'RightToLeft': {
    dimension: 'bands',
    a: 'left',
    b: 'right',
  },
  'FrontToBack': {
    dimension: 'bands',
    a: 'back',
    b: 'front',
  },
  'DeltaToGamma': {
    dimension: 'sensors',
    a: 'gamma',
    b: 'delta',
  },
  'Harmonics': {
    dimension: 'sensors',
    a: 'harmonic',
    b: 'chaos',
  }
};

function getPower(data, direction) {
  if (direction === 'left') return data[0] + data[1];
  if (direction === 'right') return data[2] + data[3];
  if (direction === 'front') return data[1] + data[2];
  if (direction === 'back') return data[0] + data[3];
}

Object.keys(ratioGraphs).forEach(ratioID => {
  const settings = ratioGraphs[ratioID];
  const svg = d3.select('svg#' + ratioID);
  const margin = { top: 20, right: 20, bottom: 20, left: 20 };
  const width = +svg.attr('width') - margin.left - margin.right;
  const height = +svg.attr('height') - margin.top - margin.bottom;
  function drawRatio() {
    if (!window.data) {
      window.requestAnimationFrame(drawRatio);
      return;
    }
    svg.selectAll('g').remove();
    const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);
    const xScale = d3.scaleLinear().range([0, width]).domain([-50, 50])
    const yScale = d3.scaleLinear().range([40, height]).domain([0, 5]);

    g.append('text')
      .attr('x', 0)
      .attr('y', 0)
      .attr('dy', '1em')
      .text(settings.a)
    g.append('text')
      .attr('x', width)
      .attr('y', 0)
      .attr('dx', '-2em')
      .attr('dy', '1em')
      .text(settings.b)
    g.append('line')
      .attr('x1', 0)
      .attr('y1', '1.1em')
      .attr('x2', width)
      .attr('y2', '1.1em')
      .attr('stroke', 'black')
    g.append('line')
      .attr('x1', width/2)
      .attr('y1', '1.1em')
      .attr('x2', width/2)
      .attr('y2', height)
      .attr('stroke', 'black')

    let as = [];
    let bs = [];
    let labels = [];
    if (settings.dimension === 'bands') {
      labels = bandOrder.map(band => bandAbbrevs[band]);
      as = bandOrder.map(band => getPower(data.bands[band], settings.a));
      bs = bandOrder.map(band => getPower(data.bands[band], settings.b));
    } else if (settings.dimension === 'sensors') {
      labels = sensors;
      if (settings.a === 'harmonic') {
        as = sensors.map((sensor, sensorIdx) => getVariance(data.fft.map(d => d[sensorIdx])));
        bs = sensors.map((sensor, sensorIdx) => getHarmonicVariance(data.fft.map(d => d[sensorIdx])));
      } else {
        as = sensors.map((sensor, sensorIdx) => data.bands[settings.a][sensorIdx])
        bs = sensors.map((sensor, sensorIdx) => data.bands[settings.b][sensorIdx])
      }
    } else {
      throw new Error("Unknown dimension " + settings.dimension);
    }

    for (let idx = 0; idx < as.length; idx ++) {
      const a = as[idx];
      const b = bs[idx];
      const label = labels[idx];
      const lineSize = b - a;
      g.append('text')
        .attr('x', width / 2)
        .attr('dx', '0.1em')
        .attr('y', yScale(idx))
        .text(label)
      g.append('line')
        .attr('class', 'ratio-line')
        .attr('y1', yScale(idx))
        .attr('y2', yScale(idx))
        .attr('x1', width / 2)
        .attr('x2', xScale(lineSize))
        .attr('stroke', 'steelblue')
        .attr('stroke-width', 4)
    }
    window.requestAnimationFrame(drawRatio);
  }
  window.requestAnimationFrame(drawRatio);
})


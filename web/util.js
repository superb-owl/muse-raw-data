const sensors = ['TP9', 'AF7', 'AF8', 'TP10'];
const bandOrder = [
  'delta',
  'theta',
  'alpha',
  'beta',
  'gamma',
];
const bandColors = {
  delta: '#09188a',
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

function getVariance(data) {
  const avg = data.reduce((a, c) => a + c, 0.0) / data.length;
  const avgSq = data.reduce((a, c) => a + c * c, 0.0) / data.length;
  return avgSq - avg * avg;
}

function getHarmonicVariance(data) {
  let totalVariance = 0.0;
  let numHarmonics = 0;
  for (let base = 1; base < data.length; base++) {
    let harmonicData = [];
    for (let harmonic = base; harmonic < data.length; harmonic = harmonic * 2) {
      harmonicData.push(data[harmonic - 1]);
    }
    if (harmonicData.length > 1) {
      totalVariance += getVariance(harmonicData);
      numHarmonics++;
    }
  }
  return totalVariance / numHarmonics;
}


import { samples } from './samples.js?v=2';

const gallery = document.getElementById('gallery');
const player = document.getElementById('player');
const frame = document.getElementById('sample-frame');
const title = document.getElementById('player-title');
const category = document.getElementById('player-category');
const help = document.getElementById('player-help');
const closeButton = document.getElementById('close');
const reloadButton = document.getElementById('reload');
let currentSample = null;

function sizePlayer() {
  const widthFromViewport = window.innerWidth - 28;
  const widthFromHeight = (window.innerHeight - 160) * 4 / 3;
  const width = Math.max(0, Math.min(1120, widthFromViewport, widthFromHeight));
  player.style.width = `${width}px`;
}

function sampleUrl(sample) {
  return `samples/${sample.id}/index.html`;
}

function openSample(sample) {
  currentSample = sample;
  sizePlayer();
  title.textContent = sample.title;
  category.textContent = sample.category;
  help.textContent = sample.help;
  frame.classList.add('is-loading');
  frame.src = sampleUrl(sample);
  player.showModal();
}

function closeSample() {
  player.close();
  frame.classList.add('is-loading');
  frame.src = 'about:blank';
  currentSample = null;
}

samples.forEach((sample, index) => {
  const card = document.createElement('article');
  card.className = 'sample-card';
  card.innerHTML = `
    <div class="sample-preview" role="img" aria-label="${sample.title} preview">
      <img class="preview-image" src="previews/${sample.id}.png" alt="" loading="lazy">
      <span class="preview-spinner" aria-hidden="true"></span>
      <span class="preview-status">Loading preview</span>
    </div>
    <span class="sample-index">${String(index + 1).padStart(2, '0')}</span>
    <span class="sample-category">${sample.category}</span>
    <h3>${sample.title}</h3>
    <p>${sample.description}</p>
    <button class="run" type="button">Run sample</button>
  `;

  const image = card.querySelector('.preview-image');
  image.addEventListener('load', () => card.classList.add('preview-ready'));
  image.addEventListener('error', () => {
    card.querySelector('.preview-status').textContent = 'Preview unavailable';
    card.classList.add('preview-error');
  });
  card.querySelector('.run').addEventListener('click', () => openSample(sample));
  gallery.appendChild(card);
});

closeButton.addEventListener('click', closeSample);
reloadButton.addEventListener('click', () => {
  if (currentSample) {
    frame.classList.add('is-loading');
    frame.src = sampleUrl(currentSample);
  }
});
frame.addEventListener('load', () => {
  if (currentSample) frame.classList.remove('is-loading');
});
window.addEventListener('resize', () => {
  if (player.open) sizePlayer();
});
player.addEventListener('cancel', (event) => {
  event.preventDefault();
  closeSample();
});
player.addEventListener('click', (event) => {
  if (event.target === player) closeSample();
});

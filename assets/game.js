const audioIds = ['move', 'food', 'turn', 'win', 'die'];
const audios = {};
let audioContext;
let pointerStart = null;
let longPressTimer = null;
let longPressTriggered = false;

const gestureThreshold = 18;
const longPressDelay = 500;

function sendAction(action) {
  CallHandler('game', 'action', action);
}

function setup(showControls) {
  buildCells(document.getElementById('board'), 200);
  buildCells(document.getElementById('next'), 16);
  document.getElementById('controls').hidden = !showControls;

  const AudioContextClass = window.AudioContext || window.webkitAudioContext;
  if (!AudioContextClass) return;
  audioContext = new AudioContextClass();
  audioIds.forEach(function(id) {
    fetch(cross_asset_domain_ + 'wave/' + id + '.wav')
        .then(function(response) {
          return response.arrayBuffer();
        })
        .then(function(data) {
          return audioContext.decodeAudioData(data);
        })
        .then(function(buffer) {
          audios[id] = buffer;
        })
        .catch(function() {});
  });
}

function buildCells(element, count) {
  const fragment = document.createDocumentFragment();
  for (let i = 0; i < count; ++i) {
    const cell = document.createElement('div');
    cell.className = 'cell';
    fragment.appendChild(cell);
  }
  element.replaceChildren(fragment);
}

function paint(element, state) {
  for (let i = 0; i < state.length; ++i)
    element.children[i].dataset.piece = state[i];
}

function renderGame(
    board, next, score, lines, level, paused, gameOver, cleanupPhase,
    cleanupRow) {
  const boardElement = document.getElementById('board');
  boardElement.querySelectorAll('.cell-clearing, .cell-moving').forEach(
      function(cell) {
        cell.classList.remove('cell-clearing', 'cell-moving');
      });
  paint(boardElement, board);
  if (cleanupRow >= 0 && cleanupRow < 20) {
    if (cleanupPhase === 1) {
      for (let column = 0; column < 10; ++column) {
        boardElement.children[cleanupRow * 10 + column]
            .classList.add('cell-clearing');
      }
    } else if (cleanupPhase === 2) {
      for (let row = 0; row < cleanupRow; ++row) {
        for (let column = 0; column < 10; ++column) {
          const cell = boardElement.children[row * 10 + column];
          if (cell.dataset.piece !== '0') cell.classList.add('cell-moving');
        }
      }
    }
  }
  paint(document.getElementById('next'), next);
  document.getElementById('score').textContent = score;
  document.getElementById('lines').textContent = lines;
  document.getElementById('level').textContent = level;
  document.getElementById('pause').textContent = paused ? 'Resume' : 'Pause';

  const overlay = document.getElementById('overlay');
  overlay.hidden = !paused && !gameOver;
  document.getElementById('overlay-title').textContent =
      gameOver ? 'Game Over' : 'Paused';
  document.getElementById('overlay-help').textContent =
      gameOver ? 'Press restart to play again' : 'Press Pause to continue';
  document.getElementById('restart').hidden = !gameOver;
}

function playAudio(id) {
  if (!audioContext || !audios[id]) return;
  if (audioContext.state === 'suspended') audioContext.resume();
  const source = audioContext.createBufferSource();
  source.buffer = audios[id];
  source.connect(audioContext.destination);
  source.start();
}

function pointerDown(event) {
  pointerStart = {x: event.clientX, y: event.clientY};
  longPressTriggered = false;
  event.currentTarget.setPointerCapture(event.pointerId);
  clearTimeout(longPressTimer);
  longPressTimer = setTimeout(function() {
    if (!pointerStart) return;
    longPressTriggered = true;
    sendAction('drop');
  }, longPressDelay);
}

function pointerMove(event) {
  if (!pointerStart) return;
  const dx = event.clientX - pointerStart.x;
  const dy = event.clientY - pointerStart.y;
  if (Math.max(Math.abs(dx), Math.abs(dy)) >= gestureThreshold) {
    clearTimeout(longPressTimer);
    longPressTimer = null;
  }
}

function pointerUp(event) {
  if (!pointerStart) return;
  clearTimeout(longPressTimer);
  longPressTimer = null;
  const dx = event.clientX - pointerStart.x;
  const dy = event.clientY - pointerStart.y;
  pointerStart = null;
  if (longPressTriggered) return;
  if (Math.max(Math.abs(dx), Math.abs(dy)) < gestureThreshold)
    sendAction('rotate');
  else if (Math.abs(dx) > Math.abs(dy))
    sendAction(dx < 0 ? 'left' : 'right');
  else
    sendAction(dy < 0 ? 'rotate' : 'down');
}

function pointerCancel() {
  clearTimeout(longPressTimer);
  longPressTimer = null;
  pointerStart = null;
  longPressTriggered = false;
}

document.addEventListener('keydown', function(event) {
  const actions = {
    ArrowLeft: 'left',
    ArrowRight: 'right',
    ArrowDown: 'down',
    ArrowUp: 'rotate',
    Space: 'drop',
    KeyP: 'pause',
    Escape: 'back'
  };
  const action = actions[event.code];
  if (action) {
    event.preventDefault();
    sendAction(action);
  }
});

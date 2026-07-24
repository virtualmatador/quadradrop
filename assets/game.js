const audioIds = ['move', 'food', 'turn', 'lock', 'win', 'die'];
const audios = {};
let audioContext;
let gamePaused = false;
let pointerStart = null;
let pointerLast = null;
let swipeRemainder = {x: 0, y: 0};
let swipeMoved = false;
let swipeActions = {x: false, y: false};
let swipeLastActionAt = 0;
let longPressTimer = null;
let longPressTriggered = false;
let renderedPieceGeneration = 0;

const gestureThreshold = 18;
const releaseMoveDelay = 150;
const longPressDelay = 500;

function sendAction(action) {
  CallHandler('game', 'action', action);
}

function setup(showControls) {
  buildCells(document.getElementById('board'), 200);
  buildCells(document.getElementById('next'), 16);
  document.getElementById('controls').hidden = !showControls;

  const AudioContextClass = window.AudioContext || window.webkitAudioContext;
  if (AudioContextClass) audioContext = new AudioContextClass();
  const loads = audioIds.map(function(id) {
    if (!audioContext) return Promise.reject();
    return new Promise(function(resolve, reject) {
      const request = new XMLHttpRequest();
      request.open(
          'GET', cross_asset_domain_ + 'wave/' + id + '.wav',
          cross_asset_async_);
      request.responseType = 'arraybuffer';
      request.onload = function() {
        audioContext.decodeAudioData(
            request.response,
            function(buffer) {
              audios[id] = buffer;
              resolve();
            },
            reject);
      };
      request.onerror = reject;
      request.send();
    });
  });
  Promise.allSettled(loads).then(function() {
    CallHandler('body', 'setup', '');
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
    board, active, next, score, lines, level, pieceGeneration, paused, gameOver,
    cleanupPhase, cleanupRow) {
  if (renderedPieceGeneration !== pieceGeneration) pointerCancel();
  renderedPieceGeneration = pieceGeneration;
  gamePaused = paused;
  const boardElement = document.getElementById('board');
  boardElement.querySelectorAll('.cell-clearing, .cell-moving')
      .forEach(function(cell) {
        cell.classList.remove('cell-clearing', 'cell-moving');
      });
  paint(boardElement, board);
  for (let i = 0; i < active.length; ++i)
    boardElement.children[i].classList.toggle('cell-active', active[i] === '1');
  if (cleanupRow >= 0 && cleanupRow < 20) {
    if (cleanupPhase === 1) {
      for (let column = 0; column < 10; ++column) {
        boardElement.children[cleanupRow * 10 + column].classList.add(
            'cell-clearing');
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
  const pauseButton = document.getElementById('pause');
  const pauseButtonLabel = paused ? 'Resume' : 'Pause';
  pauseButton.hidden = gameOver;
  pauseButton.textContent = paused ? '\u25b6' : '\u23f8';
  pauseButton.setAttribute('aria-label', pauseButtonLabel);
  pauseButton.title = pauseButtonLabel;

  const overlay = document.getElementById('overlay');
  overlay.hidden = !paused && !gameOver;
  document.getElementById('overlay-title').textContent =
      gameOver ? 'Game Over' : 'Paused';
  document.getElementById('overlay-help').textContent = gameOver ?
      'Return to the menu and reset to play again.' :
      'Press resume button to continue.';
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
  pointerLast = {x: event.clientX, y: event.clientY};
  swipeRemainder = {x: 0, y: 0};
  swipeMoved = false;
  swipeActions = {x: false, y: false};
  swipeLastActionAt = event.timeStamp;
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
  if (!pointerStart || longPressTriggered) return;
  const dx = event.clientX - pointerStart.x;
  const dy = event.clientY - pointerStart.y;
  const moveX = event.clientX - pointerLast.x;
  const moveY = event.clientY - pointerLast.y;
  pointerLast = {x: event.clientX, y: event.clientY};
  swipeRemainder.x += moveX;
  swipeRemainder.y = Math.max(0, swipeRemainder.y + moveY);

  if (Math.max(Math.abs(dx), Math.abs(dy)) < gestureThreshold) return;
  swipeMoved = true;
  clearTimeout(longPressTimer);
  longPressTimer = null;

  const bounds = event.currentTarget.getBoundingClientRect();
  const stepX = bounds.width / 10;
  const stepY = bounds.height / 20;
  const horizontalSteps = Math.floor(Math.abs(swipeRemainder.x) / stepX);
  const verticalSteps = Math.floor(swipeRemainder.y / stepY);
  const horizontalAction = swipeRemainder.x < 0 ? 'left' : 'right';
  let horizontalDone = 0;
  let verticalDone = 0;

  // Interleave diagonal movement in roughly the order that grid lines are
  // crossed instead of applying one entire axis before the other.
  while (horizontalDone < horizontalSteps || verticalDone < verticalSteps) {
    const nextHorizontal = horizontalDone < horizontalSteps ?
        (horizontalDone + 1) / horizontalSteps :
        Infinity;
    const nextVertical = verticalDone < verticalSteps ?
        (verticalDone + 1) / verticalSteps :
        Infinity;
    if (nextHorizontal <= nextVertical) {
      sendAction(horizontalAction);
      if (!pointerStart) return;
      swipeLastActionAt = event.timeStamp;
      ++horizontalDone;
      swipeActions.x = true;
    } else {
      sendAction('down');
      if (!pointerStart) return;
      swipeLastActionAt = event.timeStamp;
      ++verticalDone;
      swipeActions.y = true;
    }
  }

  swipeRemainder.x -= Math.sign(swipeRemainder.x) * horizontalSteps * stepX;
  swipeRemainder.y -= verticalSteps * stepY;
}

function pointerUp(event) {
  if (!pointerStart) return;
  pointerMove(event);
  if (!pointerStart) return;
  clearTimeout(longPressTimer);
  longPressTimer = null;
  const dx = event.clientX - pointerStart.x;
  const dy = event.clientY - pointerStart.y;
  pointerStart = null;
  pointerLast = null;
  if (longPressTriggered) return;
  if (!swipeMoved) {
    const bounds = event.currentTarget.getBoundingClientRect();
    sendAction(
        event.clientX < bounds.left + bounds.width / 2 ? 'rotate-left' :
                                                         'rotate-right');
  } else {
    if (event.timeStamp - swipeLastActionAt <= releaseMoveDelay) {
      if (!swipeActions.x && Math.abs(dx) >= gestureThreshold)
        sendAction(dx < 0 ? 'left' : 'right');
      if (!swipeActions.y && dy >= gestureThreshold) sendAction('down');
    }
  }
}

function pointerCancel() {
  clearTimeout(longPressTimer);
  longPressTimer = null;
  pointerStart = null;
  pointerLast = null;
  longPressTriggered = false;
}

document.addEventListener('keydown', function(event) {
  const actions = {
    ArrowLeft: 'left',
    ArrowRight: 'right',
    ArrowDown: 'down',
    ArrowUp: 'rotate',
    Space: 'drop',
    Escape: 'back'
  };
  let action = actions[event.code];
  if ((event.code === 'KeyP' && !gamePaused) ||
      (event.code === 'KeyR' && gamePaused))
    action = 'pause';
  if (action) {
    event.preventDefault();
    sendAction(action);
  }
});

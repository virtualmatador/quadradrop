const audioIds = [
  'move', 'step', 'food', 'turn', 'lock', 'explode', 'level', 'win', 'die'
];
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
let renderedCleanupPhase = null;
let renderedCleanupCount = 0;
let renderedPaused = null;

const gestureThreshold = 18;
const releaseMoveDelay = 150;
const longPressDelay = 500;
const cleanupPlaying = 0;
const cleanupClearing = 2;
const cleanupLevelChange = 3;
const cleanupWin = 4;
const cleanupExplode = 5;
const cleanupExploding = 6;

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

function animateStatistics(ids) {
  const panels = ids.map(function(id) {
    return document.getElementById(id).parentElement;
  });
  panels.forEach(function(panel) {
    panel.classList.remove('statistic-changing');
  });
  if (panels.length) void panels[0].offsetWidth;
  panels.forEach(function(panel) {
    panel.classList.add('statistic-changing');
  });
}

function parseExplosionMoves(state) {
  if (!state) return [];
  return state.split(';').map(function(move) {
    const cells = move.split(',').map(Number);
    if (cells.length !== 2 || !Number.isInteger(cells[0]) ||
        !Number.isInteger(cells[1]) || cells[0] < 0 || cells[0] >= 240 ||
        cells[1] < 0 || cells[1] >= 240)
      return null;
    return cells;
  }).filter(function(move) {
    return move !== null;
  });
}

function animateExplosion(board, moves) {
  const first = board.children[0].getBoundingClientRect();
  const nextColumn = board.children[1].getBoundingClientRect();
  const nextRow = board.children[10].getBoundingClientRect();
  const columnStep = nextColumn.left - first.left;
  const rowStep = nextRow.top - first.top;
  moves.forEach(function(move) {
    const source = move[0];
    const target = move[1];
    const visibleTarget = target - 40;
    if (visibleTarget < 0 || visibleTarget >= 200) return;
    const sourceColumn = source % 10;
    const sourceRow = Math.floor(source / 10);
    const targetColumn = target % 10;
    const targetRow = Math.floor(target / 10);
    const cell = board.children[visibleTarget];
    cell.style.setProperty(
        '--explode-x', (sourceColumn - targetColumn) * columnStep + 'px');
    cell.style.setProperty(
        '--explode-y', (sourceRow - targetRow) * rowStep + 'px');
    cell.classList.add('cell-exploding');
  });
}

function renderGame(
    board, active, next, score, lines, level, quadras, pieceGeneration, paused,
    gameOver, cleanupPhase, cleanupRow, cleanupCount, explosionState) {
  const explosionMoves = parseExplosionMoves(explosionState);
  const hasCleanupState = renderedCleanupPhase !== null;
  const rowResolved = hasCleanupState &&
      (cleanupCount > renderedCleanupCount ||
       (renderedCleanupPhase === cleanupClearing &&
        cleanupPhase !== cleanupClearing));
  const levelChanged = rowResolved && cleanupPhase === cleanupLevelChange;
  const enteredWin = hasCleanupState && cleanupPhase === cleanupWin &&
      renderedCleanupPhase !== cleanupWin;
  const resumedExplosion = cleanupPhase === cleanupExploding &&
      renderedCleanupPhase === cleanupExploding && renderedPaused && !paused;
  const explosionStarted = cleanupPhase === cleanupExploding &&
      explosionMoves.length > 0 &&
      (renderedCleanupPhase !== cleanupExploding || resumedExplosion);
  const explosionAccepted =
      (hasCleanupState && cleanupPhase === cleanupExplode &&
       renderedCleanupPhase !== cleanupExplode) ||
      (cleanupPhase === cleanupExploding && explosionMoves.length > 0 &&
       renderedCleanupPhase !== cleanupExplode &&
       renderedCleanupPhase !== cleanupExploding);
  renderedCleanupPhase = cleanupPhase;
  renderedCleanupCount = cleanupCount;
  renderedPaused = paused;

  if (renderedPieceGeneration !== pieceGeneration) pointerCancel();
  renderedPieceGeneration = pieceGeneration;
  gamePaused = paused;
  const boardElement = document.getElementById('board');
  const animatedCells =
      boardElement.querySelectorAll('.cell-clearing, .cell-moving');
  animatedCells.forEach(function(cell) {
    cell.classList.remove('cell-clearing', 'cell-moving');
  });
  const previousExplosionCells =
      boardElement.querySelectorAll('.cell-exploding');
  if (cleanupPhase !== cleanupExploding || explosionStarted) {
    previousExplosionCells.forEach(function(cell) {
      cell.classList.remove('cell-exploding');
      cell.style.removeProperty('--explode-x');
      cell.style.removeProperty('--explode-y');
    });
  }
  if (animatedCells.length ||
      ((cleanupPhase !== cleanupExploding || explosionStarted) &&
       previousExplosionCells.length))
    void boardElement.offsetWidth;
  paint(boardElement, board);
  for (let i = 0; i < active.length; ++i)
    boardElement.children[i].classList.toggle('cell-active', active[i] === '1');
  if (explosionStarted) animateExplosion(boardElement, explosionMoves);
  if (cleanupPhase === cleanupClearing &&
      cleanupRow >= 0 && cleanupRow < 20) {
    for (let column = 0; column < 10; ++column) {
      boardElement.children[cleanupRow * 10 + column].classList.add(
          'cell-clearing');
    }
    for (let row = 0; row < cleanupRow; ++row) {
      for (let column = 0; column < 10; ++column) {
        const cell = boardElement.children[row * 10 + column];
        if (cell.dataset.piece !== '0') cell.classList.add('cell-moving');
      }
    }
  }
  paint(document.getElementById('next'), next);
  document.getElementById('score').textContent = score;
  document.getElementById('lines').textContent = lines;
  document.getElementById('level').textContent = level;
  document.getElementById('quadras').textContent = quadras;
  const explodeButton = document.getElementById('explode');
  explodeButton.disabled = quadras <= 0 || paused || gameOver ||
      cleanupPhase !== cleanupPlaying;
  explodeButton.title = gameOver ? 'Game over' :
      (paused ? 'Resume to explode' :
       (cleanupPhase !== cleanupPlaying ? 'Wait for cleanup to finish' :
        (quadras <= 0 ? 'No Quadras available' :
                       'Explode using one Quadra (E or swipe up)')));
  const animatedStatistics = [];
  if (levelChanged) animatedStatistics.push('level');
  if (enteredWin || explosionAccepted) animatedStatistics.push('quadras');
  animateStatistics(animatedStatistics);
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
  document.getElementById('overlay-help').hidden = gameOver;
  document.getElementById('overlay-help').textContent =
      'Press resume button to continue.';
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
  const dx = event.clientX - pointerStart.x;
  const dy = event.clientY - pointerStart.y;
  const bounds = event.currentTarget.getBoundingClientRect();
  if (!longPressTriggered && !swipeActions.x && !swipeActions.y &&
      -dy >= gestureThreshold && -dy > bounds.height / 20 &&
      -dy > Math.abs(dx)) {
    pointerCancel();
    sendAction('explode');
    return;
  }
  pointerMove(event);
  if (!pointerStart) return;
  clearTimeout(longPressTimer);
  longPressTimer = null;
  pointerStart = null;
  pointerLast = null;
  if (longPressTriggered) return;
  if (!swipeMoved) {
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
    KeyZ: 'rotate-left',
    KeyX: 'rotate-right',
    KeyE: 'explode',
    KeyD: 'drop',
    Escape: 'back'
  };
  let action = actions[event.code];
  if (action === 'explode' && event.repeat) return;
  if ((event.code === 'KeyP' && !gamePaused) ||
      (event.code === 'KeyR' && gamePaused))
    action = 'pause';
  if (action) {
    event.preventDefault();
    sendAction(action);
  }
});

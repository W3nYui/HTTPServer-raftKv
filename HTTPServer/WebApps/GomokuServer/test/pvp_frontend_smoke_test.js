const assert = require('assert');
const fs = require('fs');
const path = require('path');

const page = fs.readFileSync(path.join(__dirname, '../resource/ChessGameVsPlayer.html'), 'utf8');

assert.match(page, /function applySnapshot\(snapshot\)/);
assert.match(page, /case 'raft_unavailable':/);
assert.match(page, /type: 'state_request'/);
assert.match(page, /case 'state_result':/);
assert.match(page, /applySnapshot\(m\.state\)/);

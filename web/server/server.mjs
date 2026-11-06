import http from 'node:http';
import net from 'node:net';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { randomBytes } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';
import { Server } from 'socket.io';
import { command } from './commands.mjs';

const token = () => randomBytes(24).toString('hex');
const number = (name, fallback) => Number(process.env[name] || fallback);

/** One authority process per room. Only this gateway can reach its loopback TCP port. */
export async function createGateway(options = {}) {
  const engine = options.engine || process.env.CARDGAME_ENGINE || resolve('../build/cardgame_server.exe');
  const clientDir = options.clientDir || process.env.CLIENT_DIR || fileURLToPath(new URL('../dist/', import.meta.url));
  const rooms = new Map(), sessions = new Map();
  let stopping = false;
  const server = http.createServer(async (req, res) => {
    if (req.url?.split('?')[0] === '/healthz') {
      res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
      res.end(JSON.stringify({ ok: !stopping, rooms: rooms.size, version: process.env.RELEASE_ID || 'development' }));
      return;
    }
    const path = req.url?.split('?')[0];
    const files = { '/': ['index.html','text/html; charset=utf-8'],
      '/assets/app.js': ['assets/app.js','text/javascript; charset=utf-8'],
      '/assets/style.css': ['assets/style.css','text/css; charset=utf-8'] };
    if (!files[path]) { res.writeHead(404); res.end('Not found'); return; }
    try {
      const [file, mime] = files[path];
      const body = await readFile(resolve(clientDir, file));
      res.writeHead(200, { 'Content-Type': mime, 'Cache-Control': 'no-cache' }); res.end(body);
    } catch { res.writeHead(503); res.end('Client is not built'); }
  });
  const io = new Server(server, { serveClient: false, maxHttpBufferSize: 16384,
    allowRequest: (req, callback) => {
      // Same-origin browsers only; no account or privileged commands exposed.
      let allowed = !req.headers.origin;
      try { allowed ||= new URL(req.headers.origin).host === req.headers.host; } catch {}
      callback(null, allowed);
    }
  });
  function lobby(room) {
    const data = { room: room.code, started: room.started, players: [...room.players].map(s =>
      ({ name: s.name, seat: s.seat, connected: !!s.socket?.connected })) };
    for (const s of room.players) s.socket?.emit('lobby', data);
  }
  function closeRoom(room, reason = '房间已关闭') {
    if (room.closed) return;
    room.closed = true; rooms.delete(room.code);
    for (const s of room.players) {
      clearTimeout(s.disconnectTimer); sessions.delete(s.token);
      s.socket?.emit('roomClosed', { message: reason });
      if (s.socket) s.socket.data.session = null;
      s.peer?.destroy();
    }
    room.child.kill();
  }
  async function makeRoom() {
    if (rooms.size >= (options.maxRooms || number('MAX_ROOMS', 12))) throw Error('测试房间已满，请稍后再试');
    let code; do { code = randomBytes(3).toString('hex').toUpperCase(); } while (rooms.has(code));
    const args = ['--bind','127.0.0.1','--port','0','--shop-time',String(options.shopTime || number('SHOP_TIME',60)),
      '--turn-time',String(options.turnTime || number('TURN_TIME',30))];
    if (options.seed != null) args.push('--seed', String(options.seed));
    const child = spawn(engine, args, { windowsHide: true, stdio: ['ignore','pipe','pipe'] });
    const room = { code, child, players: new Set(), started: false, closed: false, idleAt: Date.now() };
    rooms.set(code, room); // Reserve capacity before any asynchronous work.
    child.on('exit', () => closeRoom(room, '规则进程已退出，请创建新房间'));
    child.stderr.on('data', data => console.error(`Engine ${code}: ${String(data).slice(0,300)}`));
    try {
      room.port = await new Promise((resolvePort, reject) => {
        const timeout = setTimeout(() => reject(Error('规则服务启动超时')), 15000);
        const lines = createInterface({ input: child.stdout });
        const fail = () => { clearTimeout(timeout); reject(Error('规则服务无法启动')); };
        child.once('error', fail); child.once('exit', fail);
        lines.on('line', line => {
          const m = /^LISTENING 127\.0\.0\.1:(\d+)$/.exec(line);
          if (!m) return;
          clearTimeout(timeout); child.off('error', fail); child.off('exit', fail); resolvePort(Number(m[1]));
        });
      });
    } catch (error) { closeRoom(room); throw error; }
    return room;
  }
  async function connectEngine(s) {
    if (s.peer && !s.peer.destroyed && s.ready) return;
    if (s.connecting) return s.connecting;
    s.connecting = new Promise((ok, fail) => {
      const peer = net.createConnection({ host: '127.0.0.1', port: s.room.port });
      s.peer = peer; s.ready = false;
      let settled = false;
      const timer = setTimeout(() => { peer.destroy(); reject(Error('连接规则服务超时')); }, 10000);
      const reject = error => { if (!settled) { settled = true; clearTimeout(timer); fail(error); } };
      peer.setNoDelay(true);
      peer.once('connect', () => peer.write(s.engineToken ? `RECONNECT ${s.engineToken}\n` : `JOIN ${s.name}\n`));
      const lines = createInterface({ input: peer, crlfDelay: Infinity });
      lines.on('line', line => {
        if (s.peer !== peer || s.room.closed) return;
        let m;
        try { m = JSON.parse(line); } catch { peer.destroy(); return; }
        if (m.type === 'welcome' || m.type === 'reconnected') {
          s.engineToken = m.token || s.engineToken; s.seat = m.seat;
          s.sequence = Math.max(s.sequence, m.lastRequest || 0); s.ready = true;
          clearTimeout(timer); settled = true; ok(); lobby(s.room);
        } else if (m.type === 'state') {
          s.room.started = true; s.state = m;
          s.socket?.emit('state', { ...m, room: s.room.code, serverTime: Date.now() });
        } else if (m.type === 'error') {
          if (!s.ready) reject(Error(m.message));
          s.socket?.emit('notice', { message: m.message, error: true });
          const pending = s.pending.get(m.request); if (pending) { clearTimeout(pending.timer); pending.reply({ ok:false, message:m.message }); s.pending.delete(m.request); }
        } else if (m.type === 'ack' || m.type === 'duplicate') {
          const pending = s.pending.get(m.request); if (pending) { clearTimeout(pending.timer); pending.reply({ ok:true }); s.pending.delete(m.request); }
        }
      });
      peer.on('error', () => reject(Error('连接规则服务失败')));
      peer.on('close', () => {
        if (s.peer !== peer) return;
        s.ready = false; reject(Error('规则连接已关闭'));
        for (const pending of s.pending.values()) { clearTimeout(pending.timer); pending.reply({ ok:false, message:'连接中断，请查看最新状态后操作' }); }
        s.pending.clear();
        if (s.socket?.connected && !s.room.closed) s.socket.emit('notice', { error:true, message:'规则连接中断，请刷新网页重连' });
      });
    }).finally(() => { s.connecting = null; });
    return s.connecting;
  }
  async function attach(socket, s) {
    if (s.socket && s.socket !== socket && s.socket.connected) throw Error('该座位正在另一连接中使用');
    if (!socket.connected) throw Error('连接已中断');
    clearTimeout(s.disconnectTimer); s.socket = socket; socket.data.session = s;
    try {
      await connectEngine(s);
      if (!socket.connected) throw Error('连接已中断');
      if (s.state) socket.emit('state', { ...s.state, room:s.room.code, serverTime:Date.now() });
      if (s.room.started && s.ready) s.peer.write(`${++s.sequence} STATE\n`);
      lobby(s.room);
      return { ok:true, token:s.token, room:s.room.code, seat:s.seat, name:s.name };
    } catch (error) {
      socket.data.session = null; s.socket = null;
      throw error;
    }
  }
  io.on('connection', socket => {
    let joining = false, rateAt = Date.now(), rate = 0;
    const guarded = fn => async (data, reply = () => {}) => {
      if (joining || socket.data.session) return reply({ ok:false, message:'已经加入房间或正在连接' });
      joining = true;
      try { reply(await fn(data)); } catch (error) { reply({ ok:false, message:error.message }); }
      finally { joining = false; }
    };
    socket.on('join', guarded(async data => {
      const name = typeof data?.name === 'string' ? data.name.trim() : '';
      if (!name || name.length > 20 || /[\x00-\x1f\x7f]/.test(name)) throw Error('名字需要 1–20 个字，不能包含换行');
      let room;
      if (data.create === true) room = await makeRoom();
      else {
        const code = typeof data?.room === 'string' ? data.room.toUpperCase().trim() : '';
        if (!/^[A-F0-9]{6}$/.test(code) || !(room = rooms.get(code))) throw Error('房间不存在，请核对六位房间号');
      }
      if (room.closed || room.started || room.players.size >= 4) throw Error('房间已经开局或人数已满');
      const s = { token:token(), name, room, seat:null, socket:null, peer:null, engineToken:null, sequence:0, pending:new Map() };
      room.players.add(s); sessions.set(s.token, s);
      try { return await attach(socket, s); }
      catch (error) { room.players.delete(s); sessions.delete(s.token); s.peer?.destroy(); lobby(room); throw error; }
    }));
    socket.on('resume', guarded(async data => {
      const s = sessions.get(data?.token);
      if (!s || s.room.closed) throw Error('房间会话已过期，请重新加入');
      return attach(socket, s);
    }));
    socket.on('action', (data, reply = () => {}) => {
      if (Date.now() - rateAt > 1000) { rateAt = Date.now(); rate = 0; }
      if (++rate > 25) return reply({ ok:false, message:'操作太快，请稍后再试' });
      const s = socket.data.session;
      if (!s?.ready || !s.room.started || s.socket !== socket) return reply({ ok:false, message:'等待四人入座或重新连接' });
      try {
        const text = command(data), id = ++s.sequence;
        const timer = setTimeout(() => { s.pending.delete(id); reply({ ok:false, message:'操作响应超时，请刷新状态后重试' }); }, 10000);
        s.pending.set(id, { reply, timer });
        s.peer.write(`${id} ${text}\n`);
      } catch (error) { reply({ ok:false, message:error.message }); }
    });
    socket.on('disconnect', () => {
      const s = socket.data.session;
      if (!s || s.socket !== socket) return;
      s.socket = null; lobby(s.room);
      s.disconnectTimer = setTimeout(() => {
        if (s.socket?.connected || s.room.closed) return;
        s.peer?.destroy();
        if (!s.room.started) { s.room.players.delete(s); sessions.delete(s.token); lobby(s.room); }
      }, options.disconnectGrace ?? number('DISCONNECT_GRACE_MS', 5000));
    });
  });
  const cleanup = setInterval(() => {
    for (const r of rooms.values()) {
      if ([...r.players].some(s => s.socket?.connected)) r.idleAt = Date.now();
      else if (Date.now() - r.idleAt > number('ROOM_IDLE_MS', 600000)) closeRoom(r, '房间空闲已超过十分钟');
    }
  }, 10000); cleanup.unref();
  await new Promise((ok, fail) => { server.once('error',fail); server.listen(options.port ?? number('PORT',3000), options.host || process.env.HOST || '127.0.0.1', ok); });
  return { server, rooms, address: server.address(), async close() {
    stopping = true; clearInterval(cleanup);
    for (const room of [...rooms.values()]) closeRoom(room, '服务器正在重启，请稍后创建新房间');
    await new Promise(ok => io.close(ok));
  } };
}


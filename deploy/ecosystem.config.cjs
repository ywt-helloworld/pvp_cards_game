module.exports = {
  apps: [{
    name: 'cardgame-web',
    cwd: '/var/www/cardgame/server',
    script: 'main.mjs',
    instances: 1,
    exec_mode: 'fork',
    autorestart: true,
    max_memory_restart: '180M',
    kill_timeout: 5000,
    env: {
      NODE_ENV: 'production', HOST: '127.0.0.1', PORT: '3000',
      CARDGAME_ENGINE: '/var/www/cardgame/server/bin/cardgame_server',
      CLIENT_DIR: '/var/www/cardgame/client',
      MAX_ROOMS: '12', ROOM_IDLE_MS: '600000'
    }
  }]
};

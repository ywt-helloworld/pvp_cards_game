// An explicit entry point also works when PM2 loads ESM through its container.
import { createGateway } from './server.mjs';
const app = await createGateway();
console.log(`CARDGAME_WEB http://${app.address.address}:${app.address.port}`);
for (const signal of ['SIGINT','SIGTERM']) process.once(signal, async () => {
  await app.close(); process.exit(0);
});

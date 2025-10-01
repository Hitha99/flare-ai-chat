import type { Ai } from "@cloudflare/workers-types";
import { getAssetFromKV } from "@cloudflare/kv-asset-handler";

export interface Env {
  AI: Ai;
  CHAT_ROOM: DurableObjectNamespace;
  MODEL: string;
  MAX_TURNS: string;
  __STATIC_CONTENT: KVNamespace; // injected by Wrangler when [site] is set
}

type ChatMessage = { role: "system" | "user" | "assistant"; content: string };

export default {
  async fetch(req: Request, env: Env, ctx: ExecutionContext) {
    const url = new URL(req.url);

    // Serve static UI (from ./public)
    if (req.method === "GET" && url.pathname !== "/chat") {
      try {
        return await getAssetFromKV(
          { request: req, waitUntil: ctx.waitUntil },
          { ASSET_NAMESPACE: env.__STATIC_CONTENT }
        );
      } catch {
        return new Response("Not found", { status: 404 });
      }
    }

    // API endpoint
    if (url.pathname === "/chat" && req.method === "POST") {
      let payload: any;
      try {
        payload = await req.json();
      } catch {
        return new Response(JSON.stringify({ error: "Invalid JSON" }), {
          status: 400,
        });
      }

      const prompt = typeof payload?.prompt === "string" ? payload.prompt : "";
      const sessionId =
        typeof payload?.sessionId === "string" ? payload.sessionId : undefined;
      const stream = !!payload?.stream; // frontend can request streaming

      if (!prompt) {
        return new Response(JSON.stringify({ error: "Missing prompt" }), {
          status: 400,
        });
      }

      const id = sessionId ?? crypto.randomUUID();
      const stub = env.CHAT_ROOM.get(env.CHAT_ROOM.idFromName(id));

      const resp = await stub.fetch("http://chat/session", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ prompt, stream }),
      });

      const headers = new Headers(resp.headers);
      headers.set("x-session-id", id);
      return new Response(resp.body, { status: resp.status, headers });
    }

    return new Response("Not found", { status: 404 });
  },
};

// ---------------- Durable Object ----------------

export class ChatRoom implements DurableObject {
  state: DurableObjectState;
  env: Env;
  messages: ChatMessage[] = [];

  constructor(state: DurableObjectState, env: Env) {
    this.state = state;
    this.env = env;
    this.state.blockConcurrencyWhile(async () => {
      const stored = await this.state.storage.get<ChatMessage[]>("messages");
      if (stored) this.messages = stored;
    });
  }

  async fetch(req: Request) {
    const url = new URL(req.url);
    if (url.pathname !== "/session" || req.method !== "POST") {
      return new Response("Not found", { status: 404 });
    }

    const { prompt, stream } = (await req.json()) as {
      prompt: string;
      stream?: boolean;
    };

    const model = this.env.MODEL || "@cf/meta/llama-3-8b-instruct";
    const maxTurns = parseInt(this.env.MAX_TURNS || "16", 10);

    // Ensure system message exists
    if (this.messages.length === 0) {
      this.messages.push({
        role: "system",
        content: "You are a concise, helpful assistant.",
      });
    }

    this.messages.push({ role: "user", content: prompt });

    if (stream) {
      // --- Streaming mode ---
      const aiResp = await this.env.AI.run(model, {
        messages: this.messages,
        stream: true,
      });

      const { readable, writable } = new TransformStream();
      this.streamToSSE(aiResp, writable, async (finalText) => {
        this.messages.push({ role: "assistant", content: finalText });
        if (this.messages.length > maxTurns * 2) {
          this.messages = this.messages.slice(-maxTurns * 2);
        }
        await this.state.storage.put("messages", this.messages);
      });

      return new Response(readable, {
        headers: {
          "content-type": "text/event-stream; charset=utf-8",
          "cache-control": "no-cache",
        },
      });
    } else {
      // --- Non-streaming mode ---
      const result = await this.env.AI.run(model, {
        messages: this.messages,
      });

      this.messages.push({ role: "assistant", content: result.response });
      if (this.messages.length > maxTurns * 2) {
        this.messages = this.messages.slice(-maxTurns * 2);
      }
      await this.state.storage.put("messages", this.messages);

      return new Response(JSON.stringify(result), {
        headers: { "content-type": "application/json" },
      });
    }
  }

  async streamToSSE(
    aiResp: AsyncIterable<any>,
    sink: WritableStream,
    onDone: (full: string) => void
  ) {
    const writer = sink.getWriter();
    const encoder = new TextEncoder();
    let full = "";

    await writer.write(encoder.encode(`event: open\ndata: ok\n\n`));

    try {
      for await (const event of aiResp) {
        const chunk =
          typeof event === "string"
            ? event
            : event?.response ?? event?.delta ?? "";

        if (chunk) {
          full += chunk;
          await writer.write(
            encoder.encode(
              `event: token\ndata: ${JSON.stringify({ response: chunk })}\n\n`
            )
          );
        }
      }
    } catch (err) {
      await writer.write(
        encoder.encode(
          `event: error\ndata: ${JSON.stringify({ error: String(err) })}\n\n`
        )
      );
    }

    await writer.write(
      encoder.encode(
        `event: done\ndata: ${JSON.stringify({ response: full })}\n\n`
      )
    );
    await writer.close();
    onDone(full);
  }
}


  
  
  
  
  
  


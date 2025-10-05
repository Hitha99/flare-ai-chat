# ☁️ Cloudflare Workers AI · Chat

An **AI-powered chat application** built with **Cloudflare Workers AI**, **Durable Objects**, and a lightweight **chat frontend**.  
It supports **persistent conversation memory**, optional **streaming responses**, and is ready for future extensions like **voice input**.  

---

## 🚀 Features
- ⚡ **Cloudflare Workers AI** with `@cf/meta/llama-3-8b-instruct`.
- 🗂️ **Durable Objects** for session memory & persistence.
- 💬 **Chat UI** built with HTML, CSS, and vanilla JS.
- 🔄 **Streaming responses** (Server-Sent Events) or full-response mode.
- 🌍 Easy deployment on **Cloudflare Workers & Pages**.
- 🎤 Extendable with **voice input (Web Speech API or Realtime API)**.

---

## 📸 Screenshots

### ✅ Local Development Preview
![Cloudflare Workers AI Screenshot](./.assets/Cloudfare%20Workers%20AI.png)

### 💬 Conversation Example
![Cloudflare Workers AI Chat Screenshot](./.assets/Cloudfare%20Workers%20Ai%20Chat.png)

## 🛠️ Tech Stack
- **Frontend**: HTML + CSS + JavaScript
- **Backend**: Cloudflare Workers + Durable Objects
- **LLM**: LLaMA 3.3 (8B Instruct) via Workers AI
- **State Management**: Durable Objects (`CHAT_ROOM`)
- **Streaming**: Server-Sent Events (SSE)

---

## ⚡ Getting Started

### 1️⃣ Clone the repo
```bash
git clone https://github.com/Hitha99/flare-ai-chat.git
cd flare-ai-chat


🛠️ Setup & Development
1️⃣ Clone the repo
git clone https://github.com/Hitha99/flare-ai-chat.git
cd flare-ai-chat

2️⃣ Install dependencies
npm install

3️⃣ Run locally
npx wrangler dev


Now open in your browser:
👉 http://localhost:8787

🌍 Deployment on Cloudflare
Set up a workers.dev subdomain

Go to your Cloudflare Dashboard

Open Workers & Pages

Enable your free workers.dev subdomain

Deploy with Wrangler
npx wrangler deploy


Your app will be live at:
👉 https://cloudflare-ai-chat.<your-subdomain>.workers.dev

🧠 Memory & State

Each chat session is stored inside a Durable Object (CHAT_ROOM)

The frontend saves the session ID in localStorage → conversations continue seamlessly

Old messages are trimmed when exceeding the configured MAX_TURNS

🔮 Future Enhancements

🎤 Voice input using Web Speech API or Cloudflare’s Realtime API

📱 Fully responsive mobile UI

🔀 Support for multiple LLMs (Mistral, GPT, etc.)

💾 Export / save conversation history

# 🦁 Zooba 2 — Versão PSP (PPSSPP)

Este é o port oficial em C do jogo **Zooba 2** para o PlayStation Portable (PSP) e o emulador PPSSPP, compilado nativamente usando a toolchain open-source **PSPSDK**.

---

## 📁 Estrutura do Repositório

```text
Zooba2_PSP/
├── .github/
│   └── workflows/
│       └── compile.yml    # Build automatizado via GitHub Actions
├── src/
│   └── main.c             # Código-fonte principal em C (Lógica, IA, Arena e Sistema de Combate)
├── Makefile               # Script de compilação da PSPSDK
└── README.md              # Documentação e instruções de uso


# Lab 03 - Comunicação entre Processos (IPC)

## Objetivo

Implementar comunicação entre processos usando três mecanismos distintos do Linux:
pipes anônimos, pipes nomeados (FIFOs) e sinais

Cada exercício é independente. Resolva na ordem que preferir, mas a numeração reflete
uma progressão natural de complexidade.

## Estrutura

```
lab03-ipc/
├── README.md              ← você está aqui
├── Makefile               ← compila todos os exercícios
├── ex1_pipe.c             ← pipes anônimos
├── ex2_fifo.c             ← pipes nomeados (FIFO)
└── ex3_sinais.c           ← sinais entre processos
```

## Compilação

```bash
make          # compila tudo
make ex1      # compila só o exercício 1
make clean    # remove binários e FIFOs/shm residuais
```

## Entrega

Até **24/03** via commit no repositório. Todos os exercícios devem compilar sem erros
com `make`. Warnings são tolerados, mas tente eliminá-los.

---

## Referência rápida das syscalls

Consulte esta seção enquanto resolve os exercícios. Não é exaustiva: use
`man 2 <syscall>` ou `man 7 <conceito>` para detalhes.

### Pipes anônimos

```c
#include <unistd.h>

int pipe(int fd[2]);
// Cria um pipe. fd[0] = leitura, fd[1] = escrita.
// Retorna 0 em sucesso, -1 em erro.
// O pipe existe enquanto algum processo mantiver os descritores abertos.
// Capacidade típica: 64KB (Linux). Escrita bloqueia se o buffer estiver cheio.

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
// Leitura de fd[0] bloqueia se o pipe estiver vazio (e fd[1] ainda estiver aberto).
// Leitura retorna 0 quando todos os escritores fecharam fd[1] (EOF).
```

**Padrão de uso com fork():**
zzz1. Pai chama `pipe(fd)` antes do `fork()`
2. Após o fork, cada processo fecha o descritor que não usa
3. Escritor fecha `fd[0]`, leitor fecha `fd[1]`

### Pipes nomeados (FIFOs)

```c
#include <sys/stat.h>

int mkfifo(const char *pathname, mode_t mode);
// Cria um arquivo especial FIFO no sistema de arquivos.
// mode: permissões (ex: 0666).
// Depois de criado, abre-se com open() normal.
// open() bloqueia até que ambos os lados (leitura e escrita) estejam conectados.
// Remova o FIFO com unlink() ao final.
```

**Diferença fundamental:** pipes anônimos só funcionam entre processos com
ancestral comum (fork). FIFOs funcionam entre processos sem parentesco —
basta conhecer o caminho do arquivo.

### Sinais

```c
#include <signal.h>

int kill(pid_t pid, int sig);
// Envia o sinal `sig` para o processo `pid`.
// Não confundir com "matar" — kill() só entrega sinais.

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
// Registra `handler` como tratador do sinal `signum`.
// handler pode ser: SIG_DFL (padrão), SIG_IGN (ignorar), ou função sua.
// signal() é simples mas tem comportamento não-portável entre sistemas.

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
// Alternativa robusta a signal(). Permite controlar flags e máscara.
// Use sa_handler para handler simples, sa_sigaction para handler com info extra.
// Sempre inicialize a struct com memset(&sa, 0, sizeof(sa)) antes de preencher.
```

**Sinais úteis para este lab:**
| Sinal     | Número | Comportamento padrão | Uso comum                    |
|-----------|--------|----------------------|------------------------------|
| SIGUSR1   | 10     | Termina processo     | Comunicação customizada      |
| SIGUSR2   | 12     | Termina processo     | Comunicação customizada      |
| SIGCHLD   | 17     | Ignorado             | Filho terminou               |
| SIGINT    | 2      | Termina processo     | Ctrl+C                       |
| SIGTERM   | 15     | Termina processo     | Solicitação de encerramento  |

**Cuidado:** sinais são assíncronos. O handler pode executar a qualquer momento.
Use apenas funções async-signal-safe dentro de handlers (write() sim, printf() não).

---

## Exercícios

### Ex 1 — Pipe anônimo: conversor de caixa

**Cenário:** o processo pai envia uma string para o filho via pipe. O filho
converte para maiúsculas e imprime o resultado.

**O que você precisa fazer:** completar os TODOs em `ex1_pipe.c`.

**Saída esperada:**
```
[pai]  enviando: sistemas operacionais
[filho] recebido: SISTEMAS OPERACIONAIS
```

---

### Ex 2 — FIFO: chat entre processos independentes

**Cenário:** dois processos sem parentesco se comunicam via FIFO.
O programa aceita dois modos: `enviar` e `receber`. Rode em dois terminais.

**O que você precisa fazer:** completar os TODOs em `ex2_fifo.c`.

**Terminal 1:**
```bash
./ex2 receber
```
**Terminal 2:**
```bash
./ex2 enviar "mensagem qualquer"
```

**Saída no terminal 1:**
```
[receber] aguardando mensagem...
[receber] recebido: mensagem qualquer
```

---

### Ex 3 — Sinais: ping-pong entre pai e filho

**Cenário:** pai e filho trocam sinais alternadamente. O pai envia SIGUSR1
para o filho, o filho responde com SIGUSR2 para o pai. Repetem N vezes.

**O que você precisa fazer:** completar os TODOs em `ex3_sinais.c`.

**Saída esperada (N=3):**
```
[pai]  SIGUSR1 -> filho (rodada 1)
[filho] SIGUSR1 recebido, respondendo SIGUSR2 (rodada 1)
[pai]  SIGUSR2 recebido (rodada 1)
[pai]  SIGUSR1 -> filho (rodada 2)
[filho] SIGUSR1 recebido, respondendo SIGUSR2 (rodada 2)
[pai]  SIGUSR2 recebido (rodada 2)
[pai]  SIGUSR1 -> filho (rodada 3)
[filho] SIGUSR1 recebido, respondendo SIGUSR2 (rodada 3)
[pai]  SIGUSR2 recebido (rodada 3)
```

---

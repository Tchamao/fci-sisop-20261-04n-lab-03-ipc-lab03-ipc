# Conceitos: IPC no Linux

Este documento acompanha o Lab 3. Para cada mecanismo você vai encontrar um modelo mental, justificativa das syscalls, padrão correto de uso e erros comuns.

---

## 1. Pipe anônimo (`pipe()`)

### Modelo

```
                ┌─────────────────────────┐
                │         KERNEL          │
                │                         │
   fd[1] ──────►│  buffer circular (64KB) │──────► fd[0]
   (escrita)    │                         │        (leitura)
                └─────────────────────────┘

Após fork():
   PAI:   fd[0] fd[1]     (herda os dois)
   FILHO: fd[0] fd[1]     (herda os dois)

   PAI fecha fd[0] e usa fd[1] para escrever.
   FILHO fecha fd[1] e usa fd[0] para ler.
```

O kernel mantém contadores de quantos fds abertos apontam para cada extremidade. `read()` retorna EOF apenas quando o contador da extremidade de escrita chega a zero. Se o pai não fechar `fd[1]` antes do `wait()`, o filho nunca recebe EOF — `read()` bloqueia para sempre.

### Por que cada syscall existe

| Syscall | Por que |
|---------|---------|
| `pipe(fd)` | Cria o buffer no kernel e devolve dois fds — fd[0] para leitura, fd[1] para escrita. Sem isso não há canal. |
| `close(fd[X])` | Remove uma referência à extremidade. Quando todas as referências de escrita somem, leitores recebem EOF. |
| `write(fd[1], buf, n)` | Coloca dados no buffer. Bloqueia se o buffer estiver cheio. |
| `read(fd[0], buf, n)` | Retira dados do buffer. Bloqueia se o buffer estiver vazio. Retorna 0 (EOF) quando não há mais escritores. |

### Padrão

```c
int fd[2];
pipe(fd);
pid_t pid = fork();

if (pid > 0) {          /* PAI: escritor */
    close(fd[0]);       /* fecha leitura — pai não lê */
    write(fd[1], mensagem, strlen(mensagem));
    close(fd[1]);       /* sinaliza EOF pro filho */
    wait(NULL);
} else {                /* FILHO: leitor */
    close(fd[1]);       /* fecha escrita — filho não escreve */
    ssize_t n = read(fd[0], buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    /* processa buffer */
    close(fd[0]);
}
```

### Erros comuns

| Sintoma | Causa | Por que acontece |
|---------|-------|-----------------|
| `read()` bloqueia indefinidamente | Pai não fechou `fd[1]` antes do `wait()` | O kernel ainda conta o fd[1] do pai como escritor ativo — filho nunca vê EOF |
| Lixo no final da string | `buffer[n] = '\0'` ausente | `read()` não insere terminador — memória não inicializada após os dados |
| `write()` retorna `SIGPIPE` | Filho fechou `fd[0]` antes de ler tudo | Escrever em pipe sem leitores gera `SIGPIPE` por padrão |

---

## 2. FIFO — pipe nomeado (`mkfifo()`)

### Modelo

```
Processo A (receber)                     Processo B (enviar)
---------------------                    -------------------
mkfifo("/tmp/lab03_fifo")
open(O_RDONLY)   [BLOQUEIA]
                                          open(O_WRONLY)  ------+
<------------------- desbloqueia aqui --------------------------+
                    kernel sincroniza os dois open()

Ambos agora tem fds conectados ao mesmo buffer FIFO no kernel.
O arquivo em /tmp e apenas o ponto de encontro; nao armazena dados.
```

O kernel mantém contadores de leitores e escritores. `open(O_RDONLY)` bloqueia até que pelo menos um escritor abra o mesmo FIFO, e vice-versa. Isso é o handshake automático do FIFO.

### Por que cada syscall existe

| Syscall | Por que |
|---------|---------|
| `mkfifo(path, mode)` | Cria o ponto de encontro no filesystem. Sem ele, os dois processos não têm como se referenciar. |
| `open(O_RDONLY)` / `open(O_WRONLY)` | Diferente de pipes, FIFO não exige parentesco — qualquer processo pode abrir pelo caminho. O bloqueio no `open()` é o mecanismo de sincronização. |
| `unlink(path)` | Remove o arquivo do filesystem. O FIFO é persistente — sobrevive ao processo que o criou. Sem `unlink()`, fica lixo em `/tmp`. |

### Padrão

```c
/* lado receptor */
if (mkfifo(FIFO_PATH, 0666) < 0 && errno != EEXIST) {
    perror("mkfifo"); exit(1);
}
int fd = open(FIFO_PATH, O_RDONLY);   /* bloqueia aqui */
ssize_t n = read(fd, buffer, BUF_SIZE - 1);
buffer[n] = '\0';
close(fd);
unlink(FIFO_PATH);                    /* limpa o recurso */

/* lado emissor */
int fd = open(FIFO_PATH, O_WRONLY);   /* bloqueia aqui */
write(fd, msg, strlen(msg));
close(fd);
```

### Erros comuns

| Sintoma | Causa | Por que acontece |
|---------|-------|-----------------|
| Dois terminais travados simultaneamente | Nenhum dos lados abriu o lado oposto | O bloqueio no `open()` é simétrico — ambos esperam o outro |
| `mkfifo()` retorna erro e programa aborta | `errno == EEXIST` tratado como erro fatal | Se o FIFO já existe de uma execução anterior, não é problema — deve continuar |
| Dados ficam em `/tmp` após o programa | `unlink()` ausente | FIFO é recurso persistente; precisa ser removido explicitamente |

---

## 3. Sinais

### O que é um sinal

Um sinal é uma notificação assíncrona entregue pelo kernel a um processo. **Assíncrona** significa que o processo não fica verificando ativamente se chegou alguma coisa — o kernel interrompe o processo no momento que o sinal chega, como uma interrupção de hardware, mas em nível de software.

O sinal não carrega dados. É só um número inteiro. O que importa é *qual* número chegou, não *o que veio junto*.

### Os sinais são predefinidos — você não inventa novos

Todo sinal tem um número fixo definido pelo kernel e um nome no formato `SIG*`. Você não pode criar sinais novos. O conjunto é fixo e documentado em `<signal.h>`. Os mais relevantes:

| Sinal | Quem envia | Comportamento padrão se não houver handler |
|-------|------------|---------------------------------------------|
| `SIGINT` | Terminal (Ctrl+C) | Encerra o processo |
| `SIGKILL` | Qualquer processo | Encerra imediatamente — **não pode ser capturado nem ignorado** |
| `SIGTERM` | Comando `kill` do shell, outros processos | Encerra o processo |
| `SIGUSR1` | Seu código | Encerra o processo (mas a intenção é: você define o que faz) |
| `SIGUSR2` | Seu código | Encerra o processo (idem) |

`SIGUSR1` e `SIGUSR2` existem exatamente para comunicação entre seus próprios processos — sem significado predefinido pelo sistema. Você registra o handler e decide o que acontece.

### O que o processo pode fazer quando um sinal chega

Para cada sinal, o processo escolhe uma de três respostas:

1. **Comportamento padrão** (`SIG_DFL`) — o que o kernel faz se você não tocar em nada. Para a maioria dos sinais, é encerrar o processo.
2. **Ignorar** (`SIG_IGN`) — o sinal chega e não acontece nada.
3. **Handler próprio** — uma função C que você escreve e registra. O kernel a chama automaticamente quando o sinal chega.

`SIGKILL` e `SIGSTOP` não permitem opção 2 nem 3. São os únicos que o kernel garante que sempre funcionam, independente do que o processo faça.

### Modelo de entrega

```
  PAI                           KERNEL                        FILHO
  ───                           ──────                        ─────
  kill(pid_filho, SIGUSR1) ──► anota SIGUSR1 como
                                pendente no filho
                                                ──► interrompe filho
                                                    (em qualquer instrução)
                                                    salva estado do filho
                                                    executa handler_usr1()
                                                      sinal_recebido = 1
                                                    restaura estado do filho
                                                    filho retoma de onde parou
  (aguarda em pause())         ◄── entrega completa
```

O filho pode estar em qualquer ponto — no meio de um cálculo, bloqueado em `read()`, dormindo em `pause()`. O kernel para o que ele estava fazendo, executa o handler, e devolve o controle exatamente de onde parou.

### O que é "registrar" um handler

Registrar é dizer ao kernel: "quando `SIGUSR1` chegar neste processo, em vez do comportamento padrão, execute *esta* função."

Antes do registro: o processo tem o comportamento padrão (`SIG_DFL`) — que para `SIGUSR1` seria encerrar o processo.
Após o registro: o kernel chama seu handler.

Existem duas formas de registrar:

**`signal()` — simples, mas com ressalvas:**
```c
signal(SIGUSR1, meu_handler);
```
Funciona para casos simples. Historicamente, em alguns sistemas o handler era resetado para `SIG_DFL` após cada entrega — obrigando a re-registrar dentro do handler. Em Linux moderno isso não acontece, mas `signal()` carrega essa ambiguidade histórica.

**`sigaction()` — verboso, mas correto e portável:**
```c
struct sigaction sa;
memset(&sa, 0, sizeof(sa));  /* zera a struct — campos não usados devem ser 0 */
sa.sa_handler = meu_handler; /* aponta para sua função */
sigaction(SIGUSR1, &sa, NULL);
```
O `memset` antes de preencher é necessário — a struct tem outros campos que controlam comportamentos avançados (máscaras de sinais, flags). Lixo na memória causaria comportamento imprevisível. O terceiro argumento (`NULL`) seria onde o kernel guardaria o handler anterior — não precisamos aqui.

**Boa prática: use `sigaction()`.** O `signal()` existe por compatibilidade histórica.

### O handler: assinatura e restrições

Um handler é uma função C com uma assinatura específica:

```c
void meu_handler(int sig) {
    /* sig contém o número do sinal que chegou */
}
```

O parâmetro `sig` existe porque você pode registrar o mesmo handler para múltiplos sinais e distinguir qual chegou. Se não for usar, escreva `(void)sig;` para silenciar o warning do compilador — omitir o cast não quebra nada, mas o compilador reclama.

**A restrição crítica:** o handler pode interromper o processo em *qualquer* instrução — inclusive no meio de `printf()` ou `malloc()`, que usam locks internos para garantir consistência. Se o handler chamar a mesma função que foi interrompida, e essa função está segurando um lock, o handler tenta adquirir o mesmo lock — que já está tomado pelo próprio processo, que está pausado. **Deadlock. O programa trava para sempre.**

A lista de funções seguras dentro de handlers (chamadas *async-signal-safe*) é pequena: `write()`, `_exit()`, operações simples com inteiros. `printf()`, `malloc()`, `free()` e a maioria da biblioteca padrão **não estão** nessa lista.

**Regra: o handler só seta uma flag e retorna. Todo o processamento fica no loop principal.**

### A flag: por que `volatile sig_atomic_t`

A flag precisa de dois qualificadores especiais, por razões diferentes:

**`volatile`** — sem ele, o compilador otimiza o loop e o quebra:

```c
/* o compilador analisa este loop */
while (sinal_recebido == 0)
    pause();

/* e conclui: "nada dentro do loop modifica sinal_recebido,
   então o valor nunca muda — vou guardar num registrador
   e não perder tempo relendo da memória a cada iteração" */

/* resultado: o loop lê sempre o mesmo valor do registrador,
   nunca vê a atualização do handler, roda para sempre */
```

`volatile` instrui o compilador: "essa variável pode mudar por razões externas ao fluxo normal do código. Sempre releia da memória."

**`sig_atomic_t`** — em algumas arquiteturas, escrever um `int` pode exigir mais de uma instrução de máquina (escreve os primeiros 2 bytes, depois os últimos 2 bytes). Se o handler interrompe no meio de uma dessas escritas, o loop principal lê um valor corrompido — metade antigo, metade novo. `sig_atomic_t` é o tipo que o sistema garante ser lido e escrito em uma única instrução, sem risco de leitura parcial.

```c
static volatile sig_atomic_t sinal_recebido = 0;
/*     ^^^^^^^  ^^^^^^^^^^^^
/*     |        garante atomicidade da leitura/escrita no hardware
/*     garante que o compilador sempre relê da memória               */
```

### `pause()` — esperar sem consumir CPU

Um processo pode esperar um sinal de duas formas:

```c
/* errado: busy-wait — ocupa 100% de um núcleo de CPU sem fazer nada útil */
while (!sinal_recebido) { }

/* certo: pause() suspende o processo no kernel até qualquer sinal chegar */
while (!sinal_recebido)
    pause();
```

`pause()` coloca o processo em estado de espera — ele sai da fila do scheduler e não consome CPU. Quando qualquer sinal chega, o kernel entrega o sinal (executa o handler) e `pause()` retorna. O loop então verifica a flag — se o sinal certo chegou, sai; se foi outro sinal, chama `pause()` de novo.

O loop ao redor do `pause()` é necessário exatamente por isso: podem chegar sinais que você não estava esperando.

### Padrão completo

```c
static volatile sig_atomic_t sinal_recebido = 0;

void handler_usr1(int sig) {
    (void)sig;           /* silencia warning — não vamos usar o número */
z    sinal_recebido = 1;  /* só isso — nada mais é seguro aqui */
}

/* registro: conecta SIGUSR1 a handler_usr1 */
struct sigaction sa;
memset(&sa, 0, sizeof(sa));
sa.sa_handler = handler_usr1;
sigaction(SIGUSR1, &sa, NULL);

/* espera: dorme até sinal_recebido virar 1 */
while (!sinal_recebido)
    pause();
sinal_recebido = 0;   /* reseta para a próxima espera */
```

### `kill()` — o nome enganoso

`kill()` não mata nada necessariamente. É a syscall para enviar um sinal a qualquer processo:

```c
kill(pid_filho, SIGUSR1);    /* envia SIGUSR1 para o filho */
kill(getppid(), SIGUSR2);    /* envia SIGUSR2 para o pai */
kill(getpid(), SIGUSR1);     /* envia para si mesmo */
```

O nome vem dos tempos em que sinais eram usados quase exclusivamente para encerrar processos. Hoje o uso é mais amplo.

### Race condition do `pause()`

Existe uma janela de vulnerabilidade pequena mas real:

```
loop principal:             handler:
  testa: flag == 0 → TRUE
                        ← sinal chega aqui
                            flag = 1
                            handler retorna
  pause()             → dorme para sempre
                        (sinal já foi consumido, não vai chegar de novo)
```

O sinal chegou entre o teste e o `pause()`. O processo dorme para sempre. A solução correta é `sigsuspend()`, que atomicamente testa e suspende sem essa janela. Neste lab, com seres humanos na ponta e latência de milissegundos, a janela é desprezível na prática — mas vale saber que existe.

### Conexão com o Projeto 1

O padrão do lab é exatamente o mesmo do proj1:

| Conceito | Lab ex3 | Projeto 1 |
|----------|---------|-----------|
| Handler mínimo | `sinal_recebido = 1` | `keep_running = 0` |
| Flag volátil | `volatile sig_atomic_t` | `volatile sig_atomic_t` |
| Envio | `kill(pid_filho, SIGUSR1)` | `kill(workers[i], SIGTERM)` |
| Registro | `sigaction()` | `signal()` |
| Espera | `pause()` | loop de CPU |

A diferença principal: o worker do proj1 não usa `pause()` porque está em loop ativo testando senhas — não precisa dormir. Quando `SIGTERM` chega, `keep_running` vira 0 e o loop termina limpo na próxima iteração, sem interrupção abrupta no meio de uma operação.

### Erros comuns

| Sintoma | Causa | Por que acontece |
|---------|-------|-----------------|
| Funciona às vezes, trava às vezes | Race condition do `pause()` | Sinal chegou entre o teste da flag e o `pause()` |
| Processo em loop infinito | `sinal_recebido` não é resetado para 0 após consumir o sinal | Próxima iteração do loop vê a flag ainda em 1, nunca chama `pause()` — fica girando |
| Filho nunca recebe o primeiro sinal | Handler não registrado antes do `kill()` do pai | O `usleep(100000)` no pai existe para dar tempo ao filho registrar o handler antes do primeiro envio |
| Warning "unused parameter 'sig'" | Parâmetro do handler não utilizado | Adicionar `(void)sig;` no início do handler |

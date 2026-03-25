#define _DEFAULT_SOURCE  /* usleep */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/*
 * Ex 3 — Sinais: ping-pong entre pai e filho
 *
 * Pai e filho trocam sinais alternadamente:
 *   pai  --SIGUSR1-->  filho
 *   filho --SIGUSR2--> pai
 *   (repete N vezes)
 *
 * Desafio principal: sinais são assíncronos. Você precisa de um
 * mecanismo para o processo esperar até receber o sinal antes de
 * continuar. Use pause() — ele bloqueia até que um sinal seja entregue.
 *
 * IMPORTANTE: dentro do handler, use write() para imprimir, não printf().
 * printf() não é async-signal-safe. Neste exercício, como é didático,
 * vamos usar uma variável global volátil como flag e imprimir no loop
 * principal (fora do handler).
 */

#define NUM_RODADAS 3

/* Flags globais — volatile para que o compilador não otimize */
static volatile sig_atomic_t sinal_recebido = 0;

// TODO 1: Implemente o handler para SIGUSR1 (usado pelo filho).
//         Ele deve apenas setar sinal_recebido = 1.
void handler_usr1(int sig) {
    (void)sig;
    sinal_recebido = 1;
}

// TODO 2: Implemente o handler para SIGUSR2 (usado pelo pai).
//         Ele deve apenas setar sinal_recebido = 1.
void handler_usr2(int sig) {
    (void)sig;
    sinal_recebido = 1;
}

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid > 0) {
        /* --- PROCESSO PAI --- */

        // TODO 3: Registre o handler de SIGUSR2 com sigaction().
        //         Inicialize a struct com memset, preencha sa_handler,
        //         e chame sigaction().
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handler_usr2;
        sigaction(SIGUSR2, &sa, NULL);
        
        /* Pequena espera para o filho configurar seu handler */
        usleep(100000);

        for (int i = 1; i <= NUM_RODADAS; i++) {
            printf("[pai]  SIGUSR1 -> filho (rodada %d)\n", i);

            // TODO 4: Envie SIGUSR1 para o filho com kill().
            kill(pid, SIGUSR1);
            
            // TODO 5: Espere receber SIGUSR2 do filho.
            //         Use um loop: enquanto sinal_recebido == 0, chame pause().
            //         Depois resete sinal_recebido para 0.
            while (!sinal_recebido)
                pause();
            sinal_recebido = 0;

            printf("[pai]  SIGUSR2 recebido (rodada %d)\n", i);
        }

        wait(NULL);
    } else {
        /* --- PROCESSO FILHO --- */

        // TODO 6: Registre o handler de SIGUSR1 com sigaction().
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handler_usr1;
        sigaction(SIGUSR1, &sa, NULL);

        for (int i = 1; i <= NUM_RODADAS; i++) {
            // TODO 7: Espere receber SIGUSR1 do pai.
            //         Mesmo padrão: loop com pause() + flag.
            while (!sinal_recebido)
                pause();
            sinal_recebido = 0;

            printf("[filho] SIGUSR1 recebido, respondendo SIGUSR2 (rodada %d)\n", i);

            // TODO 8: Envie SIGUSR2 para o pai com kill().
            //         Use getppid() para obter o PID do pai.
            kill(getppid(), SIGUSR2);
        }
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>

/*
 * Ex 1 — Pipe anônimo: conversor de caixa
 *
 * O pai envia uma string para o filho via pipe.
 * O filho converte para maiúsculas e imprime.
 *
 * Fluxo:
 *   pai --[escrita]--> pipe --[leitura]--> filho
 *
 * Lembre-se: após o fork, feche os descritores que cada
 * processo não vai usar. Isso garante que o read() do
 * filho receba EOF quando o pai terminar de escrever.
 */

int main(void) {
    char mensagem[] = "sistemas operacionais";
    int fd[2];

    // TODO 1: Crie o pipe.
    //         Em caso de erro, imprima com perror() e retorne 1.
    if (pipe(fd) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid > 0) {
        /* --- PROCESSO PAI (escritor) --- */

        // TODO 2: Feche o descritor que o pai não usa.
        close(fd[0]);
        
        printf("[pai]  enviando: %s\n", mensagem);

        // TODO 3: Escreva a mensagem no pipe.
        write(fd[1], mensagem, strlen(mensagem));

        // TODO 4: Feche o descritor de escrita (sinaliza EOF pro filho).
        close(fd[1]);

        wait(NULL);
    } else {
        /* --- PROCESSO FILHO (leitor) --- */

        // TODO 5: Feche o descritor que o filho não usa.
        close(fd[1]);

        char buffer[256];

        // TODO 6: Leia do pipe para o buffer.
        //         read() retorna o número de bytes lidos.
        //         Use esse valor para colocar o '\0' no final do buffer.
        ssize_t n = read(fd[0], buffer, sizeof(buffer) - 1); // substitua por read()

        buffer[n] = '\0';

        // TODO 7: Converta cada caractere do buffer para maiúscula.
        //         Use toupper() de <ctype.h>.
        for (int i = 0; i < n; i++) {
            buffer[i] = toupper(buffer[i]);
        }

        printf("[filho] recebido: %s\n", buffer);

        // TODO 8: Feche o descritor de leitura.
        close(fd[0]);
    }

    return 0;
}

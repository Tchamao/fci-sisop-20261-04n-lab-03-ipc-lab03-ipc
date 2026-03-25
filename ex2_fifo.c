#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/*
 * Ex 2 — FIFO: chat entre processos independentes
 *
 * Uso:
 *   ./ex2 receber          (terminal 1 — bloqueia esperando mensagem)
 *   ./ex2 enviar "texto"   (terminal 2 — envia e encerra)
 *
 * O FIFO é criado em /tmp/lab03_fifo. Ambos os lados usam o
 * mesmo caminho para se encontrarem.
 *
 * Diferença chave em relação ao Ex 1: aqui os processos NÃO
 * têm parentesco (não são pai/filho). O FIFO no filesystem
 * é o ponto de encontro.
 */

#define FIFO_PATH "/tmp/lab03_fifo"
#define BUF_SIZE  256

void modo_receber(void) {
    // TODO 1: Crie o FIFO com mkfifo().
    //         Se já existir (errno == EEXIST), tudo bem — continue.
    //         Para qualquer outro erro, imprima com perror() e saia.
    if (mkfifo(FIFO_PATH, 0666) < 0 && errno != EEXIST) {
            perror("mkfifo");
            exit(1);
    }
    printf("[receber] aguardando mensagem...\n");

    // TODO 2: Abra o FIFO para leitura com open().
    //         Isso vai bloquear até que alguém abra o outro lado para escrita.
    int fd = open(FIFO_PATH, O_RDONLY); // substitua

    char buffer[BUF_SIZE];

    // TODO 3: Leia a mensagem do FIFO.
    ssize_t n = read(fd, buffer, BUF_SIZE - 1); // substitua
    buffer[n] = '\0';

    printf("[receber] recebido: %s\n", buffer);

    // TODO 4: Feche o descritor e remova o FIFO com unlink().
    close(fd);
    unlink(FIFO_PATH);
}

void modo_enviar(const char *msg) {
    // TODO 5: Abra o FIFO para escrita com open().
    //         Isso vai bloquear até que o receptor abra o outro lado.
    int fd = open(FIFO_PATH, O_WRONLY); // substitua

    // TODO 6: Escreva a mensagem no FIFO.
    write(fd, msg, strlen(msg));

    printf("[enviar] enviado: %s\n", msg);

    // TODO 7: Feche o descritor.
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s receber | enviar \"mensagem\"\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "receber") == 0) {
        modo_receber();
    } else if (strcmp(argv[1], "enviar") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Uso: %s enviar \"mensagem\"\n", argv[0]);
            return 1;
        }
        modo_enviar(argv[2]);
    } else {
        fprintf(stderr, "Modo desconhecido: %s\n", argv[1]);
        return 1;
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>

#define MAX_ENTRIES 128

typedef struct {
    char name[256];
    int is_dir;
    off_t size;
} entry_t;

int main() {
    // Configura o terminal para modo "raw" (sem buffer de linha e sem ecoar teclas)
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char path[1024] = "/";
    int selected = 0;

    while(1) {
        DIR *d = opendir(path);
        entry_t entries[MAX_ENTRIES];
        int count = 0;

        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL && count < MAX_ENTRIES) {
                if (strcmp(dir->d_name, ".") == 0) continue;
                if (strcmp(path, "/") == 0 && strcmp(dir->d_name, "..") == 0) continue;

                strcpy(entries[count].name, dir->d_name);

                char full[2048];
                if (strcmp(path, "/") == 0) snprintf(full, sizeof(full), "/%s", dir->d_name);
                else snprintf(full, sizeof(full), "%s/%s", path, dir->d_name);

                struct stat st;
                if (stat(full, &st) == 0) {
                    entries[count].is_dir = S_ISDIR(st.st_mode);
                    entries[count].size = st.st_size;
                } else {
                    entries[count].is_dir = 0;
                    entries[count].size = 0;
                }
                count++;
            }
            closedir(d);
        }

        // Ordenação rápida: Diretórios primeiro, depois em ordem alfabética
        for (int i = 0; i < count - 1; i++) {
            for (int j = i + 1; j < count; j++) {
                int swap = 0;
                if (entries[i].is_dir != entries[j].is_dir) {
                    swap = entries[j].is_dir;
                } else {
                    swap = strcmp(entries[i].name, entries[j].name) > 0;
                }
                if (swap) {
                    entry_t tmp = entries[i];
                    entries[i] = entries[j];
                    entries[j] = tmp;
                }
            }
        }

        if (selected >= count) selected = count > 0 ? count - 1 : 0;

        // Limpa a tela e desenha o TUI
        printf("\033[2J\033[H");
        printf("\033[44;37;1m --- Robu OS TUI File Manager --- \033[0m\n");
        printf("\033[36m Path:\033[0m %s\n", path);
        printf("----------------------------------------\n");

        if (count == 0) {
            printf("  (Diretório Vazio)\n");
        } else {
            for (int i = 0; i < count; i++) {
                if (i == selected) printf("\033[47;30m> ");
                else printf("  ");

                if (entries[i].is_dir) printf("\033[34m[DIR ]\033[0m %-20s", entries[i].name);
                else printf("\033[32m[FILE]\033[0m %-20s %8ld B", entries[i].name, (long)entries[i].size);

                if (i == selected) printf("\033[0m\n");
                else printf("\n");
            }
        }

        printf("----------------------------------------\n");
        printf("[w/s] Navegar   [ENTER] Abrir   [q] Sair\n");

        // Captura Entrada
        int c = getchar();
        if (c == 'q') break;
        if (c == 'w' || c == 65) { // 'w' ou Seta pra Cima
            if (selected > 0) selected--;
        }
        if (c == 's' || c == 66) { // 's' ou Seta pra Baixo
            if (selected < count - 1) selected++;
        }
        if (c == 27) { // ANSI Escape ('[A' ou '[B')
            getchar(); // Pula o '['
            int arr = getchar();
            if (arr == 'A' && selected > 0) selected--;
            if (arr == 'B' && selected < count - 1) selected++;
        }
        
        if (c == '\n' || c == '\r') {
            if (count > 0) {
                if (entries[selected].is_dir) {
                    if (strcmp(entries[selected].name, "..") == 0) {
                        char *last = strrchr(path, '/');
                        if (last == path) *(last+1) = '\0';
                        else if (last != NULL) *last = '\0';
                    } else {
                        if (strcmp(path, "/") != 0) strcat(path, "/");
                        strcat(path, entries[selected].name);
                    }
                    selected = 0;
                } else {
                    // É um arquivo, vamos tentar visualizar uma prévia via cat embutido
                    printf("\033[2J\033[H\033[33m--- Lendo: %s ---\033[0m\n\n", entries[selected].name);
                    char full[2048];
                    if (strcmp(path, "/") == 0) snprintf(full, sizeof(full), "/%s", entries[selected].name);
                    else snprintf(full, sizeof(full), "%s/%s", path, entries[selected].name);
                    
                    FILE *f = fopen(full, "r");
                    if(f) {
                        char buf[128];
                        int lines = 0;
                        while(fgets(buf, sizeof(buf), f) && lines < 20) {
                            printf("%s", buf);
                            if (strchr(buf, '\n')) lines++;
                        }
                        fclose(f);
                    } else {
                        printf("Não foi possível ler o arquivo.\n");
                    }
                    printf("\n\n\033[47;30m [ Pressione qualquer tecla para voltar ] \033[0m\n");
                    getchar();
                }
            }
        }
    }

    // Restaura o terminal original
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\033[2J\033[H"); // Limpa tela ao sair
    return 0;
}

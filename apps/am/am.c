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
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char path[1024] = "/";
    int selected = 0;
    int dirty = 1; // Só redesenha quando houver alteração!

    entry_t entries[MAX_ENTRIES];
    int count = 0;

    while(1) {
        // RENDERIZAÇÃO INTELIGENTE: Só lê o VFS e desenha se algo mudou
        if (dirty) {
            count = 0;
            DIR *d = opendir(path);
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

            // Ordenação: Diretórios primeiro
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

            // Desenha a Interface
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
            fflush(stdout);
            dirty = 0; // Trava o redesenho até a próxima tecla!
        }

        int c = getchar();
        
        // CESSÃO DE CPU: Se nenhuma tecla foi pressionada, dorme 20ms e libera o microkernel!
        if (c == EOF || c <= 0) {
            usleep(20000); 
            continue;
        }

        if (c == 'q') break;

        if (c == 'w' || c == 'W') {
            if (selected > 0) { selected--; dirty = 1; }
        }
        else if (c == 's' || c == 'S') {
            if (selected < count - 1) { selected++; dirty = 1; }
        }
        else if (c == 27) { // Setas ANSI (\033[A e \033[B)
            usleep(1000);
            int c2 = getchar();
            if (c2 == '[') {
                int c3 = getchar();
                if (c3 == 'A' && selected > 0) { selected--; dirty = 1; }
                else if (c3 == 'B' && selected < count - 1) { selected++; dirty = 1; }
            }
        }
        else if (c == '\n' || c == '\r') {
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
                    dirty = 1;
                } else {
                    // Prévia de Arquivo
                    printf("\033[2J\033[H\033[33m--- Lendo: %s ---\033[0m\n\n", entries[selected].name);
                    char full[2048];
                    if (strcmp(path, "/") == 0) snprintf(full, sizeof(full), "/%s", entries[selected].name);
                    else snprintf(full, sizeof(full), "%s/%s", path, entries[selected].name);
                    
                    FILE *f = fopen(full, "r");
                    if (f) {
                        char buf[128];
                        int lines = 0;
                        while (fgets(buf, sizeof(buf), f) && lines < 20) {
                            printf("%s", buf);
                            if (strchr(buf, '\n')) lines++;
                        }
                        fclose(f);
                    } else {
                        printf("Não foi possível ler o arquivo.\n");
                    }
                    printf("\n\n\033[47;30m [ Pressione qualquer tecla para voltar ] \033[0m\n");
                    fflush(stdout);

                    while (1) {
                        int k = getchar();
                        if (k != EOF && k > 0) break;
                        usleep(20000);
                    }
                    dirty = 1;
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}

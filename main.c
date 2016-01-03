#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int execute(char input[1024]) {
    printf("geia sou sofia");
    exit(0); // Μόνο για τώρα

}

int main()
{
    char input[1024]; // Παραδοχή: Ένα command του shell δεν ξεπερνά τους 1024 χαρακτήρες

    while (1) {
        errno = 0;
        // Εκτύπωση προτροπής στην κονσόλα του shell
        char cwd[1024];
        getcwd(cwd, sizeof(cwd)); // Εύρεση του τρέχοντα φακέλου που βρίσκεται το shell
        if (errno) {
            perror("An error occured");
            return 1;
        }

        printf("SquaredShell:%s:$ ", cwd);
        fgets(input,1024,stdin);

        if(input[strlen(input)-1] == '\n') // Η fgets διαβάζει και το Enter, γι'αυτό χρειάζεται αντικατάσταση
        {
            input[strlen(input)-1] = '\0';
        }
        execute(input);
    }

    return 0;
}

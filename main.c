#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>

int g_commandMax = 1024; // Παραδοχή: Ένα command του shell δεν ξεπερνά τους 1024 χαρακτήρες
int g_commandMaxWords = 128;
int g_currentDirectoryMax = 1024; // Παραδοχή: Ένα current directory path δεν ξεπερνά τους 1024 χαρακτήρες

/* Επιστρέφει το Home directory του χρήστη */
char* getHomeDir() {
    uid_t uid = getuid();
    struct passwd* pwd = getpwuid(uid);
    if (!pwd) {
        fprintf(stderr,"User with %u ID is unknown.\n", uid);
        exit(EXIT_FAILURE);
    }
    return pwd->pw_dir;
}

/* Αλλαγή του current directory */
int changeDir(char* to) {
    errno = 0;
    chdir(to);
    if(errno) {
        perror("An error occured");
        return EXIT_FAILURE;
    }
    return 0;
}

/* Αποδέσμευση μνήμης που δεσμεύεται από char** */
void Deallocate(char **array,int length)
{
    int i = 0;
    for(i = 0; i<length; i++) {
        free(array[i]);
    }
    free(array);
}


/* Εκτέλεση εξωτερικού προγράμματος */
int Launch(char** args) {
    pid_t pid = fork();

    if(pid == 0) { // Διεργασία παιδί - Εκτελεί το εξωτερικό πρόγραμμα
        execvp(args[0],args);
    } else if(pid > 0) { // Διεργασία γονέας
        int status;
        do {
            waitpid(pid, &status, WUNTRACED); // Περίμενε τη διεργασία παιδί, για να μη μένουν zombies διεργασίες
        } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // Η διεργασία παιδί είτε τερματίστηκε (κανονικά ή με error) είτε "σκοτώθηκε" από signal
    } else { // pid < 0 -> Error
        fprintf(stderr,"Fork() Error");
        return EXIT_FAILURE;
    }
    return 0;
}

/* Εκτέλεση Εντολής */
int ExecuteInput(char** args) {
    if(args == NULL) {
        return EXIT_FAILURE; // Δόθηκε κενή εντολή
    }
    if(strcmp(args[0],"cd") == 0) { // Αλλαγή directory
        if(args[1] == NULL) { // Δεν υπάρχει path για την αλλαγή του directory
        //Επιστροφή στο Home Directory του χρήστη
            char* homeDir = getHomeDir();
            if(changeDir(homeDir) == EXIT_FAILURE) {
                return EXIT_FAILURE;
            }
        } else {
            changeDir(args[1]);
        }
    } else if(strcmp(args[0],"exit") == 0) {
        exit(EXIT_SUCCESS);
    } else {
        return Launch(args);
    }
    return 0;
}

/* Ανάγνωση εντολής */
char* ReadInput() {
    char* input;
    errno = 0;
    input = malloc(g_commandMax*sizeof(char));

    if(errno) {
        perror("An error occured");
        exit(EXIT_FAILURE);
    }

    fgets(input,g_commandMax,stdin);

    if(input[strlen(input)-1] == '\n') // Η fgets διαβάζει και το Enter, γι'αυτό χρειάζεται αντικατάσταση
    {
        input[strlen(input)-1] = '\0'; // Τέλος String variable
    }

    return input;
}

/* Χωρισμός ιnput του χρήστη με βάση το κενό */
char** TokenizeInput(char* input,int* length) {
    const char delimiters[] = " ";
    char *token;
    char **tokens;
    errno = 0;
    tokens = malloc(g_commandMaxWords*sizeof(char*));
    int i = 0;

    if(errno) {
        perror("An error occured");
        exit(EXIT_FAILURE);
    }

    token = strtok(input, delimiters); // Pointer στο τελευταίο token που βρέθηκε

    while( token != NULL ) // Όσο υπάρχουν και άλλα tokens
    {
        tokens[i] = token;
        i+=1;
        token = strtok(NULL, delimiters);
    }
    tokens[i] = NULL; //TRIAL
    *length = i;
    return tokens;
}

/* Ανακατεύθυνση εισόδου/εξόδου από/σε αρχείο */ // ΔΕ ΛΕΙΤΟΥΡΓΕΙ ΑΚΟΜΗ
int Redirect(char* redirectSymbol,char* filename) {
    int out,in;

    if(strcmp(redirectSymbol,">") == 0) {
        errno = 0;
        out = open(filename, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
        if(errno) {
            perror("An error occured");
            return EXIT_FAILURE;
        }
        dup2(out,1);
        close(out);
    } else if(strcmp(redirectSymbol,">>") == 0) {
        errno = 0;
        out = open(filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
        if(errno) {
            perror("An error occured");
            return EXIT_FAILURE;
        }
        dup2(out,1);
        close(out);
    } else { // RedirectSymbol == "<"
        errno = 0;
        in = open(filename, O_RDONLY, S_IRUSR);
        if(errno) {
            perror("An error occured");
            return EXIT_FAILURE;
        }
        dup2(in,0);
        close(in);
    }
    return 0;
}

char** ParseInput(char* input) {
    int numOfTokens;
    int numOfArgs = 0;
    char** args;

    char** tokens = TokenizeInput(input,&numOfTokens);

    // Πέρασμα για έλεγχο ύπαρξης redirection και την απαρίθμηση των args
    int i;
    for(i=0; i<numOfTokens; i++) {
        if(strcmp(tokens[i],">") == 0 || strcmp(tokens[i],">>") == 0 || strcmp(tokens[i],"<") == 0) {
            printf("Here 0");
            Redirect(tokens[i],tokens[i+1]); // To tokekens[i+1] περιέχει το αρχείο που χρησιμοποιείται στο redirection
            i++; // Το όνομα του αρχείου που θα γίνει το redirection δεν πρέπει να περιλαμβάνεται στα args
        } else if(!(i == numOfTokens-1 && strcmp(tokens[i],"&") == 0)) { // Το & δεν πρέπει να συμπεριληφθεί στα args, άρα ούτε και να καταμετρηθεί
            numOfArgs++;
        }
    }
    if(numOfArgs == 0) { // Για εντολή της μορφής "> file.txt"
        return NULL;
    }
    args = malloc((numOfArgs+1)*sizeof(char*));
    int j = 0;
    for(i=0; i<numOfTokens; i++) {
        if(strcmp(tokens[i],">") == 0 || strcmp(tokens[i],">>") == 0 || strcmp(tokens[i],"<") == 0) {
            i++; // Το όνομα του αρχείου που θα γίνει το redirection δεν πρέπει να περιλαμβάνεται στα args
        } else if(!(i == numOfTokens-1 && strcmp(tokens[i],"&") == 0)) {
            args[j] = (char*)malloc(sizeof(char) * (strlen(tokens[i]) + 1));
            strcpy(args[j],tokens[i]);
            //args[j] = tokens[i];
            j++;
        }
    }
    args[j] = NULL;
    return args;
}

int main()
{
    char* input;
    char** args;
    //int length; //TRIAL

    // Αλλαγή του current directory στο home του user
    char* homeDir = getHomeDir();
    changeDir(homeDir);

    while (1) {
        errno = 0;
        // Εκτύπωση προτροπής στην κονσόλα του shell
        char cwd[g_currentDirectoryMax];
        getcwd(cwd, sizeof(cwd)); // Εύρεση του τρέχοντα φακέλου που βρίσκεται το shell
        if (errno) {
            perror("An error occured");
            exit(EXIT_FAILURE);
        }

        printf("SquaredShell:%s:$ ", cwd);
        input = ReadInput(); // Ανάγνωση εντολής χρήστη
        //args = TokenizeInput(input,&length); //TRIAL
        args = ParseInput(input);
        if(ExecuteInput(args) == EXIT_FAILURE) {
                return EXIT_FAILURE;
        }
    }

    return 0;
}

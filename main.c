#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <stdbool.h>

int g_commandMax = 1024; // Παραδοχή: Ένα command του shell δεν ξεπερνά τους 1024 χαρακτήρες
int g_commandMaxWords = 128;
int g_currentDirectoryMax = 1024; // Παραδοχή: Ένα current directory path δεν ξεπερνά τους 1024 χαρακτήρες
bool g_redirect = 0;
int g_out;
int g_in;

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
int ExecuteInput(char** args, int length) {
    if(args == NULL) {
        return EXIT_FAILURE; // Δόθηκε κενή εντολή
    }
    // Έλεγχος για το αν υπάρχει διασωλήνωση -  Αν υπάρχει που χωρίζεται η πρώτη από τη δεύτερη εντολή
    int i = 0;
    char** leftArgs = (char**)malloc(sizeof(char*) * length); // Περιλαμβάνει τα args πριν τη διασωλήνωση
    int left_i = 0; // H θέση που βρισκόμαστε στον πίνακα leftArgs
    char** rightArgs = (char**)malloc(sizeof(char*) * length); // Περιλαμβάνει τα args μετά τη διασωλήνωση
    int right_i = 0; // H θέση που βρισκόμαστε στον πίνακα rightArgs
    bool left = 1; // Ελέγχει αν είμαστε πριν ή μετά τη διασωλήνωση εφόσον αυτή υπάρχει
    while(args[i] != NULL) {
        if(strcmp(args[i],"|") != 0 && left == 1) { // Η αριστερή εντολή της διασωλήνωσης
            leftArgs[left_i] = (char*)malloc(sizeof(char) * (strlen(args[i]) + 1));
            strcpy(leftArgs[left_i],args[i]);
            left_i++;
        } else if(strcmp(args[i],"|") != 0 && left == 0) { // Η δεξιά εντολή της διασωλήνωσης
            rightArgs[right_i] = (char*)malloc(sizeof(char) * (strlen(args[i]) + 1));
            strcpy(rightArgs[right_i],args[i]);
            right_i++;
        } else if(strcmp(args[i],"|") == 0) { // Ελέγχει αν υπάρχει διασωλήνωση
            left = 0;
        }
        rightArgs[right_i] = NULL;
        leftArgs[left_i] = NULL;
        i++;
    }

    // Εκτέλεση εντολής
    if(rightArgs[0] != NULL) { // Αν υπάρχει διασωλήνωση
        errno = 0;
        int pipefd[2];
        pipe(pipefd);
        if(errno) {
            perror("An error occured");
            exit(EXIT_FAILURE);
        }

        errno = 0;
        pid_t pid; // Δημιουργώ διεργασία παιδί
        if(errno) {
            perror("An error occured");
            exit(EXIT_FAILURE);
        }

        // Το παιδί εκτελεί το αριστερό μέρος της διασωλήνωσης
        if(fork() == 0) {
            dup2(pipefd[0],0); // Ανάγνωση από είσοδο pipe όχι από stdin
            close(pipefd[1]);
            errno =0;
            execvp(rightArgs[0],rightArgs);
            if(errno) {
                perror("An error occured");
                exit(EXIT_FAILURE);
            }
            waitpid(pid,NULL,0);
        } else if((pid=fork()) == 0) { // Διεργασία γονέας
            dup2(pipefd[1],1); // Εγγραφή στην έξοδο του pipe όχι στο stdout
            close(pipefd[0]);
            errno =0;
            execvp(leftArgs[0],leftArgs);
            if(errno) {
                perror("An error occured");
                exit(EXIT_FAILURE);
            }
            waitpid(pid,NULL,0);
        } else {
            waitpid(pid,NULL,0);
        }
    } else { // Δεν υπάρχει διασωλήνωση
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
    }
    Deallocate(leftArgs,left_i);
    Deallocate(rightArgs,right_i);
    return 0;
}

/* Ανάγνωση εντολής */
char* ReadInput() {
    char* input;
    errno = 0;
    input = (char*)malloc(g_commandMax*sizeof(char));

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

/* Χωρισμός input του χρήστη με βάση το κενό */
char** TokenizeInput(char* input,int* length) {
    const char delimiters[] = " ";
    char *token;
    char **tokens;
    errno = 0;
    tokens = (char**)malloc(g_commandMaxWords*sizeof(char*));
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

/* Ανακατεύθυνση εισόδου/εξόδου από/σε αρχείο */
int Redirect(char* redirectSymbol,char* filename) {
    int out,in;
    g_out = dup(1);
    g_in = dup(0);
    /*
    // Όλα τα αρχεία αποθηκεύονται στο directory του shell, όχι στο current directory
    char path[150];
    strcpy(path,getHomeDir());
    strcat(path,"/");
    strcat(path,filename);
    */

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
    g_redirect = 1;
    return 0;
}

/* Ανακατεύθυνση εισόδου/εξόδου πίσω στο stdin/stdout αντίστοιχα */
void directBack() {
    dup2(g_out,1);
    dup2(g_in,0);
    g_redirect = 0;
}

/* Με δεδομένη μία λιστα από tokens επιλέγει τα arguments για την κλήση των εσωτερικών/εξωτερικών προγραμμάτων */
char** ParseInput(char* input, int* length) {
    int numOfTokens;
    int numOfArgs = 0;
    char** args;

    char** tokens = TokenizeInput(input,&numOfTokens);

    // Πέρασμα για έλεγχο ύπαρξης redirection και την απαρίθμηση των args
    int i;
    for(i=0; i<numOfTokens; i++) {
        if(strcmp(tokens[i],">") == 0 || strcmp(tokens[i],">>") == 0 || strcmp(tokens[i],"<") == 0) {
            Redirect(tokens[i],tokens[i+1]); // To tokekens[i+1] περιέχει το αρχείο που χρησιμοποιείται στο redirection
            i++; // Το όνομα του αρχείου που θα γίνει το redirection δεν πρέπει να περιλαμβάνεται στα args
        } else if(!(i == numOfTokens-1 && strcmp(tokens[i],"&") == 0)) { // Το & δεν πρέπει να συμπεριληφθεί στα args, άρα ούτε και να καταμετρηθεί
            numOfArgs++;
        }
    }
    if(numOfArgs == 0) { // Για εντολή της μορφής "> file.txt"
        return NULL;
    }
    args = (char**)malloc((numOfArgs+1)*sizeof(char*));
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
    *length = j;
    args[j] = NULL;
    return args;
}

int main()
{
    char* input;
    char** args;
    int length;

    // Αλλαγή του current directory στο home του user
    char* homeDir = getHomeDir();
    changeDir(homeDir);

    while (1) {
        // Εκτύπωση προτροπής στην κονσόλα του shell
        errno = 0;
        char cwd[g_currentDirectoryMax];
        getcwd(cwd, sizeof(cwd)); // Εύρεση του τρέχοντα φακέλου που βρίσκεται το shell
        if (errno) {
            perror("An error occured");
            exit(EXIT_FAILURE);
        }
        printf("SquaredShell:%s:$ ", cwd);

        // Κύριο πρόγραμμα
        input = ReadInput(); // 1. Ανάγνωση εντολής χρήστη
        args = ParseInput(input, &length); // 2. Επεξεργασία εντολής χρήστη
        if(ExecuteInput(args,length) == EXIT_FAILURE) { // 3. Εκτέλεση εντολής χρήστη
                return EXIT_FAILURE;
        }
        // Αν έχει γίνει ανακατεύθυνση σε αρχείο πρέπει να γίνει πάλι ανακατεύθυνση του output στην κονσόλα
        if(g_redirect) {
            directBack();
        }
    }

    return 0;
}

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
#include <sys/time.h>

int g_commandMax = 1024; // Παραδοχή: Ένα command του shell δεν ξεπερνά τους 1024 χαρακτήρες
int g_commandMaxWords = 128;
int g_currentDirectoryMax = 1024; // Παραδοχή: Ένα current directory path δεν ξεπερνά τους 1024 χαρακτήρες
bool g_redirect = 0;
int g_out;
int g_in;

int running_pid; //i diergasia pou trexei sto ipovathro.
//Arxika arxikopoieitai sto miden, deixnontas oti den iparxei deirgasia sto upovathro

struct process{  //o komvos tis listas
    int id;
    struct process *next
}*head=NULL;

/* prosthetei tin diergasia sto telos tis listas */
struct process* addProcessInList(int process_id, struct process *head){


    //create new node
    struct process *newNode = (struct process*)malloc(sizeof(struct process));

    if(newNode == NULL){
        fprintf(stderr, "Unable to allocate memory for new node\n");
        exit(-1);
    }

    newNode->id = process_id;
    newNode->next = NULL;

    if(head == NULL){ //an i lista einai adeia
        head = newNode;
        return head;
    }

    else //prosthese tin diergasia sto telos tis listas
    {
        struct process *current = head;
        while (current->next!=NULL) {
                current=current->next;

        };
        current->next=newNode;
        return head;
    }

}


/*diagrafei tin prwti diergasia tis listas*/
struct process* deleteFirstProcess(struct process *head){

    struct process *temp;
    temp=head->next;
    free(head);
    return temp;

}


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

int Launch(char** args,int background) {  //dexetai ta orismata kai mia metabliti pou deixnei an i diergasia prepei na ekteleste sto upovathro
    pid_t pid = fork();

    if(pid == 0) { // Διεργασία παιδί - Εκτελεί το εξωτερικό πρόγραμμα
        execvp(args[0],args);

    } else if(pid > 0) { // Διεργασία γονέας
          if(background == 0){ //an den einai sto background
                int status;
                do {
                waitpid(pid, &status, WUNTRACED); // Περίμενε τη διεργασία παιδί, για να μη μένουν zombies διεργασίες
                } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // Η διεργασία παιδί είτε τερματίστηκε (κανονικά ή με error) είτε "σκοτώθηκε" από signal
           }
           else{ //an ekteleitai sto background
                kill(pid,SIGSTOP);
                head = addProcessInList(pid,head); //h diergasia mpainei stin lista

           }

    } else { // pid < 0 -> Error
        fprintf(stderr,"Fork() Error");
        return EXIT_FAILURE;
    }
    return 0;
}



/* Εκτέλεση Εντολής */
int ExecuteInput(char** args, int length,int background) {
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

             return Launch(args,background);
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
char** ParseInput(char* input, int* length,int* background) {
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
        }
        else if(strcmp(tokens[i],"&") == 0){ //an i diergasia prepei na ektelestei sto ipovathro, allazei i timi tis background
                *background = 1;
        }
        else if(!(i == numOfTokens-1 && strcmp(tokens[i],"&") == 0)) { // Το & δεν πρέπει να συμπεριληφθεί στα args, άρα ούτε και να καταμετρηθεί

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

 void Round_Robbin(int signal){

    int status;
    if(head == NULL){
        //printf("den uparxei deirgasia sto ipovathro\n");
    }
    else{       // an iparxei estw kai mia dieradia sti lista
        if(running_pid == 0){     //an den ekteleitai kaimia diergasia sto ipovathro
            running_pid=head->id;   //i deiergasia poy tha eketelestei einai i prwti tis listas
            kill(running_pid,SIGCONT);
        }
        else{   //an iparxei idi diergasia apo ti lista i opoia ekteleitai
            pid_t check = waitpid(running_pid,&status,WNOHANG); //elegxoume tin katastasi tis ekteloumenhs diergasias
            if(check == 0){     //an i diergasia den exei termatisei, alla exei teleiwsei o xronos poy tis dinei o RR
                kill(running_pid,SIGSTOP);     //tin stamatame
                head = addProcessInList(running_pid,head);     //mapinei sto telos tis listas
                head = deleteFirstProcess(head);    //diagrafetai apo tin arxi tis lsitas (afou exei metaferthei sto telos)
                running_pid = head->id; //dialegoume tin epomenh gia na sunexisei tin ektelesi tis
                kill(running_pid,SIGCONT);
            }
            else if(check == -1){
                printf("Egine lathos sthn ektelesi tis diergasias");
            }
            else{
                printf("i diergasia me pid: %d teleiwse",running_pid);
                head = deleteFirstProcess(head); //otan teleiwsei, diafrafetai apo tin lista
                running_pid = 0; //kai h diergasia poy ekteleitai arxikopoieitai sto miden gia na ksekinisie na ekteleitai i epomeni diergasia
            }


        }
    }



}







int main()
{
    char* input;
    char** args;
    int length;

    // Αλλαγή του current directory στο home του user
    char* homeDir = getHomeDir();
    changeDir(homeDir);

    int background = 0; //flag metavliti poy deixnei an i entoli prepei an ektelestei sto uipovathro

    struct timeval value={1,0};
	struct timeval interval={1,0};
	struct itimerval timer={interval,value};
	setitimer(ITIMER_REAL, &timer, 0);

	signal(SIGALRM,&Round_Robbin); //kalei tin sunartisis Round_Robbin kathe 1 sec

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

                args = ParseInput(input,&length,&background);
            //    printf(" to background einai: %d",background);
             //   if(background==1)
            //        printf(" stin main einai backgrond \n");
            //    else
             //       printf("stin main den einai backgrond \n");
                    //prosoxi allagi kai stin executeInput(allagi sto orisma to background)



        //args = ParseInput(input, &length); // 2. Επεξεργασία εντολής χρήστη
        if(ExecuteInput(args,length,background) == EXIT_FAILURE) { // 3. Εκτέλεση εντολής χρήστη
                return EXIT_FAILURE;
        }
        // Αν έχει γίνει ανακατεύθυνση σε αρχείο πρέπει να γίνει πάλι ανακατεύθυνση του output στην κονσόλα
        if(g_redirect) {

            directBack();
        }
    }

    return 0;
}

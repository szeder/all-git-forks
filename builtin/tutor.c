#include <stdio.h>

#include <string.h>
#include <stdlib.h> // exit ()

//#define First "\
//#/bin/bash \n \
//echo \"HEEEEELLLOOO BASH SCRIPT\" \n\
//"

int main() {

        char begin_end[4];
        char yes1[] = "Yes";
        char yes2[] = "yes";
        
        printf("Welcome to Git Tutor!!\nGit Tutor is a tutorial that seeks to teach by example \nand supply the information to complete tasks that Git tutor assigns.");
        printf("\nDo you want to continue? Yes or No: ");
        scanf("%s", begin_end);
        if ((strcmp(yes1, begin_end) == 0)  || (strcmp(begin_end, yes2) == 0)) {
                printf("You are beginning the tutorial now!\n");
        }
        else {
                exit(-1);
        }
		system("First.sh>>clear"); //add checker 
		printf("Following three repositories are created:\n1. master \n2. origin \n3. upstream");
		printf("\nRemote is created for origin and upstream...");
        return 0;
}

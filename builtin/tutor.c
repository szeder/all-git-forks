#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit ()
#include <unistd.h>

int cmd_tutor(int argc, const char **argv, const char *prefix)
{
        char begin_end[4];
        char yes1[] = "Yes";
        char yes2[] = "yes";
        int loopi=0;
		FILE *dummyfile;
		
		for (; loopi<50; loopi++){
			printf("\n");
		}
		
        printf("Welcome to Git Tutor.\n\n\n\nGit Tutor is a tutorial that seeks to teach Git through a step-by step approach.");
        printf("\nDo you want to continue? Yes or No: ");
        scanf("%s", begin_end);
        if ((strcmp(yes1, begin_end) == 0)  || (strcmp(begin_end, yes2) == 0)) {
                printf("You are beginning the tutorial now!\n");
        }
        else {
				printf("Exiting tutorial.\n");
                exit(-1);
        }
        //      system("First.sh>>clear"); //add checker
		mkdir("./upstream/");
		mkdir("./master/");
		//mkdir("./origin/");
		//system("mkdir ./upstream/");
		//system("mkdir ./origin/");
		system("git init -q --bare ./origin.git/");
		system("git init -q ./upstream/");
		system("git clone -q ./origin.git ./master");
		//system("touch ./upstream/test.txt");
		dummyfile = fopen("./upstream/test.txt","w");
		fclose(dummyfile);
		//system("cd ./upstream");
		chdir("./upstream");

		//there must a check for account's default identity for git being already set.
		system("git add .");
		system("git commit -qm \"Initializing contents of repository\"");
		chdir("..");

		//system("git clone --bare -q ./origin/ origin.git");
		//system("rm -rf ./origin/");
		//rmdir("./origin");
		chdir("./master");
		//system("git remote rm origin");
		//system("cd ..");
		printf("Following three repositories are created:\n1. master \n2. origin \n3. upstream");
		printf("\nRemote is created for origin and upstream...");
        return 0;
}

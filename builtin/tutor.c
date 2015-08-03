#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit ()
#include <unistd.h>

 
void string_compare(char a[], char b[], char c[])
{
		int count = 1;
		char yes_no[4];
		char yes1[] = "Yes";
        char yes2[] = "yes";
		if ((strcmp(a, b)) == 0)
		{
			printf("Correct! Your file(s) are %s\n", c);
		}
		else
		{
			while(strcmp(a, b) != 0)
			{
				printf("Wrong! try again: ");
				gets(b);
				count++;
				if ((count%5)==0)
				{
					printf("Do you want to get the answer? (yes or no) If no, you will continue: ");
					gets(yes_no);
					if ((strcmp(yes1, yes_no) == 0)  || (strcmp(yes_no, yes2) == 0)) 
					{
						printf("%s\n", a);
						printf("Lets try it now: ");
						gets(b);
						count++;
					}
				}
				
			}
			printf("Correct! Your file(s) are %s\n", c);
			//fetch the file
		}
}
void status (char a[], char b[])
{
		int count = 1;
		char yes_no[4];
		char yes1[] = "Yes";
        char yes2[] = "yes";
		if ((strcmp(a, b)) == 0)
		{
			printf("Correct!!\n");
			system(a);
		}
		else
		{
			while(strcmp(a, b) != 0)
			{
				printf("Wrong! try again: ");
				gets(b);
				count++;
				if ((count%5)==0)
				{
					printf("Do you want to get the answer? (yes or no) If no, you will continue: ");
					gets(yes_no);
					if ((strcmp(yes1, yes_no) == 0)  || (strcmp(yes_no, yes2) == 0)) 
					{
						printf("%s\n", a);
						printf("Lets try it now: ");
						gets(b);
						count++;
					}
				}
			}
			printf("Correct!!\n");
			system(a);
		}
}

int cmd_tutor(int argc, const char **argv, const char *prefix)
{
        char begin_end[3];
		char read_input[100];
		char yes_no[4];
        char yes1[] = "Yes";
        char yes2[] = "yes";

        int loopi=0;
		FILE *dummyfile;
		
		for (; loopi<50; loopi++){
			printf("\n");
		}
		
        printf("Welcome to Git Tutor.\n\n\n\nGit Tutor is a tutorial that seeks to teach Git through a step-by step approach.");

		char fetch_str[] = "fetched";
		char merge_str[] = "merged";
		char push_str[] = "pushed";
		char add_str[] = "added";
		char commit_str[] = "commited";
		char fetch_tut[] = "git fetch upstream";
		char merge_tut[] = "git merge upstream/master";
		char push1_tut[] = "git push origin master";
		char add_tut[] = "git add .";
		char commit_tut[] = "git commit -m \"";
		char push2_tut[] = "git push origin/master";
		char diff_tut[] = "git diff";
		char status_tut[] = "git status";
		
		int flag;
		int T_F = 0;
        printf("Welcome to Git Tutor!!\nGit Tutor is a tutorial that seeks to teach by example \nand supply the information to complete tasks that Git tutor assigns.");

        printf("\nDo you want to continue? Yes or No: ");
		scanf("%s", begin_end);
		printf("Input: %s \n", begin_end);
        if ((strcmp(yes1, begin_end) == 0)  || (strcmp(begin_end, yes2) == 0)) {
                printf("You are beginning the tutorial now!\n");
        }
        else {
				printf("Exiting tutorial.\n");
                exit(-1);
        }

		//create directories
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
		
		//
		// add origin and upstream 
		system("git remote add origin ./origin.git");
		system("git remote add upstream ./upstream");
		printf("\nRemote is created for origin and upstream...");
		
		//fetch
		printf("Fetch Command: Download objects and refs from another repository.");
		printf("\nHint: git fetch [Repository Name] \nType the command: ");
		gets(read_input);
		string_compare(fetch_tut, read_input, fetch_str);
		system("git fetch upstream");
		read_input[0]='\0';
		
		//merge
		printf("\nMerge Command: Join two or more development histories together");
		printf("\nHint: git merge [Repository Name/LocalBranch]...\nType the command: ");
		gets(read_input);
		string_compare(merge_tut, read_input, merge_str);
		system("git merge upstream/master");
		read_input[0]='\0';
		
		//push
		printf("\nPush Command: Update remote refs along with associated objects");
		printf("\nHint: git push [Repository Name] [Local Branch]...\nType the command: ");
		gets(read_input);
		string_compare(push1_tut, read_input, push_str);
		system("git push origin master");
		read_input[0]='\0';
		
		//add
		printf("\nAdd Command: Add file contents to the index");
		printf("\nHint: git add [option]...\nType the command: ");
		gets(read_input);
		string_compare(add_tut, read_input, add_str);
		system("git add .");
		read_input[0]='\0';
		
		//commit
		printf("\nRecord changes to the repository");
		printf("\nHint: git commit [option] [Branch]...\nType the command: ");
		int c = 1;
		int len = sizeof(commit_tut);
		LOOP:
			if ((c%5)==0)
			{
				printf("Do you want to get the answer? (yes or no) If no, you will continue: ");
				gets(yes_no);
				if((strcmp(yes1, yes_no) == 0)  || (strcmp(yes_no, yes2) == 0))
				{
					printf("git commit -m \"type any message\"");
					printf("\nLets try it now: ");
				}
				else
				{
					printf("Type the command to commit: ");
				}
			}
			gets(read_input);
			c++;
			int count = 0;
			while (count < len-1)
			{
				if (commit_tut[count] == read_input[count])
				{
					T_F = 0;
					//printf("True %c:%c\n", commit_tut[count], read_input[count]);
				}
				else 
				{
					T_F = 1;
					//printf("False %c:%c\n", commit_tut[count], read_input[count]);
				}
				count++;
			}
			if (T_F == 0)
			{
				printf("Correct! Your file(s) are %s\n", commit_str);
				//system(read_input);
			}
			else
			{
				printf("Wrong! try again: ");
				goto LOOP;
			}
		
		
		//push
		printf("Push Command: Update remote refs along with associated objects");
		printf("\nHint: git push [Repository Name] [Local Branch]...\nType the command: ");
		gets(read_input);
		string_compare(push1_tut, read_input, push_str);
		system("git push origin master");
		read_input[0]='\0';
		
		printf("\nDiff Command: Show changes between commits, commit and working tree, etc");
		printf("\nHint: git diff...\nType the command: ");
		gets(read_input);
		status(diff_tut, read_input);
		read_input[0]='\0';
		
		
		
        return 0;
}

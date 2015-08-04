#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit ()
#include <unistd.h>

/* Future HELP stuff

static const char * const builtin_tutor_usage[] = {
	N_("git tutor [<options>] [--]"),
	NULL
}

static struct option builtin_tutor_options[] = {
	OPT_BOOL('g', "gui", &show_all, N_("Git Tutor user interface")),
	OPT_END(),
};
*/
	
void string_compare(char a[], char b[], char c[]){
		
		int count = 1;
		char yes_no[4];
		char yes1[] = "Yes";
        char yes2[] = "yes";
		
		printf("input : %s \n", b);
		printf("what should be : |%s| \n", a);
		if ((strcmp(a, b)) == 0)
		{
			printf("Correct! Your file(s) are %s\n", c);
		}
		else
		{
			while(strcmp(a, b) != 0)
			{
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
				printf("\ninput : |%s| \n", b);
				printf("Wrong! try again: ");
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

		//remove repos
		system("rm -rf ./master");
		system("rm -rf ./origin.git");
		system("rm -rf ./upstream");


		/* Check for GUI arguments and grab it, then activate it */

		char *Gui1 = "--gui";
		char *Gui2 = "-g";
		
		if(argc==2 && ( strcmp(argv[1],Gui1) ==0 || strcmp(argv[1],Gui2) == 0)){
			mkdir("./.gittutor");
			chdir("./.gittutor");
			if(access("GitTutor.html", F_OK) !=0) {
				printf("Launching GUI... Please wait.\n");
				system("curl -so GitTutor.html https://raw.githubusercontent.com/joey-walker/COMP650-SeniorProject/master/GitTutor/GitTutor.html");
				system("curl -so Picture1.png  https://raw.githubusercontent.com/joey-walker/COMP650-SeniorProject/master/GitTutor/Picture1.png");
				system("curl -so local.PNG https://raw.githubusercontent.com/joey-walker/COMP650-SeniorProject/master/GitTutor/local.PNG");
				system("curl -so padnote.PNG https://raw.githubusercontent.com/joey-walker/COMP650-SeniorProject/master/GitTutor/padnote.PNG");
				system("curl -so repo_created.png https://raw.githubusercontent.com/joey-walker/COMP650-SeniorProject/master/GitTutor/repo_created.png");
			}
			system("start GitTutor.html");
			exit(1);
		}else if (argc >= 2 ){
			if( strcmp(argv[1],Gui1) != 0 || strcmp(argv[1],Gui2) != 0){
				printf("Git Tutor did not recognize the additional argument\n Correct usage is 'git tutor --gui' or 'git tutor -g'");
				exit(1);
			}
		}
		
		/*													*/
		
		
        char begin_end[3];
		char yes_no[4];
        char yes1[] = "Yes";
        char yes2[] = "yes";

        int loopi=0;
		FILE *dummyfile;
	 char read_input[100];
		char fetch_str[] = "fetched";
		char merge_str[] = "merged";
		char push_str[] = "pushed";
		char add_str[] = "added";
		char commit_str[] = "committed";
		char fetch_tut[] = "git fetch upstream";
		char fetch_input[19];
		char merge_tut[] = "git merge upstream/master";
		char merge_input[sizeof(merge_tut)];
		char push1_tut[] = "git push origin master";
		char push1_input[sizeof(push1_tut)];
		char add_tut[] = "git add .";
		char commit_tut[] = "git commit -m \"";
		char push2_tut[] = "git push origin/master";
		char diff_tut[] = "git diff";
		char status_tut[] = "git status";
		
		int flag;
		int T_F = 0;

		for (; loopi<1000; loopi++){
			printf("\n");
		}
        //Start
		printf("Git Tutor is a tutorial that seeks to teach Git through a step-by step approach.\n");
        printf("Do you wish to continue? (Yes or No) \n");
		printf("Input >> ");
		scanf("%s", begin_end);
        
		if ((strcmp(yes1, begin_end) != 0)  && (strcmp(begin_end, yes2) != 0)) {
				printf("Exiting tutorial.\n");
                exit(1);
        }
			
		//create directories
		mkdir("./upstream/");
		mkdir("./master/");
		//mkdir("./origin/");
		//Git repo setup
		system("git init -q --bare ./origin.git/");
		system("git init -q ./upstream/");
		system("git clone -q ./origin.git ./master");
		
		dummyfile = fopen("./upstream/test.txt","w");
		fclose(dummyfile);
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

		// add origin and upstream 
		//system("git remote add origin ./origin.git"); < -don't need for some reason
		system("git remote add upstream ../upstream");
		//printf("\nRemote is created for origin and upstream...");
		
		loopi=0;
		for (; loopi<1000; loopi++){
			printf("\n");
		}
		
		printf("Beginning Git Tutor\n");
		printf("The following three repositories are created:\n1. master - our local repository \n2. origin.git - Our repository to which we will send our changes \n3. upstream - Our repository where we will grab changes from\n\n");
		
		//Fetching
		printf("Tutorial 1: Fetching \n");
		printf("Summary: 'git fetch' allows us to retrieve references to changes in other\ngit repositories and bring it to our current git repository.\n");
		printf("Instructions:  Fetch the changes from the upstream repository.\n");
		printf("\nHint: git fetch <Repository Name> \n");

		while(1){
			printf("Input >> ");
			fflush(stdin);
			fgets(fetch_input, sizeof(fetch_input), stdin);
			
			if(strcmp(fetch_tut,fetch_input)==0)
				break;
			else
				printf("incorrect please try again\n");
		}
		//string_compare(fetch_tut, read_input, fetch_str);
		system("git fetch upstream");
		
		//merge
		printf("\n\nTutorial 2: Merging \n");
		printf("Summary: 'git merge will combine the references from another repository with a branch located on local repository\n");
		printf("Instructions: Merge the changes from the upstream repository into the master branch \n");
		printf("\nHint: git merge [Repository Name/LocalBranch]\n");

		while(1){
			printf("Input >> ");
			fflush(stdin);
			fgets(merge_input, sizeof(merge_input), stdin);
			
			if(strcmp(merge_tut,merge_input)==0)
				break;
			else
				printf("incorrect please try again\n");
		}
		
		system("git merge upstream/master");
		//read_input[0]='\0';
		
		
		//push
		printf("\nTutorial 3: Pushing \n");
		printf("Summary: 'git push allows us to move our changes from one repository to a new host.\n");
		printf("Instructions: Push the changes in our master repository to our origin repository on our master branch\n");
		printf("\nHint: git push [target Repository Name] [Local Branch]\nType the command: ");
		//push1_input

		while(1){
			printf("Input >> ");
			fflush(stdin);
			fgets(push1_input, sizeof(push1_input), stdin);
			
			if(strcmp(push1_tut,push1_input)==0)
				break;
			else
				printf("incorrect please try again\n");
		}
		
		system("git push origin master");
		//read_input[0]='\0';
		
		return 0;
		//add
		printf("Tutorial 4: Adding \n");
		printf("\nAdd Command: Add file contents to the index");
		printf("\nHint: git add [option]...\nType the command: ");
		gets(read_input);
		string_compare(add_tut, read_input, add_str);
		system("git add .");
		read_input[0]='\0';
		
		//commit
		printf("Tutorial 5: Committing \n");
		printf("\nRecord changes to the repository");
		printf("\nHint: git commit [option] [Branch]...\nType the command: ");
		int c = 1;
		int len = sizeof(commit_tut);
		
		while(1){
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
			if (T_F != 0)
			{
				printf("Correct! Your file(s) are %s\n", commit_str);
				break;
				//system(read_input);
			}
			else
			{
				printf("Wrong! try again: ");
			}
		}
		
		
		//push
		printf("Tutorial 6: Pushing \n");
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
		
		
		printf("Tutorial complete, congratulations\n");
		
        return 0;
}

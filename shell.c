#include<stdio.h>
#include<string.h>
#include<sys/wait.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/utsname.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/unistd.h>
#include<signal.h>

#define DELIMITERS " \t\r\n\a"
#define COMMAND_LENGTH 1024
#define SINGLE_COMMAND_LENGTH 64

char arr[100000][50], home[1024], *builtin_str[] = {
  "cd","pwd","exit","help","echo"};

int stack[10000],exist[10000], in, out, input, output, colon, position, top;

// Built in commands.
int help(char **args) {
	printf("Use the man command for information about a command.\n");
	return 1;
}

int echo(char **args) {
	in=0;out=0;
	int j=0,y;
	while(args[j]!=NULL) {
		if(strcmp(args[j], ">") == 0) {
			out=1;
			output=j;
		}

		if(strcmp(args[j], ">>") == 0) {
			out=2;
			output=j;
		}
		j++;
	}

	if(out==1) {
		y=dup(1);
		int fd= creat(args[output+1], 0644);
		dup2(fd,1);
		int i=1;
		while(i<output-1) {
			printf("%s ",args[i]);
			i++;
		}
		printf("%s",args[i]);
		printf("\n");
		close(fd);
		dup2(y,1);
	} else if(out==2) {
		y=dup(1);
		int fd = open(args[output+1], O_APPEND|O_WRONLY|O_CREAT, 0666);
		dup2(fd,1);
		int i=1;
		while(i<output) {
			printf("%s ",args[i]);
			i++;
		}
		printf("\n");
		close(fd);
		dup2(y,1);
	} else {
		int i=1;
		while(args[i]!=NULL) {
			printf("%s ",args[i]);
			i++;
		}
		printf("\n");
	}
	return 1;
}

int pwd(char **args) {
	char path[1024];
	getcwd(path, sizeof(path));
	in=0;out=0;
	int j=0,y;
	while(args[j]!=NULL) {
		if(strcmp(args[j],">") == 0) {
			out=1;
			output=j;
		}
		if(strcmp(args[j],">>") == 0) {
			out=2;
			output=j;
		}
		j++;
	}
	if(out==1) {
		y=dup(1);
		int fd= creat(args[output+1],0644);
		dup2(fd,1);
		printf("%s\n",path);
		close(fd);
		dup2(y,1);
	} else if(out==2) {
		y=dup(1);
		int fd=open(args[output+1],O_APPEND|O_WRONLY|O_CREAT,0666);
		dup2(fd,1);
		printf("%s\n",path);
		close(fd);
		dup2(y,1);
	} else {
		printf("%s\n",path);
	}
	return 1;
}

int cd(char **args) {
	if (args[1] == NULL||*args[1]=='~') {
		args[1]=home;
	}

	if (chdir(args[1]) != 0) {
		perror("bash");
	}
	return 1;
}

int exit_prog(char **args){return 0;}

int (*builtin_func[]) (char **) = {&cd, &pwd, &exit_prog, &help, &echo};

int num_of_builtin() {
	return sizeof(builtin_str) / sizeof(char *);
}

int launch(char **args) {
	pid_t pid,wpid;
	int status, y, background=0;
	if(strcmp(args[position-1], "&") == 0) {
		background=1;
		args[position-1]=NULL;
	}

	pid=fork();
	if(pid==0) {
		y=dup(1);
		in=0;out=0;
		int j=0;
		while(args[j] != NULL) {
			if(strcmp(args[j], "<") == 0) {
				in=1;
				input=j;
      }

			if(strcmp(args[j], ">") == 0) {
				out=1;
				output=j;
			} else if(strcmp(args[j], ">>") == 0) {
				out=2;
				output=j;
			}
			j++;
		}

		if(in==1 && out==1) {
			int fd1= open(args[input+1],O_RDONLY);
			dup2(fd1,0);
			close(fd1);
			int fd2= open(args[output+1],O_WRONLY|O_CREAT,0666);
			dup2(fd2,1);
			close(fd2);
			execlp(args[0],args[0],NULL);
		} else {
			if(in==1) {
				int fd0= open(args[input+1],O_RDONLY,0);
				dup2(fd0,0);
				close(fd0);
				args[input+1]=NULL;
				args[input]=NULL;
			}

			if(out == 1) {
				int fd1= creat(args[output+1],0644);
				dup2(fd1,1);
				close(fd1);
				args[output]=NULL;
				args[output+1]=NULL;
			} else if(out == 2) {
				int fd1 = open(args[output+1],O_APPEND|O_WRONLY|O_CREAT,0666);
				dup2(fd1,1);
				close(fd1);
				args[output]=NULL;
				args[output+1]=NULL;
			}
		}

		if(execvp(args[0], args) == -1) {
			perror("bash");
		}

		dup2(y,1);
		exit(EXIT_FAILURE);
	} else if (pid < 0) {
		perror("bash");
	} else {
		if(background == 0) {
			do {
				wpid=waitpid(pid, &status, WUNTRACED);
			}
			while(!WIFEXITED(status) && !WIFSIGNALED(status));
		} else {
			strcpy(arr[pid], args[0]);
			stack[top]=pid;
			exist[pid]=1;
			top++;
			printf("[%d]	%d\n", top, pid);
		}
	}
	return 1;
}

int run(char **args) {
	if (args[0] == NULL) return 1;

	for (int i=0; i<(sizeof(builtin_str)/sizeof(char *)); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			return (*builtin_func[i])(args);
		}
	}

	return launch(args);
}

char**check_more_commands(char*line) {
	int bufsize=SINGLE_COMMAND_LENGTH,position=0;
	char**tokens=malloc(bufsize*sizeof(char*));
	char*token;

  if(!tokens) {
		fprintf(stderr,"bash: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token=strtok(line, (";"));
	while(token!=NULL) {
		tokens[position]=token;
		colon++;
		position++;
		if (position>=bufsize) {
			bufsize+=SINGLE_COMMAND_LENGTH;
			tokens=realloc(tokens,bufsize*sizeof(char*));
			if(!tokens) {
				fprintf(stderr,"bash:allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		token=strtok(NULL,(";"));
	}
	tokens[position]=NULL;
	return tokens;
}

char**parse_line(char *line) {
	int bufsize=SINGLE_COMMAND_LENGTH;
	position=0;
	char**tokens=malloc(bufsize*sizeof(char*));
	char*token;
	if(!tokens) {
		fprintf(stderr,"bash: allocation error\n");
		exit(EXIT_FAILURE);
	}
	token=strtok(line,DELIMITERS);
	while(token!=NULL) {
		tokens[position]=token;
		position++;
		if (position>=bufsize) {
			bufsize+=SINGLE_COMMAND_LENGTH;
			tokens=realloc(tokens,bufsize*sizeof(char*));
			if(!tokens) {
				fprintf(stderr,"bash:allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		token=strtok(NULL,DELIMITERS);
	}
	tokens[position]=NULL;
	return tokens;
}

int piping(char *line) {
	char **commands=malloc(256 * sizeof(char *));
	char *command;
	int pos=0,s=0,r=0;
	command = strtok(line,("|"));
	while(command!=NULL) {
		commands[pos]=command;
		command=strtok(NULL,("|"));
		pos++;
	}
	commands[pos]=NULL;
	if(pos==1) {
		char **args;
		args=parse_line(line);
		s=run(args);
		return s;
	}
	int p[2];
	pid_t pid;
	int input=0,i;
	char **args_commands;
	for(i=0;i<pos;i++) {
		pipe(p);
		pid=fork();
		if(pid==-1) exit(EXIT_FAILURE);
		else if(pid==0) {
			dup2(input,0);
			if(commands[i+1]!=NULL)
				dup2(p[1],1);
			close(p[0]);
			args_commands=parse_line(commands[i]);
			s=run(args_commands);
			free(args_commands);
			exit(EXIT_FAILURE);
		} else {
			wait(NULL);
			close(p[1]);
			input=p[0];
		}
	}
	return 1;
}

char *get_line(void) {
	int bufsize=COMMAND_LENGTH;
	int position=0;
	char*buffer=malloc(sizeof(char)*bufsize);
	int c;

	if(!buffer) {
		fprintf(stderr,"bash: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while(1) {
		c=getchar();
		if (c==EOF||c=='\n') {
			buffer[position]='\0';
			return buffer;
		} else {
			buffer[position]=c;
		}
		position++;
		if(position>=bufsize) {
			bufsize+=COMMAND_LENGTH;
			if(!buffer) {
				fprintf(stderr,"bash: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

// This method run in a forever loop until exit is specified.
void active_loop(void) {
	char **args, **commands, *line;
	getcwd(home, sizeof(home));
	top=0;
	int stat, i, status = 1;
	while(status) {
		colon=0;
		struct utsname info;
		uname(&info);
		char path[1024];
		getcwd(path,sizeof(path));
		i=strlen(home);

		pid_t t;
		while((t = waitpid(-1, &stat, WNOHANG)) > 0) {
			exist[t]=0;
			printf("%s with pid %d excited normally\n",arr[t],t);
		}

    // printing user information.
		printf("<%s@%s:",getlogin(),info.nodename);
		if(strcmp(home, path) == 0) printf("~");
		else if (strcmp(home, path) < 0) {
			printf("~");
			while(path[i]!='\0') {
				printf("%c",path[i]);
				i++;
			}
		} else printf("%s",path);
		printf("> ");

    // getting command input.
		line = get_line();
		if(strcmp(line, "jobs") == 0) {
			for(i=0;i<top;i++) {
				if(exist[stack[i]]==1)
					printf("[%d] %s [%d]\n", i+1, arr[stack[i]], stack[i]);
			}
			status=1;
		} else {
			commands=check_more_commands(line);
			for(i=0;i<colon;i++) {
				status=piping(commands[i]);
				//			args=parse_line(commands[i]);
				//			status=run(args);
				//			free(args);
			}
			free(line);
		}
	}
}

int main(int argc, char**argv)
{
	// control loop taking care of running commands until exit is called.
	active_loop();

	return EXIT_SUCCESS;
}

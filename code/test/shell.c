#include "syscall.h"

char help_str[] =
"----------------------------------\n"
"help : show this message          \n"
"pwd : print current path          \n"
"ls : list files in current path   \n"
"cd <path> : change current path   \n"
"exit : exit this shell            \n"
"p <file> : print content of a file\n"
"x <program> : execute a program   \n"
"mkdir <dir> : create a directory  \n"
"rmdir <dir> : remove a directory  \n"
"nf <file> : create a new file     \n"
"rm <file> : remove a file         \n"
"----------------------------------\n";
int len_help_str = 35 * 13;

char invalid_str[] = "command not found\n";
int len_invalid_str = 18;

int main(){
    SpaceId newProc;
    OpenFileId input = ConsoleInput;
    OpenFileId output = ConsoleOutput;
    OpenFileId newFile;
    char prompt[2], ch, cmd[60];
    int i;
    prompt[0] = '>';
    prompt[1] = '>';
    while(1){
        Write(prompt, 2, output);
        i = 0;
        do {
            Read(&cmd[i], 1, input); 
        } while( cmd[i++] != '\n' );
        cmd[--i] = '\0';
        if(i==0)continue;
        if(cmd[0]=='h'&&cmd[1]=='e'&&cmd[2]=='l'&&cmd[3]=='p'&&cmd[4]=='\0'){
            Write(help_str, len_help_str, output);
        }else if(cmd[0]=='p'&&cmd[1]=='w'&&cmd[2]=='d'&&cmd[3]=='\0'){
            Pwd();
        }else if(cmd[0]=='l'&&cmd[1]=='s'&&cmd[2]=='\0'){
            Ls();
        }else if(cmd[0]=='c'&&cmd[1]=='d'&&cmd[2]==' '){
            Cd(cmd+3);
        }else if(cmd[0]=='e'&&cmd[1]=='x'&&cmd[2]=='i'&&cmd[3]=='t'&&cmd[4]=='\0'){
            Exit(0);
        }else if(cmd[0]=='p'&&cmd[1]==' '){
            newFile = Open(cmd+2);
            while(Read(&ch, 1, newFile))Write(&ch, 1, output);
            Close(newFile);
        }else if(cmd[0]=='x'&&cmd[1]==' '){
            newProc = Exec(cmd+2);
            Join(newProc);
        }else if(cmd[0]=='m'&&cmd[1]=='k'&&cmd[2]=='d'&&cmd[3]=='i'&&cmd[4]=='r'&&cmd[5]==' '){
            Mkdir(cmd+6);
        }else if(cmd[0]=='r'&&cmd[1]=='m'&&cmd[2]=='d'&&cmd[3]=='i'&&cmd[4]=='r'&&cmd[5]==' '){
            Rmdir(cmd+6);
        }else if(cmd[0]=='n'&&cmd[1]=='f'&&cmd[2]==' '){
            Create(cmd+3);
        }else if(cmd[0]=='r'&&cmd[1]=='m'&&cmd[2]==' '){
            Remove(cmd+3);
        }else{
            Write(invalid_str, len_invalid_str, output);
        }
    }
}


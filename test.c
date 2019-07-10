#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h> 

void sighandle(int sig);

int main(int argc, char* argv[]){

  signal(SIGINT, sighandle);
 
  printf("Running all test cases \n");
 
  printf("\n*** Test case 0, configure *** \n");
  pid_t child_0;
  if((child_0 = fork()) == 0 ){
    char *args[] = {"./WTF", "configure", "localhost", "17000", (char*)0};
    execv(args[0], args);                                                                                             
    perror("Execv error child_0");                                                                                  
  } 
  waitpid(child_0, NULL, 0);


  printf("\n*** Starting server ***\n");
  pid_t child_1;
  if((child_1 = fork()) == 0 ){
    char *args1[] = {"./WTFserver", "17000", (char*)0};
    execv(args1[0], args1);
    perror("Execv error child_1");
  }



  
  printf("\n*** Test case 1: create project named TESTCASE ***\n");
  pid_t child_2;
  if((child_2 = fork()) == 0 ){
    char *args2[] = {"./WTF", "create", "TESTCASE", (char*)0};
    execv(args2[0], args2);
    perror("Execv error child_2");
  }
  waitpid(child_2, NULL, 0);
  
  

  
  printf("\n*** Test case 2: add file (file1.txt) to TESTCASE ***\n");
  char address[100];
  sprintf(address, "TESTCASE/file1.txt");
  int filedescriptor = open(address, O_WRONLY | O_CREAT, 0777);  
  
  FILE *fp = fopen(address, "w");
  fprintf(fp, "This is a test run. \n");
  fclose(fp);

  pid_t child_3;
  if((child_3 = fork()) == 0 ){
    char *args3[] = {"./WTF", "add", "TESTCASE", "file1.txt",(char*)0};
    execv(args3[0], args3);
    perror("Execv error child_3");
  }
  waitpid(child_3, NULL, 0);
  


  printf("\n*** Test case 3: commit the project***\n");
  pid_t child_4;
  if((child_4 = fork()) == 0 ){
    char *args4[] = {"./WTF", "commit", "TESTCASE", (char*)0};
    execv(args4[0], args4);
    perror("Execv error child_4");
  }
  waitpid(child_4, NULL, 0);



  printf("\n***Test case 4, push the project***\n");
  pid_t child_4_2;
  if((child_4_2 = fork()) == 0 ){
    char *args42[] = {"./WTF", "push", "TESTCASE", (char*)0};
    execv(args42[0], args42);
    perror("Execv error child_4_2");
  }
  waitpid(child_4_2, NULL, 0);

  
  printf("\n*** Now we will commit and push 2 more files ***\n");
    
  bzero(address,100);
  sprintf(address, "TESTCASE/file2.txt");
  filedescriptor = open(address, O_WRONLY | O_CREAT, 0777);

  FILE *fp2 = fopen(address, "w");
  fprintf(fp2, "This is a test run. File number 2. \n");
  fclose(fp2);

  pid_t child_5;
  if((child_5 = fork()) == 0 ){
    char *args5[] = {"./WTF", "commit", "TESTCASE", "file2.txt",(char*)0};
    execv(args5[0], args5);
    perror("Execv error child_5");
  }
  waitpid(child_5, NULL, 0);


  pid_t child_5_2;
  if((child_5_2 = fork()) == 0 ){
    char *args5_2[] = {"./WTF", "push", "TESTCASE", "file2.txt",(char*)0};
    execv(args5_2[0], args5_2);
    perror("Execv error child_5_2");
  }
  waitpid(child_5_2, NULL, 0);


  bzero(address,100);
  sprintf(address, "TESTCASE/file3.txt");
  filedescriptor = open(address, O_WRONLY | O_CREAT, 0777);  
  FILE *fp3 = fopen(address, "w");
  fprintf(fp3, "This is a test run. File number 2. \n");
  fclose(fp3);
  pid_t child_6;
  if((child_5 = fork()) == 0 ){
    char *args6[] = {"./WTF", "commit", "TESTCASE", "file3.txt",(char*)0};
    execv(args6[0], args6);
    perror("Execv error child_5");
  }
  waitpid(child_6, NULL, 0);
  
  pid_t child_6_2;
  if((child_6_2 = fork()) == 0 ){
    char *args6_2[] = {"./WTF", "push", "TESTCASE", "file3.txt",(char*)0};
    execv(args6_2[0], args6_2);
    perror("Execv error child_6_2");
  }
  waitpid(child_6_2, NULL, 0);
  



  printf("\n*** Test case 5: current version ***\n");
  pid_t child_7;
  if((child_7 = fork()) == 0 ){
    char *args7[] = {"./WTF", "checkout", "TESTCASE",(char*)0};
    execv(args7[0], args7);
    perror("Execv error child_7");
  }
  waitpid(child_7, NULL, 0);





  printf("\n*** Test case 6: history ***\n");
  pid_t child_8;
  if((child_8 = fork()) == 0 ){
    char *args8[] = {"./WTF", "history", "TESTCASE",(char*)0};
    execv(args8[0], args8);
    perror("Execv error child_8");
  }
  waitpid(child_8, NULL, 0);



  printf("\n*** Test case 7: Update ***\n");
  pid_t child_9;
  if((child_9 = fork()) == 0 ){
    char *args9[] = {"./WTF", "update", "TESTCASE",(char*)0};
    execv(args9[0], args9);
    perror("Execv error child_9");
  }
  waitpid(child_9, NULL, 0);


  

  printf("\n*** Test case 8: upgrade ***\n");
  pid_t child_9_2;
  if((child_9_2 = fork()) == 0 ){
    char *args92[] = {"./WTF", "upgrade", "TESTCASE",(char*)0};
    execv(args92[0], args92);
    perror("Execv error child_9_2");
  }
  waitpid(child_9_2, NULL, 0);


  /*
  pid_t child_9_2;
  if((child_9_2 = fork()) == 0 ){
    char *args92[] = {"./WTF", "add", "TESTCASE", "file2.txt",(char*)0};
    execv(args92[0], args92);
    perror("Execv error child_9_2");
    _exit(1);
  }
  waitpid(child_9_2, NULL, 0);
  */

  
  // Case 9
  printf("\n*** Test case 9: checkout***\n");
  pid_t child_9_3;
  if((child_9_3 = fork()) == 0 ){
    char *args93[] = {"./WTF", "checkout", "TESTCASE",(char*)0};
    execv(args93[0], args93);
    perror("Execv error child_9_3");
  }
  waitpid(child_9_3, NULL, 0);



  
  /*
  // Case 10
  //printf("\n*** Test case 10: checkout  ***\n");
  pid_t child_10_1;
  if((child_10_1 = fork()) == 0 ){
    char *args101[] = {"./WTF", "checkout", "TESTCASE",(char*)0};
    execv(args101[0], args101);
    perror("Execv error child_10_1");
  }
  waitpid(child_10_1, NULL, 0);


  char *f1 = "TESTCASE/file1.txt";
  unlink(f1);
  char *f2 = "TESTCASE/file2.txt";
  unlink(f2); 
  char *f3 = "TESTCASE/file3.txt";
  unlink(f3);
  char *f4 = "TESTCASE/.manifest";
  unlink(f4);
  char *f5 = "TESTCASE/";
  int temp = rmdir(f5);
  */

  
  // Case 10 skipped
  // Case 11
  printf("\n*** Test case 10: rollback ***\n");
  pid_t child_10;
  if((child_10 = fork()) == 0 ){
    char *args10[] = {"./WTF", "rollback", "TESTCASE", "2", (char*)0};
    execv(args10[0], args10);
    perror("Execv error child_10");
  }
  waitpid(child_10, NULL, 0);
  
  
  // Case 12
  printf("\n*** Test case 11: remove ***\n");
  pid_t child_11;
  if((child_11 = fork()) == 0 ){
    char *args11[] = {"./WTF", "remove", "TESTCASE", "file1.txt", (char*)0};
    execv(args11[0], args11);
    perror("Execv error child_11");
  }
  waitpid(child_11, NULL, 0);




  printf("\n*** Test case 12: current version ***\n");
  pid_t child_13;
  if((child_13 = fork()) == 0 ){
    char *args13[] = {"./WTF", "currentversion", "TESTCASE", (char*)0};
    execv(args13[0], args13);
    perror("Execv error child_13");
  }
  waitpid(child_13, NULL, 0);


  
  printf("\n*** Test case 13: update ***\n");
  pid_t child_14;
  if((child_14 = fork()) == 0 ){
    char *args14[] = {"./WTF", "update", "TESTCASE", (char*)0};
    execv(args14[0], args14);
    perror("Execv error child_14");
  }
  waitpid(child_14, NULL, 0);



    
  printf("\n*** Test case 14: destroy ***\n");
  pid_t child_12;
  if((child_12 = fork()) == 0 ){
    char *args12[] = {"./WTF", "destroy", "TESTCASE", (char*)0};
    execv(args12[0], args12);
    perror("Execv error child_12");
  }
  waitpid(child_12, NULL, 0);
  


  
  printf("\n*** Test case 15: EXIT (SIGINT) ***\n");


  waitpid(child_1, NULL, 0);  
  return 0;
}

void sighandle(int sig){
  printf("EXITING... \n");
}
  

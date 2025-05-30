#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[])
{    
    
    if(argc <= 1)//no arg parsed
    {
        printf("No argument parsed\n\rEnter arguments in this format: [path to file] [string to be written to the file]\n\r");
        return 1;
    }
    else if(argc < 3)
    {
        printf("Expecting two arguments\n\r"); 
        printf("Enter arguments in this format: [path to file] [string to be written to the file]\n\r");       
        return 1;
    }
    else if(argc == 3)
    {
        //check if the dir exist and write to the file
        // printf("no. of arg : %d \n\r dir : %s\n\r",argc, argv[1]);
        
        const char *writefile = argv[1];
        const char *writestr = argv[2];
             
        
        printf("path to file: %s\n\rwrite string: %s\n\r", writefile, writestr);

        //open file if the file already exists, create it.
        // fd = open(writefile, O_RDWR|O_CREAT|O_TRUNC|O_APPEND,S_IRWXU);
        int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC,S_IRWXU);
        if (fd == -1)//fd<0
        {
            perror("Failed to open file");
            
            openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
            syslog(LOG_ERR,"Failed to open %s : %s", writefile, strerror(errno));//Sends a log message to the syslog system.
            closelog(); //Closes the log connection
            return 1;
        }
        else{//fd>0
            // printf("opening file : %d\n\r", errno);
            perror("Opening file...");
            
            //write to file
            ssize_t ws;//size of text to be written in byte
            int cd; // close descriptor

            ws = write (fd, writefile, 12);
            cd = close(fd);
            // cd = close(fd);
            // perror("close after writting ");

            if (ws == -1) {
                perror("Failed to write to file");
                // close(fd);
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_ERR,"Failed to write to %s : %s", argv[1], strerror(errno));//Sends a log message to the syslog system.
                closelog(); //Closes the log connection

                cd = close(fd); //close file
                return 1;
            }
            else if(ws != (ssize_t) strlen(writestr)){
                printf("Partial write : wrote %zu bytes instead of %zu bytes\r\n", ws, strlen(writestr));
                
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_DEBUG,"Partial write to %s, %zu bytes written instead of %zu bytes : %s", argv[1], ws, strlen(writestr), strerror(errno));//Sends a log message to the syslog system.
                closelog(); //Closes the log connection
                
                cd = close(fd); //close file
                return 1;
            }
            else ;

            // Close the file descriptor
            if (cd == -1) {
                errno = 0;
                perror("close");
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_ERR,"Closing %s... : %s", argv[1], strerror(errno));//Sends a log message to the syslog system.
                closelog(); //Closes the log connection
                
                return 1;
            }


            perror("writing file...");

            openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
            syslog(LOG_DEBUG,"Writing \"%s\" to %s : %s", argv[2], argv[1], strerror(errno));//Sends a log message to the syslog system.
            closelog(); //Closes the log connection (optional but good practice).Helps free up any associated resources.
            return 0;            
        }
                
    }

    // return 0;
}

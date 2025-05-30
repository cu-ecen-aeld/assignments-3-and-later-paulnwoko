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
        if (fd == -1)
        {
            perror("Failed to open file");
            
            openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
            syslog(LOG_ERR,"Failed to open %s : %s", writefile, strerror(errno));//Sends a log message to the syslog system.
            closelog(); //Closes the log connection
            return 1;
        }
        else{
            // printf("opening file : %d\n\r", errno);
            perror("Opening file...");
            
            //write to file
            ssize_t ws;//size of text to be written in byte
            ws = write (fd, writefile, strlen (writestr));
            int cl = close(fd);

            if (ws == -1) {
                perror("Failed to write to file");
                // close(fd);
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_ERR,"Failed to write to %s : %s", argv[1], strerror(errno));//Sends a log message to the syslog system.
                closelog(); //Closes the log connection
                
                return 1;
            }
            else if(ws != (ssize_t) strlen(writestr)){
                perror("Partial write");
                // close(fd);
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_ERR,"Partial write to %s : %s", argv[1], strerror(errno));//Sends a log message to the syslog system.
                closelog(); //Closes the log connection
                
                return 1;
            }
              // Close the file descriptor
            if (cl == -1) {
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

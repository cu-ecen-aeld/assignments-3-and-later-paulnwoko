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
    else if(argc != 3)
    {
        printf("Expecting two arguments\n\r"); 
        printf("Enter arguments in this format: [path to file] [string to be written to the file]\n\r");       
        return 1;
    }
    else if(argc == 3)
    {
        //get path to file and string to written to file from the parsed arguments
        const char *writefile = argv[1];
        const char *writestr = argv[2];
                
        printf("path to file: %s\n\rwrite string: %s\n\r", writefile, writestr);

        //open file if the file already exists, else create it.
        // fd = open(writefile, O_RDWR|O_CREAT|O_TRUNC|O_APPEND,S_IRWXU);
        int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC,S_IRWXU);
        
        //fd<0 failed to open
        if (fd == -1)
        {
            perror("Failed to open file");
            
            openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
            syslog(LOG_ERR,"Failed to open %s : %s", writefile, strerror(errno));//Sends a log message to the syslog system.
            closelog(); //Closes the log connection
            return 1;
        }
        else{//fd>0 openned successfully
            // printf("opening file : %d\n\r", errno);
            perror("Opening file...");
            
            ssize_t ws;//size of text to be written in byte
            //int cd; // close descriptor

            //write to file
            ws = write (fd, writestr, strlen(writestr));
            // cd = close(fd);            

            if (fd == -1) {
                perror("Failed to write to file");
                // close(fd);
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_ERR,"Failed to write to %s : %s", argv[1], strerror(errno));//Sends a log message to the syslog system.
                closelog(); //Closes the log connection

                //close(fd); //close file
                if (close (fd) == -1)
                    perror ("close");
                
                return 1;
            }
            else if((ws != (ssize_t) strlen(writestr)) & (fd != -1)){ //ie if the fd indicate sucessfull write and size written is less than achual size
                printf("Partial write : wrote %zu bytes out of %zu bytes\r\n", ws, strlen(writestr));
                
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_DEBUG,"Partial write to %s, wrote %zu bytes out of %zu bytes : %s", argv[1], ws, strlen(writestr), strerror(errno));//Sends a log message to the syslog system.
                closelog(); //Closes the log connection
                
                //close(fd); //close file
                if (close (fd) == -1)
                    perror ("close");

                return 1;
            }
            else{
                perror("writing file...");
                // printf("%d\r\n", fd);
                openlog("writer_app", LOG_PID | LOG_CONS, LOG_USER);//
                syslog(LOG_DEBUG,"Writing \"%s\" to %s, %zu/%zu bytes written : %s", argv[2], argv[1], ws, strlen(writestr), strerror(errno));//Sends a log message to the syslog system.
                //close(fd);
                if (close (fd) == -1)
                    perror ("close");
                closelog(); //Closes the log connection
                return 0;
            }
        }
    }

    return 0;
}
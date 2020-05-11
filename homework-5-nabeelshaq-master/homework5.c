#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/dir.h>

#define BACKLOG (10)

void serve_request(int);

char * req_str = "HTTP/1.0 200 OK\r\n"
"Content-type: %s; charset=UTF-8\r\n\r\n";

char * not_found = "HTTP/1.0 404 Not Found\r\n"
"Content-type: text/html; charset=UTF-8\r\n\r\n%s";

char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
"<title>Directory listing for %s</title>"
"<body>"
"<h2>Directory listing for %s</h2><hr><ul>";

// snprintf(output_buffer,4096,index_hdr,filename,filename);


char * index_body = "<li><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr></body></html>";

char* request_str(char* filename) {
    char *buffer = malloc(sizeof(char) * 257);
    char *mime_check = strstr(filename, ".");
    char *mime = "text/html";
    if (strcmp(mime_check,".htm") == 0 || strcmp(mime_check, ".html" ) == 0) {
        mime = "text/html";
    }
    else if (strcmp(mime_check,".txt" ) == 0) {
        mime = "text/plain";
    }
    else if (strcmp(mime_check, ".jpeg") == 0 || strcmp(mime_check, ".jpg" ) == 0) {
        mime = "image/jpeg";
    }
    else if (strcmp(mime_check, ".gif" ) == 0) {
        mime = "image/gif";
    }
    else if (strcmp(mime_check, ".png" ) == 0) {
        mime = "image/png";
    }
    else if (strcmp(mime_check, ".pdf" ) == 0) {
        mime = "application/pdf";
    }
    snprintf(buffer, 257, req_str, mime);
    return buffer;
}

// example.c get_directory_contents
char* get_directory_contents(char* directory_path)
{
    char* directory_listing = (char*) malloc(sizeof(char)*1013);
    
    directory_path = "404 Not Found";
    
    // open directory path up
    DIR* path = opendir(directory_path);
    
    // check to see if opening up directory was successful
    if(path != NULL)
    {
        directory_listing[0] = '\0';
        
        // stores underlying info of files and sub_directories of directory_path
        struct dirent* underlying_file = NULL;
        
        // iterate through all of the  underlying files of directory_path
        while((underlying_file = readdir(path)) != NULL)
        {
            strcat(directory_listing, underlying_file->d_name);
            strcat(directory_listing, "\n");
        }
        
        closedir(path);
    }
    snprintf(directory_listing,1016, not_found, directory_path);
    return directory_listing;
}


/* The function that each new thread will execute when it begin. To make this
 * generic, it returns a (void *) and takes a (void *) as it's argument.  In C,
 * (void *) is essentially a typeless pointer to anything that you can cast to
 * anything else when necessary. For example, look at the argument to this
 * thread function.  Its type is (void *), but since we KNOW that the type of
 * the argument is really a (struct thread_arg *), the first thing we do is cast
 * it to that type, to make it usable. */
void *thread_function(void *argument_value) {
    //    struct thread_arg *my_argument = (struct thread_arg *) argument_value;
    //
    //    printf("Hi, I'm thread number %d, but I prefer to go by %s.\n",
    //           my_argument->thread_number, my_argument->name);
    int thread_number = *((int *) argument_value);
    free (argument_value);
    serve_request(thread_number);
    close(thread_number);
    
    return NULL;
}

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X"
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request
 *
 * Does not modify the given request string.
 * The returned resource should be free'd by the caller function.
 */
char* parseRequest(char* request) {
    //assume file paths are no more than 256 bytes + 1 for null.
    char *buffer = malloc(sizeof(char)*257);
    memset(buffer, 0, 257);
    
    if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0;
    
    sscanf(request, "GET %s HTTP/1.", buffer);
    return buffer;
}


void serve_request(int client_fd){
    int read_fd;
    int bytes_read;
    int file_offset = 0;
    char client_buf[4096];
    char send_buf[4096];
    char filename[4096];
    char * requested_file;
    memset(client_buf,0,4096);
    memset(filename,0,4096);
    struct stat buf;
    while(1){
        
        file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
        if(strstr(client_buf,"\r\n\r\n"))
            break;
    }
    requested_file = parseRequest(client_buf);
    //send(client_fd,request_str,strlen(request_str),0);
    // take requested_file, add a . to beginning, open that file
    filename[0] = '.';
    strncpy(&filename[1],requested_file,4095);
    read_fd = open(filename,0,0);
    if ( read_fd == -1 ){
        strncpy(&filename[1],"404.html",4095);
        read_fd = open(filename,0,0);
        send(client_fd, get_directory_contents(filename), strlen(get_directory_contents(filename)), 0);
    }
    else{
        stat(filename, &buf);
        if (S_ISDIR(buf.st_mode)) {
            snprintf(filename, 257, "%s%s", filename, "index.html");
            read_fd = open(filename , 0, 0);
            send(client_fd, request_str(filename),strlen(request_str(filename)), 0);
        }
        else{
            send(client_fd, request_str(requested_file), strlen(request_str(requested_file)), 0);
        }
        while(1){
            bytes_read = read(read_fd,send_buf,4096);
            if(bytes_read == 0)
                break;
            
            send(client_fd,send_buf,bytes_read,0);
        }
    }
    
    close(read_fd);
    close(client_fd);
    return;
}

/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
    /* For checking return values. */
    int retval;
    
    /* Read the port number from the first command line argument. */
    int port = atoi(argv[1]);
    
    /* No longer ignore second command line arg */
    chdir(argv[2]);
    
    /* Create a socket to which clients will connect. */
    int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }
    
    /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
    int reuse_true = 1;
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));
    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }
    
    /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */
    
    
    struct sockaddr_in6 addr;   // internet socket address data structure
    addr.sin6_family = AF_INET6;  //ipv6?
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces
    
    
    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if(retval < 0) {
        perror("Error binding to port");
        exit(1);
    }
    
    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    if(retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }
    
    while(1) {
        /* Declare a socket for the client connection. */
        int *sock= malloc(sizeof(int));
        //int sock = NULL;
        //char buffer[256];
        //int num_threads;
        
        // thread_example.c code
        /* Allocate some memory for the thread structures that the pthreads library
         * uses to store thread state information. */
        pthread_t *threads = malloc(sizeof(pthread_t));
        if (threads == NULL) {
            printf("malloc() failed\n");
            exit(1);
        }
        
        /* Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from. */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr);
        
        /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
        *sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if(sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }
        
        /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */
        
        /* ALWAYS check the return value of send().  Also, don't hardcode
         * values.  This is just an example.  Do as I say, not as I do, etc. */
        //serve_request(sock);
        
        // thread_example.c code
        //retval = pthread_create(threads, NULL, thread_function, (int *)sock);
        retval = pthread_create(threads, NULL, thread_function, sock);
        if (retval) {
            perror("pthread_create() failed");
            exit(1);
        }
        /* Tell the OS to clean up the resources associated with that client
         * connection, now that we're done with it. */
        free(threads);
    }
    
    close(server_sock);
}

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_MAX 3
#define DIRECTION_MAX 35
#define VALUE_MAX 30

#define IN  0
#define OUT 1
#define LOW  0
#define HIGH 1

#define PIN 23
#define	PIN2 26
int water_state = 0, motion_state = 0;

void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

static int GPIOExport(int pin) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (-1 == fd) {
	    fprintf(stderr, "Failed to open export for writing!\n");
	    return(-1);
    }
    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

static int GPIOUnexport(int pin) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (-1 == fd) {
	    fprintf(stderr, "Failed to open export for writing!\n");
	    return(-1);
    }
    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

static int GPIODirection(int pin, int dir) {
    static const char s_directions_str[] = "in\0out";
    char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
    int fd;
    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
	    fprintf(stderr, "Failed to open gpio direction for writing!\n");
	    return(-1);
    }
    if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
	    fprintf(stderr, "Failed to set direction!\n");
	    return(-1);
    }
    close(fd);
    return(0);
}

static int GPIORead(int pin) {
    char path[VALUE_MAX];
    char value_str[3];
    int fd;
    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd) {
	    fprintf(stderr, "Failed to open gpio value for reading!\n");
	    return(-1);
    }
    if (-1 == read(fd, value_str, 3)) {
	    fprintf(stderr, "Failed to read value!\n");
	    return(-1);
    }
    close(fd);
    return(atoi(value_str));
}

static int GPIOWrite(int pin, int value) {
    static const char s_values_str[] = "01";
    char path[VALUE_MAX];
    int fd;
    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
	    fprintf(stderr, "Failed to open gpio value for writing!\n");
	    return(-1);
    }
    if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
	    fprintf(stderr, "Failed to write value!\n");
	    return(-1);
    }
    close(fd);
    return(0);
}

void *water_thd() {
    water_state = GPIORead(PIN);
    usleep(500000);
}

void *motion_thd() {
    motion_state = GPIORead(PIN2);
    usleep(500000);
}

int main(int argc, char *argv[]) {
    int state;
    int serv_sock,clnt_sock=-1;
    struct sockaddr_in serv_addr,clnt_addr;
    socklen_t clnt_addr_size;
    char msg[3];
    
    if (-1 == GPIOExport(PIN) || -1 == GPIOExport(PIN2))
	    return(1);
    usleep(100000);
    if (-1 == GPIODirection(PIN, IN) || -1 == GPIODirection(PIN2, IN))
	    return(2);
    usleep(10000);
   
    if(argc!=2){
	    printf("Usage : %s <port>\n",argv[0]);
    }
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
	    error_handling("socket() error");
    memset(&serv_addr, 0 , sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));
    if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr))==-1)
	    error_handling("bind() error");
    if(listen(serv_sock,5) == -1)
	    error_handling("listen() error");
    if(clnt_sock<0){
	    clnt_addr_size = sizeof(clnt_addr);
	    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
	    if(clnt_sock == -1)
		    error_handling("accept() error");   
    }
    while(1) {       
      pthread_t p_thread[2];
      int thr_id;
      int status;

      thr_id = pthread_create(&p_thread[0], NULL, water_thd, NULL);
      if (thr_id < 0) {
        perror("thread create error : ");
        exit(0);
      }
      thr_id = pthread_create(&p_thread[1], NULL, motion_thd, NULL);
      if (thr_id < 0) {
        perror("thread create error : ");
        exit(0);
      }
      pthread_join(p_thread[0], (void**)&status);
      pthread_join(p_thread[1], (void**)&status);
      printf("water : %d   motion : %d\n", water_state, motion_state);
      state = water_state * 10 + motion_state;
      snprintf(msg,3,"%d",state);
      write(clnt_sock, msg, sizeof(msg));

      usleep(5000);
    }
    close(clnt_sock);
    close(serv_sock);
    if (-1 == GPIOUnexport(PIN) || -1 == GPIOUnexport(PIN2))
	    return(7);
    return(0);
}

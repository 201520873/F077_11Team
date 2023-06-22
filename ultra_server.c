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
#define	VALUE_MAX 30

#define IN  0
#define OUT 1
#define LOW  0
#define HIGH 1

#define POUT 23
#define PIN 24

#define	POUT2 5
#define PIN2 6

double time1, distance1;
double time2, distance2;

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

void *ultrawave1_thd() {
    clock_t start_t, end_t;
    if (-1 == GPIOWrite(POUT, 1)) {
	printf("gpio write/trigger err\n");
	exit(0);
    }
    usleep(10);
    GPIOWrite(POUT, 0);
    
    while(GPIORead(PIN) == 0) {
	start_t = clock();
    }
    while(GPIORead(PIN) == 1) {
	end_t = clock();
    }
    
    time1 = (double)(end_t-start_t)/CLOCKS_PER_SEC;
    distance1 = time1/2*34000;
}

void *ultrawave2_thd() {
    clock_t start2_t, end2_t;
    if (-1 == GPIOWrite(POUT2, 1)) {
	printf("gpio write/trigger err\n");
	exit(0);
    }
    usleep(10);
    GPIOWrite(POUT2, 0);
    
    while(GPIORead(PIN2) == 0) {
	start2_t = clock();
    }
    while(GPIORead(PIN2) == 1) {
	end2_t = clock();
    }
    
    time2 = (double)(end2_t-start2_t)/CLOCKS_PER_SEC;
    distance2 = time2/2*34000;
}

int main(int argc, char *argv[]) {
    int dist1, dist2, dist_total;
    int serv_sock,clnt_sock=-1;
    struct sockaddr_in serv_addr,clnt_addr;
    socklen_t clnt_addr_size;
    char msg[5];
    
    if (-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN))
	    return(1);
    usleep(100000);
    if (-1 == GPIOExport(POUT2) || -1 == GPIOExport(PIN2))
    	return(2);
    usleep(100000);
    if (-1 == GPIODirection(POUT, OUT) || -1 == GPIODirection(PIN, IN))
	    return(3);
    if (-1 == GPIODirection(POUT2, OUT) || -1 == GPIODirection(PIN2, IN))
     	return(4);
    if (-1 == GPIOWrite(POUT, 0))
    	return(5);
    usleep(10000);
    if (-1 == GPIOWrite(POUT2, 0))
	    return(6);
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

        thr_id = pthread_create(&p_thread[0], NULL, ultrawave1_thd, NULL);
        if (thr_id < 0) {
          perror("thread create error : ");
          exit(0);
        }
        thr_id = pthread_create(&p_thread[1], NULL, ultrawave2_thd, NULL);
        if (thr_id < 0) {
          perror("thread create error : ");
          exit(0);
        }
        pthread_join(p_thread[0], (void**)&status);
        pthread_join(p_thread[1], (void**)&status);

        printf("time : %.4lf   %.4lf\n", time1, time2);
        printf("distance : %.2lfcm    %.2lfcm\n", distance1, distance2);

        dist1 = (int)distance1;
        if (dist1 > 99) dist1 = 99;
        dist2 = (int)distance2;
        if (dist2 > 99) dist2 = 99;
        dist_total = dist1 * 100 + dist2;

        snprintf(msg,5,"%d",dist_total);
        write(clnt_sock, msg, sizeof(msg));
        printf("msg = %s\n",msg);

        usleep(400000);
    }
    close(clnt_sock);
    close(serv_sock);
    if (-1 == GPIOUnexport(POUT) || -1 == GPIOUnexport(PIN))
	    return(7);
    return(0);
}

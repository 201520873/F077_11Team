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
#define DIRECTION_MAX 45

#define IN  0
#define OUT 1
#define	PWM 0

#define LOW  0
#define HIGH 1

#define	PIN 20
#define POUT 21

#define VALUE_MAX 256

int state = 1, prev_state = 1;
int button_on = 1;
int button_stop = 0;
char msg[5];
    
void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

static int PWMExport(int pwmnum){
	char buffer[BUFFER_MAX];
	int bytes_written;
	int fd;
	fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
	if(-1 == fd) {
		fprintf(stderr, "Failed to open in export!\n");
		return -1;
	}
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
	write(fd, buffer, bytes_written);
	close(fd);
	sleep(1);
	return 0;
}

static int PWMUnexport(int pwmnum){
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
	fd = open("/sys/class/pwm/pwmchip0/unexport", O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open in unexport!\n");
		return -1;
	}
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
	write(fd, buffer, bytes_written);
	close(fd);
	sleep(1);
	return 0;
}

static int PWMEnable(int pwmnum){
	static const char s_unenable_str[] = "0";
	static const char s_enable_str[] = "1";
	char path[DIRECTION_MAX];
	int fd;
	snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
	fd = open(path, O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open in enable!\n");
		return -1;
	}
	write(fd, s_unenable_str, strlen(s_unenable_str));
	close(fd);
	fd = open(path, O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open in enable!\n");
		return -1;
	}
	write(fd,s_enable_str,strlen(s_enable_str));
	close(fd);
	return 0;
}

static int PWMUnable(int pwmnum){
	static const char s_unable_str[] = "0";
	char path[DIRECTION_MAX];
	int fd;
	snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
	fd = open(path, O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open in enable!\n");
		return -1;
	}
	write(fd, s_unable_str, strlen(s_unable_str));
	close(fd);
	return 0;
}

static int PWMWritePeriod(int pwmnum, int value){
	char s_values_str[VALUE_MAX];
	char path[VALUE_MAX];
	int fd, byte;
	snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/period", pwmnum);
	fd = open(path, O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open in period!\n");
		return -1;
	}
	byte = snprintf(s_values_str,10, "%d", value);
	if(-1 == write(fd, s_values_str, byte)){
		fprintf(stderr, "Failed to wrtie value in period!\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

static int PWMWriteDutyCycle(int pwmnum, int value){
	char path[VALUE_MAX];
	char s_values_str[VALUE_MAX];
	int fd, byte;
	snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pwmnum);
	fd = open(path, O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open in duty_cycle!\n");
		return -1;
	}
	byte = snprintf(s_values_str,10,"%d",value);
	if(-1 == write(fd, s_values_str, byte)){
		fprintf(stderr, "Failed to write value! in duty_cycle!\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
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

void *button_thd() {
	button_stop = 0;
	//button_stop = 0 while buzzer_thd send 1
	while (button_stop == 0) {
		state = GPIORead(PIN);
		if (prev_state == 0 && state == 1) {
			button_on = (button_on+1)%2;
			printf("Button!\n");
		}
		prev_state = state;
	}
}

void *buzzer_thd() {
	int dist_total, dist1, dist2;
	
	printf("Receive message from Server : %s\n", msg);
	dist_total = atoi(msg);
	dist1 = dist_total / 100;
	dist2 = dist_total % 100;
	printf("dist1 : %d    dist2 : %d    button_on : %d\n", dist1, dist2, button_on);
	
	//detect near wall
	if (dist1 < 40 && dist2 < 50 && button_on == 1) {
		PWMWritePeriod(0, 1600000);
		PWMWriteDutyCycle(0, 1000000);
		usleep(100000);
	}
	//detect far wall
	else if (dist1 < 70 && dist2 < 80 && button_on == 1) {
		PWMWritePeriod(0, 3000000);
		PWMWriteDutyCycle(0, 1000000);
		usleep(100000);
	}
	//detect near obstacle
	else if (dist1 < 40 && button_on == 1) {
		PWMWritePeriod(0, 1600000);
		PWMWriteDutyCycle(0, 1000000);
		usleep(50000);
		PWMWriteDutyCycle(0, 0);
		usleep(50000);
	}
	//detect far obstacle
	else if (dist1 < 70 && button_on == 1) {
		PWMWritePeriod(0, 3000000);
		PWMWriteDutyCycle(0, 1000000);
		usleep(50000);
		PWMWriteDutyCycle(0, 0);
		usleep(50000);
	}
	//detect nothing
	else {
		PWMWriteDutyCycle(0, 0);
		usleep(100000);
	}
	usleep(300000);
	//stop button_thd
	button_stop++;
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    int str_len;

    if(argc!=3){
        printf("Usage : %s <IP> <port>\n",argv[0]);
        exit(1);
    }
    
    if (-1 == PWMExport(PWM))
        return(1);
    usleep(100000);
    if (-1 == PWMWritePeriod(PWM,2000000))
        return(2);
    if (-1 == PWMWriteDutyCycle(PWM,0))
		    return(3);
    if (-1 == PWMEnable(PWM))
		    return(4);

    if (-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN))
       return(5);
       usleep(100000);
    if (-1 == GPIODirection(POUT, OUT) || -1 == GPIODirection(PIN, IN))
       return(6);
    if (-1 == GPIOWrite(POUT,1))
       return(7);
    usleep(10000);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1)
       error_handling("socket() error");
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));  
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
       error_handling("connect() error");

    while(1) {                      
      pthread_t p_thread[2];
      int thr_id;
      int status;

      thr_id = pthread_create(&p_thread[0], NULL, button_thd, NULL);
      if (thr_id < 0) {
        perror("thread create error : ");
        exit(0);
      }
      str_len = read(sock, msg, sizeof(msg));
      if(str_len == -1)
        error_handling("read() error");

      thr_id = pthread_create(&p_thread[1], NULL, buzzer_thd, NULL);
      if (thr_id < 0) {
        perror("thread create error : ");
        exit(0);
      }
      pthread_join(p_thread[1], (void**)&status);
      pthread_join(p_thread[0], (void**)&status);
      usleep(5000);
	} 
  close(sock);
	if (-1 == PWMUnexport(PWM))
    return(8);
  if (-1 == GPIOUnexport(POUT) || -1 == GPIOUnexport(PIN))
		return(9);   
  return(0);
}

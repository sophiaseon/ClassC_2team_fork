#define SERVER_IP "192.168.10.3"
#define SERVER_PORT 25000

#define DATE_SIZE 64
#define MSG_SIZE 64
typedef struct {
	char date[DATE_SIZE];
	char msg[MSG_SIZE];
} info_t;


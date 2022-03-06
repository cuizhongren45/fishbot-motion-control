#include "udp_client.h"

extern int16_t target_spped_left;
extern int16_t target_spped_right;


void print_hex(char *buffer, int len){
	int i;
	printf("********************************************************\n");
	for(i = 1; i <= len; i++){
		printf("0x%02X ",buffer[i-1]);					
		if(i % 16 == 0){
			printf("\n");
		}
	}
	printf("\n");
	printf("********************************************************\n");
}


// #define HOST_IP_ADDR "10.55.23.25"
#define HOST_IP_ADDR "192.168.1.102"
#define PORT 3334

static const char *TAG = "udp_client";


int sock = -1;
struct sockaddr_in dest_addr;
uint8_t send_error = 0;

void send_data(char *data,uint8_t len)
{
    if(sock>0)
    {
        // vTaskSuspendAll();
        int err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        // xTaskResumeAll();
        if (err < 0 && send_error != 1 ) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d ,sock:%d ,send_error:%d", errno, sock,send_error);
            if(errno==12) return; 
            send_error = 1;
            while (send_error)
            {
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                ESP_LOGE(TAG, "Delay wait sockt reconnect! sock:%d",sock);
            }
        }
    }
}


static motor frame = {
    .START=0x7D,
    .TARGET = 0x01,
    .CODE = 0X01,
    .data_len = 0x04,
    .spped_left = 0,
    .spped_right = 0,
    .sum = 0,
    .END = 0x7E,
};

static void handleData(char* data_buff,uint8_t data_len)
{
    switch(data_buff[2])
    {
        case 0x01: memcpy(&frame,data_buff,data_len); break;
    }
    // print_hex((char*)&frame,sizeof(frame));
    if(frame.sum==calc_checksum((char *)&frame+4,frame.data_len))
    {
        // ESP_LOGI(TAG, "speed_left:%d  spped_right:%d  ",frame.spped_left,frame.spped_right);
        target_spped_left = frame.spped_left;
        target_spped_right = frame.spped_right;
    }
}

static void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    int addr_family = 0;
    int ip_protocol = 0;
    uint32_t non_blocking=1;
    struct timeval timeout={
        .tv_sec = 0,
        .tv_usec = 20000,
    }; 
    
    while (1) {
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            // break;
            continue;
        }
        ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

        ioctlsocket(sock,FIONBIO,&non_blocking);
        //设置为非阻塞
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        send_error = 0;


        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
        int len=0 ;
        while (1) {
            len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len >= 0) {
                // rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                handleData(rx_buffer,len);
            }
            if(send_error){
                shutdown(sock, 0);
                close(sock);
                break;
            }
            // Error occurred during receiving
            // if (len < 0) {
            //     ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            //     break;
            // }
            // // Data received
            // else {
            //     rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
            //     ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
            //     // ESP_LOGI(TAG, "%s", rx_buffer);
            // }
            vTaskDelay(10 / portTICK_PERIOD_MS);
            // ESP_LOGI(TAG ,"sock:%d len:%d", sock,len);
        }

        
        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}


void my_udp_init()
{    
    xTaskCreate(udp_client_task, "upd_client", 4096, NULL, 5, NULL);
}


uint8_t calc_checksum(char *p_data, int32_t data_len)
{
    uint8_t sum = 0;
    while (data_len--) {
        sum += *p_data++;
    }
    return sum;
}
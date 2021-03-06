// skyrelay3.c -- skypush and skyrel project in one
//                with normal forming packets based on vcard input
//

#include <stdio.h>
#include <stdlib.h>

#include <string.h>  
#include <time.h>

#include <fcntl.h>
#include <io.h>

#include "miracl_lib/miracl.h"
#include "short_types.h"

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")


#include "relays_util.h"

struct _relays relays;

miracl *mip;


char REMOTE_VERSION[0x100];
int REMOTE_VERSION_LEN;

//
// converting 2 bytes of ptr from ascii to hex
//
int convert_str_to_hex(char *ptr) {
        int hex_digit=0;

        if ((ptr[0]>=0x30) && (ptr[0]<=0x39)){
            hex_digit=ptr[0]-0x30;
        };
        if ((ptr[0]>='A') && (ptr[0]<='F')){
            hex_digit=ptr[0]-0x41+0x0A;
        };
        if ((ptr[0]>='a') && (ptr[0]<='f')){
            hex_digit=ptr[0]-0x61+0x0A;
        };
        hex_digit=hex_digit<<4;

        if ((ptr[1]>=0x30) && (ptr[1]<=0x39)){
            hex_digit+=ptr[1]-0x30;
        };
        if ((ptr[1]>='A') && (ptr[1]<='F')){
            hex_digit+=ptr[1]-0x41+0x0A;
        };
        if ((ptr[1]>='a') && (ptr[1]<='f')){
            hex_digit+=ptr[1]-0x61+0x0A;
        };

        return hex_digit;
};


int parse_ipport(char *str, unsigned int *addr, int *port, int parse_debug) {
    char *ptr;

    ptr = strtok(str,":");
    if (ptr == NULL) {
        printf("port parsing error\n");
        return -1;
    };
    if (parse_debug) {
        printf("ptr: %s\n", ptr);
    };
    *addr = inet_addr(ptr);

    ptr = strtok(NULL,":");
    if (ptr == NULL) {
        printf("port parsing error\n");
        return -1;
    };
    if (parse_debug) {
        printf("ptr: %s\n", ptr);
    };
    *port = htons(atol(ptr));

    return 0;
};


int parse_input(char *str, char *remote_vcard, char *destip, unsigned short *destport) {
    char userid[1024];
    char ipport_sup[1024];
    char ipport_ext[1024];
    char ipport_int[1024];
    unsigned long addr;
    int parse_debug;
    int port;
    int len;
    int i;
    int j;
    int pos;
    int hex;
    int s_len;
    char *some;

    parse_debug = 0;

    printf("\n");

    //
    // format
    //

    /*
        0x2092822a149a12d0-s
        -s64.4.23.172:40002
        -r213.87.137.106:60231
        -l10.180.1.202:60231
    */

    //
    // parse userid (hostid)
    //

    if ((str[0] == '0') && (str[1] == 'x')) {
        j = 0;
        for (i=2;i<18;i+=2) {
            hex = convert_str_to_hex(str+i);
            userid[j] = hex & 0xff;
            //printf("hex = %02X\n", hex);
            j++;
        };
    } else {
        printf("hostid format error\n");
    };

    if (parse_debug) {
        show_memory(userid, 8, "userid:");
    };

    //
    // parse addresses
    //

    len = strlen(str);
    for (i=20; i<len; i++){
        if ((str[i] == '-') && (str[i+1] == 's')) {
            j=i+1;
            while((str[j] != '-') && (j<len)) { j++; };
            s_len = j - i - 2;
            memcpy(ipport_sup, str+i+2, s_len);
            ipport_sup[s_len] = 0;
            if (parse_debug) {
                printf("ipport_sup: %s\n", ipport_sup);
            };
        };
        if ((str[i] == '-') && (str[i+1] == 'r')) {
            j=i+1;
            while((str[j] != '-') && (j<len)) { j++; };
            s_len = j - i - 2;
            memcpy(ipport_ext, str+i+2, s_len);
            ipport_ext[s_len] = 0;
            if (parse_debug) {
                printf("ipport_ext: %s\n", ipport_ext);
            };
        };
        if ((str[i] == '-') && (str[i+1] == 'l')) {
            j=i+1;
            while((str[j] != '-') && (j<len)) { j++; };
            s_len = j - i - 2;
            memcpy(ipport_int, str+i+2, s_len);
            ipport_int[s_len] = 0;
            if (parse_debug) {
                printf("ipport_int: %s\n", ipport_int);
            };    
        };
    };


    //
    // forming vcard
    //

    memcpy(remote_vcard, userid, 8);
    pos = 8;

    // always -s- node
    remote_vcard[8] = 0x01;
    pos++;

    parse_ipport(ipport_int, &addr, &port, parse_debug);
    if (parse_debug) {
        printf("addr: %08X\n", addr);
        printf("port: %02X\n", port);
    };
    memcpy(remote_vcard+pos, &addr, 4);
    pos+=4;
    memcpy(remote_vcard+pos, &port, 2);
    pos+=2;

    parse_ipport(ipport_sup, &addr, &port, parse_debug);
    if (parse_debug) {
        printf("addr: %08X\n", addr);
        printf("port: %02X\n", port);
    };
    memcpy(remote_vcard+pos, &addr, 4);
    pos+=4;
    memcpy(remote_vcard+pos, &port, 2);
    pos+=2;

    // saving supernode ip for udp target ip:port
    strcpy(destip, ipport_sup);
    len = strlen(destip);
    while((i<len) && (destip[i] != ':')) { i++; };
    destip[i]=0;
    *destport = ntohs(port);

    parse_ipport(ipport_ext, &addr, &port, parse_debug);
    if (parse_debug) {
        printf("addr: %08X\n", addr);
        printf("port: %02X\n", port);
    };
    memcpy(remote_vcard+pos, &addr, 4);
    pos+=4;
    memcpy(remote_vcard+pos, &port, 2);
    pos+=2;

    // remote card 27 bytes len
    if (parse_debug) {
        show_memory(remote_vcard, 0x1B, "remote_vcard:");
    };

    return 0;
};


int skyrelay_main(char *myip, char *remote_name, char *vcard, char *output) {
    int ret;
    char destip[1024];
    unsigned short destport;
    int conn_id;
    int relay_id;
    unsigned int ip_int;
    char relayip[0x100];
    unsigned short relayport;
    char remote_vcard[1024];
    
    mip = mirsys(100, 0);
    srand( time(NULL) );

    tcp_talk_init();

    REMOTE_VERSION_LEN = 0;
    memset(REMOTE_VERSION, 0x00, sizeof(REMOTE_VERSION));

    parse_input(vcard, &remote_vcard, &destip, &destport);

    printf("destip: %s\n", destip);
    printf("destport: %d\n", destport);
    printf("remote name: %s\n", remote_name);
    show_memory(remote_vcard, 0x1B, "remote_vcard:");


    ret = main_skypush_getrelays(myip, destip, destport);
    if (ret <= 0) {
        printf("Get relays failed.\n");
        return -1;
    };

    relay_id = main_skyrel(&conn_id);
    if (conn_id <= 0) {
        printf("Relay connection failed.\n");
        return -1;
    };

    ip_int = relays.relay[relay_id].ip;
    sprintf(relayip, "%u.%u.%u.%u", ip_int>>24, (ip_int>>16)&0xFF, (ip_int>>8)&0xFF, ip_int&0xFF);
    relayport = relays.relay[relay_id].port;

    ret = main_skypush_connect(myip, destip, destport, relayip, relayport, conn_id, remote_name, remote_vcard);
    if (ret <= 0) {
        printf("Skype udp push packet failed.\n");
        return -1;
    };

    skyrel_answer();

    if (REMOTE_VERSION_LEN > 0) {
        memcpy(output, REMOTE_VERSION, REMOTE_VERSION_LEN);
    };

    tcp_talk_deinit();

    return 0;
};


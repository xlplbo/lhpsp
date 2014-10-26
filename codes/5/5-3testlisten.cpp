#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool stop = false;
static void handle_term( int sig )
{
	printf("cathy a stop sig:%d, and process pid:%d exit ...\n", sig, getpid());
    stop = true;
}

/*
 * $ a.out 127.0.0.1 8989
 * $ ps -ef|grep a.out
 * $ kill pid 默认产生SIGTERM信号
 *
 */

int main( int argc, char* argv[] )
{

	printf("process pid:%d create ...\n", getpid());
    
	signal( SIGTERM, handle_term ); //注册软件终止信号处理函数

    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int sock = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sock >= 0 );

    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int ret = bind( sock, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( sock, SOMAXCONN );
    assert( ret != -1 );

    while ( ! stop )
    {
        sleep( 1 );
    }

    close( sock );
    return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

static const char* request = "GET http://localhost/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxx";

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epoll_fd, int fd )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

/* 写入给定长度的字符 */
bool write_nbytes( int sockfd, const char* buffer, int len )
{
    int bytes_write = 0;
    printf( "write out %d bytes to socket %d\n", len, sockfd );
    while( 1 ) 
    {   
        bytes_write = send( sockfd, buffer, len, 0 );
        if ( bytes_write == -1 )
        {   
            return false;
        }   
        else if ( bytes_write == 0 ) 
        {   
            return false;
        }   

        len -= bytes_write;
        buffer = buffer + bytes_write;
        if ( len <= 0 ) 
        {   
            return true;
        }   
    }   
}

bool read_once( int sockfd, char* buffer, int len )
{
    int bytes_read = 0;
    memset( buffer, '\0', len );
    bytes_read = recv( sockfd, buffer, len, 0 );
    if ( bytes_read == -1 )
    {
        return false;
    }
    else if ( bytes_read == 0 )
    {
        return false;
    }
	printf( "read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer );

    return true;
}

void start_conn( int epoll_fd, int num, const char* ip, int port )
{
    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    for ( int i = 0; i < num; ++i )
    {
        sleep( 1 );
        int sockfd = socket( PF_INET, SOCK_STREAM, 0 );
        
        if( sockfd < 0 )
        {
            continue;
        }

        if (  connect( sockfd, ( struct sockaddr* )&address, sizeof( address ) ) < 0  )
        {
            printf( "connect failed %d\n", i );
        }
        addfd( epoll_fd, sockfd ); // 所有的sockfd都注册到epoll中管理
		printf("build connection %d\n", i);
    }
}

void close_conn( int epoll_fd, int sockfd )
{
    epoll_ctl( epoll_fd, EPOLL_CTL_DEL, sockfd, 0 );
    close( sockfd );
}

int main( int argc, char* argv[] )
{
    assert( argc == 4 );
    int epoll_fd = epoll_create( 100 );
    /* ip = argv[1] 测试Ip地址
	 * port = argv[2] 测试port端口
	 * count = argv[3] 测试连接数量
	 */
	start_conn( epoll_fd, atoi( argv[ 3 ] ), argv[1], atoi( argv[2] ) );
    epoll_event events[ 10000 ];
    char buffer[ 2048 ];
    while ( 1 )
    {
		/* epoll_wait超时时间为2000
		 * 因为所有的connect成功后的sockfd都被epoll管理
		 * 所以必须设定超时时间，以正确的关闭tcp连接
		 * */
        int fds = epoll_wait( epoll_fd, events, 10000, 2000 ); 
        for ( int i = 0; i < fds; i++ )
        {   
            int sockfd = events[i].data.fd;
            if ( events[i].events & EPOLLIN )
            {   
				/* 读取成功，关闭连接 */
                if ( ! read_once( sockfd, buffer, 2048 ) )
                {
                    close_conn( epoll_fd, sockfd );
                }
				/* 注册写事件 */
                struct epoll_event event;
                event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl( epoll_fd, EPOLL_CTL_MOD, sockfd, &event );
            }
            else if( events[i].events & EPOLLOUT ) 
            {
				/* 写入成功，关闭连接*/
                if ( ! write_nbytes( sockfd, request, strlen( request ) ) )
                {
                    close_conn( epoll_fd, sockfd );
                }
				/* 注册读事件 */
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl( epoll_fd, EPOLL_CTL_MOD, sockfd, &event );
            }
            else if( events[i].events & EPOLLERR )
            {
				/* 发生错误关闭连接 */
                close_conn( epoll_fd, sockfd );
            }
        }
    }
}


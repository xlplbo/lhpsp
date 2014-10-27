#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024
struct fds
{
   int epollfd;
   int sockfd;
   //pthread_mutex_t mutex;
};

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epollfd, int fd, bool oneshot )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if( oneshot )
    {
		/* 该文件描述符fd只会被触发一次
		 * 保证只有一个线程处理fd
		 * 处理完成之后需重新EPOLL_CTL_MOD注册事件保证下一次能被触发到
		 */
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

/* 保证下一次能被触发到 */
void reset_oneshot( int epollfd, int fd )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl( epollfd, EPOLL_CTL_MOD, fd, &event );
}

void* worker( void* arg )
{
    int sockfd = ( (fds*)arg )->sockfd;
    int epollfd = ( (fds*)arg )->epollfd;
	//pthread_mutex_t mutex = ( (fds*)arg)->mutex;
	//pthread_mutex_lock(&mutex);
    printf( "start new thread to receive data on fd: %d\n", sockfd );
    char buf[ BUFFER_SIZE ];
    memset( buf, '\0', BUFFER_SIZE );
    while( 1 )
    {
        int ret = recv( sockfd, buf, BUFFER_SIZE-1, 0 );
        if( ret == 0 )
        {
            close( sockfd );
            printf( "foreiner closed the connection\n" );
            break;
        }
        else if( ret < 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
				/* read完后重置EPOLLONESHOT属性 */
		        reset_oneshot( epollfd, sockfd );
				//pthread_mutex_unlock(&mutex);
				printf( "read later\n" );
                break;
            }
        }
        else
        {
            printf( "get content: %s\n", buf );
            sleep( 5 );
			/* EPOLLONESHOT对socked文件描述符是独占的
			 * 一旦业务逻辑处理事件过长将会导致不会响应下一次read事件
			 * 导致数据丢失
			 * 所以这种方法是不可取的
			 */
        }
    }
    printf( "end thread receiving data on fd: %d\n", sockfd );

	return NULL;
}

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create( 5 );
    assert( epollfd != -1 );
    addfd( epollfd, listenfd, false );

	//pthread_mutex_t mutex;
    while( 1 )
    {
        int ret = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ret < 0 )
        {
            printf( "epoll failure\n" );
            break;
        }
    
        for ( int i = 0; i < ret; i++ )
        {
            int sockfd = events[i].data.fd;
            if ( sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                addfd( epollfd, connfd, true ); // 设置为EPOLLONESHOT
				//addfd( epollfd, connfd, false);
            }
            else if ( events[i].events & EPOLLIN )
            {
				/* worker没有执行完read事件不会再在该sockfd上触发
				 * 数据被丢弃
				 */
				printf("epoll cathy event: EPOLLIN\n");
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;
				//fds_for_new_worker.mutex = mutex;
                pthread_create( &thread, NULL, worker, ( void* )&fds_for_new_worker );
            }
            else
            {
                printf( "something else happened \n" );
            }
        }
    }

    close( listenfd );
    return 0;
}

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "log.h"

static int level = LOG_INFO;
static int LOG_BUFFER_SIZE = 2048;
static const char* loglevels[] =
{
    "emerge!", "alert!", "critical!", "error!", "warn!", "notice:", "info:", "debug:"
};

/* 设置记录日志的等级 */
void set_loglevel( int log_level )
{
    level = log_level;
}

/* log_level记录log等级
 * file_name记录日志的源文件
 * line_num记录日志的源文件所在行
 * format格式化
 * ...可变参数
 * 日志输出到stdout
 */
void log( int log_level,  const char* file_name, int line_num, const char* format, ... )
{
    if ( log_level > level )
    {
        return;
    }

    time_t tmp = time( NULL );
    struct tm* cur_time = localtime( &tmp );
    if ( ! cur_time )
    {
        return;
    }

    char arg_buffer[ LOG_BUFFER_SIZE ];
    memset( arg_buffer, '\0', LOG_BUFFER_SIZE );
    strftime( arg_buffer, LOG_BUFFER_SIZE - 1, "[ %x %X ] ", cur_time );
    printf( "%s", arg_buffer );
    printf( "%s:%04d ", file_name, line_num );
    printf( "%s ", loglevels[ log_level - LOG_EMERG ] );

    va_list arg_list;
    va_start( arg_list, format );
    memset( arg_buffer, '\0', LOG_BUFFER_SIZE );
    vsnprintf( arg_buffer, LOG_BUFFER_SIZE - 1, format, arg_list );
    printf( "%s\n", arg_buffer );
    fflush( stdout );
    va_end( arg_list );
}

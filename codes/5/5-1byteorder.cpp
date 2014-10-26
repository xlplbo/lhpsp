#include <stdio.h>
/*
 *大端模式（网络字节序）低位高字节，高位低字节
 *小断模式（主机字节序）低位低字节，高位高字节， x86机器
 *
 */
void byteorder()
{
	union
	{
		short value;
		char union_bytes[ sizeof( short ) ];
	} test;
	test.value = 0x0102;
	if (  ( test.union_bytes[ 0 ] == 1 ) && ( test.union_bytes[ 1 ] == 2 ) )
	{
		printf( "big endian\n" );
	}
	else if ( ( test.union_bytes[ 0 ] == 2 ) && ( test.union_bytes[ 1 ] == 1 ) )
	{
		printf( "little endian\n" );
	}
	else
	{
		printf( "unknown...\n" );
	}
}

int main(int agrc, char* argv[])
{
	byteorder();
	return 0;
}

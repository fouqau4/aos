#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <unistd.h>
using namespace std;

// define maximum of clients as n*3, n clients for each group
#define MAX_CLIENT_NUM 8*3
#define MAX_BUFFER_LEN 128+2

void run_srv( uint16_t srv_port );
void run_cli( string srv_ip, uint16_t srv_port );
void *srv_to_cli( void *args )
{
	int fd = *( int* )args;
	system( "cd srv; pwd" );
	
	char buffer_write[MAX_BUFFER_LEN], buffer_read[MAX_BUFFER_LEN];
	int msg_len, current_receive_len, total_receive_len;

	while( ( total_receive_len = read( fd, buffer_read, MAX_BUFFER_LEN ) ) != 0 )
	{
		sscanf( buffer_read, "%2x", &msg_len );
		while( total_receive_len < msg_len + 2 )
		{
			current_receive_len = read( fd, buffer_read + total_receive_len, MAX_BUFFER_LEN - total_receive_len );
			total_receive_len += current_receive_len;
		}
		cout << msg_len << " " << buffer_read << endl;
	}
}

int main()
{
	system("clear");
	// for user to select the execution role
	int option;

	cout << " * Welcome to the System " << endl
		 << " * please select the role " << endl
		 << " * [0] for SERVER " << endl
		 << " * [1] for CLIENT " << endl
		 << " > ";

	cin >> option;
	
	uint16_t srv_port;
	string srv_ip;
	switch( option )
	{
	case 0:
		cout << " * Please enter the listenning port : ";
		cin >> srv_port;
		run_srv( srv_port );
		break;
	case 1:
		cout << " * Please enter the Server's IP : ";
		cin >> srv_ip;
		cout << " * Please enter the Server's port : ";
		cin >> srv_port;
		run_cli( srv_ip, srv_port );
		break;
	default:
		cout << " * Please enter 0 or 1 " << endl;
	}

	return 0;
}

void run_cli( string srv_ip, uint16_t srv_port )
{
	cout << " [C] Run as Client " << endl;
	int fd;
	puts("create socket");
	//create socket
	if( ( fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == -1 )
	{
		fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
		perror("");
		exit( 1 );
	}
	
	struct sockaddr_in srv;
	srv.sin_family = AF_INET;
	srv.sin_port = htons( srv_port );
	srv.sin_addr.s_addr = inet_addr( srv_ip.c_str() );

	puts("connect");
	if( connect( fd, ( struct sockaddr* ) &srv, sizeof( srv ) ) < 0 )
	{
		fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
		perror("");
		exit( 1 );
	}

	char buffer_write[MAX_BUFFER_LEN], buffer_read[MAX_BUFFER_LEN];
	
	getchar();
	fgets( buffer_write + 3, MAX_BUFFER_LEN - 1, stdin );
	( buffer_write + 3 )[strlen( buffer_write + 3 ) - 1] = '\0';
	sprintf( buffer_write, "%2x", strlen( buffer_write + 3 ) );
	for( int i = 0 ; i < strlen( buffer_write + 3 ) + 3; i++)
		cout<<buffer_write[i]<<" ";

//	while( write( fd, buffer_write, ) )
	puts( "HI I'm Client.");

	close( fd );
}

void run_srv( uint16_t srv_port )
{
    system( "clear" );
    cout << " [S] Run as Server " << endl;

    //create listenning socket
    struct sockaddr_in srv, cli;
    int listen_fd;
    socklen_t cli_len = sizeof( cli );

    if( ( listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == -1 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl( INADDR_ANY );
    srv.sin_port = htons( srv_port );
	puts("bind");
    //bind
    if( bind( listen_fd, ( struct sockaddr* ) &srv, sizeof( srv ) ) < 0 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }
	puts("listen");
    //listen
    if( listen( listen_fd, 5 ) == -1 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

    int connect_fd;
	uint32_t current_clients_num = 0;
	pthread_t threadId_of_cli[MAX_CLIENT_NUM];

    while( 1 )
    {
		puts("accept");
        //accept
        if( ( connect_fd = accept( listen_fd, ( struct sockaddr* ) &cli, &cli_len ) ) == -1 )
        {
            fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
            perror("");
            exit( 1 );
        }

		puts("create threads");
		//create threads
		if( ( pthread_create( &threadId_of_cli[current_clients_num], NULL, srv_to_cli, ( void * )&connect_fd ) ) != 0 )
		{
			fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
			perror("");
			exit( 1 );
		}
		
		sleep( 1 );
    }

	close( listen_fd );
}


#include <cstdlib>
#include <cstdio>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

void run_srv();


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
	
	
	switch( option )
	{
	case 0:
		run_srv();
		break;
	default:
		cout << " * Please enter 0 or 1 " << endl;
	}

	return 0;
}
void run_srv()
{
    system( "clear; cd srv; pwd" );
    cout << " [S] Run as Server " << endl;

    //create listenning socket
    struct sockaddr_in srv, cli;
    int listen_fd;
    int cli_len = sizeof( cli );

    if( ( listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == -1 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl( INADDR_ANY );
    srv.sin_port = htons( atoi( srv_port ) );

    //bind
    if( bind( listen_fd, ( struct sockaddr* ) &srv, sizeof( srv ) ) < 0 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

    //listen
    if( listen( listen_fd, 5 ) == -1 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

    int temp_fd;

    while( 1 )
    {
        //accept
        if( ( temp_fd = accept( listen_fd, ( struct sockaddr* ) &cli, &cli_len ) ) == -1 )
        {
            fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
            perror("");
            exit( 1 );
        }

    }
}


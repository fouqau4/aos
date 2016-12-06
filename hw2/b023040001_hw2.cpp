#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <map>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <unistd.h>
using namespace std;

// define maximum of clients as n*3, n clients for each group
#define MAX_CLIENT_NUM 8*3
#define MAX_BUFFER_LEN 128+3
#define MAX_USERNAME_LEN 20
#define MAX_FILENAME_LEN 30
#define MAX_COMMAND_LEN 20
#define MAX_PERMISSION_LEN 6+1

struct status_node
{
	char status;
	int count;
};

map< string, struct status_node > files_status;

pthread_mutex_t lock_user_list;
pthread_mutex_t lock_operated_file;

void run_srv( uint16_t srv_port );
void run_cli( string srv_ip, uint16_t srv_port );
void write_all( int fd, char buffer_write[] );
int read_all( int fd, char buffer_read[] );
int identify_command( char *command );

void *srv_to_cli( void *args )
{
	int fd = *( int* )args;	
	
	char buffer_write[MAX_BUFFER_LEN], buffer_read[MAX_BUFFER_LEN], username[MAX_USERNAME_LEN];

	//user login
	FILE *user_list;

	read_all( fd, buffer_read );
	strncpy( username, buffer_read + 3, MAX_USERNAME_LEN - 1 );

	int find = 0, group;
	pthread_mutex_lock( &lock_user_list );
	if( ( user_list = fopen( "srv/user_list", "a+" ) ) == NULL )
	{
		fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
	    perror("");
		pthread_mutex_unlock( &lock_user_list );
    	exit( 1 );
	}
	else
	{
		rewind( user_list );
		while( ( fscanf( user_list, "%d %s", &group, buffer_read ) ) == 2 )
		{
			if( strncmp( buffer_read, username, strlen( username ) ) == 0 )
			{
				cout << " [S] Old user : " << username << endl;
				find = 1;

				memset( buffer_write, 0, MAX_BUFFER_LEN );
				sprintf( buffer_write + 3, "%d", group );
				write_all( fd, buffer_write );
				break;
			}
		}
		if( !find )
		{
			memset( buffer_write, 0, MAX_BUFFER_LEN );
			sprintf( buffer_write + 3, "new " );
			write_all( fd, buffer_write );

			read_all( fd, buffer_read );
			sscanf( buffer_read + 3, "%d", &group );

			fseek( user_list, 0, SEEK_END );
			fprintf( user_list, "%d %s\n", group, username );
			fclose( user_list );
			cout << " [S] New user : " << username << endl;
			
			memset( buffer_read, 0, MAX_BUFFER_LEN );
			sprintf( buffer_read, "rm -rf cli/%s; cd cli; mkdir %s", username, username );
			system( buffer_read );
		}
	}
	pthread_mutex_unlock( &lock_user_list );

	//identify the command
	char *temp, command[MAX_COMMAND_LEN], filename[MAX_FILENAME_LEN], permission[MAX_PERMISSION_LEN], owner[MAX_USERNAME_LEN], write_mode;
	string current_file;
	int access, file_group;
	FILE *current_operated_file;
	cout << " * Read command : ";
	while( read_all( fd, buffer_read ) != 0 )
	{
		puts( buffer_read + 3 );
		temp = strtok( buffer_read + 3, " \t\n\0" );
		if( temp != NULL )
		{
			memset( command, 0, MAX_COMMAND_LEN );
			strncpy( command, temp, strlen( temp ) );
			switch( identify_command( command ) )
			{
			//new
			case 0:
				//filename
				temp = strtok( NULL, " \t\n\0" );
				if( temp != NULL )
				{
					memset( filename, 0, MAX_FILENAME_LEN );
					strncpy( filename, temp, strlen( temp ) );
					//permission
					temp = strtok( NULL, " \t\n\0" );
					if( temp != NULL )
					{
						memset( permission, 0, MAX_PERMISSION_LEN );
						strncpy( permission, temp, strlen( temp ) );
						permission[6] = '\0';

						memset( buffer_read, 0, MAX_BUFFER_LEN );
						sprintf( buffer_read, "cd srv; touch %s ;echo %s %s %d > .%s", filename, permission, username, group, filename );
						system( buffer_read );

						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write + 3, "ls -l srv/%s ", filename );
						write_all( fd, buffer_write );
					}
					else
					{
						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write +3, "echo [C] Wrong command " );
						write_all( fd, buffer_write );
					}
				}
				else
                {
                    memset( buffer_write, 0, MAX_BUFFER_LEN );
                    sprintf( buffer_write +3, "echo [C] Wrong command " );
                    write_all( fd, buffer_write );
                }
				break;
			//read
			case 1:
				//filename
				temp = strtok( NULL, " \t\n\0" );
				if( temp != NULL )
				{
					memset( filename, 0, MAX_FILENAME_LEN );
					strncpy( filename, temp, strlen( temp ) );
					current_file = filename;

					access = 0;

					while( !access )
					{
						pthread_mutex_lock( &lock_operated_file );
						switch( files_status[current_file].status )
						{
						case 0:
							files_status[current_file].status = 'r';
						case 'r':
							files_status[current_file].count++;
							access = 1;
							break;
						case 'w':
							pthread_mutex_unlock( &lock_operated_file );
							sleep( 1 );
							continue;
						}
						pthread_mutex_unlock( &lock_operated_file );
					}

					memset( buffer_write, 0, MAX_BUFFER_LEN );
					sprintf( buffer_write, "srv/.%s", filename );
					if( ( current_operated_file = fopen( buffer_write, "r" ) ) == NULL )
					{
						fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
				        perror("");
				        exit( 1 );						
					}

					memset( permission, 0, MAX_PERMISSION_LEN );
					memset( owner, 0, MAX_USERNAME_LEN );
					fscanf( current_operated_file, "%s %s %d", permission, owner, &file_group );
					fclose( current_operated_file );

					if( ( ( strncmp( owner, username, strlen( username ) ) == 0 ) && ( permission[0] == 'r' ) ) ||
						( ( file_group == group ) && ( permission[2] == 'r' ) ) ||
						( permission[4] == 'r' ) )
					{
						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write, "cp srv/%s cli/%s/%s", filename, username, filename );
						system( buffer_write );

						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write + 3, "ls -l cli/%s/%s ", username, filename );
						puts( buffer_write + 3 );
						write_all( fd, buffer_write );

						pthread_mutex_lock( &lock_operated_file );
						if( --files_status[current_file].count == 0 )
							files_status[current_file].status = '-';
						pthread_mutex_unlock( &lock_operated_file );
					}
					else
					{
						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write + 3, "echo \" [C] You did not get the access permission ! \"" );
						write_all( fd, buffer_write );
					}
				}
				else
				{
                    memset( buffer_write, 0, MAX_BUFFER_LEN );
                    sprintf( buffer_write +3, "echo [C] Wrong command " );
                    write_all( fd, buffer_write );
				}
				break;
				//write
				case 2:
				temp = strtok( NULL, " \t\n\0" );
				if( temp != NULL )
				{
					memset( filename, 0, MAX_FILENAME_LEN );
					strncpy( filename, temp, strlen( temp ) );

					temp = strtok( NULL, " \t\n\0" );
					if( temp != NULL )
					{
						write_mode = temp[0];

						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write, "srv/.%s", filename );
						if( ( current_operated_file = fopen( buffer_write, "r" ) ) == 0 )
						{
    	                    fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
	                        perror("");
                        	exit( 1 );
                   		}

						memset( owner, 0, MAX_USERNAME_LEN );
						memset( permission, 0, MAX_PERMISSION_LEN );
						fscanf( current_operated_file, "%s %s %d", permission, owner, &file_group );
						fclose( current_operated_file );

						if( ( ( strncmp( username, owner, strlen( owner ) ) == 0 ) && ( permission[1] == 'w' ) ) ||
							( ( group == file_group ) && ( permission[3] == 'w' ) ) ||
							( permission[5] == 'w' ) )
						{
							access = 0;
							current_file = filename;
							while( !access )
							{
								pthread_mutex_lock( &lock_operated_file );
								if( files_status[current_file].status == '-' || files_status[current_file].status == 0 )
								{
									files_status[current_file].status = 'w';
									pthread_mutex_unlock( &lock_operated_file );

			                        memset( buffer_write, 0, MAX_BUFFER_LEN );
									if( write_mode == 'o' )
				                        sprintf( buffer_write, "cp cli/%s/%s srv", username, filename );
									system( buffer_write );

									memset( buffer_write, 0, MAX_BUFFER_LEN );
									sprintf( buffer_write + 3, "ls -l srv/%s ", filename );
									write_all( fd, buffer_write );

									access = 1;
									pthread_mutex_lock( &lock_operated_file );
									files_status[current_file].status = '-';
									pthread_mutex_unlock( &lock_operated_file );
								}
								else
								{
									pthread_mutex_unlock( &lock_operated_file );
									sleep( 1 );
									continue;
								}
							}
						}
						else
						{
							memset( buffer_write, 0, MAX_BUFFER_LEN );
							sprintf( buffer_write + 3, "echo \" [C] You did not get the permission ! \"" );
							write_all( fd, buffer_write ); 
						}
					}
					else
					{
        	            memset( buffer_write, 0, MAX_BUFFER_LEN );
    	                sprintf( buffer_write +3, "echo [C] Wrong command " );
	                    write_all( fd, buffer_write );
					}
				}
				else
                {
                    memset( buffer_write, 0, MAX_BUFFER_LEN );
                    sprintf( buffer_write +3, "echo [C] Wrong command " );
                    write_all( fd, buffer_write );
                }

				break;
			default :
				cout << " [S] Unknown command " << endl;
			}
		}
		while( temp != NULL )
		{
			puts( temp );
			temp = strtok( NULL, " \t\n\0" );
		}
		
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

	pthread_mutex_init( &lock_user_list, NULL );
	pthread_mutex_init( &lock_operated_file, NULL );
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

	char buffer_write[MAX_BUFFER_LEN], buffer_read[MAX_BUFFER_LEN], username[MAX_USERNAME_LEN];

	getchar();

	cout << " [C] Username : ";
	fgets( username, MAX_USERNAME_LEN - 1, stdin );
	
	sprintf( buffer_write + 3, "%s", username );
	write_all( fd, buffer_write );

	int group;
	read_all( fd, buffer_read );
	if( strncmp( buffer_read + 3, "new", 3 ) == 0 )
	{
        cout << " [C] Please select your group : " << endl
             << "   0 : AOS_students " << endl
             << "   1 : CSE_students " << endl
             << "   2 : other_students " << endl;
        cin >> group;
		getchar();

		memset( buffer_write, 0, MAX_BUFFER_LEN );
		sprintf( buffer_write + 3, "%d", group );
		write_all( fd, buffer_write );
	}
	else
	{
		sscanf( buffer_read, "%d", &group );
	}
	
	switch( group )
	{
	case 0:
		cout << " [C] you are at the group : AOS_students " << endl;
		break;
	case 1:
		cout << " [C] you are at the group : CSE_students " << endl;
		break;
	case 2:
		cout << " [C] you are at the group : other_students " << endl;
		break;
	}

	while( fgets( buffer_write + 3, MAX_BUFFER_LEN - 1, stdin ) )
	{
		write_all( fd, buffer_write );
		memset( buffer_write, 0, MAX_BUFFER_LEN );

		read_all( fd, buffer_read );
		puts(buffer_read + 3);
		system( buffer_read + 3 );
	}
	puts( "HI I'm Client.");

	close( fd );
}

void write_all( int fd, char buffer_write[] )
{
	int msg_len, writen_len, current_writen_len;

	( buffer_write + 3 )[strlen( buffer_write + 3 ) - 1] = '\0';
    msg_len = strlen( buffer_write + 3 );
    sprintf( buffer_write, "%2x", msg_len );
    msg_len += 3;

    writen_len = 0;
    while( ( current_writen_len = write( fd, buffer_write + writen_len, msg_len - writen_len ) ) < msg_len - writen_len )
    {
        writen_len += current_writen_len;
    }

}

int read_all( int fd, char buffer_read[] )
{
	memset( buffer_read, 0, MAX_BUFFER_LEN );
	int total_receive_len, msg_len, current_receive_len;
	if( ( total_receive_len = read( fd, buffer_read, MAX_BUFFER_LEN ) ) != 0 )
    {
        sscanf( buffer_read, "%2x", &msg_len );
        while( total_receive_len < msg_len + 2 )
        {
            current_receive_len = read( fd, buffer_read + total_receive_len, MAX_BUFFER_LEN - total_receive_len );
            total_receive_len += current_receive_len;
        }
        // cout << msg_len << " " << buffer_read << endl << buffer_read + 3<< endl;
		return 1;
    }
	else
		return 0;

}

int identify_command( char *command )
{
	if( strncmp( command, "new", 3 ) == 0 )
		return 0;
	else if( strncmp( command, "read", 4 ) == 0 )
		return 1;
	else if( strncmp( command, "write", 5 ) == 0 )
        return 2;
	else if( strncmp( command, "change", 6 ) == 0 )
        return 3;
	else if( strncmp( command, "information", 11 ) == 0 )
        return 4;
	else
		return -1;
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

